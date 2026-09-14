#pragma once

#include "core/game_profile.h"

// Packing rules from gameinfo.gi AddonConfig/VpkDirectories, the same list cs2_workshop_manager packs from.
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
