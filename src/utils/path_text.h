#pragma once

// path::string() is ANSI on Windows; Steam, the config and Qt are UTF-8.
namespace PathText {
	inline std::string ToUtf8(const std::filesystem::path& path) {
		const std::u8string text = path.u8string();
		return {text.begin(), text.end()};
	}

	inline std::string ToGenericUtf8(const std::filesystem::path& path) {
		const std::u8string text = path.generic_u8string();
		return {text.begin(), text.end()};
	}

	inline std::filesystem::path FromUtf8(std::string_view text) {
		return std::u8string(text.begin(), text.end());
	}
} // namespace PathText
