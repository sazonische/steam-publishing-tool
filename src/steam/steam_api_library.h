#pragma once

// Valve ships steam_api64.dll only as a DLL: embedded as a resource, imports /DELAYLOAD, Load() before the first Steamworks call.
namespace SteamApiLibrary {
	// DLL next to the exe wins, otherwise the embedded copy unpacked into AppLocalData (rewritten when the bytes differ).
	bool Load(std::string& errorMessage);
	const std::filesystem::path& GetLoadedPath();
} // namespace SteamApiLibrary
