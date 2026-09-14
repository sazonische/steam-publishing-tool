#pragma once

#include "core/game_profile.h"
#include "core/update_checker.h"
#include "steam/publish_pipeline.h"
#include "steam/workshop_service.h"
#include "ui/preview_image_loader.h"
#include "ui/published_items_model.h"

#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QAction>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QTableView>

#include <filesystem>
#include <optional>

class CMainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit CMainWindow(const GameProfile& gameProfile, QWidget* parent = nullptr);

	void EditItemWhenLoaded(PublishedFileId_t publishedFileId);
	// A second copy of the tool was started: show this window instead.
	void BringToFront();

private:
	void BuildActions();
	void BuildToolBar();
	void BuildMenuBar();
	void BuildLanguageMenu(class QMenu* parentMenu);
	void OnLanguageChosen(const std::string& language);
	void BuildItemsTable();

	void RefreshPublishedItems();
	void OnPublishedItemsLoaded(const std::vector<WorkshopItem>& items);
	void OnPublishedItemsPageLoaded(int loadedCount, int totalCount);
	void OnPublishedItemsFailed(const QString& errorMessage);

	void OnEditClicked();
	void OnItemDoubleClicked();
	void OnNewSubmissionClicked();
	void OnReuploadClicked();
	bool PreparePublishContext(PublishContext& context, bool interactive);
	void OpenItemDialog(int mode, const WorkshopItem& item, bool interactiveContext);
	void OnUpdateSubmitted(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement);
	void OnPublishCancelled();
	void OnPublishFailedInBackground(const QString& errorMessage);

	void OnDeleteClicked();
	void OnItemDeleted(PublishedFileId_t publishedFileId);
	void OnItemDeleteFailed(PublishedFileId_t publishedFileId, const QString& errorMessage);

	void OnViewInWorkshopClicked();
	void OnCopyIdClicked();
	void OnCopyLinkClicked();
	void OnOpenAddonFolderClicked();
	void OnOpenPublishedFolderClicked();
	std::optional<std::filesystem::path> FindAddonDirectoryFor(PublishedFileId_t publishedFileId) const;
	std::optional<std::filesystem::path> FindPublishedFolderFor(PublishedFileId_t publishedFileId) const;
	void CheckForUpdates(bool requestedByUser);
	void OnUpdateAvailable(const QString& version, const QString& releaseUrl);
	void OnUpdateCheckUpToDate(const QString& version);
	void OnUpdateCheckFailed(const QString& reason);
	void OnChangeGameClicked();
	void OnPathsClicked();
	void OnUploadPathsClicked();
	void OpenPathsDialog(int tab);
	void RefreshGamePathStatus();
	void OnSelectionChanged();
	void OnTableContextMenu(const QPoint& position);

	PublishedFileId_t GetSelectedPublishedFileId() const;
	void UpdateStatusText();

	const GameProfile& _gameProfile;
	PublishedFileId_t _pendingEditPublishedFileId{0};

	CWorkshopService* _workshopService{nullptr};
	CPublishPipeline* _publishPipeline{nullptr};
	bool _gamePathValid{false};
	CPreviewImageLoader* _previewImageLoader{nullptr};
	CPublishedItemsModel* _itemsModel{nullptr};
	QSortFilterProxyModel* _sortProxyModel{nullptr};

	QTableView* _itemsTableView{nullptr};
	QLabel* _statusLabel{nullptr};
	QLabel* _gamePathLabel{nullptr};

	QAction* _refreshAction{nullptr};
	QAction* _newItemAction{nullptr};
	QAction* _reuploadAction{nullptr};
	QAction* _editItemAction{nullptr};
	QAction* _deleteItemAction{nullptr};
	QAction* _viewInWorkshopAction{nullptr};
	QAction* _copyIdAction{nullptr};
	QAction* _copyLinkAction{nullptr};
	QAction* _openAddonFolderAction{nullptr};
	QAction* _openPublishedFolderAction{nullptr};
	CUpdateChecker* _updateChecker{nullptr};
	bool _updateCheckRequestedByUser{false};
	QAction* _changeGameAction{nullptr};
	QAction* _pathsAction{nullptr};
	QAction* _uploadPathsAction{nullptr};
};
