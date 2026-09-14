#pragma once

#include "steam/workshop_item.h"
#include "steam/workshop_update.h"

#include <QtCore/QObject>
#include <QtCore/QString>

#include <steam/steam_api.h>

#include <filesystem>
#include <functional>
#include <utility>
#include <vector>

// Asynchronous workshop operations on top of ISteamUGC. Results arrive through CCallResult
// in SteamAPI_RunCallbacks and are forwarded as Qt signals — the UI lives on signals only.
class CWorkshopService : public QObject {
	Q_OBJECT

public:
	explicit CWorkshopService(QObject* parent = nullptr);

	// Steam otherwise reads and writes item texts in the client's UI language, so a Russian client
	// would silently edit the Russian translation; the same code is used for both directions.
	void SetLanguage(std::string language);
	const std::string& GetLanguage() const { return _language; }

	void RequestPublishedItems();
	void DeleteItem(PublishedFileId_t publishedFileId);

	// Creates an empty workshop item; content and metadata are uploaded by a separate SubmitUpdate.
	void CreateItem();

	// Assembles StartItemUpdate + setters + SubmitItemUpdate. false — the request was rejected before
	// sending (invalid preview file, too many tags and so on); the reason is in errorMessage.
	bool SubmitUpdate(const WorkshopUpdateRequest& request, QString& errorMessage);

	// Legacy Source 1 path (Portal 2): the file first goes to the user's Steam Cloud, then is
	// published through ISteamRemoteStorage::PublishWorkshopFile or swapped on an existing item
	// through CreatePublishedFileUpdateRequest. Metadata still travels through SubmitUpdate.
	// Return false to abort; the half-written cloud stream is cancelled and nothing is kept.
	using CloudUploadProgress = std::function<bool(uint64_t bytesDone, uint64_t bytesTotal)>;
	bool UploadFileToCloud(const std::filesystem::path& localPath, const std::string& cloudFileName, const CloudUploadProgress& progress, QString& errorMessage);
	void DeleteCloudFile(const std::string& cloudFileName);
	void PublishLegacyFile(const std::string& cloudFileName);
	bool UpdateLegacyFileContent(PublishedFileId_t publishedFileId, const std::string& cloudFileName, const std::string& changeNote, QString& errorMessage);
	WorkshopUpdateProgress GetUpdateProgress() const;

	bool IsQueryInProgress() const { return _queryInProgress; }
	bool IsUpdateInProgress() const { return _updateHandle != k_UGCUpdateHandleInvalid; }

Q_SIGNALS:
	void PublishedItemsPageLoaded(int loadedCount, int totalCount);
	void PublishedItemsLoaded(const std::vector<WorkshopItem>& items);
	void PublishedItemsFailed(const QString& errorMessage);

	void ItemCreated(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement);
	void ItemCreateFailed(const QString& errorMessage);

	void ItemDeleted(PublishedFileId_t publishedFileId);
	void ItemDeleteFailed(PublishedFileId_t publishedFileId, const QString& errorMessage);

	void UpdateSubmitted(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement);
	void UpdateFailed(PublishedFileId_t publishedFileId, const QString& errorMessage);

	void LegacyFilePublished(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement);
	void LegacyFilePublishFailed(const QString& errorMessage);
	void LegacyFileUpdated(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement);
	void LegacyFileUpdateFailed(PublishedFileId_t publishedFileId, const QString& errorMessage);

private:
	// Steam hands results over from inside SteamAPI_RunCallbacks, which does not re-enter. A slot that
	// opens a modal dialog there (the --edit path did) would starve every later callback, so results
	// leave the service through the event loop, one tick later.
	template<typename Emitter>
	void Deliver(Emitter&& emitter) {
		QMetaObject::invokeMethod(this, std::forward<Emitter>(emitter), Qt::QueuedConnection);
	}

	void RequestPublishedItemsPage(uint32_t page);
	void OnPublishedItemsQueryCompleted(SteamUGCQueryCompleted_t* result, bool ioFailure);
	void OnDeleteItemCompleted(DeleteItemResult_t* result, bool ioFailure);
	void OnCreateItemCompleted(CreateItemResult_t* result, bool ioFailure);
	void OnLegacyPublishCompleted(RemoteStoragePublishFileResult_t* result, bool ioFailure);
	void OnLegacyUpdateCompleted(RemoteStorageUpdatePublishedFileResult_t* result, bool ioFailure);
	void OnSubmitUpdateCompleted(SubmitItemUpdateResult_t* result, bool ioFailure);

	static WorkshopItem ReadWorkshopItem(UGCQueryHandle_t queryHandle, uint32_t index, const SteamUGCDetails_t& details);

	std::string _language{"english"};
	CCallResult<CWorkshopService, SteamUGCQueryCompleted_t> _publishedItemsCallResult;
	CCallResult<CWorkshopService, DeleteItemResult_t> _deleteItemCallResult;
	CCallResult<CWorkshopService, CreateItemResult_t> _createItemCallResult;
	CCallResult<CWorkshopService, RemoteStoragePublishFileResult_t> _legacyPublishCallResult;
	CCallResult<CWorkshopService, RemoteStorageUpdatePublishedFileResult_t> _legacyUpdateCallResult;
	PublishedFileId_t _legacyUpdatingPublishedFileId{0};
	bool _legacyPublishInProgress{false};
	bool _createInProgress{false};
	CCallResult<CWorkshopService, SubmitItemUpdateResult_t> _submitUpdateCallResult;

	std::vector<WorkshopItem> _accumulatedItems;
	uint32_t _currentPage{0};
	bool _queryInProgress{false};

	PublishedFileId_t _deletingPublishedFileId{0};

	UGCUpdateHandle_t _updateHandle{k_UGCUpdateHandleInvalid};
	PublishedFileId_t _updatingPublishedFileId{0};
	// Tag strings live until SubmitItemUpdate completes: SteamParamStringArray_t holds raw
	// pointers and there is no telling when Steam finishes reading them.
	std::vector<std::string> _updateTagStrings;
	std::vector<const char*> _updateTagPointers;
};
