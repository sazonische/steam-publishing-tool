#include "core/bsp_map.h"

namespace BspMap {

	namespace {

		constexpr uint32_t BSP_IDENT = 0x50534256; // 'VBSP'
		constexpr size_t BSP_LUMP_COUNT = 64;
		constexpr size_t ENTITY_LUMP_INDEX = 0;
		constexpr size_t MAX_ENTITY_LUMP_SIZE = 64ULL * 1024 * 1024;

#pragma pack(push, 1)
		struct BspLump {
			int32_t fileOffset;
			int32_t fileLength;
			int32_t version;
			char fourCC[4];
		};

		struct BspHeader {
			uint32_t ident;
			int32_t version;
			std::array<BspLump, BSP_LUMP_COUNT> lumps;
			int32_t mapRevision;
		};
#pragma pack(pop)

		using Entity = std::map<std::string, std::string>;

		// The entity lump is text like { "key" "value" ... } { ... }; keys inside one entity may
		// repeat (outputs), but we only need classname/targetname/painttype and the like.
		std::vector<Entity> ParseEntityLump(std::string_view text) {
			std::vector<Entity> entities;
			size_t position = 0;

			auto readQuoted = [&](std::string& out) -> bool {
				while (position < text.size() && text[position] != '"' && text[position] != '}') {
					++position;
				}
				if (position >= text.size() || text[position] == '}') {
					return false;
				}
				const size_t end = text.find('"', position + 1);
				if (end == std::string_view::npos) {
					return false;
				}
				out.assign(text.substr(position + 1, end - position - 1));
				position = end + 1;
				return true;
			};

			while (position < text.size()) {
				const size_t open = text.find('{', position);
				if (open == std::string_view::npos) {
					break;
				}
				position = open + 1;
				Entity entity;
				std::string key;
				std::string value;
				while (readQuoted(key) && readQuoted(value)) {
					entity.emplace(std::move(key), std::move(value));
				}
				const size_t close = text.find('}', position);
				if (close == std::string_view::npos) {
					break;
				}
				position = close + 1;
				entities.push_back(std::move(entity));
			}
			return entities;
		}

		std::string_view Get(const Entity& entity, const char* key) {
			const auto iterator = entity.find(key);
			return iterator == entity.end() ? std::string_view() : std::string_view(iterator->second);
		}

		void AddTag(std::vector<std::string>& tags, const char* tag) {
			if (std::ranges::find(tags, tag) == tags.end()) {
				tags.emplace_back(tag);
			}
		}

		// Entity -> element tag mapping, as in elements.kv of p2-publishing-tool.
		void CollectElementTags(const Entity& entity, std::vector<std::string>& tags) {
			const std::string_view className = Get(entity, "classname");
			if (className == "npc_portal_turret_floor") {
				AddTag(tags, "Turret");
			} else if (className == "prop_tractor_beam") {
				AddTag(tags, "Tractor Beam");
			} else if (className == "trigger_catapult") {
				AddTag(tags, "Faith Plate");
			} else if (className == "prop_laser_relay") {
				AddTag(tags, "Laser Relay");
			} else if (className == "env_portal_laser") {
				AddTag(tags, "Laser Emitter");
			} else if (className == "prop_wall_projector") {
				AddTag(tags, "Light Bridge");
			} else if (className == "logic_relay" && Get(entity, "disable_pti_audio") == "1") {
				AddTag(tags, "Custom Story");
			} else if (className == "paint_sphere" || className == "prop_paint_bomb" || className == "info_paint_sprayer") {
				const std::string_view paintType = Get(entity, "painttype");
				if (paintType == "0") {
					AddTag(tags, "Bounce Gel");
				} else if (paintType == "2") {
					AddTag(tags, "Speed Gel");
				} else if (paintType == "3") {
					AddTag(tags, "Conversion Gel");
				}
			}
		}

	} // namespace

	std::optional<BspMapInfo> Inspect(const std::filesystem::path& bspPath, std::string& errorMessage) {
		std::ifstream file(bspPath, std::ios::binary);
		if (!file) {
			errorMessage = std::format("cannot open {}", PathText::ToUtf8(bspPath));
			return std::nullopt;
		}

		BspHeader header{};
		file.read(reinterpret_cast<char*>(&header), sizeof(header));
		if (!file || header.ident != BSP_IDENT) {
			errorMessage = "not a Source BSP file (VBSP signature missing)";
			return std::nullopt;
		}

		BspMapInfo info;
		info.version = header.version;
		std::error_code errorCode;
		info.fileSize = std::filesystem::file_size(bspPath, errorCode);

		const BspLump& entityLump = header.lumps[ENTITY_LUMP_INDEX];
		if (entityLump.fileOffset <= 0 || entityLump.fileLength <= 0 || static_cast<size_t>(entityLump.fileLength) > MAX_ENTITY_LUMP_SIZE) {
			errorMessage = "entity lump is missing or corrupt";
			return std::nullopt;
		}

		std::string entityText(static_cast<size_t>(entityLump.fileLength), '\0');
		file.seekg(entityLump.fileOffset);
		file.read(entityText.data(), entityLump.fileLength);
		if (!file) {
			errorMessage = "cannot read the entity lump";
			return std::nullopt;
		}
		// The lump ends with a NUL; anything after it is alignment garbage.
		if (const size_t terminator = entityText.find('\0'); terminator != std::string::npos) {
			entityText.resize(terminator);
		}

		const std::vector<Entity> entities = ParseEntityLump(entityText);
		info.entityCount = entities.size();
		for (const Entity& entity : entities) {
			const std::string_view className = Get(entity, "classname");
			if (className == "info_player_start") {
				info.hasPlayerStart = true;
			} else if (className == "info_coop_spawn") {
				info.hasCoopSpawn = true;
			}
			if (Get(entity, "targetname") == "@relay_pti_level_end") {
				info.hasPtiEndRelay = true;
			}
			CollectElementTags(entity, info.suggestedTags);
		}

		// Maps from Hammer are tagged Custom Visuals to tell them apart from Puzzle Maker builds.
		std::vector<std::string> categoryTags;
		if (info.hasCoopSpawn) {
			categoryTags.emplace_back("Cooperative");
		} else if (info.hasPlayerStart) {
			categoryTags.emplace_back("Singleplayer");
		}
		categoryTags.emplace_back("Custom Visuals");
		info.suggestedTags.insert_range(info.suggestedTags.begin(), categoryTags);
		return info;
	}

} // namespace BspMap
