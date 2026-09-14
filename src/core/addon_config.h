#pragma once

#include "core/game_profile.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Addon packing rules from gameinfo.gi (the AddonConfig/VpkDirectories block). This is the same list
// cs2_workshop_manager builds the workshop VPK from: only the listed folders and files go into
// the archive, everything else (_bakeresourcecache, tools_*.bin, ServerConfig.vdf) stays home.
struct AddonVpkRules {
	std::vector<std::string> includes; // paths relative to the addon root, folders or files
	std::vector<std::string> excludes; // win over include when the path lies inside
	std::string requiredTag;		   // CS2WorkshopManager/RequiredTag, "CS2" for CS2
};

namespace AddonConfig {
	// gameinfo lives at profile.gameInfoPath relative to the game root.
	std::optional<AddonVpkRules> Load(const GameProfile& profile, const std::filesystem::path& gamePath, std::string& errorMessage);

	// true — Valve would put a file with this relative path into the VPK.
	bool IsPathPacked(const AddonVpkRules& rules, const std::filesystem::path& relativePath);
} // namespace AddonConfig
