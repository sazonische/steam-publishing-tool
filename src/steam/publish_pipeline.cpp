#include "steam/publish_pipeline.h"

#include "core/publish_data.h"
#include "vpk/vpk_writer.h"

CPublishPipeline::CPublishPipeline(CWorkshopService* workshopService, QObject* parent) :
	QObject(parent),
	_workshopService(workshopService) {
	connect(_workshopService, &CWorkshopService::ItemCreated, this, &CPublishPipeline::OnItemCreated);
	connect(_workshopService, &CWorkshopService::ItemCreateFailed, this, &CPublishPipeline::OnItemCreateFailed);
	connect(_workshopService, &CWorkshopService::UpdateSubmitted, this, &CPublishPipeline::OnUpdateSubmitted);
	connect(_workshopService, &CWorkshopService::UpdateFailed, this, &CPublishPipeline::OnUpdateFailed);
	connect(_workshopService, &CWorkshopService::LegacyFilePublished, this, &CPublishPipeline::OnLegacyFilePublished);
	connect(_workshopService, &CWorkshopService::LegacyFilePublishFailed, this, &CPublishPipeline::OnLegacyFilePublishFailed);
	connect(_workshopService, &CWorkshopService::LegacyFileUpdated, this, &CPublishPipeline::OnLegacyFileUpdated);
	connect(_workshopService, &CWorkshopService::LegacyFileUpdateFailed, this, &CPublishPipeline::OnLegacyFileUpdateFailed);
}

bool CPublishPipeline::IsBspJob() const {
	return _job.profile && _job.profile->contentKind == WORKSHOP_CONTENT_SOURCE1_BSP;
}

bool CPublishPipeline::Start(PublishJob job, QString& errorMessage) {
	if (_running) {
		errorMessage = tr("Another publish is still running");
		return false;
	}
	if (!job.manifest.errorMessage.empty()) {
		errorMessage = tr("Cannot read all addon files: %1").arg(QString::fromStdString(job.manifest.errorMessage));
		return false;
	}
	const bool bspJob = job.profile && job.profile->contentKind == WORKSHOP_CONTENT_SOURCE1_BSP;
	uint64_t contentSize = job.manifest.totalSize;
	if (bspJob) {
		std::error_code errorCode;
		if (job.bspPath.empty() || !std::filesystem::is_regular_file(job.bspPath, errorCode)) {
			errorMessage = tr("The map file does not exist: %1").arg(QString::fromStdWString(job.bspPath.wstring()));
			return false;
		}
		contentSize = std::filesystem::file_size(job.bspPath, errorCode);
	} else if (job.manifest.files.empty()) {
		errorMessage = tr("The addon folder has nothing to pack — check Upload paths");
		return false;
	}
	if (contentSize > WORKSHOP_CONTENT_MAX_SIZE) {
		errorMessage = tr("The content is %1, above the %2 Workshop limit").arg(QLocale().formattedDataSize(static_cast<qint64>(contentSize)), QLocale().formattedDataSize(static_cast<qint64>(WORKSHOP_CONTENT_MAX_SIZE)));
		return false;
	}
	if (job.kind == PublishJob::PUBLISH_REUPLOAD && job.request.publishedFileId == 0) {
		errorMessage = tr("Re-upload needs an existing published file id");
		return false;
	}

	_job = std::move(job);
	_running = true;
	_createdThisRun = false;
	_legalAgreementNeeded = false;
	_cancelRequested = false;

	if (bspJob) {
		// Legacy flow: the file goes to Cloud, then PublishWorkshopFile creates the item with its content.
		StartBspUpload();
		return true;
	}

	if (_job.kind == PublishJob::PUBLISH_NEW) {
		Q_EMIT StageChanged(tr("Creating workshop item…"));
		_workshopService->CreateItem();
		return true;
	}

	StartPacking();
	return true;
}

void CPublishPipeline::Cancel() {
	if (!_running || _cancelRequested) {
		return;
	}
	_cancelRequested = true;
	LogMessage(LOG_INFO, "Publish cancel requested\n");
	Q_EMIT StageChanged(tr("Cancelling…"));
}

void CPublishPipeline::StartBspUpload() {
	_job.cloudFileName = "mymaps/" + PathText::ToUtf8(_job.bspPath.filename());
	Q_EMIT StageChanged(tr("Uploading %1 to Steam Cloud…").arg(QString::fromStdWString(_job.bspPath.filename().wstring())));

	// Cloud writes go chunk by chunk into the local Steam cache; pump events between chunks so the
	// progress bar repaints, SteamAPI_RunCallbacks keeps running and Cancel gets through.
	int lastPercent = -1;
	const CWorkshopService::CloudUploadProgress progress = [this, &lastPercent](uint64_t bytesDone, uint64_t bytesTotal) {
		const int percent = bytesTotal == 0 ? 100 : static_cast<int>(bytesDone * 100 / bytesTotal);
		if (percent != lastPercent) {
			lastPercent = percent;
			Q_EMIT PackProgress(percent, tr("%1 of %2").arg(QLocale().formattedDataSize(static_cast<qint64>(bytesDone)), QLocale().formattedDataSize(static_cast<qint64>(bytesTotal))));
		}
		QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		return !_cancelRequested.load();
	};

	QString errorMessage;
	if (!_workshopService->UploadFileToCloud(_job.bspPath, _job.cloudFileName, progress, errorMessage)) {
		if (_cancelRequested) {
			FinishCancelled();
		} else {
			Fail(errorMessage);
		}
		return;
	}
	if (_cancelRequested) {
		_workshopService->DeleteCloudFile(_job.cloudFileName);
		FinishCancelled();
		return;
	}

	if (_job.kind == PublishJob::PUBLISH_NEW) {
		Q_EMIT StageChanged(tr("Creating workshop item…"));
		_workshopService->PublishLegacyFile(_job.cloudFileName);
		return;
	}

	Q_EMIT StageChanged(tr("Replacing item content…"));
	if (!_workshopService->UpdateLegacyFileContent(_job.request.publishedFileId, _job.cloudFileName, _job.request.changeNote, errorMessage)) {
		_workshopService->DeleteCloudFile(_job.cloudFileName);
		Fail(errorMessage);
	}
}

void CPublishPipeline::OnLegacyFilePublished(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement) {
	if (!_running || !IsBspJob() || _job.kind != PublishJob::PUBLISH_NEW) {
		return;
	}
	_workshopService->DeleteCloudFile(_job.cloudFileName);
	_job.request.publishedFileId = publishedFileId;
	_createdThisRun = true;
	_legalAgreementNeeded = userNeedsToAcceptLegalAgreement;
	if (_cancelRequested) {
		FinishCancelled();
		return;
	}

	// The content is in place; title, description, preview, tags and visibility go through the usual SubmitUpdate.
	Q_EMIT StageChanged(tr("Uploading details to Steam…"));
	QString submitError;
	if (!_workshopService->SubmitUpdate(_job.request, submitError)) {
		Fail(submitError);
		return;
	}
	Q_EMIT UploadStarted();
}

void CPublishPipeline::OnLegacyFilePublishFailed(const QString& errorMessage) {
	if (!_running || !IsBspJob()) {
		return;
	}
	_workshopService->DeleteCloudFile(_job.cloudFileName);
	Fail(tr("Steam could not publish the map: %1").arg(errorMessage));
}

void CPublishPipeline::OnLegacyFileUpdated(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement) {
	if (!_running || !IsBspJob() || publishedFileId != _job.request.publishedFileId) {
		return;
	}
	_workshopService->DeleteCloudFile(_job.cloudFileName);
	LogMessage(LOG_INFO, "Replaced map of %llu with %s\n", publishedFileId, PathText::ToUtf8(_job.bspPath).c_str());
	_legalAgreementNeeded = _legalAgreementNeeded || userNeedsToAcceptLegalAgreement;
	if (_cancelRequested) {
		// Steam has already swapped the file; only the page changes are dropped.
		FinishCancelled();
		return;
	}

	// The legacy update only swaps the file; page edits from the same window go through a regular SubmitUpdate.
	if (HasMetadataChanges(_job.request)) {
		Q_EMIT StageChanged(tr("Uploading details to Steam…"));
		QString submitError;
		if (!_workshopService->SubmitUpdate(_job.request, submitError)) {
			Fail(submitError);
			return;
		}
		Q_EMIT UploadStarted();
		return;
	}

	_running = false;
	Q_EMIT Finished(publishedFileId, _legalAgreementNeeded);
}

void CPublishPipeline::OnLegacyFileUpdateFailed(PublishedFileId_t publishedFileId, const QString& errorMessage) {
	if (!_running || !IsBspJob() || publishedFileId != _job.request.publishedFileId) {
		return;
	}
	_workshopService->DeleteCloudFile(_job.cloudFileName);
	Fail(tr("Steam could not replace the map: %1").arg(errorMessage));
}

void CPublishPipeline::OnItemCreated(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement) {
	if (!_running || _job.kind != PublishJob::PUBLISH_NEW) {
		return;
	}
	_job.request.publishedFileId = publishedFileId;
	_createdThisRun = true;
	_legalAgreementNeeded = userNeedsToAcceptLegalAgreement;
	if (_cancelRequested) {
		FinishCancelled();
		return;
	}
	StartPacking();
}

void CPublishPipeline::OnItemCreateFailed(const QString& errorMessage) {
	if (!_running || _job.kind != PublishJob::PUBLISH_NEW) {
		return;
	}
	Fail(tr("Steam could not create the item: %1").arg(errorMessage));
}

std::filesystem::path CPublishPipeline::OutputDirectory() const {
	return _job.publishedRoot / std::to_string(_job.request.publishedFileId);
}

void CPublishPipeline::StartPacking() {
	Q_EMIT StageChanged(tr("Packing %1 into VPK…").arg(QString::fromStdString(_job.addon.name)));

	// Packing means minutes of reading and writing hundreds of megabytes; the main thread has to
	// keep pumping SteamAPI_RunCallbacks and painting progress.
	const std::filesystem::path outputDirectory = OutputDirectory();
	const std::string baseName = std::to_string(_job.request.publishedFileId);
	const PublishJob job = _job;

	_packThread = QThread::create([this, job, outputDirectory, baseName]() {
		std::string errorMessage;
		int lastPercent = -1;
		const VpkWriter::ProgressCallback progress = [this, &lastPercent](size_t filesDone, size_t filesTotal, uint64_t bytesDone, uint64_t bytesTotal) {
			const int percent = bytesTotal == 0 ? 100 : static_cast<int>(bytesDone * 100 / bytesTotal);
			if (percent != lastPercent) {
				lastPercent = percent;
				const QString detail = tr("%1 / %2 files, %3 of %4").arg(filesDone).arg(filesTotal).arg(QLocale().formattedDataSize(static_cast<qint64>(bytesDone))).arg(QLocale().formattedDataSize(static_cast<qint64>(bytesTotal)));
				QMetaObject::invokeMethod(this, [this, percent, detail]() { Q_EMIT PackProgress(percent, detail); }, Qt::QueuedConnection);
			}
			return !_cancelRequested.load();
		};

		const std::optional<VpkWriter::WriteResult> result = VpkWriter::Write(job.addon.directory, job.manifest, outputDirectory, baseName, VpkWriter::WriteOptions{}, progress, errorMessage);
		bool success = result.has_value();
		if (success) {
			PublishData publishData;
			publishData.title = !job.displayTitle.empty() ? job.displayTitle : job.request.title.value_or(job.addon.name);
			publishData.sourceFolder = job.addon.name;
			publishData.publishTime = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());
			success = PublishDataFile::Write(outputDirectory, publishData, errorMessage);
		}

		const QString errorText = QString::fromStdString(errorMessage);
		QMetaObject::invokeMethod(this, [this, success, errorText]() { OnPacked(success, errorText); }, Qt::QueuedConnection);
	});
	connect(_packThread, &QThread::finished, _packThread, &QObject::deleteLater);
	_packThread->start();
}

void CPublishPipeline::OnPacked(bool success, const QString& errorMessage) {
	_packThread = nullptr;
	if (!_running) {
		return;
	}
	if (_cancelRequested) {
		FinishCancelled();
		return;
	}
	if (!success) {
		Fail(tr("Packing failed: %1").arg(errorMessage));
		return;
	}

	_job.request.contentFolder = PathText::ToUtf8(OutputDirectory());
	Q_EMIT StageChanged(tr("Uploading to Steam…"));

	QString submitError;
	if (!_workshopService->SubmitUpdate(_job.request, submitError)) {
		Fail(submitError);
		return;
	}
	Q_EMIT UploadStarted();
}

void CPublishPipeline::OnUpdateSubmitted(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement) {
	if (!_running || publishedFileId != _job.request.publishedFileId) {
		return;
	}
	if (_cancelRequested) {
		FinishCancelled();
		return;
	}
	_running = false;
	const std::string source = IsBspJob() ? PathText::ToUtf8(_job.bspPath.filename()) : _job.addon.name;
	LogMessage(LOG_INFO, "Published %llu from '%s'\n", publishedFileId, source.c_str());
	Q_EMIT Finished(publishedFileId, _legalAgreementNeeded || userNeedsToAcceptLegalAgreement);
}

void CPublishPipeline::OnUpdateFailed(PublishedFileId_t publishedFileId, const QString& errorMessage) {
	if (!_running || publishedFileId != _job.request.publishedFileId) {
		return;
	}
	if (_cancelRequested) {
		FinishCancelled();
		return;
	}
	Fail(tr("Upload failed: %1").arg(errorMessage));
}

void CPublishPipeline::DeleteCreatedItem() {
	if (!_createdThisRun || _job.request.publishedFileId == 0) {
		return;
	}
	// Like cs2_workshop_manager: never leave an empty item without content in the workshop.
	LogMessage(LOG_WARN, "Deleting the item %llu created for this upload\n", _job.request.publishedFileId);
	_workshopService->DeleteItem(_job.request.publishedFileId);
	if (!_job.publishedRoot.empty()) {
		std::error_code errorCode;
		std::filesystem::remove_all(OutputDirectory(), errorCode);
	}
	_createdThisRun = false;
}

void CPublishPipeline::Fail(const QString& errorMessage) {
	_running = false;
	QString fullMessage = errorMessage;
	if (_createdThisRun && _job.request.publishedFileId != 0) {
		fullMessage += tr("\n\nThe empty workshop item %1 created for this upload is being deleted.").arg(_job.request.publishedFileId);
		DeleteCreatedItem();
	}
	LogMessage(LOG_ERROR, "Publish failed: %s\n", errorMessage.toUtf8().constData());
	Q_EMIT Failed(fullMessage);
}

void CPublishPipeline::FinishCancelled() {
	_running = false;
	DeleteCreatedItem();
	LogMessage(LOG_INFO, "Publish cancelled\n");
	Q_EMIT Cancelled();
}
