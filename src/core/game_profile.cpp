#include "core/game_profile.h"

namespace GameProfiles {

	const std::vector<GameProfile>& All() {
		// Tag lists come from the workshop filter sidebar of each game; the publish dialog lets
		// the user add free-form tags on top.
		static const std::vector<GameProfile> profiles = {
			{
				.appId = 730,
				.id = "cs2",
				.displayName = "Counter-Strike 2",
				.installFolder = "Counter-Strike Global Offensive",
				.installMarker = "game/csgo/gameinfo.gi",
				.gameInfoPath = "game/csgo/gameinfo.gi",
				.addonsRoot = "game/csgo_addons",
				.mapsFolder = "",
				.contentKind = WORKSHOP_CONTENT_SOURCE2_ADDON,
				.requiredTags = {"CS2", "Map"},
				.tagGroups = {
					{"Game Mode", {"Classic", "Deathmatch", "Demolition", "Armsrace", "Custom", "Training", "Co-op Strike", "Wingman", "Flying Scoutsman"}},
				},
				.tagsSectionTitle = "Tags and game modes",
			},
			{
				.appId = 620,
				.id = "portal2",
				.displayName = "Portal 2",
				.installFolder = "Portal 2",
				.installMarker = "portal2/gameinfo.txt",
				.gameInfoPath = "",
				.addonsRoot = "",
				.mapsFolder = "portal2/maps",
				.contentKind = WORKSHOP_CONTENT_SOURCE1_BSP,
				.requiredTags = {},
				// Categories — the Portal 2 workshop filter; Elements — tags p2-publishing-tool derives
				// from map entities (elements.kv). Pre-filled automatically from the BSP.
				.tagGroups = {
					{"Categories", {"Singleplayer", "Cooperative", "Custom Visuals", "Custom Story"}},
					{"Elements", {"Turret", "Bounce Gel", "Speed Gel", "Conversion Gel", "Tractor Beam", "Faith Plate", "Laser Relay", "Laser Emitter", "Light Bridge"}},
				},
				.tagsSectionTitle = "Tags, categories and elements",
			},
		};
		return profiles;
	}

	const GameProfile* FindById(std::string_view id) {
		for (const GameProfile& profile : All()) {
			if (profile.id == id) {
				return &profile;
			}
		}
		return nullptr;
	}

	const GameProfile* FindByAppId(uint32_t appId) {
		for (const GameProfile& profile : All()) {
			if (profile.appId == appId) {
				return &profile;
			}
		}
		return nullptr;
	}

} // namespace GameProfiles
