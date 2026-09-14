#pragma once

#include "core/addon_library.h"
#include "core/game_profile.h"
#include "steam/workshop_service.h"
#include "steam/workshop_update.h"

#include <atomic>

// Everything the publish dialogs need to know about the game: the addon list and packing
// rules (Source 2) or the maps folder (Source 1).
struct PublishContext {
	std::vector<AddonInfo> addons;
	AddonVpkRules rules;
	std::filesystem::path publishedRoot; // <addonsRoot>/vpks
	std::filesystem::path mapsFolder;	 // Source 1: portal2/maps, empty when the game is not found
};

// New item: CreateItem -> pack -> SubmitUpdate. Existing: pack -> SubmitUpdate, metadata in the same call.
// Source 1: no packing, the BSP goes to Steam Cloud, then PublishWorkshopFile / UpdatePublishedFileFile.
struct PublishJob {
	enum Kind : uint8_t {
		PUBLISH_NEW = 0,
		PUBLISH_REUPLOAD,
	};

	Kind kind{PUBLISH_NEW};
	const GameProfile* profile{nullptr};
	AddonInfo addon;
	AddonManifest manifest;
	std::filesystem::path publishedRoot; // <addonsRoot>/vpks — <id>/ with the chunks is created here
	WorkshopUpdateRequest request;		 // for PUBLISH_NEW publishedFileId is filled in after CreateItem

	// For publish_data.txt: the item title even when this update does not change it.
	std::string displayTitle;

	// Source 1: a single map file.
	std::filesystem::path bspPath;
	std::string cloudFileName; // mymaps/<file>.bsp in Steam Cloud, removed once published
};

// Packing runs on its own thread, Steam results arrive through callbacks on the main thread.
// A new item whose upload fails or is cancelled is deleted. An update Steam already accepted cannot be aborted, Cancel() waits for it.
class CPublishPipeline : public QObject {
	Q_OBJECT

public:
	explicit CPublishPipeline(CWorkshopService* workshopService, QObject* parent = nullptr);

	bool Start(PublishJob job, QString& errorMessage);
	void Cancel();
	bool IsRunning() const { return _running; }
	CWorkshopService* GetWorkshopService() const { return _workshopService; }

Q_SIGNALS:
	void StageChanged(const QString& stageText);
	void PackProgress(int percent, const QString& detail);
	void UploadStarted();
	void Finished(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement);
	void Failed(const QString& errorMessage);
	void Cancelled();

private:
	void OnItemCreated(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement);
	void OnItemCreateFailed(const QString& errorMessage);
	void StartPacking();
	void OnPacked(bool success, const QString& errorMessage);
	void StartBspUpload();
	void OnLegacyFilePublished(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement);
	void OnLegacyFilePublishFailed(const QString& errorMessage);
	void OnLegacyFileUpdated(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement);
	void OnLegacyFileUpdateFailed(PublishedFileId_t publishedFileId, const QString& errorMessage);
	bool IsBspJob() const;
	void OnUpdateSubmitted(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement);
	void OnUpdateFailed(PublishedFileId_t publishedFileId, const QString& errorMessage);
	void Fail(const QString& errorMessage);
	void FinishCancelled();
	void DeleteCreatedItem();

	std::filesystem::path OutputDirectory() const;

	CWorkshopService* _workshopService{nullptr};
	PublishJob _job;
	bool _running{false};
	bool _createdThisRun{false};
	bool _legalAgreementNeeded{false};
	std::atomic<bool> _cancelRequested{false}; // read from the pack thread's progress callback
	QThread* _packThread{nullptr};
};
