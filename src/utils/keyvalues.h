#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Minimal text KeyValues (VDF) parser: libraryfolders.vdf, appmanifest_*.acf,
// publish_data.txt, addoninfo.txt. Supports quotes with escapes, // comments,
// nested blocks; #base/#include directives and [$WIN32] conditions are skipped.
struct KeyValuesNode {
	std::string key;
	std::string value;
	std::vector<KeyValuesNode> children;
	bool isBlock{false};

	// VDF keys are case-insensitive — Steam itself writes them either way.
	const KeyValuesNode* Find(std::string_view childKey) const;
	std::string_view GetString(std::string_view childKey, std::string_view fallback = {}) const;
};

namespace KeyValues {
	std::optional<KeyValuesNode> ParseText(std::string_view text, std::string& errorMessage);
	std::optional<KeyValuesNode> ParseFile(const std::filesystem::path& filePath, std::string& errorMessage);
} // namespace KeyValues
