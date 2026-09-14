#pragma once

#include "core/game_profile.h"

// Where the path came from. Priority top to bottom: command line argument, config.json, auto-detection.
enum PathSource : uint8_t {
	PATH_SOURCE_NONE = 0,
	PATH_SOURCE_COMMAND_LINE,
	PATH_SOURCE_CONFIG,
	PATH_SOURCE_AUTO,
};

std::string_view PathSourceName(PathSource source);

struct ResolvedPath {
	std::filesystem::path path;
	PathSource source{PATH_SOURCE_NONE};
	bool valid{false};
	std::string problem; // empty when valid
};

struct ResolvedGamePaths {
	ResolvedPath steam;
	ResolvedPath game;
	std::filesystem::path addonsRoot; // empty when the game has no addons or game is invalid
};

// Combines overrides and auto-detection into final paths. An explicitly set but broken path is
// not replaced by auto-detection: the user has to see that their setting does not work.
class CGamePaths : public Singleton<CGamePaths> {
public:
	void SetCommandLineSteamPath(std::optional<std::filesystem::path> steamPath) { _commandLineSteamPath = std::move(steamPath); }
	void SetCommandLineGamePath(std::optional<std::filesystem::path> gamePath) { _commandLineGamePath = std::move(gamePath); }

	ResolvedPath ResolveSteamPath() const;
	ResolvedGamePaths Resolve(const GameProfile& profile) const;

	static bool ValidateGamePath(const GameProfile& profile, const std::filesystem::path& gamePath, std::string& problem);

private:
	std::optional<std::filesystem::path> _commandLineSteamPath;
	std::optional<std::filesystem::path> _commandLineGamePath;
};

inline CGamePaths& GamePaths() { return CGamePaths::Instance(); }
