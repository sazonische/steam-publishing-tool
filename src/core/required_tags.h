#pragma once

#include "core/game_profile.h"

#include <string>
#include <string_view>
#include <vector>

// Tags a game insists on for its workshop items: the profile's list plus whatever
// gameinfo.gi (CS2WorkshopManager/RequiredTag) names. The game file wins over the
// profile because Valve changes it without telling anyone.
namespace RequiredTags {

	std::vector<std::string> Resolve(const GameProfile& gameProfile);

	// Required tags the item lacks, compared case-insensitively (Steam lower-cases tags).
	std::vector<std::string> Missing(const std::vector<std::string>& requiredTags, const std::vector<std::string>& itemTags);

	bool ContainsTag(const std::vector<std::string>& tags, std::string_view wanted);

} // namespace RequiredTags
