#pragma once

#include <filesystem>
#include <string>

// steam_api64.dll exists only as a DLL; Valve does not ship a static version. To keep the exe
// a single file, the DLL is embedded as a resource and the imports are built with /DELAYLOAD:
// it must be loaded from here before the first Steamworks call, or delay-load searches PATH.
namespace SteamApiLibrary {
	// Priority: steam_api64.dll next to the exe (dev build, manual replacement) -> the embedded copy
	// unpacked into AppLocalData. The unpacked copy is overwritten when the bytes differ.
	bool Load(std::string& errorMessage);
	const std::filesystem::path& GetLoadedPath();
} // namespace SteamApiLibrary
