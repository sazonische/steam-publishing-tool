#pragma once

// publish_data.txt next to the VPK chunks in vpks/<id>/ — this is how cs2_workshop_manager remembers
// which addon folder an item was built from. Same format, so both tools understand each other.
struct PublishData {
	std::string title;
	std::string sourceFolder;
	uint64_t publishTime{0}; // unix time
};

namespace PublishDataFile {
	constexpr const char* FILE_NAME = "publish_data.txt";

	bool Write(const std::filesystem::path& directory, const PublishData& publishData, std::string& errorMessage);
} // namespace PublishDataFile
