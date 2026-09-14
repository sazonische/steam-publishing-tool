#pragma once

// What the BSP tells us about a Source 1 (Portal 2) map: format version, mode from the spawns,
// the PTI relay and the tags derived from entities (as in p2-publishing-tool).
struct BspMapInfo {
	int32_t version{0};
	uint64_t fileSize{0};
	bool hasPlayerStart{false}; // info_player_start -> Singleplayer
	bool hasCoopSpawn{false};	// info_coop_spawn -> Cooperative (wins over Singleplayer)
	// @relay_pti_level_end: without it the map cannot be finished from the Community Test Chambers queue.
	bool hasPtiEndRelay{false};
	size_t entityCount{0};
	std::vector<std::string> suggestedTags;
};

namespace BspMap {
	constexpr int32_t PORTAL2_BSP_VERSION = 21;

	std::optional<BspMapInfo> Inspect(const std::filesystem::path& bspPath, std::string& errorMessage);
} // namespace BspMap
