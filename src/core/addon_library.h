#pragma once

#include "core/addon_config.h"

struct AddonInfo {
	std::string name; // folder name, also source_folder in publish_data.txt
	std::filesystem::path directory;
	bool isTemplate{false};					   // addoninfo.txt: IsTemplate 1 — never published
	std::optional<uint64_t> publishedFileId;   // from vpks/<id>/publish_data.txt when published before
	std::optional<std::string> publishedTitle; // title from the same file
};

struct AddonFileEntry {
	std::filesystem::path relativePath;
	uint64_t size{0};
};

struct AddonManifest {
	std::vector<AddonFileEntry> files;
	uint64_t totalSize{0};
	std::vector<std::filesystem::path> skippedTopLevelEntries; // root folders/files outside the rules
	std::vector<std::filesystem::path> skippedFiles;		   // including exclusions inside allowed folders
	std::string errorMessage;								   // an incomplete list must not be published
};

namespace AddonLibrary {
	// Addon folders without the service ones (vpks, workshop_items), linked to already published IDs.
	std::vector<AddonInfo> Enumerate(const std::filesystem::path& addonsRoot);

	AddonManifest BuildManifest(const std::filesystem::path& addonDirectory, const AddonVpkRules& rules);
} // namespace AddonLibrary
