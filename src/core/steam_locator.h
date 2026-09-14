#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Finds the Steam installation and games the same way the client does: registry -> steamapps ->
// libraryfolders.vdf (libraries on other drives) -> appmanifest_<appid>.acf -> installdir.
namespace SteamLocator {
	// HKCU\Software\Valve\Steam\SteamPath, then HKLM\SOFTWARE\WOW6432Node\Valve\Steam\InstallPath.
	std::optional<std::filesystem::path> FindSteamInstallPath();
	bool IsSteamInstallPath(const std::filesystem::path& steamPath, std::string& problem);

	// All Steam libraries including the main <steamPath>/steamapps. Paths are normalised,
	// missing directories dropped (a disconnected external drive is a normal situation).
	std::vector<std::filesystem::path> FindLibraryFolders(const std::filesystem::path& steamPath);

	// <library>/steamapps/common/<installdir> from the manifest. nullopt — the game is not installed.
	std::optional<std::filesystem::path> FindGameInstallPath(const std::filesystem::path& steamPath, uint32_t appId);
	// Same, but when the manifest cannot be read (Steam rewrites appmanifest_<appid>.acf the moment
	// a Steamworks app starts, and holds it for a few milliseconds) the folder is retried and then
	// looked up as steamapps/common/<installFolder> in every library.
	std::optional<std::filesystem::path> FindGameInstallPath(const std::filesystem::path& steamPath, uint32_t appId, std::string_view installFolder);
} // namespace SteamLocator
