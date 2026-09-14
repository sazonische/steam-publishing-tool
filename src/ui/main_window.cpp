#include "ui/main_window.h"

#include "core/addon_library.h"
#include "core/app_config.h"
#include "core/app_paths.h"
#include "core/game_paths.h"
#include "core/required_tags.h"
#include "core/upload_rules.h"
#include "steam/steam_names.h"
#include "steam/steam_session.h"
#include "ui/delete_confirm_dialog.h"
#include "ui/game_select_dialog.h"
#include "ui/item_edit_dialog.h"
#include "ui/paths_dialog.h"
#include "ui/published_item_delegate.h"

namespace {

	constexpr const char* WORKSHOP_FILE_PAGE_URL = "https://steamcommunity.com/sharedfiles/filedetails/?id=%1";
	constexpr int TABLE_ROW_HEIGHT = 112;
	constexpr int UPDATE_CHECK_DELAY_MS = 4000;

} // namespace

CMainWindow::CMainWindow(const GameProfile& gameProfile, QWidget* parent) :
	QMainWindow(parent),
	_gameProfile(gameProfile) {
	setWindowTitle(QStringLiteral("Steam Publishing Tool %1 — %2 — %3").arg(QLatin1String(APP_VERSION), QtText::FromStringView(gameProfile.displayName), QString::fromStdString(SteamSession().GetPersonaName())));
	setWindowIcon(QIcon(":/icons/app.svg"));
	resize(1600, 900);

	_workshopService = new CWorkshopService(this);
	_workshopService->SetLanguage(AppConfig().Data().workshopLanguage);
	_publishPipeline = new CPublishPipeline(_workshopService, this);
	_previewImageLoader = new CPreviewImageLoader(this);
	_itemsModel = new CPublishedItemsModel(this);
	_updateChecker = new CUpdateChecker(this);

	_sortProxyModel = new QSortFilterProxyModel(this);
	_sortProxyModel->setSourceModel(_itemsModel);
	_sortProxyModel->setSortRole(CPublishedItemsModel::ROLE_SORT_VALUE);
	_sortProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

	BuildActions();
	BuildToolBar();
	BuildMenuBar();
	BuildItemsTable();

	_statusLabel = new QLabel(this);
	statusBar()->addWidget(_statusLabel, 1);
	_gamePathLabel = new QLabel(this);
	statusBar()->addPermanentWidget(_gamePathLabel);
	auto* versionLabel = new QLabel(QStringLiteral("v%1").arg(QLatin1String(APP_VERSION)), this);
	versionLabel->setObjectName("hintLabel");
	versionLabel->setToolTip(tr("Steam Publishing Tool %1 · Help → Check for Updates… looks for a newer release").arg(QLatin1String(APP_VERSION)));
	statusBar()->addPermanentWidget(versionLabel);
	RefreshGamePathStatus();

	connect(_workshopService, &CWorkshopService::PublishedItemsLoaded, this, &CMainWindow::OnPublishedItemsLoaded);
	connect(_workshopService, &CWorkshopService::PublishedItemsPageLoaded, this, &CMainWindow::OnPublishedItemsPageLoaded);
	connect(_workshopService, &CWorkshopService::PublishedItemsFailed, this, &CMainWindow::OnPublishedItemsFailed);
	connect(_workshopService, &CWorkshopService::ItemDeleted, this, &CMainWindow::OnItemDeleted);
	connect(_workshopService, &CWorkshopService::ItemDeleteFailed, this, &CMainWindow::OnItemDeleteFailed);
	connect(_workshopService, &CWorkshopService::UpdateSubmitted, this, &CMainWindow::OnUpdateSubmitted);
	connect(_previewImageLoader, &CPreviewImageLoader::PreviewLoaded, _itemsModel, &CPublishedItemsModel::SetPreviewPixmap);
	connect(_publishPipeline, &CPublishPipeline::Cancelled, this, &CMainWindow::OnPublishCancelled);
	connect(_publishPipeline, &CPublishPipeline::Failed, this, &CMainWindow::OnPublishFailedInBackground);

	connect(_updateChecker, &CUpdateChecker::UpdateAvailable, this, &CMainWindow::OnUpdateAvailable);
	connect(_updateChecker, &CUpdateChecker::UpToDate, this, &CMainWindow::OnUpdateCheckUpToDate);
	connect(_updateChecker, &CUpdateChecker::CheckFailed, this, &CMainWindow::OnUpdateCheckFailed);

	OnSelectionChanged();
	RefreshPublishedItems();
	QTimer::singleShot(UPDATE_CHECK_DELAY_MS, this, [this]() { CheckForUpdates(false); });
}

void CMainWindow::BuildActions() {
	_newItemAction = new QAction(QIcon(":/icons/new.svg"), tr("New Submission…"), this);
	_newItemAction->setIconText(tr("New"));
	_newItemAction->setShortcut(QKeySequence::New);
	_newItemAction->setToolTip(tr("Publish an addon folder as a new workshop item"));
	connect(_newItemAction, &QAction::triggered, this, &CMainWindow::OnNewSubmissionClicked);

	_reuploadAction = new QAction(QIcon(":/icons/reupload.svg"), tr("Re-Upload"), this);
	_reuploadAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_U));
	_reuploadAction->setToolTip(tr("Pack the addon folder again and replace the item's content on Steam"));
	connect(_reuploadAction, &QAction::triggered, this, &CMainWindow::OnReuploadClicked);

	_editItemAction = new QAction(QIcon(":/icons/edit.svg"), tr("Edit Workshop Info"), this);
	_editItemAction->setIconText(tr("Edit"));
	_editItemAction->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_E), QKeySequence(Qt::Key_Return)});
	_editItemAction->setToolTip(tr("Edit title, description, preview and tags"));
	connect(_editItemAction, &QAction::triggered, this, &CMainWindow::OnEditClicked);

	_viewInWorkshopAction = new QAction(QIcon(":/icons/view.svg"), tr("View in Workshop"), this);
	_viewInWorkshopAction->setIconText(tr("View"));
	_viewInWorkshopAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));
	_viewInWorkshopAction->setToolTip(tr("Open the item page on the Steam Workshop"));
	connect(_viewInWorkshopAction, &QAction::triggered, this, &CMainWindow::OnViewInWorkshopClicked);

	_copyIdAction = new QAction(tr("Copy Published File ID"), this);
	_copyIdAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
	connect(_copyIdAction, &QAction::triggered, this, &CMainWindow::OnCopyIdClicked);

	_copyLinkAction = new QAction(tr("Copy Workshop Link"), this);
	_copyLinkAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
	connect(_copyLinkAction, &QAction::triggered, this, &CMainWindow::OnCopyLinkClicked);

	_openAddonFolderAction = new QAction(tr("Open Addon Folder"), this);
	_openAddonFolderAction->setToolTip(tr("The addon folder this item was published from"));
	connect(_openAddonFolderAction, &QAction::triggered, this, &CMainWindow::OnOpenAddonFolderClicked);

	_openPublishedFolderAction = new QAction(tr("Open Published VPK Folder"), this);
	_openPublishedFolderAction->setToolTip(tr("The vpks/<id> folder with the chunks that went to Steam"));
	connect(_openPublishedFolderAction, &QAction::triggered, this, &CMainWindow::OnOpenPublishedFolderClicked);

	_deleteItemAction = new QAction(QIcon(":/icons/delete.svg"), tr("Delete Submission"), this);
	_deleteItemAction->setIconText(tr("Delete"));
	_deleteItemAction->setShortcut(QKeySequence::Delete);
	_deleteItemAction->setToolTip(tr("Delete the item from the Steam Workshop"));
	connect(_deleteItemAction, &QAction::triggered, this, &CMainWindow::OnDeleteClicked);

	_refreshAction = new QAction(QIcon(":/icons/refresh.svg"), tr("Refresh"), this);
	_refreshAction->setShortcut(QKeySequence::Refresh);
	_refreshAction->setToolTip(tr("Reload the list of published items from Steam"));
	connect(_refreshAction, &QAction::triggered, this, &CMainWindow::RefreshPublishedItems);

	_uploadPathsAction = new QAction(QIcon(":/icons/uploadpaths.svg"), tr("Upload Paths…"), this);
	_uploadPathsAction->setIconText(tr("Upload Paths"));
	_uploadPathsAction->setToolTip(tr("Which addon folders are packed into the workshop VPK"));
	connect(_uploadPathsAction, &QAction::triggered, this, &CMainWindow::OnUploadPathsClicked);

	_pathsAction = new QAction(QIcon(":/icons/folder.svg"), tr("Paths…"), this);
	_pathsAction->setIconText(tr("Paths"));
	_pathsAction->setToolTip(tr("Steam and game folders"));
	connect(_pathsAction, &QAction::triggered, this, &CMainWindow::OnPathsClicked);

	_changeGameAction = new QAction(QIcon(":/icons/game.svg"), tr("Change Game…"), this);
	_changeGameAction->setIconText(tr("Game"));
	_changeGameAction->setToolTip(tr("Switch to another game's workshop"));
	connect(_changeGameAction, &QAction::triggered, this, &CMainWindow::OnChangeGameClicked);
}

void CMainWindow::BuildToolBar() {
	QToolBar* toolBar = addToolBar(tr("Main"));
	toolBar->setMovable(false);
	toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	toolBar->setIconSize(QSize(32, 32));

	toolBar->addAction(_newItemAction);
	toolBar->addAction(_reuploadAction);
	toolBar->addAction(_editItemAction);
	toolBar->addAction(_viewInWorkshopAction);
	toolBar->addAction(_deleteItemAction);
	toolBar->addAction(_refreshAction);

	auto* spacerWidget = new QWidget(toolBar);
	spacerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	toolBar->addWidget(spacerWidget);

	toolBar->addAction(_uploadPathsAction);
	toolBar->addAction(_pathsAction);
	toolBar->addAction(_changeGameAction);
}

void CMainWindow::BuildMenuBar() {
	QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
	fileMenu->addAction(_refreshAction);
	BuildLanguageMenu(fileMenu);
	fileMenu->addSeparator();
	fileMenu->addAction(_uploadPathsAction);
	fileMenu->addAction(_pathsAction);
	fileMenu->addAction(_changeGameAction);
	fileMenu->addSeparator();
	QAction* exitAction = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
	exitAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F4));

	QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
	editMenu->addAction(_newItemAction);
	editMenu->addAction(_reuploadAction);
	editMenu->addAction(_editItemAction);
	editMenu->addAction(_viewInWorkshopAction);
	editMenu->addSeparator();
	editMenu->addAction(_copyIdAction);
	editMenu->addAction(_copyLinkAction);
	editMenu->addAction(_openAddonFolderAction);
	editMenu->addAction(_openPublishedFolderAction);
	editMenu->addSeparator();
	editMenu->addAction(_deleteItemAction);

	QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
	helpMenu->addAction(tr("Open Settings Folder"), this, []() {
		QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::AppHome()));
	});
	helpMenu->addAction(tr("Open Log Folder"), this, []() {
		QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::LogDir()));
	});
	helpMenu->addSeparator();
	helpMenu->addAction(tr("Check for Updates…"), this, [this]() { CheckForUpdates(true); });
	helpMenu->addAction(tr("About"), this, [this]() {
		QMessageBox::about(this, tr("About Steam Publishing Tool"), tr("<b>Steam Publishing Tool</b> %1<br>Steam Workshop publishing for Counter-Strike 2 and Portal 2.<br><br>Built with Qt %2 and the Steamworks SDK.").arg(APP_VERSION, QLatin1String(qVersion())));
	});
}

void CMainWindow::BuildLanguageMenu(QMenu* parentMenu) {
	QMenu* languageMenu = parentMenu->addMenu(tr("Workshop &Language"));
	languageMenu->setToolTipsVisible(true);
	auto* group = new QActionGroup(languageMenu);
	group->setExclusive(true);
	const std::string& current = _workshopService->GetLanguage();
	for (const SteamNames::WorkshopLanguage& language : SteamNames::WorkshopLanguages()) {
		QAction* action = languageMenu->addAction(QtText::FromStringView(language.displayName));
		action->setCheckable(true);
		action->setChecked(language.apiName == current);
		action->setToolTip(tr("Read and write item titles and descriptions in this language"));
		group->addAction(action);
		const std::string apiName(language.apiName);
		connect(action, &QAction::triggered, this, [this, apiName]() { OnLanguageChosen(apiName); });
	}
}

void CMainWindow::OnLanguageChosen(const std::string& language) {
	if (language == _workshopService->GetLanguage()) {
		return;
	}
	_workshopService->SetLanguage(language);
	AppConfig().Data().workshopLanguage = language;
	AppConfig().Save();
	_statusLabel->setText(tr("Workshop texts in %1, reloading…").arg(QtText::FromStringView(SteamNames::LanguageDisplayName(language))));
	RefreshPublishedItems();
}

void CMainWindow::BuildItemsTable() {
	_itemsTableView = new QTableView(this);
	_itemsTableView->setModel(_sortProxyModel);
	_itemsTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
	_itemsTableView->setSelectionMode(QAbstractItemView::SingleSelection);
	_itemsTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	_itemsTableView->setAlternatingRowColors(false);
	_itemsTableView->setItemDelegateForColumn(CPublishedItemsModel::COLUMN_TITLE, new CPublishedItemDelegate(_itemsTableView));
	_itemsTableView->setSortingEnabled(true);
	_itemsTableView->setWordWrap(true);
	_itemsTableView->setShowGrid(false);
	_itemsTableView->setContextMenuPolicy(Qt::CustomContextMenu);
	_itemsTableView->verticalHeader()->setVisible(false);
	_itemsTableView->verticalHeader()->setDefaultSectionSize(std::max(TABLE_ROW_HEIGHT, QFontMetrics(_itemsTableView->font()).height() * 4 + 40));
	_itemsTableView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	_itemsTableView->sortByColumn(CPublishedItemsModel::COLUMN_UPDATED, Qt::DescendingOrder);

	QHeaderView* header = _itemsTableView->horizontalHeader();
	header->setStretchLastSection(false);
	header->setSectionResizeMode(QHeaderView::Interactive);
	header->setMinimumSectionSize(100);
	header->setSectionResizeMode(CPublishedItemsModel::COLUMN_TITLE, QHeaderView::Stretch);
	header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	header->resizeSection(CPublishedItemsModel::COLUMN_VISIBILITY, 120);
	header->resizeSection(CPublishedItemsModel::COLUMN_UPDATED, 160);
	header->resizeSection(CPublishedItemsModel::COLUMN_SIZE, 120);
	for (const int column : {CPublishedItemsModel::COLUMN_PREVIEW, CPublishedItemsModel::COLUMN_TAGS, CPublishedItemsModel::COLUMN_DESCRIPTION, CPublishedItemsModel::COLUMN_CREATED, CPublishedItemsModel::COLUMN_PUBLISHED_ID}) {
		_itemsTableView->hideColumn(column);
	}
	_itemsTableView->setMinimumWidth(760);

	setCentralWidget(_itemsTableView);

	connect(_itemsTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CMainWindow::OnSelectionChanged);
	connect(_itemsTableView, &QTableView::doubleClicked, this, &CMainWindow::OnItemDoubleClicked);
	connect(_itemsTableView, &QTableView::customContextMenuRequested, this, &CMainWindow::OnTableContextMenu);
}

void CMainWindow::RefreshPublishedItems() {
	if (_workshopService->IsQueryInProgress()) {
		return;
	}
	_refreshAction->setEnabled(false);
	_statusLabel->setText(tr("Loading published items…"));
	_workshopService->RequestPublishedItems();
}

void CMainWindow::OnPublishedItemsLoaded(const std::vector<WorkshopItem>& items) {
	_itemsModel->SetItems(items);
	_refreshAction->setEnabled(true);
	UpdateStatusText();

	for (const WorkshopItem& item : items) {
		_previewImageLoader->Request(item.publishedFileId, QUrl(QString::fromStdString(item.previewUrl)));
	}

	if (_pendingEditPublishedFileId != 0) {
		const PublishedFileId_t publishedFileId = _pendingEditPublishedFileId;
		_pendingEditPublishedFileId = 0;
		for (int row = 0; row < _sortProxyModel->rowCount(); ++row) {
			if (_sortProxyModel->index(row, 0).data(CPublishedItemsModel::ROLE_PUBLISHED_FILE_ID).toULongLong() == publishedFileId) {
				_itemsTableView->selectRow(row);
				break;
			}
		}
		OnEditClicked();
	}
}

void CMainWindow::BringToFront() {
	setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
	show();
	raise();
	activateWindow();
}

void CMainWindow::EditItemWhenLoaded(PublishedFileId_t publishedFileId) {
	_pendingEditPublishedFileId = publishedFileId;
}

void CMainWindow::OnItemDoubleClicked() {
	if (_reuploadAction->isEnabled()) {
		OnReuploadClicked();
	} else {
		OnEditClicked();
	}
}

void CMainWindow::OnEditClicked() {
	const WorkshopItem* item = _itemsModel->FindItem(GetSelectedPublishedFileId());
	if (item) {
		OpenItemDialog(CItemEditDialog::MODE_EDIT, *item, false);
	}
}

bool CMainWindow::PreparePublishContext(PublishContext& context, bool interactive) {
	const ResolvedGamePaths paths = GamePaths().Resolve(_gameProfile);
	context = {};

	if (_gameProfile.contentKind == WORKSHOP_CONTENT_SOURCE1_BSP) {
		if (paths.game.valid && !_gameProfile.mapsFolder.empty()) {
			context.mapsFolder = paths.game.path / _gameProfile.mapsFolder;
		}
		return true;
	}

	if (!paths.game.valid) {
		if (interactive) {
			QMessageBox::warning(this, tr("Publish"), tr("Game folder not found: %1\n\nSet it in Paths… first.").arg(QString::fromStdString(paths.game.problem)));
		}
		return false;
	}
	if (_gameProfile.contentKind != WORKSHOP_CONTENT_SOURCE2_ADDON) {
		if (interactive) {
			QMessageBox::information(this, tr("Publish"), tr("Content upload for %1 is not supported yet.").arg(QtText::FromStringView(_gameProfile.displayName)));
		}
		return false;
	}

	std::string errorMessage;
	const std::optional<EffectiveUploadRules> effectiveRules = UploadRules::Resolve(_gameProfile, paths.game.path, errorMessage);
	if (!effectiveRules.has_value()) {
		if (interactive) {
			QMessageBox::warning(this, tr("Publish"), tr("Cannot load upload rules: %1").arg(QString::fromStdString(errorMessage)));
		}
		return false;
	}
	context.rules = effectiveRules->rules;

	for (AddonInfo& addon : AddonLibrary::Enumerate(paths.addonsRoot)) {
		if (!addon.isTemplate) {
			context.addons.push_back(std::move(addon));
		}
	}
	if (context.addons.empty()) {
		if (interactive) {
			QMessageBox::information(this, tr("Publish"), tr("No addon folders found in %1.\n\nCreate and compile an addon in the Workshop Tools first.").arg(QString::fromStdWString(paths.addonsRoot.wstring())));
		}
		return false;
	}

	context.publishedRoot = paths.addonsRoot / "vpks";
	return true;
}

void CMainWindow::OnNewSubmissionClicked() {
	OpenItemDialog(CItemEditDialog::MODE_NEW, WorkshopItem{}, true);
}

void CMainWindow::OnReuploadClicked() {
	const WorkshopItem* item = _itemsModel->FindItem(GetSelectedPublishedFileId());
	if (item) {
		OpenItemDialog(CItemEditDialog::MODE_REUPLOAD, *item, true);
	}
}

void CMainWindow::OpenItemDialog(int mode, const WorkshopItem& item, bool interactiveContext) {
	if (_publishPipeline->IsRunning()) {
		QMessageBox::information(this, tr("Publish"), tr("Another publish is still running."));
		return;
	}
	PublishContext context;
	const bool contextReady = PreparePublishContext(context, interactiveContext);
	if (!contextReady && interactiveContext) {
		return;
	}

	const auto dialogMode = static_cast<CItemEditDialog::Mode>(mode);
	CItemEditDialog dialog(_gameProfile, dialogMode, item, std::move(context), _previewImageLoader, _publishPipeline, this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	if (dialogMode == CItemEditDialog::MODE_NEW) {
		_statusLabel->setText(tr("Published %1, refreshing…").arg(dialog.GetPublishedFileId()));
	} else {
		_statusLabel->setText(tr("Item %1 updated, refreshing…").arg(item.publishedFileId));
	}
	RefreshPublishedItems();
}

void CMainWindow::OnUpdateSubmitted(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement) {
	Q_UNUSED(userNeedsToAcceptLegalAgreement);
	_statusLabel->setText(tr("Item %1 updated, refreshing…").arg(publishedFileId));
	RefreshPublishedItems();
}

void CMainWindow::OnPublishCancelled() {
	_statusLabel->setText(tr("Upload cancelled, refreshing…"));
	RefreshPublishedItems();
}

void CMainWindow::OnPublishFailedInBackground(const QString& errorMessage) {
	if (findChild<CItemEditDialog*>()) {
		return;
	}
	_statusLabel->setText(tr("Upload failed: %1").arg(errorMessage.section(QLatin1Char('\n'), 0, 0)));
	RefreshPublishedItems();
}

void CMainWindow::OnPublishedItemsPageLoaded(int loadedCount, int totalCount) {
	_statusLabel->setText(tr("Loading published items… %1 / %2").arg(loadedCount).arg(totalCount));
}

void CMainWindow::OnPublishedItemsFailed(const QString& errorMessage) {
	_refreshAction->setEnabled(true);
	_statusLabel->setText(tr("Failed to load published items: %1").arg(errorMessage));
	QMessageBox::warning(this, tr("Steam Workshop"), tr("Failed to load published items.\n\n%1").arg(errorMessage));
}

void CMainWindow::OnDeleteClicked() {
	const PublishedFileId_t publishedFileId = GetSelectedPublishedFileId();
	const WorkshopItem* item = _itemsModel->FindItem(publishedFileId);
	if (!item) {
		return;
	}

	QPixmap previewPixmap;
	if (const QByteArray* rawImageData = _previewImageLoader->GetRawImageData(publishedFileId)) {
		previewPixmap = QPixmap::fromImage(QImage::fromData(*rawImageData));
	}
	CDeleteConfirmDialog confirmDialog(*item, previewPixmap, this);
	if (confirmDialog.exec() != QDialog::Accepted) {
		return;
	}

	_deleteItemAction->setEnabled(false);
	_statusLabel->setText(tr("Deleting %1…").arg(publishedFileId));
	_workshopService->DeleteItem(publishedFileId);
}

void CMainWindow::OnItemDeleted(PublishedFileId_t publishedFileId) {
	_itemsModel->RemoveItem(publishedFileId);
	OnSelectionChanged();
	UpdateStatusText();
}

void CMainWindow::OnItemDeleteFailed(PublishedFileId_t publishedFileId, const QString& errorMessage) {
	OnSelectionChanged();
	UpdateStatusText();
	QMessageBox::warning(this, tr("Delete workshop item"), tr("Failed to delete item %1.\n\n%2").arg(publishedFileId).arg(errorMessage));
}

void CMainWindow::OnViewInWorkshopClicked() {
	const PublishedFileId_t publishedFileId = GetSelectedPublishedFileId();
	if (publishedFileId == 0) {
		return;
	}
	QDesktopServices::openUrl(QUrl(QString::fromLatin1(WORKSHOP_FILE_PAGE_URL).arg(publishedFileId)));
}

void CMainWindow::OnChangeGameClicked() {
	CGameSelectDialog dialog(this);
	dialog.SetCurrentGame(_gameProfile.id);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	const GameProfile* selectedGame = dialog.GetSelectedGame();
	if (!selectedGame) {
		return;
	}

	if (dialog.ShouldRememberChoice()) {
		AppConfig().Data().rememberedGameId = std::string(selectedGame->id);
	} else {
		AppConfig().Data().rememberedGameId.reset();
	}
	AppConfig().Save();

	if (selectedGame->appId == _gameProfile.appId) {
		return;
	}

	// Steamworks cannot re-initialise under another AppID in the same process — restart.
	const QString gameIdArgument = QtText::FromStringView(selectedGame->id);
	if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--game"), gameIdArgument})) {
		QMessageBox::warning(this, tr("Change game"), tr("Failed to relaunch the application."));
		return;
	}
	QCoreApplication::quit();
}

void CMainWindow::OnPathsClicked() {
	OpenPathsDialog(CPathsDialog::TAB_FOLDERS);
}

void CMainWindow::OnUploadPathsClicked() {
	OpenPathsDialog(CPathsDialog::TAB_UPLOAD_PATHS);
}

void CMainWindow::OpenPathsDialog(int tab) {
	CPathsDialog pathsDialog(this);
	pathsDialog.ShowTab(static_cast<CPathsDialog::Tab>(tab));
	connect(&pathsDialog, &CPathsDialog::PathsChanged, this, &CMainWindow::RefreshGamePathStatus);
	pathsDialog.exec();
}

void CMainWindow::RefreshGamePathStatus() {
	const ResolvedGamePaths paths = GamePaths().Resolve(_gameProfile);
	_gamePathValid = paths.game.valid;
	_itemsModel->SetRequiredTags(RequiredTags::Resolve(_gameProfile));
	OnSelectionChanged();
	if (paths.game.valid) {
		_gamePathLabel->setText(QStringLiteral("%1   ·   %2").arg(QDir::toNativeSeparators(QString::fromStdWString(paths.game.path.wstring())), QtText::FromStringView(PathSourceName(paths.game.source))));
		_gamePathLabel->setStyleSheet(QString());
		_gamePathLabel->setToolTip(QString());
		return;
	}
	if (_gameProfile.contentKind == WORKSHOP_CONTENT_SOURCE1_BSP) {
		_gamePathLabel->setText(tr("Game folder not found — you will pick the .bsp file yourself. Set the folder in Paths… to start in its maps folder."));
		_gamePathLabel->setStyleSheet("color: #e0b050;");
	} else {
		_gamePathLabel->setText(tr("Game folder not found — publishing disabled. Set it in Paths…"));
		_gamePathLabel->setStyleSheet("color: #ff5a5a;");
	}
	_gamePathLabel->setToolTip(QString::fromStdString(paths.game.problem));
}

void CMainWindow::OnSelectionChanged() {
	const bool hasSelection = GetSelectedPublishedFileId() != 0;
	_editItemAction->setEnabled(hasSelection);
	_deleteItemAction->setEnabled(hasSelection);
	const bool canPublishContent = _gameProfile.contentKind == WORKSHOP_CONTENT_SOURCE1_BSP || (_gamePathValid && !_gameProfile.addonsRoot.empty());
	_reuploadAction->setEnabled(hasSelection && canPublishContent);
	_newItemAction->setEnabled(canPublishContent);
	UpdateStatusText();
	_viewInWorkshopAction->setEnabled(hasSelection);
	_copyIdAction->setEnabled(hasSelection);
	_copyLinkAction->setEnabled(hasSelection);
	const PublishedFileId_t selectedId = GetSelectedPublishedFileId();
	_openAddonFolderAction->setEnabled(hasSelection && FindAddonDirectoryFor(selectedId).has_value());
	_openPublishedFolderAction->setEnabled(hasSelection && FindPublishedFolderFor(selectedId).has_value());
}

void CMainWindow::OnCopyIdClicked() {
	const PublishedFileId_t publishedFileId = GetSelectedPublishedFileId();
	if (publishedFileId != 0) {
		QGuiApplication::clipboard()->setText(QString::number(publishedFileId));
		_statusLabel->setText(tr("Copied %1 to the clipboard").arg(publishedFileId));
	}
}

void CMainWindow::OnCopyLinkClicked() {
	const PublishedFileId_t publishedFileId = GetSelectedPublishedFileId();
	if (publishedFileId != 0) {
		QGuiApplication::clipboard()->setText(QString::fromLatin1(WORKSHOP_FILE_PAGE_URL).arg(publishedFileId));
		_statusLabel->setText(tr("Copied the Workshop link of %1 to the clipboard").arg(publishedFileId));
	}
}

// The addon an item came from is known through vpks/<id>/publish_data.txt; games without addons (Portal 2) have none.
std::optional<std::filesystem::path> CMainWindow::FindAddonDirectoryFor(PublishedFileId_t publishedFileId) const {
	if (publishedFileId == 0 || _gameProfile.addonsRoot.empty()) {
		return std::nullopt;
	}
	const ResolvedGamePaths paths = GamePaths().Resolve(_gameProfile);
	if (!paths.game.valid) {
		return std::nullopt;
	}
	for (const AddonInfo& addon : AddonLibrary::Enumerate(paths.addonsRoot)) {
		if (addon.publishedFileId.has_value() && *addon.publishedFileId == publishedFileId) {
			return addon.directory;
		}
	}
	return std::nullopt;
}

std::optional<std::filesystem::path> CMainWindow::FindPublishedFolderFor(PublishedFileId_t publishedFileId) const {
	if (publishedFileId == 0 || _gameProfile.addonsRoot.empty()) {
		return std::nullopt;
	}
	const ResolvedGamePaths paths = GamePaths().Resolve(_gameProfile);
	if (!paths.game.valid) {
		return std::nullopt;
	}
	const std::filesystem::path folder = paths.addonsRoot / "vpks" / std::to_string(publishedFileId);
	std::error_code errorCode;
	return std::filesystem::is_directory(folder, errorCode) ? std::optional<std::filesystem::path>(folder) : std::nullopt;
}

void CMainWindow::OnOpenAddonFolderClicked() {
	if (const std::optional<std::filesystem::path> folder = FindAddonDirectoryFor(GetSelectedPublishedFileId())) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdWString(folder->wstring())));
	}
}

void CMainWindow::OnOpenPublishedFolderClicked() {
	if (const std::optional<std::filesystem::path> folder = FindPublishedFolderFor(GetSelectedPublishedFileId())) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdWString(folder->wstring())));
	}
}

void CMainWindow::CheckForUpdates(bool requestedByUser) {
	_updateCheckRequestedByUser = requestedByUser;
	if (requestedByUser) {
		_statusLabel->setText(tr("Checking for updates…"));
	}
	_updateChecker->Check();
}

void CMainWindow::OnUpdateAvailable(const QString& version, const QString& releaseUrl) {
	const std::optional<std::string>& skipped = AppConfig().Data().skippedUpdateVersion;
	if (!_updateCheckRequestedByUser && skipped.has_value() && QString::fromStdString(*skipped) == version) {
		LogMessage(LOG_INFO, "Update %s was skipped earlier, not asking again\n", version.toUtf8().constData());
		return;
	}

	QMessageBox box(this);
	box.setWindowTitle(tr("Update available"));
	box.setIcon(QMessageBox::Information);
	box.setText(tr("Steam Publishing Tool %1 is available.").arg(version));
	box.setInformativeText(tr("You are running %1. The release page has the single-file exe; your settings and published items are not touched by an update.").arg(QLatin1String(APP_VERSION)));
	QPushButton* openButton = box.addButton(tr("Open release page"), QMessageBox::AcceptRole);
	QPushButton* skipButton = box.addButton(tr("Skip %1").arg(version), QMessageBox::DestructiveRole);
	box.addButton(tr("Later"), QMessageBox::RejectRole);
	box.setDefaultButton(openButton);
	box.exec();

	if (box.clickedButton() == openButton) {
		QDesktopServices::openUrl(QUrl(releaseUrl));
	} else if (box.clickedButton() == skipButton) {
		AppConfig().Data().skippedUpdateVersion = version.toStdString();
		if (!AppConfig().Save()) {
			LogMessage(LOG_WARN, "Could not remember the skipped update version\n");
		}
	}
}

void CMainWindow::OnUpdateCheckUpToDate(const QString& version) {
	if (_updateCheckRequestedByUser) {
		QMessageBox::information(this, tr("Check for Updates"), tr("You are running the newest release, %1.").arg(version));
		UpdateStatusText();
	}
}

void CMainWindow::OnUpdateCheckFailed(const QString& reason) {
	if (_updateCheckRequestedByUser) {
		QMessageBox::warning(this, tr("Check for Updates"), tr("Could not reach GitHub to check for updates: %1").arg(reason));
		UpdateStatusText();
	}
}

void CMainWindow::OnTableContextMenu(const QPoint& position) {
	const QModelIndex clickedIndex = _itemsTableView->indexAt(position);
	if (!clickedIndex.isValid()) {
		return;
	}
	_itemsTableView->selectRow(clickedIndex.row());

	QMenu contextMenu(this);
	contextMenu.addAction(_reuploadAction);
	contextMenu.setDefaultAction(_reuploadAction);
	contextMenu.addAction(_editItemAction);
	contextMenu.addAction(_viewInWorkshopAction);
	contextMenu.addSeparator();
	contextMenu.addAction(_copyIdAction);
	contextMenu.addAction(_copyLinkAction);
	contextMenu.addAction(_openAddonFolderAction);
	contextMenu.addAction(_openPublishedFolderAction);
	contextMenu.addSeparator();
	contextMenu.addAction(_deleteItemAction);
	contextMenu.exec(_itemsTableView->viewport()->mapToGlobal(position));
}

PublishedFileId_t CMainWindow::GetSelectedPublishedFileId() const {
	const QModelIndexList selectedRows = _itemsTableView->selectionModel()->selectedRows();
	if (selectedRows.isEmpty()) {
		return 0;
	}
	return static_cast<PublishedFileId_t>(selectedRows.first().data(CPublishedItemsModel::ROLE_PUBLISHED_FILE_ID).toULongLong());
}

void CMainWindow::UpdateStatusText() {
	const int itemsCount = static_cast<int>(_itemsModel->GetItems().size());
	QString text = tr("%n published item(s)", nullptr, itemsCount);
	if (const WorkshopItem* selectedItem = _itemsModel->FindItem(GetSelectedPublishedFileId())) {
		text = tr("Title: \"%1\"   Published FileID: %2").arg(QString::fromStdString(selectedItem->title)).arg(selectedItem->publishedFileId);
	}
	_statusLabel->setText(text + QStringLiteral("   ·   %1").arg(QString::fromStdString(SteamSession().GetPersonaName())));
}
