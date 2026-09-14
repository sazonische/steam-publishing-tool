#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <steam/steam_api.h>

// A new additional preview: a path to an image or a YouTube video ID.
struct WorkshopPreviewAddition {
	EItemPreviewType type{k_EItemPreviewType_Image};
	std::string pathOrVideoId;
};

// What to change on the item. Steam leaves unset optional fields alone — so one request
// serves both a title-only edit and a full re-upload.
struct WorkshopUpdateRequest {
	PublishedFileId_t publishedFileId{0};

	std::optional<std::string> title;
	std::optional<std::string> description;
	std::string changeNote;
	std::optional<ERemoteStoragePublishedFileVisibility> visibility;
	// The full tag list: SetItemTags replaces all tags of the item at once.
	std::optional<std::vector<std::string>> tags;

	// The main preview: a local file < 1 MB (JPG/PNG/GIF, GIF stays animated).
	std::optional<std::string> previewFilePath;
	// Indices of existing additional previews to remove; applied in descending order so
	// removals do not shift indices not yet processed.
	std::vector<uint32_t> removePreviewIndices;
	std::vector<WorkshopPreviewAddition> addPreviews;

	// Content folder for SetItemContent (for CS2 — the already built VPK chunks).
	std::optional<std::string> contentFolder;
};

struct WorkshopUpdateProgress {
	EItemUpdateStatus status{k_EItemUpdateStatusInvalid};
	uint64_t bytesProcessed{0};
	uint64_t bytesTotal{0};
};

// Steam limits for validation in the UI.
constexpr size_t WORKSHOP_TITLE_MAX_LENGTH = k_cchPublishedDocumentTitleMax - 1;
constexpr size_t WORKSHOP_DESCRIPTION_MAX_LENGTH = k_cchPublishedDocumentDescriptionMax - 1;
constexpr size_t WORKSHOP_CHANGE_NOTE_MAX_LENGTH = k_cchPublishedDocumentChangeDescriptionMax - 1;
constexpr int64_t WORKSHOP_PREVIEW_MAX_FILE_SIZE = 1024 * 1024;
constexpr size_t WORKSHOP_TAGS_MAX_COUNT = 100;
// cs2_workshop_manager refuses to build a VPK above 2.0 GB ("VPK: Exceeded 2.0 GB limit").
constexpr uint64_t WORKSHOP_CONTENT_MAX_SIZE = 2ULL * 1024 * 1024 * 1024;

// True when the request changes anything on the Workshop page. A change note alone does not count:
// Steam would record an update with nothing in it.
inline bool HasMetadataChanges(const WorkshopUpdateRequest& request) {
	return request.title.has_value() || request.description.has_value() || request.visibility.has_value() || request.tags.has_value() || request.previewFilePath.has_value() || !request.addPreviews.empty() || !request.removePreviewIndices.empty();
}
