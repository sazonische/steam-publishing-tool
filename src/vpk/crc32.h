#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

// CRC-32 (IEEE 802.3, polynomial 0xEDB88320) — the same as zlib and the one Valve writes into VPK entries.
namespace Crc32 {

	constexpr std::array<uint32_t, 256> BuildTable() {
		std::array<uint32_t, 256> table{};
		for (uint32_t index = 0; index < 256; ++index) {
			uint32_t value = index;
			for (int bit = 0; bit < 8; ++bit) {
				value = (value & 1U) != 0 ? (0xEDB88320U ^ (value >> 1U)) : (value >> 1U);
			}
			table[index] = value;
		}
		return table;
	}

	inline constexpr std::array<uint32_t, 256> TABLE = BuildTable();

	inline uint32_t Update(uint32_t crc, std::span<const std::byte> bytes) {
		uint32_t value = ~crc;
		for (const std::byte byte : bytes) {
			value = TABLE[(value ^ static_cast<uint8_t>(byte)) & 0xFFU] ^ (value >> 8U);
		}
		return ~value;
	}

} // namespace Crc32
