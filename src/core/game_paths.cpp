#include "core/game_paths.h"

#include "core/app_config.h"
#include "core/steam_locator.h"

namespace {

} // namespace

std::string_view PathSourceName(PathSource source) {
	switch (source) {
		case PATH_SOURCE_COMMAND_LINE: return "command line";
		case PATH_SOURCE_CONFIG: return "settings";
		case PATH_SOURCE_AUTO: return "detected via Steam";
		default: return "not set";
	}
}

ResolvedPath CGamePaths::ResolveSteamPath() const {
	ResolvedPath resolved;

	if (_commandLineSteamPath.has_value()) {
		resolved.path = *_commandLineSteamPath;
		resolved.source = PATH_SOURCE_COMMAND_LINE;
	} else if (const std::optional<std::string>& configuredPath = AppConfig().Data().steamPath; configuredPath.has_value() && !configuredPath->empty()) {
		resolved.path = PathText::FromUtf8(*configuredPath);
		resolved.source = PATH_SOURCE_CONFIG;
	} else if (const std::optional<std::filesystem::path> detectedPath = SteamLocator::FindSteamInstallPath()) {
		resolved.path = *detectedPath;
		resolved.source = PATH_SOURCE_AUTO;
	} else {
		resolved.problem = "Steam installation not found in the registry";
		return resolved;
	}

	resolved.valid = SteamLocator::IsSteamInstallPath(resolved.path, resolved.problem);
	return resolved;
}

ResolvedGamePaths CGamePaths::Resolve(const GameProfile& profile) const {
	ResolvedGamePaths resolved;
	resolved.steam = ResolveSteamPath();

	if (_commandLineGamePath.has_value()) {
		resolved.game.path = *_commandLineGamePath;
		resolved.game.source = PATH_SOURCE_COMMAND_LINE;
	} else if (const std::optional<std::string> configuredPath = AppConfig().GetGameInstallPath(profile.id); configuredPath.has_value() && !configuredPath->empty()) {
		resolved.game.path = PathText::FromUtf8(*configuredPath);
		resolved.game.source = PATH_SOURCE_CONFIG;
	} else if (resolved.steam.valid) {
		if (const std::optional<std::filesystem::path> detectedPath = SteamLocator::FindGameInstallPath(resolved.steam.path, profile.appId, profile.installFolder)) {
			resolved.game.path = *detectedPath;
			resolved.game.source = PATH_SOURCE_AUTO;
		} else {
			resolved.game.problem = std::format("AppID {} is not installed in any Steam library", profile.appId);
			return resolved;
		}
	} else {
		resolved.game.problem = "Steam path is unknown, cannot detect the game folder";
		return resolved;
	}

	resolved.game.valid = ValidateGamePath(profile, resolved.game.path, resolved.game.problem);
	if (resolved.game.valid && !profile.addonsRoot.empty()) {
		resolved.addonsRoot = resolved.game.path / profile.addonsRoot;
	}
	return resolved;
}

bool CGamePaths::ValidateGamePath(const GameProfile& profile, const std::filesystem::path& gamePath, std::string& problem) {
	std::error_code errorCode;
	if (gamePath.empty()) {
		problem = "path is empty";
		return false;
	}
	if (!std::filesystem::is_directory(gamePath, errorCode)) {
		problem = "directory does not exist";
		return false;
	}
	// The marker is a file only this game has: it guards against picking a neighbouring folder.
	if (!profile.installMarker.empty() && !std::filesystem::is_regular_file(gamePath / profile.installMarker, errorCode)) {
		problem = std::format("{} not found — this does not look like a {} folder", profile.installMarker, profile.displayName);
		return false;
	}
	problem.clear();
	return true;
}
