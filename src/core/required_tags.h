#pragma once

#include "core/game_profile.h"

// Required workshop tags: the profile list plus gameinfo.gi CS2WorkshopManager/RequiredTag, which wins.
namespace RequiredTags {

	std::vector<std::string> Resolve(const GameProfile& gameProfile);

	// Required tags the item lacks, compared case-insensitively (Steam lower-cases tags).
	std::vector<std::string> Missing(const std::vector<std::string>& requiredTags, const std::vector<std::string>& itemTags);

	bool ContainsTag(const std::vector<std::string>& tags, std::string_view wanted);

} // namespace RequiredTags
