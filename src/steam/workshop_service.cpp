#include "steam/workshop_service.h"

#include "steam/steam_names.h"
#include "steam/steam_session.h"

namespace {

	constexpr uint32_t PREVIEW_URL_BUFFER_SIZE = 1024;
	constexpr uint32_t TAG_BUFFER_SIZE = 256;
	constexpr uint32_t ORIGINAL_FILE_NAME_BUFFER_SIZE = 260;

	std::vector<std::string> SplitTags(std::string_view commaSeparatedTags) {
		std::vector<std::string> tags;
		size_t start = 0;
		while (start <= commaSeparatedTags.size()) {
			size_t end = commaSeparatedTags.find(',', start);
			if (end == std::string_view::npos) {
				end = commaSeparatedTags.size();
			}
			std::string_view tag = commaSeparatedTags.substr(start, end - start);
			if (!tag.empty()) {
				tags.emplace_back(tag);
			}
			start = end + 1;
		}
		return tags;
	}

} // namespace

CWorkshopService::CWorkshopService(QObject* parent) :
	QObject(parent) {
}

void CWorkshopService::SetLanguage(std::string language) {
	_language = language.empty() ? std::string("english") : std::move(language);
}

void CWorkshopService::RequestPublishedItems() {
	if (_queryInProgress) {
		LogMessage(LOG_WARN, "RequestPublishedItems: query already in progress\n");
		return;
	}
	if (!SteamSession().IsInitialized()) {
		Q_EMIT PublishedItemsFailed(tr("Steam is not initialized"));
		return;
	}

	_accumulatedItems.clear();
	_queryInProgress = true;
	RequestPublishedItemsPage(1);
}

void CWorkshopService::RequestPublishedItemsPage(uint32_t page) {
	const AppId_t appId = SteamSession().GetAppId();
	const AccountID_t accountId = SteamSession().GetAccountId();

	const UGCQueryHandle_t queryHandle = SteamUGC()->CreateQueryUserUGCRequest(
		accountId,
		k_EUserUGCList_Published,
		k_EUGCMatchingUGCType_Items,
		k_EUserUGCListSortOrder_LastUpdatedDesc,
		appId,
		appId,
		page
	);
	if (queryHandle == k_UGCQueryHandleInvalid) {
		_queryInProgress = false;
		Q_EMIT PublishedItemsFailed(tr("Failed to create UGC query request"));
		return;
	}

	// Without SetReturnLongDescription Steam truncates the description, and without AdditionalPreviews
	// it withholds the list of extra images and videos — both are needed to edit the item.
	SteamUGC()->SetReturnLongDescription(queryHandle, true);
	SteamUGC()->SetLanguage(queryHandle, _language.c_str());
	SteamUGC()->SetReturnAdditionalPreviews(queryHandle, true);
	SteamUGC()->SetReturnKeyValueTags(queryHandle, true);
	SteamUGC()->SetReturnMetadata(queryHandle, true);

	_currentPage = page;
	const SteamAPICall_t apiCall = SteamUGC()->SendQueryUGCRequest(queryHandle);
	if (apiCall == k_uAPICallInvalid) {
		SteamUGC()->ReleaseQueryUGCRequest(queryHandle);
		_queryInProgress = false;
		Q_EMIT PublishedItemsFailed(tr("Failed to send UGC query request"));
		return;
	}

	LogMessage(LOG_DEBUG, "Requesting published items page %u (AppID %u, account %u)\n", page, appId, accountId);
	_publishedItemsCallResult.Set(apiCall, this, &CWorkshopService::OnPublishedItemsQueryCompleted);
}

void CWorkshopService::OnPublishedItemsQueryCompleted(SteamUGCQueryCompleted_t* result, bool ioFailure) {
	const UGCQueryHandle_t queryHandle = result->m_handle;

	if (ioFailure || result->m_eResult != k_EResultOK) {
		SteamUGC()->ReleaseQueryUGCRequest(queryHandle);
		_queryInProgress = false;
		const QString errorMessage = ioFailure ? tr("IO failure while querying Steam") : tr("Steam returned %1").arg(QtText::FromStringView(SteamNames::Result(result->m_eResult)));
		LogMessage(LOG_ERROR, "Published items query failed: %s\n", errorMessage.toUtf8().constData());
		Deliver([this, errorMessage]() { Q_EMIT PublishedItemsFailed(errorMessage); });
		return;
	}

	for (uint32_t index = 0; index < result->m_unNumResultsReturned; ++index) {
		SteamUGCDetails_t details{};
		if (!SteamUGC()->GetQueryUGCResult(queryHandle, index, &details)) {
			LogMessage(LOG_WARN, "GetQueryUGCResult failed for index %u on page %u\n", index, _currentPage);
			continue;
		}
		if (details.m_eResult != k_EResultOK) {
			LogMessage(LOG_WARN, "Item %llu returned %s, skipping\n", details.m_nPublishedFileId, SteamNames::Result(details.m_eResult).data());
			continue;
		}
		_accumulatedItems.push_back(ReadWorkshopItem(queryHandle, index, details));
	}

	SteamUGC()->ReleaseQueryUGCRequest(queryHandle);

	const int totalCount = static_cast<int>(result->m_unTotalMatchingResults);
	const int loadedCount = static_cast<int>(_accumulatedItems.size());
	Deliver([this, loadedCount, totalCount]() { Q_EMIT PublishedItemsPageLoaded(loadedCount, totalCount); });

	// Steam returns kNumUGCResultsPerPage per request; keep paging while pages are full and
	// we have not collected everything m_unTotalMatchingResults promised.
	const bool pageWasFull = result->m_unNumResultsReturned == kNumUGCResultsPerPage;
	const bool hasMore = static_cast<uint32_t>(loadedCount) < result->m_unTotalMatchingResults;
	if (pageWasFull && hasMore) {
		RequestPublishedItemsPage(_currentPage + 1);
		return;
	}

	_queryInProgress = false;
	LogMessage(LOG_INFO, "Loaded %d published items\n", loadedCount);
	Deliver([this, items = _accumulatedItems]() { Q_EMIT PublishedItemsLoaded(items); });
}

WorkshopItem CWorkshopService::ReadWorkshopItem(UGCQueryHandle_t queryHandle, uint32_t index, const SteamUGCDetails_t& details) {
	WorkshopItem item;
	item.publishedFileId = details.m_nPublishedFileId;
	item.ownerSteamId = details.m_ulSteamIDOwner;
	item.consumerAppId = details.m_nConsumerAppID;
	item.title = details.m_rgchTitle;
	item.description = details.m_rgchDescription;
	item.visibility = details.m_eVisibility;
	item.timeCreated = details.m_rtimeCreated;
	item.timeUpdated = details.m_rtimeUpdated;
	item.totalFilesSize = details.m_ulTotalFilesSize;
	item.previewFileSize = details.m_nPreviewFileSize;
	item.votesUp = details.m_unVotesUp;
	item.votesDown = details.m_unVotesDown;
	item.score = details.m_flScore;
	item.banned = details.m_bBanned;

	const auto readStatistic = [queryHandle, index](EItemStatistic statistic) -> uint64_t {
		uint64 value = 0;
		return SteamUGC()->GetQueryUGCStatistic(queryHandle, index, statistic, &value) ? value : 0;
	};
	item.subscriptions = readStatistic(k_EItemStatistic_NumSubscriptions);
	item.favorites = readStatistic(k_EItemStatistic_NumFavorites);
	item.websiteViews = readStatistic(k_EItemStatistic_NumUniqueWebsiteViews);

	// Legacy items (Portal 2) leave m_ulTotalFilesSize empty — their size is in m_nFileSize.
	if (item.totalFilesSize == 0 && details.m_nFileSize > 0) {
		item.totalFilesSize = static_cast<uint64_t>(details.m_nFileSize);
	}

	const uint32_t tagsCount = SteamUGC()->GetQueryUGCNumTags(queryHandle, index);
	if (tagsCount > 0) {
		std::array<char, TAG_BUFFER_SIZE> tagBuffer{};
		for (uint32_t tagIndex = 0; tagIndex < tagsCount; ++tagIndex) {
			if (SteamUGC()->GetQueryUGCTag(queryHandle, index, tagIndex, tagBuffer.data(), static_cast<uint32_t>(tagBuffer.size()))) {
				item.tags.emplace_back(tagBuffer.data());
			}
		}
	} else {
		item.tags = SplitTags(details.m_rgchTags);
	}

	std::array<char, PREVIEW_URL_BUFFER_SIZE> previewUrlBuffer{};
	if (SteamUGC()->GetQueryUGCPreviewURL(queryHandle, index, previewUrlBuffer.data(), static_cast<uint32_t>(previewUrlBuffer.size()))) {
		item.previewUrl = previewUrlBuffer.data();
	}

	const uint32_t additionalPreviewsCount = SteamUGC()->GetQueryUGCNumAdditionalPreviews(queryHandle, index);
	item.additionalPreviews.reserve(additionalPreviewsCount);
	for (uint32_t previewIndex = 0; previewIndex < additionalPreviewsCount; ++previewIndex) {
		std::array<char, PREVIEW_URL_BUFFER_SIZE> urlOrVideoIdBuffer{};
		std::array<char, ORIGINAL_FILE_NAME_BUFFER_SIZE> originalFileNameBuffer{};
		EItemPreviewType previewType = k_EItemPreviewType_Image;
		const bool readOk = SteamUGC()->GetQueryUGCAdditionalPreview(
			queryHandle,
			index,
			previewIndex,
			urlOrVideoIdBuffer.data(),
			static_cast<uint32_t>(urlOrVideoIdBuffer.size()),
			originalFileNameBuffer.data(),
			static_cast<uint32_t>(originalFileNameBuffer.size()),
			&previewType
		);
		if (!readOk) {
			continue;
		}
		item.additionalPreviews.push_back({previewType, urlOrVideoIdBuffer.data(), originalFileNameBuffer.data()});
	}

	return item;
}

void CWorkshopService::CreateItem() {
	if (!SteamSession().IsInitialized()) {
		Q_EMIT ItemCreateFailed(tr("Steam is not initialized"));
		return;
	}
	if (_createInProgress) {
		Q_EMIT ItemCreateFailed(tr("Another item is still being created"));
		return;
	}

	// k_EWorkshopFileTypeCommunity — a regular workshop item, like maps from cs2_workshop_manager.
	const SteamAPICall_t apiCall = SteamUGC()->CreateItem(SteamSession().GetAppId(), k_EWorkshopFileTypeCommunity);
	if (apiCall == k_uAPICallInvalid) {
		Q_EMIT ItemCreateFailed(tr("Failed to start CreateItem"));
		return;
	}

	_createInProgress = true;
	LogMessage(LOG_INFO, "Creating workshop item for AppID %u\n", SteamSession().GetAppId());
	_createItemCallResult.Set(apiCall, this, &CWorkshopService::OnCreateItemCompleted);
}

void CWorkshopService::OnCreateItemCompleted(CreateItemResult_t* result, bool ioFailure) {
	_createInProgress = false;

	if (ioFailure || result->m_eResult != k_EResultOK) {
		const QString errorMessage = ioFailure ? tr("IO failure while creating item") : tr("Steam returned %1").arg(QtText::FromStringView(SteamNames::Result(result->m_eResult)));
		LogMessage(LOG_ERROR, "CreateItem failed: %s\n", errorMessage.toUtf8().constData());
		Deliver([this, errorMessage]() { Q_EMIT ItemCreateFailed(errorMessage); });
		return;
	}

	LogMessage(LOG_INFO, "Created workshop item %llu (legal agreement needed: %d)\n", result->m_nPublishedFileId, result->m_bUserNeedsToAcceptWorkshopLegalAgreement ? 1 : 0);
	Deliver([this, publishedFileId = result->m_nPublishedFileId, needsAgreement = result->m_bUserNeedsToAcceptWorkshopLegalAgreement]() { Q_EMIT ItemCreated(publishedFileId, needsAgreement); });
}

void CWorkshopService::DeleteItem(PublishedFileId_t publishedFileId) {
	if (!SteamSession().IsInitialized()) {
		Q_EMIT ItemDeleteFailed(publishedFileId, tr("Steam is not initialized"));
		return;
	}
	if (_deletingPublishedFileId != 0) {
		Q_EMIT ItemDeleteFailed(publishedFileId, tr("Another delete is still in progress"));
		return;
	}

	const SteamAPICall_t apiCall = SteamUGC()->DeleteItem(publishedFileId);
	if (apiCall == k_uAPICallInvalid) {
		Q_EMIT ItemDeleteFailed(publishedFileId, tr("Failed to start DeleteItem"));
		return;
	}

	_deletingPublishedFileId = publishedFileId;
	LogMessage(LOG_INFO, "Deleting workshop item %llu\n", publishedFileId);
	_deleteItemCallResult.Set(apiCall, this, &CWorkshopService::OnDeleteItemCompleted);
}

void CWorkshopService::OnDeleteItemCompleted(DeleteItemResult_t* result, bool ioFailure) {
	const PublishedFileId_t publishedFileId = _deletingPublishedFileId;
	_deletingPublishedFileId = 0;

	if (ioFailure || result->m_eResult != k_EResultOK) {
		const QString errorMessage = ioFailure ? tr("IO failure while deleting item") : tr("Steam returned %1").arg(QtText::FromStringView(SteamNames::Result(result->m_eResult)));
		LogMessage(LOG_ERROR, "Delete item %llu failed: %s\n", publishedFileId, errorMessage.toUtf8().constData());
		Deliver([this, publishedFileId, errorMessage]() { Q_EMIT ItemDeleteFailed(publishedFileId, errorMessage); });
		return;
	}

	LogMessage(LOG_INFO, "Workshop item %llu deleted\n", publishedFileId);
	Deliver([this, publishedFileId]() { Q_EMIT ItemDeleted(publishedFileId); });
}

bool CWorkshopService::SubmitUpdate(const WorkshopUpdateRequest& request, QString& errorMessage) {
	if (!SteamSession().IsInitialized()) {
		errorMessage = tr("Steam is not initialized");
		return false;
	}
	if (IsUpdateInProgress()) {
		errorMessage = tr("Another update is still in progress");
		return false;
	}
	if (request.publishedFileId == 0) {
		errorMessage = tr("Published file id is not set");
		return false;
	}
	if (request.previewFilePath.has_value()) {
		const QFileInfo previewFileInfo(QString::fromStdString(*request.previewFilePath));
		if (!previewFileInfo.isFile()) {
			errorMessage = tr("Preview file does not exist: %1").arg(previewFileInfo.filePath());
			return false;
		}
		if (previewFileInfo.size() >= WORKSHOP_PREVIEW_MAX_FILE_SIZE) {
			errorMessage = tr("Preview file must be smaller than 1 MB (now %1)").arg(QLocale().formattedDataSize(previewFileInfo.size()));
			return false;
		}
	}
	for (const WorkshopPreviewAddition& addition : request.addPreviews) {
		if (addition.type != k_EItemPreviewType_Image) {
			continue;
		}
		const QFileInfo additionFileInfo(QString::fromStdString(addition.pathOrVideoId));
		if (!additionFileInfo.isFile() || additionFileInfo.size() >= WORKSHOP_PREVIEW_MAX_FILE_SIZE) {
			errorMessage = tr("Additional preview must exist and be smaller than 1 MB: %1").arg(additionFileInfo.filePath());
			return false;
		}
	}
	if (request.tags.has_value() && request.tags->size() > WORKSHOP_TAGS_MAX_COUNT) {
		errorMessage = tr("Too many tags: %1 (max %2)").arg(request.tags->size()).arg(WORKSHOP_TAGS_MAX_COUNT);
		return false;
	}

	const UGCUpdateHandle_t updateHandle = SteamUGC()->StartItemUpdate(SteamSession().GetAppId(), request.publishedFileId);
	if (updateHandle == k_UGCUpdateHandleInvalid) {
		errorMessage = tr("StartItemUpdate failed");
		return false;
	}

	auto failWith = [&](const QString& message) {
		LogMessage(LOG_ERROR, "SubmitUpdate %llu rejected: %s\n", request.publishedFileId, message.toUtf8().constData());
		errorMessage = message;
		return false;
	};

	if (!SteamUGC()->SetItemUpdateLanguage(updateHandle, _language.c_str())) {
		return failWith(tr("SetItemUpdateLanguage rejected '%1'").arg(QString::fromStdString(_language)));
	}

	if (request.title.has_value() && !SteamUGC()->SetItemTitle(updateHandle, request.title->c_str())) {
		return failWith(tr("SetItemTitle rejected the title (max %1 characters)").arg(WORKSHOP_TITLE_MAX_LENGTH));
	}
	if (request.description.has_value() && !SteamUGC()->SetItemDescription(updateHandle, request.description->c_str())) {
		return failWith(tr("SetItemDescription rejected the description (max %1 characters)").arg(WORKSHOP_DESCRIPTION_MAX_LENGTH));
	}
	if (request.visibility.has_value() && !SteamUGC()->SetItemVisibility(updateHandle, *request.visibility)) {
		return failWith(tr("SetItemVisibility failed"));
	}

	if (request.tags.has_value()) {
		_updateTagStrings = *request.tags;
		_updateTagPointers.clear();
		_updateTagPointers.reserve(_updateTagStrings.size());
		for (const std::string& tag : _updateTagStrings) {
			_updateTagPointers.push_back(tag.c_str());
		}
		SteamParamStringArray_t tagsArray{};
		tagsArray.m_ppStrings = _updateTagPointers.data();
		tagsArray.m_nNumStrings = static_cast<int32>(_updateTagPointers.size());
		if (!SteamUGC()->SetItemTags(updateHandle, &tagsArray, false)) {
			return failWith(tr("SetItemTags failed"));
		}
	}

	if (request.previewFilePath.has_value() && !SteamUGC()->SetItemPreview(updateHandle, request.previewFilePath->c_str())) {
		return failWith(tr("SetItemPreview failed"));
	}

	std::vector<uint32_t> removeIndices = request.removePreviewIndices;
	std::sort(removeIndices.begin(), removeIndices.end(), std::greater<uint32_t>());
	for (uint32_t previewIndex : removeIndices) {
		if (!SteamUGC()->RemoveItemPreview(updateHandle, previewIndex)) {
			return failWith(tr("RemoveItemPreview failed for index %1").arg(previewIndex));
		}
	}

	for (const WorkshopPreviewAddition& addition : request.addPreviews) {
		bool added = false;
		if (addition.type == k_EItemPreviewType_YouTubeVideo) {
			added = SteamUGC()->AddItemPreviewVideo(updateHandle, addition.pathOrVideoId.c_str());
		} else {
			added = SteamUGC()->AddItemPreviewFile(updateHandle, addition.pathOrVideoId.c_str(), addition.type);
		}
		if (!added) {
			return failWith(tr("Failed to add preview: %1").arg(QString::fromStdString(addition.pathOrVideoId)));
		}
	}

	if (request.contentFolder.has_value() && !SteamUGC()->SetItemContent(updateHandle, request.contentFolder->c_str())) {
		return failWith(tr("SetItemContent failed for %1").arg(QString::fromStdString(*request.contentFolder)));
	}

	const SteamAPICall_t apiCall = SteamUGC()->SubmitItemUpdate(updateHandle, request.changeNote.empty() ? nullptr : request.changeNote.c_str());
	if (apiCall == k_uAPICallInvalid) {
		return failWith(tr("SubmitItemUpdate failed"));
	}

	_updateHandle = updateHandle;
	_updatingPublishedFileId = request.publishedFileId;
	LogMessage(LOG_INFO, "Submitting update for %llu (handle %llx)\n", request.publishedFileId, updateHandle);
	_submitUpdateCallResult.Set(apiCall, this, &CWorkshopService::OnSubmitUpdateCompleted);
	return true;
}

WorkshopUpdateProgress CWorkshopService::GetUpdateProgress() const {
	WorkshopUpdateProgress progress;
	if (!IsUpdateInProgress()) {
		return progress;
	}
	uint64 bytesProcessed = 0;
	uint64 bytesTotal = 0;
	progress.status = SteamUGC()->GetItemUpdateProgress(_updateHandle, &bytesProcessed, &bytesTotal);
	progress.bytesProcessed = bytesProcessed;
	progress.bytesTotal = bytesTotal;
	return progress;
}

void CWorkshopService::OnSubmitUpdateCompleted(SubmitItemUpdateResult_t* result, bool ioFailure) {
	const PublishedFileId_t publishedFileId = _updatingPublishedFileId;
	_updateHandle = k_UGCUpdateHandleInvalid;
	_updatingPublishedFileId = 0;
	_updateTagStrings.clear();
	_updateTagPointers.clear();

	if (ioFailure || result->m_eResult != k_EResultOK) {
		const QString errorMessage = ioFailure ? tr("IO failure while submitting update") : tr("Steam returned %1").arg(QtText::FromStringView(SteamNames::Result(result->m_eResult)));
		LogMessage(LOG_ERROR, "Update of %llu failed: %s\n", publishedFileId, errorMessage.toUtf8().constData());
		Deliver([this, publishedFileId, errorMessage]() { Q_EMIT UpdateFailed(publishedFileId, errorMessage); });
		return;
	}

	LogMessage(LOG_INFO, "Update of %llu submitted (legal agreement needed: %d)\n", publishedFileId, result->m_bUserNeedsToAcceptWorkshopLegalAgreement ? 1 : 0);
	Deliver([this, publishedFileId, needsAgreement = result->m_bUserNeedsToAcceptWorkshopLegalAgreement]() { Q_EMIT UpdateSubmitted(publishedFileId, needsAgreement); });
}

bool CWorkshopService::UploadFileToCloud(const std::filesystem::path& localPath, const std::string& cloudFileName, const CloudUploadProgress& progress, QString& errorMessage) {
	if (!SteamSession().IsInitialized()) {
		errorMessage = tr("Steam is not initialized");
		return false;
	}

	QFile localFile(QString::fromStdWString(localPath.wstring()));
	if (!localFile.open(QIODevice::ReadOnly)) {
		errorMessage = tr("Cannot read %1: %2").arg(localFile.fileName(), localFile.errorString());
		return false;
	}
	const qint64 totalSize = localFile.size();

	uint64 totalQuota = 0;
	uint64 availableQuota = 0;
	if (SteamRemoteStorage()->GetQuota(&totalQuota, &availableQuota) && static_cast<uint64>(totalSize) > availableQuota) {
		errorMessage = tr("Steam Cloud quota exceeded: the map is %1, %2 available").arg(QLocale().formattedDataSize(totalSize), QLocale().formattedDataSize(static_cast<qint64>(availableQuota)));
		return false;
	}

	// Leftovers of a previous failed attempt take up quota — remove them before writing.
	if (SteamRemoteStorage()->FileExists(cloudFileName.c_str())) {
		SteamRemoteStorage()->FileDelete(cloudFileName.c_str());
	}

	const UGCFileWriteStreamHandle_t streamHandle = SteamRemoteStorage()->FileWriteStreamOpen(cloudFileName.c_str());
	if (streamHandle == k_UGCFileStreamHandleInvalid) {
		errorMessage = tr("Steam Cloud refused to open a write stream for %1").arg(QString::fromStdString(cloudFileName));
		return false;
	}

	constexpr qint64 CHUNK_SIZE = 1024 * 1024;
	QByteArray chunk;
	qint64 bytesDone = 0;
	while (!localFile.atEnd()) {
		chunk = localFile.read(CHUNK_SIZE);
		if (chunk.isEmpty()) {
			break;
		}
		if (!SteamRemoteStorage()->FileWriteStreamWriteChunk(streamHandle, chunk.constData(), static_cast<int32>(chunk.size()))) {
			SteamRemoteStorage()->FileWriteStreamCancel(streamHandle);
			errorMessage = tr("Steam Cloud rejected a chunk of %1").arg(QString::fromStdString(cloudFileName));
			return false;
		}
		bytesDone += chunk.size();
		if (progress && !progress(static_cast<uint64_t>(bytesDone), static_cast<uint64_t>(totalSize))) {
			SteamRemoteStorage()->FileWriteStreamCancel(streamHandle);
			errorMessage = tr("Upload cancelled");
			return false;
		}
	}

	if (!SteamRemoteStorage()->FileWriteStreamClose(streamHandle)) {
		errorMessage = tr("Steam Cloud failed to finish writing %1").arg(QString::fromStdString(cloudFileName));
		return false;
	}
	LogMessage(LOG_INFO, "Uploaded %s to Steam Cloud as %s (%lld bytes)\n", PathText::ToUtf8(localPath).c_str(), cloudFileName.c_str(), static_cast<long long>(totalSize));
	return true;
}

void CWorkshopService::DeleteCloudFile(const std::string& cloudFileName) {
	if (!SteamSession().IsInitialized() || cloudFileName.empty()) {
		return;
	}
	if (SteamRemoteStorage()->FileExists(cloudFileName.c_str())) {
		SteamRemoteStorage()->FileDelete(cloudFileName.c_str());
		LogMessage(LOG_DEBUG, "Deleted cloud file %s\n", cloudFileName.c_str());
	}
}

void CWorkshopService::PublishLegacyFile(const std::string& cloudFileName) {
	if (!SteamSession().IsInitialized()) {
		Q_EMIT LegacyFilePublishFailed(tr("Steam is not initialized"));
		return;
	}
	if (_legacyPublishInProgress) {
		Q_EMIT LegacyFilePublishFailed(tr("Another publish is still in progress"));
		return;
	}

	// Title, description, preview and tags are set later through SubmitUpdate — only the file here.
	// Unlisted at this step so the empty item does not flash publicly before it is filled in.
	const SteamAPICall_t apiCall = SteamRemoteStorage()->PublishWorkshopFile(cloudFileName.c_str(), "", SteamSession().GetAppId(), "", "", k_ERemoteStoragePublishedFileVisibilityUnlisted, nullptr, k_EWorkshopFileTypeCommunity);
	if (apiCall == k_uAPICallInvalid) {
		Q_EMIT LegacyFilePublishFailed(tr("PublishWorkshopFile could not be started"));
		return;
	}
	_legacyPublishInProgress = true;
	LogMessage(LOG_INFO, "PublishWorkshopFile(%s) for AppID %u\n", cloudFileName.c_str(), SteamSession().GetAppId());
	_legacyPublishCallResult.Set(apiCall, this, &CWorkshopService::OnLegacyPublishCompleted);
}

void CWorkshopService::OnLegacyPublishCompleted(RemoteStoragePublishFileResult_t* result, bool ioFailure) {
	_legacyPublishInProgress = false;
	if (ioFailure || result->m_eResult != k_EResultOK) {
		const QString errorMessage = ioFailure ? tr("IO failure while publishing the file") : tr("Steam returned %1").arg(QtText::FromStringView(SteamNames::Result(result->m_eResult)));
		LogMessage(LOG_ERROR, "PublishWorkshopFile failed: %s\n", errorMessage.toUtf8().constData());
		Deliver([this, errorMessage]() { Q_EMIT LegacyFilePublishFailed(errorMessage); });
		return;
	}
	LogMessage(LOG_INFO, "PublishWorkshopFile created item %llu (legal agreement needed: %d)\n", result->m_nPublishedFileId, result->m_bUserNeedsToAcceptWorkshopLegalAgreement ? 1 : 0);
	Deliver([this, publishedFileId = result->m_nPublishedFileId, needsAgreement = result->m_bUserNeedsToAcceptWorkshopLegalAgreement]() { Q_EMIT LegacyFilePublished(publishedFileId, needsAgreement); });
}

bool CWorkshopService::UpdateLegacyFileContent(PublishedFileId_t publishedFileId, const std::string& cloudFileName, const std::string& changeNote, QString& errorMessage) {
	if (!SteamSession().IsInitialized()) {
		errorMessage = tr("Steam is not initialized");
		return false;
	}
	if (_legacyUpdatingPublishedFileId != 0) {
		errorMessage = tr("Another content update is still in progress");
		return false;
	}

	const PublishedFileUpdateHandle_t updateHandle = SteamRemoteStorage()->CreatePublishedFileUpdateRequest(publishedFileId);
	if (updateHandle == k_PublishedFileUpdateHandleInvalid) {
		errorMessage = tr("CreatePublishedFileUpdateRequest failed for %1").arg(publishedFileId);
		return false;
	}
	if (!SteamRemoteStorage()->UpdatePublishedFileFile(updateHandle, cloudFileName.c_str())) {
		errorMessage = tr("UpdatePublishedFileFile rejected %1").arg(QString::fromStdString(cloudFileName));
		return false;
	}
	if (!changeNote.empty() && !SteamRemoteStorage()->UpdatePublishedFileSetChangeDescription(updateHandle, changeNote.c_str())) {
		errorMessage = tr("UpdatePublishedFileSetChangeDescription rejected the change note");
		return false;
	}

	const SteamAPICall_t apiCall = SteamRemoteStorage()->CommitPublishedFileUpdate(updateHandle);
	if (apiCall == k_uAPICallInvalid) {
		errorMessage = tr("CommitPublishedFileUpdate could not be started");
		return false;
	}
	_legacyUpdatingPublishedFileId = publishedFileId;
	LogMessage(LOG_INFO, "CommitPublishedFileUpdate(%llu) with %s\n", publishedFileId, cloudFileName.c_str());
	_legacyUpdateCallResult.Set(apiCall, this, &CWorkshopService::OnLegacyUpdateCompleted);
	return true;
}

void CWorkshopService::OnLegacyUpdateCompleted(RemoteStorageUpdatePublishedFileResult_t* result, bool ioFailure) {
	const PublishedFileId_t publishedFileId = _legacyUpdatingPublishedFileId;
	_legacyUpdatingPublishedFileId = 0;
	if (ioFailure || result->m_eResult != k_EResultOK) {
		const QString errorMessage = ioFailure ? tr("IO failure while updating the file") : tr("Steam returned %1").arg(QtText::FromStringView(SteamNames::Result(result->m_eResult)));
		LogMessage(LOG_ERROR, "CommitPublishedFileUpdate(%llu) failed: %s\n", publishedFileId, errorMessage.toUtf8().constData());
		Deliver([this, publishedFileId, errorMessage]() { Q_EMIT LegacyFileUpdateFailed(publishedFileId, errorMessage); });
		return;
	}
	LogMessage(LOG_INFO, "Content of %llu replaced\n", publishedFileId);
	Deliver([this, publishedFileId, needsAgreement = result->m_bUserNeedsToAcceptWorkshopLegalAgreement]() { Q_EMIT LegacyFileUpdated(publishedFileId, needsAgreement); });
}
