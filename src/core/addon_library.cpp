#include "core/addon_library.h"

#include "utils/keyvalues.h"

#include <charconv>
#include <cwctype>

namespace AddonLibrary {

	namespace {

		// NTFS directory order: UTF-16 code units compared after upper-casing, '/' between path segments.
		bool CompareLikeWindowsDirectory(const std::filesystem::path& left, const std::filesystem::path& right) {
			const std::wstring leftPath = left.generic_wstring();
			const std::wstring rightPath = right.generic_wstring();
			return std::ranges::lexicographical_compare(leftPath, rightPath, [](wchar_t a, wchar_t b) { return std::towupper(a) < std::towupper(b); });
		}

	} // namespace

	namespace {

		constexpr const char* PUBLISHED_VPKS_FOLDER = "vpks";
		constexpr const char* WORKSHOP_ITEMS_FOLDER = "workshop_items";
		constexpr const char* PUBLISH_DATA_FILE = "publish_data.txt";
		constexpr const char* ADDON_INFO_FILE = "addoninfo.txt";

		struct PublishedRecord {
			uint64_t publishedFileId{0};
			std::string title;
		};

		bool EqualsIgnoreCase(const std::string& left, std::string_view right) {
			return QString::fromStdString(left).compare(QtText::FromStringView(right), Qt::CaseInsensitive) == 0;
		}

		// vpks/<id>/publish_data.txt: "source_folder" -> addon folder. One addon may have been published
		// several times (tests, re-uploads) — the most recent publish_time wins.
		std::map<std::string, PublishedRecord> ReadPublishedRecords(const std::filesystem::path& addonsRoot) {
			std::map<std::string, PublishedRecord> recordsBySourceFolder;
			std::map<std::string, uint64_t> publishTimeBySourceFolder;

			std::error_code errorCode;
			const std::filesystem::path vpksRoot = addonsRoot / PUBLISHED_VPKS_FOLDER;
			for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(vpksRoot, errorCode)) {
				if (!entry.is_directory()) {
					continue;
				}
				uint64_t publishedFileId = 0;
				const std::string folderName = PathText::ToUtf8(entry.path().filename());
				if (std::from_chars(folderName.data(), folderName.data() + folderName.size(), publishedFileId).ec != std::errc()) {
					continue;
				}

				std::string errorMessage;
				const std::optional<KeyValuesNode> root = KeyValues::ParseFile(entry.path() / PUBLISH_DATA_FILE, errorMessage);
				const KeyValuesNode* publishData = root ? root->Find("publish_data") : nullptr;
				if (!publishData) {
					continue;
				}

				const std::string sourceFolder = QtText::FromStringView(publishData->GetString("source_folder")).toLower().toStdString();
				if (sourceFolder.empty()) {
					continue;
				}
				uint64_t publishTime = 0;
				const std::string_view publishTimeText = publishData->GetString("publish_time");
				std::from_chars(publishTimeText.data(), publishTimeText.data() + publishTimeText.size(), publishTime);

				const auto previousTimeIterator = publishTimeBySourceFolder.find(sourceFolder);
				if (previousTimeIterator != publishTimeBySourceFolder.end() && previousTimeIterator->second >= publishTime) {
					continue;
				}
				publishTimeBySourceFolder[sourceFolder] = publishTime;
				recordsBySourceFolder[sourceFolder] = {publishedFileId, std::string(publishData->GetString("title"))};
			}
			return recordsBySourceFolder;
		}

		bool ReadIsTemplate(const std::filesystem::path& addonDirectory) {
			std::string errorMessage;
			const std::optional<KeyValuesNode> root = KeyValues::ParseFile(addonDirectory / ADDON_INFO_FILE, errorMessage);
			const KeyValuesNode* addonInfo = root ? root->Find("AddonInfo") : nullptr;
			return addonInfo && addonInfo->GetString("IsTemplate") == "1";
		}

	} // namespace

	std::vector<AddonInfo> Enumerate(const std::filesystem::path& addonsRoot) {
		std::vector<AddonInfo> addons;

		std::error_code errorCode;
		if (!std::filesystem::is_directory(addonsRoot, errorCode)) {
			return addons;
		}

		const std::map<std::string, PublishedRecord> publishedRecords = ReadPublishedRecords(addonsRoot);

		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(addonsRoot, errorCode)) {
			if (!entry.is_directory()) {
				continue;
			}
			const std::string name = PathText::ToUtf8(entry.path().filename());
			if (EqualsIgnoreCase(name, PUBLISHED_VPKS_FOLDER) || EqualsIgnoreCase(name, WORKSHOP_ITEMS_FOLDER)) {
				continue;
			}

			AddonInfo addon;
			addon.name = name;
			addon.directory = entry.path();
			addon.isTemplate = ReadIsTemplate(entry.path());

			const auto recordIterator = publishedRecords.find(QString::fromStdString(name).toLower().toStdString());
			if (recordIterator != publishedRecords.end()) {
				addon.publishedFileId = recordIterator->second.publishedFileId;
				addon.publishedTitle = recordIterator->second.title;
			}
			addons.push_back(std::move(addon));
		}

		std::ranges::sort(addons, [](const AddonInfo& left, const AddonInfo& right) {
			return QString::fromStdString(left.name).compare(QString::fromStdString(right.name), Qt::CaseInsensitive) < 0;
		});
		return addons;
	}

	AddonManifest BuildManifest(const std::filesystem::path& addonDirectory, const AddonVpkRules& rules) {
		AddonManifest manifest;
		try {
			for (const std::filesystem::directory_entry& topLevel : std::filesystem::directory_iterator(addonDirectory)) {
				const std::filesystem::path topLevelRelative = topLevel.path().lexically_relative(addonDirectory);

				if (topLevel.is_regular_file()) {
					if (AddonConfig::IsPathPacked(rules, topLevelRelative)) {
						manifest.files.push_back({topLevelRelative, topLevel.file_size()});
					} else {
						manifest.skippedTopLevelEntries.push_back(topLevelRelative);
						manifest.skippedFiles.push_back(topLevelRelative);
					}
					continue;
				}
				if (!topLevel.is_directory()) {
					continue;
				}

				bool anyFilePacked = false;
				for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(topLevel.path())) {
					if (!entry.is_regular_file()) {
						continue;
					}
					const std::filesystem::path relativePath = entry.path().lexically_relative(addonDirectory);
					if (!AddonConfig::IsPathPacked(rules, relativePath)) {
						manifest.skippedFiles.push_back(relativePath);
						continue;
					}
					anyFilePacked = true;
					manifest.files.push_back({relativePath, entry.file_size()});
				}
				if (!anyFilePacked) {
					manifest.skippedTopLevelEntries.push_back(topLevelRelative);
				}
			}
		} catch (const std::filesystem::filesystem_error& error) {
			manifest.files.clear();
			manifest.errorMessage = error.what();
			return manifest;
		}

		// Windows directory order, like cs2_workshop_manager: upper-cased compare, "pranchas_5" before "prancha_textura5".
		std::ranges::sort(manifest.files, [](const AddonFileEntry& left, const AddonFileEntry& right) {
			return CompareLikeWindowsDirectory(left.relativePath, right.relativePath);
		});
		for (const AddonFileEntry& file : manifest.files) {
			manifest.totalSize += file.size;
		}
		return manifest;
	}

} // namespace AddonLibrary
