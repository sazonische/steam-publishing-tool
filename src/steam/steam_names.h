#pragma once

// Human-readable names of Steam enums for the log and user-facing messages.
namespace SteamNames {
	std::string_view Result(EResult result);
	std::string_view Visibility(ERemoteStoragePublishedFileVisibility visibility);
	std::string_view ItemUpdateStatus(EItemUpdateStatus status);
	std::string_view PreviewType(EItemPreviewType previewType);

	// Steam API language codes (the ones SetLanguage / SetItemUpdateLanguage accept) with a display name.
	struct WorkshopLanguage {
		std::string_view apiName;
		std::string_view displayName;
	};
	std::span<const WorkshopLanguage> WorkshopLanguages();
	std::string_view LanguageDisplayName(std::string_view apiName);
} // namespace SteamNames
