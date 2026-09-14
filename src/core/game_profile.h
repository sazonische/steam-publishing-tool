#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

// How the game stores workshop content: this picks the packer and the set of editable fields.
enum WorkshopContentKind : uint8_t {
	WORKSHOP_CONTENT_GENERIC = 0,	// any folder through ISteamUGC::SetItemContent
	WORKSHOP_CONTENT_SOURCE2_ADDON, // addon folder -> VPK chunks in game/<addonsRoot>/vpks/<id>/ (CS2)
	WORKSHOP_CONTENT_SOURCE1_BSP,	// a single .bsp through legacy ISteamRemoteStorage (Portal 2)
};

// A tag group from the game's workshop filter. Empty for games with free-form tags.
struct WorkshopTagGroup {
	std::string_view title;
	std::vector<std::string_view> tags;
};

struct GameProfile {
	uint32_t appId{0};
	std::string_view id; // short key: the --game argument and the settings key
	std::string_view displayName;
	std::string_view installFolder; // folder name in steamapps/common
	std::string_view installMarker; // file relative to the game root that validates the path
	std::string_view gameInfoPath;	// gameinfo with the AddonConfig/VpkDirectories block (packing rules)
	std::string_view addonsRoot;	// relative to the game root; empty when there are no addons
	std::string_view mapsFolder;	// Source 1: where compiled .bsp files live (portal2/maps)
	WorkshopContentKind contentKind{WORKSHOP_CONTENT_GENERIC};
	// Tags without which the item is hidden from the in-game browser (CS2: "CS2" and "Map").
	// SetItemTags replaces the whole list, so they are always applied.
	std::vector<std::string_view> requiredTags;
	std::vector<WorkshopTagGroup> tagGroups;
	std::string_view tagsSectionTitle;
};

namespace GameProfiles {
	const std::vector<GameProfile>& All();
	const GameProfile* FindById(std::string_view id);
	const GameProfile* FindByAppId(uint32_t appId);
} // namespace GameProfiles
