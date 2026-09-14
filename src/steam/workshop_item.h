#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <steam/steam_api.h>

// An additional item preview: an image by URL or a YouTube/Sketchfab video ID.
struct WorkshopPreview {
	EItemPreviewType type{k_EItemPreviewType_Image};
	std::string urlOrVideoId;
	std::string originalFileName;
};

// A snapshot of a published item from SteamUGCDetails_t plus the data Steam returns
// through separate calls (preview URL, additional previews, tags as a list).
struct WorkshopItem {
	PublishedFileId_t publishedFileId{0};
	uint64_t ownerSteamId{0};
	AppId_t consumerAppId{0};

	std::string title;
	std::string description;
	std::vector<std::string> tags;
	ERemoteStoragePublishedFileVisibility visibility{k_ERemoteStoragePublishedFileVisibilityPrivate};

	uint32_t timeCreated{0};
	uint32_t timeUpdated{0};
	uint64_t totalFilesSize{0};
	int32_t previewFileSize{0};

	std::string previewUrl;
	std::vector<WorkshopPreview> additionalPreviews;

	// Statistics come from GetQueryUGCStatistic; zero when Steam has none for the item.
	uint64_t subscriptions{0};
	uint64_t favorites{0};
	uint64_t websiteViews{0};
	uint32_t votesUp{0};
	uint32_t votesDown{0};
	float score{0.0F};
	bool banned{false};
};
