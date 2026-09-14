#pragma once

#include "core/addon_config.h"
#include "core/app_config.h"
#include "core/game_profile.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Effective packing rules for a game: gameinfo.gi as is, or the user's own list from config.json
// topped up with Valve's new entries.
struct EffectiveUploadRules {
	AddonVpkRules rules;
	bool custom{false};
	std::vector<std::string> autoAdded; // what auto-sync added this time
	std::filesystem::path gameInfoPath;
};

namespace UploadRules {
	std::optional<EffectiveUploadRules> Resolve(const GameProfile& profile, const std::filesystem::path& gamePath, std::string& errorMessage);

	void CopyFromGameInfo(UploadRulesConfig& config, const AddonVpkRules& gameInfoRules);

	std::vector<std::string> SyncWithGameInfo(UploadRulesConfig& config, const AddonVpkRules& gameInfoRules);

	bool ContainsRule(const std::vector<std::string>& rules, std::string_view rule);
} // namespace UploadRules
