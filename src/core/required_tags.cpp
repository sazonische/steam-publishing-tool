#include "core/required_tags.h"

#include "core/addon_config.h"
#include "core/game_paths.h"

#include <algorithm>
#include <cctype>

namespace RequiredTags {

	namespace {

		bool EqualsIgnoreCase(std::string_view left, std::string_view right) {
			return std::ranges::equal(left, right, [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
		}

	} // namespace

	bool ContainsTag(const std::vector<std::string>& tags, std::string_view wanted) {
		return std::ranges::any_of(tags, [wanted](const std::string& tag) { return EqualsIgnoreCase(tag, wanted); });
	}

	std::vector<std::string> Resolve(const GameProfile& gameProfile) {
		std::vector<std::string> requiredTags;
		for (std::string_view tag : gameProfile.requiredTags) {
			requiredTags.emplace_back(tag);
		}

		const ResolvedGamePaths paths = GamePaths().Resolve(gameProfile);
		if (paths.game.valid && !gameProfile.gameInfoPath.empty()) {
			std::string errorMessage;
			const std::optional<AddonVpkRules> rules = AddonConfig::Load(gameProfile, paths.game.path, errorMessage);
			if (rules && !rules->requiredTag.empty() && !ContainsTag(requiredTags, rules->requiredTag)) {
				requiredTags.push_back(rules->requiredTag);
			}
		}
		return requiredTags;
	}

	std::vector<std::string> Missing(const std::vector<std::string>& requiredTags, const std::vector<std::string>& itemTags) {
		std::vector<std::string> missing;
		for (const std::string& tag : requiredTags) {
			if (!ContainsTag(itemTags, tag)) {
				missing.push_back(tag);
			}
		}
		return missing;
	}

} // namespace RequiredTags
