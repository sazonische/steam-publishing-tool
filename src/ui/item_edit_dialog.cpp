#include "ui/item_edit_dialog.h"

#include "core/app_config.h"
#include "core/game_paths.h"
#include "core/required_tags.h"
#include "media/preview_image.h"
#include "steam/steam_names.h"

namespace {

	constexpr int PREVIEW_VIEW_WIDTH = 384;
	constexpr int PREVIEW_VIEW_HEIGHT = 216;
	constexpr int PROGRESS_TICK_INTERVAL_MS = 200;
	constexpr int TAG_CHECKBOX_COLUMNS = 3;
	constexpr const char* LEGAL_AGREEMENT_URL = "steam://url/CommunityFilePage/%1";
	constexpr const char* TAG_PROPERTY = "workshopTag";

	QString FromStringView(std::string_view text) {
		return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
	}

	// Steam stores descriptions with CRLF, QPlainTextEdit returns LF — without normalisation an untouched
	// description looks changed and rides along with every update.
	std::string NormalizeLineEndings(std::string text) {
		for (size_t position = text.find("\r\n"); position != std::string::npos; position = text.find("\r\n", position)) {
			text.erase(position, 1);
		}
		return text;
	}

	// YouTube IDs are 11 characters of [A-Za-z0-9_-]; both a bare ID and a link are accepted.
	QString ExtractYouTubeVideoId(const QString& input) {
		const QString trimmed = input.trimmed();
		static const QRegularExpression idPattern(QStringLiteral("^[A-Za-z0-9_-]{11}$"));
		if (idPattern.match(trimmed).hasMatch()) {
			return trimmed;
		}
		static const QRegularExpression urlPattern(QStringLiteral("(?:v=|youtu\\.be/|shorts/|embed/)([A-Za-z0-9_-]{11})"));
		const QRegularExpressionMatch match = urlPattern.match(trimmed);
		return match.hasMatch() ? match.captured(1) : QString();
	}

} // namespace

CItemEditDialog::CItemEditDialog(const GameProfile& gameProfile, Mode mode, const WorkshopItem& item, PublishContext publishContext, CPreviewImageLoader* previewImageLoader, CPublishPipeline* publishPipeline, QWidget* parent) :
	QDialog(parent),
	_gameProfile(gameProfile),
	_item(item),
	_workshopService(publishPipeline->GetWorkshopService()),
	_previewImageLoader(previewImageLoader),
	_mode(mode),
	_isNewSubmission(mode == MODE_NEW),
	_publishContext(std::move(publishContext)),
	_publishPipeline(publishPipeline) {
	_contentAvailable = gameProfile.contentKind == WORKSHOP_CONTENT_SOURCE1_BSP || !_publishContext.addons.empty();

	switch (_mode) {
		case MODE_NEW: setWindowTitle(tr("New workshop item — %1").arg(FromStringView(gameProfile.displayName))); break;
		case MODE_REUPLOAD: setWindowTitle(tr("Update — %1 (%2)").arg(QString::fromStdString(item.title)).arg(item.publishedFileId)); break;
		default: setWindowTitle(tr("Edit — %1 (%2)").arg(QString::fromStdString(item.title)).arg(item.publishedFileId)); break;
	}
	resize(1080, 800);

	for (uint32_t previewIndex = 0; previewIndex < item.additionalPreviews.size(); ++previewIndex) {
		const WorkshopPreview& preview = item.additionalPreviews[previewIndex];
		AdditionalPreviewEntry entry;
		entry.existing = true;
		entry.existingIndex = previewIndex;
		entry.addition.type = preview.type;
		entry.addition.pathOrVideoId = preview.type == k_EItemPreviewType_Image && !preview.originalFileName.empty() ? preview.originalFileName : preview.urlOrVideoId;
		_additionalPreviews.push_back(entry);
	}

	BuildUi();
	ShowCurrentPreview();
	RefreshAdditionalPreviewsList();
	if (_isNewSubmission) {
		OnAddonSelectionChanged();
	}
	OnTitleChanged();
	OnDescriptionChanged();
	UpdateSummary();

	_progressTimer.setInterval(PROGRESS_TICK_INTERVAL_MS);
	connect(&_progressTimer, &QTimer::timeout, this, &CItemEditDialog::OnUpdateProgressTick);
	connect(_workshopService, &CWorkshopService::UpdateSubmitted, this, &CItemEditDialog::OnUpdateSubmitted);
	connect(_workshopService, &CWorkshopService::UpdateFailed, this, &CItemEditDialog::OnUpdateFailed);
	connect(_publishPipeline, &CPublishPipeline::StageChanged, this, &CItemEditDialog::OnPipelineStageChanged);
	connect(_publishPipeline, &CPublishPipeline::PackProgress, this, &CItemEditDialog::OnPipelinePackProgress);
	connect(_publishPipeline, &CPublishPipeline::UploadStarted, this, &CItemEditDialog::OnPipelineUploadStarted);
	connect(_publishPipeline, &CPublishPipeline::Finished, this, &CItemEditDialog::OnPipelineFinished);
	connect(_publishPipeline, &CPublishPipeline::Failed, this, &CItemEditDialog::OnPipelineFailed);
}

void CItemEditDialog::BuildUi() {
	setObjectName("itemEditor");
	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(16, 16, 16, 16);
	mainLayout->setSpacing(12);
	mainLayout->addWidget(BuildHeader());
	mainLayout->addWidget(BuildContentSection());

	auto* tabs = new QTabWidget(this);
	tabs->setObjectName("editorTabs");
	tabs->tabBar()->setObjectName("editorTabBar");
	tabs->tabBar()->setDrawBase(false);
	_tabs = tabs;
	auto* pageWidget = new QWidget(tabs);
	auto* pageLayout = new QVBoxLayout(pageWidget);
	pageLayout->setContentsMargins(0, 12, 0, 0);
	pageLayout->setSpacing(16);
	auto* detailsLayout = new QHBoxLayout();
	detailsLayout->setSpacing(24);
	detailsLayout->addWidget(BuildMetadataSection(), 1);
	detailsLayout->addWidget(BuildPreviewSection(), 0, Qt::AlignTop);
	pageLayout->addLayout(detailsLayout, 1);
	_pageTabIndex = tabs->addTab(pageWidget, tr("Text and preview"));
	auto* tagsPage = new QWidget(tabs);
	auto* tagsLayout = new QVBoxLayout(tagsPage);
	tagsLayout->setContentsMargins(0, 12, 0, 0);
	tagsLayout->addWidget(BuildTagsAndModesSection());
	tagsLayout->addStretch();
	tabs->addTab(tagsPage, FromStringView(_gameProfile.tagsSectionTitle));
	auto* mediaPage = new QWidget(tabs);
	auto* mediaLayout = new QVBoxLayout(mediaPage);
	mediaLayout->setContentsMargins(0, 12, 0, 0);
	mediaLayout->addWidget(BuildAdditionalPreviewsSection());
	tabs->addTab(mediaPage, tr("Images && videos"));

	auto* changeNotePage = new QWidget(tabs);
	auto* changeNoteLayout = new QVBoxLayout(changeNotePage);
	changeNoteLayout->setContentsMargins(0, 12, 0, 0);
	changeNoteLayout->setSpacing(12);
	_changeNoteHintLabel = new QLabel(changeNotePage);
	_changeNoteHintLabel->setWordWrap(true);
	changeNoteLayout->addWidget(_changeNoteHintLabel);
	_changeNoteEdit = new QPlainTextEdit(changeNotePage);
	_changeNoteEdit->setPlaceholderText(tr("This note appears in the item's change history."));
	changeNoteLayout->addWidget(_changeNoteEdit, 1);
	_changeNoteTabIndex = tabs->addTab(changeNotePage, tr("Change note"));
	UpdateChangeNoteHint();

	_progressBar = new QProgressBar(this);
	_progressBar->setRange(0, 100);
	_progressBar->setVisible(false);

	_statusLabel = new QLabel(this);
	_statusLabel->setObjectName("hintLabel");

	_cancelButton = new QPushButton(tr("Cancel"), this);
	QString submitText = tr("Submit");
	if (_isNewSubmission) {
		submitText = tr("Publish");
	} else if (IsReplacingContent()) {
		submitText = tr("Update");
	}
	_submitButton = new QPushButton(submitText, this);
	_submitButton->setObjectName("primaryButton");
	_submitButton->setDefault(true);

	_summaryLabel = new QLabel(this);
	_summaryLabel->setObjectName("hintLabel");
	_summaryLabel->setWordWrap(true);
	_summaryLabel->setTextFormat(Qt::PlainText);

	auto* bottomLayout = new QHBoxLayout();
	bottomLayout->addWidget(_statusLabel, 1);

	bottomLayout->addWidget(_cancelButton);
	bottomLayout->addWidget(_submitButton);

	mainLayout->addWidget(tabs, 1);
	mainLayout->addWidget(_summaryLabel);
	mainLayout->addWidget(_progressBar);
	mainLayout->addLayout(bottomLayout);

	connect(_changeNoteEdit, &QPlainTextEdit::textChanged, this, &CItemEditDialog::UpdateSummary);
	connect(_customTagsEdit, &QLineEdit::textChanged, this, &CItemEditDialog::UpdateSummary);
	for (QCheckBox* checkBox : _requiredTagCheckBoxes) {
		connect(checkBox, &QCheckBox::toggled, this, &CItemEditDialog::UpdateSummary);
	}
	for (QCheckBox* checkBox : _groupTagCheckBoxes) {
		connect(checkBox, &QCheckBox::toggled, this, &CItemEditDialog::UpdateSummary);
	}
	connect(_visibilityComboBox, &QComboBox::currentIndexChanged, this, &CItemEditDialog::UpdateSummary);

	connect(_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
	connect(_submitButton, &QPushButton::clicked, this, &CItemEditDialog::OnSubmitClicked);
}

QWidget* CItemEditDialog::BuildHeader() {
	QString headline;
	QString hint;
	switch (_mode) {
		case MODE_NEW:
			headline = tr("Publish a new map");
			hint = tr("Choose the content, fill in the text, tags and preview, then press Publish. The item is created on Steam only when everything is ready.");
			break;
		case MODE_REUPLOAD:
			headline = tr("Update \"%1\"").arg(QString::fromStdString(_item.title));
			hint = tr("ID %1 · published %2 · %3 on Steam. The new content replaces the current one; the page fields below are sent only if you change them.").arg(_item.publishedFileId).arg(QLocale().toString(QDateTime::fromSecsSinceEpoch(_item.timeCreated), QLocale::ShortFormat)).arg(QLocale().formattedDataSize(static_cast<qint64>(_item.totalFilesSize)));
			break;
		default:
			headline = tr("Edit \"%1\"").arg(QString::fromStdString(_item.title));
			hint = tr("ID %1 · last updated %2 · %3 on Steam. ").arg(_item.publishedFileId).arg(QLocale().toString(QDateTime::fromSecsSinceEpoch(_item.timeUpdated), QLocale::ShortFormat)).arg(QLocale().formattedDataSize(static_cast<qint64>(_item.totalFilesSize)));
			hint += _contentAvailable ? tr("Tick \"Replace the map content\" below to upload new files together with the page changes.") : tr("The map content can only be replaced when the game folder and its addons are found (see Paths…).");
			break;
	}
	if (_mode == MODE_REUPLOAD && !_contentAvailable) {
		hint = tr("ID %1 · no addon folders were found, so the content cannot be replaced right now — check Paths… and compile the map first.").arg(_item.publishedFileId);
	}
	if (!_isNewSubmission && (_item.subscriptions > 0 || _item.favorites > 0 || _item.websiteViews > 0)) {
		hint += QStringLiteral(" ") + tr("Right now: %1 subscribers, %2 favorites, %3 page views, %4 up / %5 down.").arg(QLocale().toString(static_cast<qulonglong>(_item.subscriptions)), QLocale().toString(static_cast<qulonglong>(_item.favorites)), QLocale().toString(static_cast<qulonglong>(_item.websiteViews)), QLocale().toString(_item.votesUp), QLocale().toString(_item.votesDown));
	}

	auto* header = new QWidget(this);
	auto* layout = new QVBoxLayout(header);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(2);
	auto* headlineLabel = new QLabel(headline, header);
	headlineLabel->setObjectName("titleLabel");
	headlineLabel->setTextFormat(Qt::PlainText);
	auto* hintLabel = new QLabel(hint, header);
	hintLabel->setObjectName("hintLabel");
	hintLabel->setWordWrap(true);
	hintLabel->setTextFormat(Qt::PlainText);
	layout->addWidget(headlineLabel);
	layout->addWidget(hintLabel);
	return header;
}

QWidget* CItemEditDialog::CreateContentWidget() {
	if (_gameProfile.contentKind == WORKSHOP_CONTENT_SOURCE1_BSP) {
		_bspContentWidget = new CBspContentWidget(_publishContext.mapsFolder, this);
		connect(_bspContentWidget, &CBspContentWidget::MapInspected, this, &CItemEditDialog::OnMapInspected);
		if (!_isNewSubmission && !_publishContext.mapsFolder.empty()) {
			std::error_code errorCode;
			const std::filesystem::path guessedPath = _publishContext.mapsFolder / (_item.title + ".bsp");
			if (std::filesystem::is_regular_file(guessedPath, errorCode)) {
				_bspContentWidget->SetBspPath(guessedPath);
			}
		}
		return _bspContentWidget;
	}

	_contentWidget = new CAddonContentWidget(_gameProfile, _publishContext.addons, _publishContext.rules, this);
	connect(_contentWidget, &CAddonContentWidget::SelectionChanged, this, &CItemEditDialog::OnAddonSelectionChanged);
	if (!_isNewSubmission) {
		_contentWidget->SelectAddonFor(_item.publishedFileId, _item.title);
	}
	return _contentWidget;
}

QWidget* CItemEditDialog::BuildContentSection() {
	if (_isNewSubmission) {
		return CreateContentWidget();
	}

	auto* section = new QWidget(this);
	auto* layout = new QVBoxLayout(section);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(6);

	_replaceContentCheckBox = new QCheckBox(tr("Replace the map content"), section);
	_replaceContentCheckBox->setChecked(_mode == MODE_REUPLOAD && _contentAvailable);
	_replaceContentCheckBox->setEnabled(_contentAvailable);
	if (!_contentAvailable) {
		_replaceContentCheckBox->setToolTip(tr("No addon folders found — set the game folder in Paths… and compile the map first."));
	}
	layout->addWidget(_replaceContentCheckBox);

	_contentContainer = new QWidget(section);
	auto* containerLayout = new QVBoxLayout(_contentContainer);
	containerLayout->setContentsMargins(0, 0, 0, 0);
	if (_contentAvailable) {
		containerLayout->addWidget(CreateContentWidget());
	}
	_contentContainer->setVisible(_contentAvailable);
	_contentContainer->setEnabled(_replaceContentCheckBox->isChecked());
	layout->addWidget(_contentContainer);

	connect(_replaceContentCheckBox, &QCheckBox::toggled, this, &CItemEditDialog::OnReplaceContentToggled);
	return section;
}

bool CItemEditDialog::IsReplacingContent() const {
	return _isNewSubmission || (_replaceContentCheckBox && _replaceContentCheckBox->isChecked() && _contentAvailable);
}

void CItemEditDialog::OnReplaceContentToggled(bool checked) {
	_contentContainer->setEnabled(checked);
	_submitButton->setText(checked ? tr("Update") : tr("Submit"));
	UpdateChangeNoteHint();
	UpdateSummary();
}

bool CItemEditDialog::IsChangeNoteRequired() const {
	return !_isNewSubmission && IsReplacingContent();
}

void CItemEditDialog::UpdateChangeNoteHint() {
	if (!_changeNoteHintLabel) {
		return;
	}
	_changeNoteHintLabel->setText(IsChangeNoteRequired() ? tr("Describe what changed in this update — required when the map content is replaced.") : tr("Describe what changed in this update (optional)."));
	_tabs->setTabText(_changeNoteTabIndex, IsChangeNoteRequired() ? tr("Change note *") : tr("Change note"));
}

void CItemEditDialog::UpdateSummary() {
	if (!_summaryLabel) {
		return;
	}
	QStringList parts;

	if (IsReplacingContent()) {
		if (_bspContentWidget) {
			const std::filesystem::path bspPath = _bspContentWidget->GetBspPath();
			parts.append(bspPath.empty() ? tr("map file: not chosen") : tr("map file: %1").arg(QString::fromStdWString(bspPath.filename().wstring())));
		} else if (_contentWidget) {
			const AddonInfo* addon = _contentWidget->GetSelectedAddon();
			const AddonManifest& manifest = _contentWidget->GetManifest();
			parts.append(addon ? tr("content: %1, %2 files, %3").arg(QString::fromStdString(addon->name)).arg(manifest.files.size()).arg(QLocale().formattedDataSize(static_cast<qint64>(manifest.totalSize))) : tr("content: addon not chosen"));
		}
	} else if (!_isNewSubmission) {
		parts.append(tr("content unchanged"));
	}

	if (_isNewSubmission) {
		parts.append(_selectedPreviewPath.isEmpty() ? tr("preview: missing") : tr("preview: %1").arg(QFileInfo(_selectedPreviewPath).fileName()));
		parts.append(tr("visibility: %1").arg(_visibilityComboBox->currentText()));
	} else {
		if (_titleEdit->text().trimmed().toStdString() != _item.title) {
			parts.append(tr("title"));
		}
		if (_descriptionEdit->toPlainText().toStdString() != NormalizeLineEndings(_item.description)) {
			parts.append(tr("description"));
		}
		if (static_cast<ERemoteStoragePublishedFileVisibility>(_visibilityComboBox->currentData().toInt()) != _item.visibility) {
			parts.append(tr("visibility: %1").arg(_visibilityComboBox->currentText()));
		}
		if (!_selectedPreviewPath.isEmpty()) {
			parts.append(tr("preview image"));
		}
		int addedPreviews = 0;
		int removedPreviews = 0;
		for (const AdditionalPreviewEntry& entry : _additionalPreviews) {
			addedPreviews += entry.existing ? 0 : 1;
			removedPreviews += entry.existing && entry.markedForRemoval ? 1 : 0;
		}
		if (addedPreviews > 0 || removedPreviews > 0) {
			parts.append(tr("media: %1 added, %2 removed").arg(addedPreviews).arg(removedPreviews));
		}
		if (TagsChanged()) {
			parts.append(tr("tags"));
		}
		if (_contentWidget && IsReplacingContent() && _contentWidget->ExceedsSizeLimit()) {
			parts.append(tr("content is over the size limit"));
		}
	}
	if (!_changeNoteEdit->toPlainText().trimmed().isEmpty()) {
		parts.append(tr("change note"));
	} else if (IsChangeNoteRequired()) {
		parts.append(tr("change note: missing"));
	}

	if (!_isNewSubmission && !IsReplacingContent() && PageChanges().isEmpty()) {
		_summaryLabel->setText(tr("Nothing changed yet — edit the text, tags or media, or tick \"Replace the map content\"."));
		return;
	}
	_summaryLabel->setText((_isNewSubmission ? tr("Will publish: ") : tr("Will send: ")) + parts.join(QStringLiteral(" · ")));
}

bool CItemEditDialog::TagsChanged() const {
	if (_isNewSubmission) {
		return true;
	}
	const std::vector<std::string> tags = CollectTags();
	if (tags.size() != _item.tags.size()) {
		return true;
	}
	return std::ranges::any_of(tags, [this](const std::string& tag) { return !RequiredTags::ContainsTag(_item.tags, tag); });
}

QStringList CItemEditDialog::PageChanges() const {
	QStringList changes;
	if (_titleEdit->text().trimmed().toStdString() != _item.title) {
		changes.append(tr("title"));
	}
	if (_descriptionEdit->toPlainText().toStdString() != NormalizeLineEndings(_item.description)) {
		changes.append(tr("description"));
	}
	if (static_cast<ERemoteStoragePublishedFileVisibility>(_visibilityComboBox->currentData().toInt()) != _item.visibility) {
		changes.append(tr("visibility"));
	}
	if (!_selectedPreviewPath.isEmpty()) {
		changes.append(tr("preview"));
	}
	if (std::ranges::any_of(_additionalPreviews, [](const AdditionalPreviewEntry& entry) { return !entry.existing || entry.markedForRemoval; })) {
		changes.append(tr("media"));
	}
	if (TagsChanged()) {
		changes.append(tr("tags"));
	}
	return changes;
}

QWidget* CItemEditDialog::BuildPreviewSection() {
	auto* groupBox = new QGroupBox(tr("Preview image"), this);
	groupBox->setFixedWidth(PREVIEW_VIEW_WIDTH + 26);

	_previewLabel = new QLabel(groupBox);
	_previewLabel->setFixedSize(PREVIEW_VIEW_WIDTH, PREVIEW_VIEW_HEIGHT);
	_previewLabel->setAlignment(Qt::AlignCenter);
	_previewLabel->setStyleSheet("background-color: #262627; border: 1px solid #1a1a1a;");

	_previewInfoLabel = new QLabel(groupBox);
	_previewInfoLabel->setObjectName("hintLabel");
	_previewInfoLabel->setWordWrap(true);

	auto* browseButton = new QPushButton(QIcon(":/icons/folder.svg"), tr("Choose image…"), groupBox);

	_previewSizeComboBox = new QComboBox(groupBox);
	for (const PreviewImage::SizePreset& preset : PreviewImage::SizePresets()) {
		_previewSizeComboBox->addItem(preset.title);
	}
	_previewSizeComboBox->setCurrentIndex(2); // 1280 × 720 — fits in 1 MB almost always

	_fitPreviewButton = new QPushButton(tr("Fit to 1 MB (JPEG)"), groupBox);
	_fitPreviewButton->setEnabled(false);

	auto* fitLayout = new QHBoxLayout();
	fitLayout->addWidget(_previewSizeComboBox, 1);
	fitLayout->addWidget(_fitPreviewButton);

	auto* layout = new QVBoxLayout(groupBox);
	layout->setContentsMargins(12, 12, 12, 12);
	layout->setSpacing(8);
	layout->addWidget(_previewLabel, 0, Qt::AlignHCenter);
	layout->addWidget(_previewInfoLabel);
	layout->addWidget(browseButton);
	layout->addLayout(fitLayout);

	connect(browseButton, &QPushButton::clicked, this, &CItemEditDialog::OnBrowsePreviewClicked);
	connect(_fitPreviewButton, &QPushButton::clicked, this, &CItemEditDialog::OnFitPreviewClicked);
	return groupBox;
}

QWidget* CItemEditDialog::BuildAdditionalPreviewsSection() {
	auto* groupBox = new QGroupBox(tr("Additional previews"), this);

	_additionalPreviewsList = new QListWidget(groupBox);
	_additionalPreviewsList->setSelectionMode(QAbstractItemView::SingleSelection);
	_additionalPreviewsList->setIconSize(QSize(24, 24));
	_additionalPreviewsList->setSpacing(2);
	_additionalPreviewsHintLabel = new QLabel(groupBox);
	_additionalPreviewsHintLabel->setObjectName("hintLabel");
	_additionalPreviewsHintLabel->setWordWrap(true);

	auto* addImageButton = new QPushButton(QIcon(":/icons/new.svg"), tr("Add image…"), groupBox);
	auto* addVideoButton = new QPushButton(QIcon(":/icons/new.svg"), tr("Add YouTube…"), groupBox);
	_removePreviewButton = new QPushButton(QIcon(":/icons/delete.svg"), tr("Remove"), groupBox);
	_removePreviewButton->setEnabled(false);

	auto* buttonsLayout = new QHBoxLayout();
	buttonsLayout->addWidget(addImageButton);
	buttonsLayout->addWidget(addVideoButton);
	buttonsLayout->addStretch();
	buttonsLayout->addWidget(_removePreviewButton);

	auto* layout = new QVBoxLayout(groupBox);
	layout->setContentsMargins(12, 12, 12, 12);
	layout->setSpacing(8);
	layout->addWidget(_additionalPreviewsHintLabel);
	layout->addWidget(_additionalPreviewsList, 1);
	layout->addLayout(buttonsLayout);

	connect(addImageButton, &QPushButton::clicked, this, &CItemEditDialog::OnAddPreviewImageClicked);
	connect(addVideoButton, &QPushButton::clicked, this, &CItemEditDialog::OnAddPreviewVideoClicked);
	connect(_removePreviewButton, &QPushButton::clicked, this, &CItemEditDialog::OnRemovePreviewClicked);
	connect(_additionalPreviewsList, &QListWidget::currentRowChanged, this, [this](int row) {
		const bool selected = row >= 0 && static_cast<size_t>(row) < _additionalPreviews.size();
		_removePreviewButton->setEnabled(selected);
		_removePreviewButton->setText(selected && _additionalPreviews[static_cast<size_t>(row)].markedForRemoval ? tr("Undo removal") : tr("Remove"));
	});
	return groupBox;
}

QWidget* CItemEditDialog::BuildMetadataSection() {
	auto* groupBox = new QGroupBox(tr("Details"), this);

	_titleEdit = new QLineEdit(QString::fromStdString(_item.title), groupBox);
	_titleEdit->setMaxLength(static_cast<int>(WORKSHOP_TITLE_MAX_LENGTH));
	_titleCounterLabel = new QLabel(groupBox);
	_titleCounterLabel->setObjectName("hintLabel");

	_visibilityComboBox = new QComboBox(groupBox);
	for (const ERemoteStoragePublishedFileVisibility visibility : {k_ERemoteStoragePublishedFileVisibilityPublic, k_ERemoteStoragePublishedFileVisibilityFriendsOnly, k_ERemoteStoragePublishedFileVisibilityPrivate, k_ERemoteStoragePublishedFileVisibilityUnlisted}) {
		_visibilityComboBox->addItem(FromStringView(SteamNames::Visibility(visibility)), static_cast<int>(visibility));
	}
	_visibilityComboBox->setCurrentIndex(_visibilityComboBox->findData(static_cast<int>(_isNewSubmission ? k_ERemoteStoragePublishedFileVisibilityPrivate : _item.visibility)));

	_descriptionEdit = new QPlainTextEdit(QString::fromStdString(_item.description), groupBox);
	_descriptionEdit->setPlaceholderText(tr("Description. Steam BBCode is supported: [h1], [b], [url=...], [img], [list]…"));
	_descriptionEdit->setMinimumHeight(180);
	_descriptionCounterLabel = new QLabel(groupBox);
	_descriptionCounterLabel->setObjectName("hintLabel");

	auto* layout = new QGridLayout(groupBox);
	layout->setContentsMargins(12, 12, 12, 12);
	layout->setHorizontalSpacing(8);
	layout->setVerticalSpacing(8);
	layout->setColumnMinimumWidth(0, 83);
	layout->setColumnStretch(1, 1);
	const QString languageName = FromStringView(SteamNames::LanguageDisplayName(AppConfig().Data().workshopLanguage));
	layout->addWidget(new QLabel(tr("Title (%1)").arg(languageName), groupBox), 0, 0);
	layout->addWidget(_titleEdit, 0, 1);
	layout->addWidget(_titleCounterLabel, 1, 1, Qt::AlignRight);
	layout->addWidget(new QLabel(tr("Visibility"), groupBox), 2, 0);
	layout->addWidget(_visibilityComboBox, 2, 1);
	layout->addWidget(new QLabel(tr("Description (%1)").arg(languageName), groupBox), 3, 0, Qt::AlignTop);
	layout->addWidget(_descriptionEdit, 3, 1);
	layout->setRowStretch(3, 1);
	layout->addWidget(_descriptionCounterLabel, 4, 1, Qt::AlignRight);

	connect(_titleEdit, &QLineEdit::textChanged, this, &CItemEditDialog::OnTitleChanged);
	connect(_descriptionEdit, &QPlainTextEdit::textChanged, this, &CItemEditDialog::OnDescriptionChanged);
	return groupBox;
}

QWidget* CItemEditDialog::BuildTagsAndModesSection() {
	auto* groupBox = new QGroupBox(FromStringView(_gameProfile.tagsSectionTitle), this);
	groupBox->setObjectName("editorTags");
	auto* layout = new QVBoxLayout(groupBox);
	layout->setContentsMargins(12, 12, 12, 12);
	layout->setSpacing(8);

	// Required tags are visible and pre-checked — the same way cs2_workshop_manager sets them.
	// They can be unchecked, but without them the map is hidden from the in-game browser, hence the warning.
	const std::vector<std::string> requiredTags = RequiredTags::Resolve(_gameProfile);
	if (!requiredTags.empty()) {
		auto* requiredLabel = new QLabel(tr("Required by the game"), groupBox);
		layout->addWidget(requiredLabel);

		auto* requiredLayout = new QHBoxLayout();
		for (const std::string& tag : requiredTags) {
			auto* checkBox = new QCheckBox(QString::fromStdString(tag), groupBox);
			checkBox->setProperty(TAG_PROPERTY, QString::fromStdString(tag));
			checkBox->setChecked(true);
			checkBox->setToolTip(tr("Set automatically by the game's own workshop manager. Without it the item is hidden from the in-game browser."));
			requiredLayout->addWidget(checkBox);
			_requiredTagCheckBoxes.push_back(checkBox);
			connect(checkBox, &QCheckBox::toggled, this, &CItemEditDialog::OnRequiredTagToggled);
		}
		requiredLayout->addStretch();
		layout->addLayout(requiredLayout);

		_requiredTagWarningLabel = new QLabel(groupBox);
		_requiredTagWarningLabel->setStyleSheet("color: #ff5a5a;");
		_requiredTagWarningLabel->setWordWrap(true);
		_requiredTagWarningLabel->setVisible(false);
		layout->addWidget(_requiredTagWarningLabel);
	}

	for (const WorkshopTagGroup& tagGroup : _gameProfile.tagGroups) {
		auto* groupLabel = new QLabel(FromStringView(tagGroup.title), groupBox);
		layout->addWidget(groupLabel);

		auto* gridLayout = new QGridLayout();
		int tagIndex = 0;
		for (std::string_view tag : tagGroup.tags) {
			// In a button text & is a mnemonic, so it is escaped and the tag itself is stored separately.
			auto* checkBox = new QCheckBox(FromStringView(tag).replace('&', QStringLiteral("&&")), groupBox);
			checkBox->setProperty(TAG_PROPERTY, FromStringView(tag));
			checkBox->setChecked(RequiredTags::ContainsTag(_item.tags, tag));
			gridLayout->addWidget(checkBox, tagIndex / TAG_CHECKBOX_COLUMNS, tagIndex % TAG_CHECKBOX_COLUMNS);
			_groupTagCheckBoxes.push_back(checkBox);
			++tagIndex;
		}
		layout->addLayout(gridLayout);
	}

	QStringList customTags;
	for (const std::string& tag : _item.tags) {
		bool known = RequiredTags::ContainsTag(requiredTags, tag);
		for (const WorkshopTagGroup& tagGroup : _gameProfile.tagGroups) {
			known = known || RequiredTags::ContainsTag({tagGroup.tags.begin(), tagGroup.tags.end()}, tag);
		}
		if (!known) {
			customTags.append(QString::fromStdString(tag));
		}
	}

	_customTagsEdit = new QLineEdit(customTags.join(QStringLiteral(", ")), groupBox);
	_customTagsEdit->setPlaceholderText(tr("Custom tags, comma separated (e.g. Contest, Guide)"));

	auto* customLayout = new QFormLayout();
	customLayout->addRow(tr("Custom"), _customTagsEdit);
	layout->addLayout(customLayout);

	return groupBox;
}

void CItemEditDialog::OnRequiredTagToggled() {
	QStringList missing;
	for (const QCheckBox* checkBox : _requiredTagCheckBoxes) {
		if (!checkBox->isChecked()) {
			missing.append(checkBox->property(TAG_PROPERTY).toString());
		}
	}
	_requiredTagWarningLabel->setVisible(!missing.isEmpty());
	if (!missing.isEmpty()) {
		_requiredTagWarningLabel->setText(tr("Without %1 the item will not show up in the in-game map browser and the game's workshop manager will flag it.").arg(missing.join(QStringLiteral(", "))));
	}
}

void CItemEditDialog::ShowCurrentPreview() {
	if (_isNewSubmission) {
		_previewLabel->setText(tr("No preview yet — choose an image (required)"));
		_previewInfoLabel->setText(tr("JPG, PNG or animated GIF, under 1 MB"));
		return;
	}
	_previewInfoLabel->setText(tr("Current preview: %1").arg(QLocale().formattedDataSize(_item.previewFileSize)));

	// The loader caches: if the preview is already downloaded, the signal fires synchronously from Request.
	if (!_previewImageLoader) {
		return;
	}
	connect(_previewImageLoader, &CPreviewImageLoader::PreviewLoaded, this, &CItemEditDialog::OnCurrentPreviewLoaded);
	_previewLabel->setText(tr("Loading preview…"));
	_previewImageLoader->Request(_item.publishedFileId, QUrl(QString::fromStdString(_item.previewUrl)));
}

void CItemEditDialog::OnCurrentPreviewLoaded(PublishedFileId_t publishedFileId, const QPixmap& pixmap) {
	if (publishedFileId != _item.publishedFileId || !_selectedPreviewPath.isEmpty()) {
		return;
	}
	_previewLabel->setPixmap(pixmap.scaled(_previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

	const QByteArray* rawImageData = _previewImageLoader->GetRawImageData(publishedFileId);
	const bool animated = rawImageData && rawImageData->startsWith("GIF8");
	_previewInfoLabel->setText(tr("Current preview: %1 × %2, %3%4").arg(pixmap.width()).arg(pixmap.height()).arg(QLocale().formattedDataSize(_item.previewFileSize)).arg(animated ? tr(", animated GIF") : QString()));
}

void CItemEditDialog::ShowPreviewFile(const QString& filePath) {
	_selectedPreviewPath = filePath;

	if (_previewMovie) {
		_previewMovie->stop();
		_previewMovie->deleteLater();
		_previewMovie = nullptr;
	}

	if (PreviewImage::IsAnimatedGif(filePath)) {
		// QMovie plays the animation as is — exactly how the GIF will look on the workshop.
		_previewMovie = new QMovie(filePath, QByteArray(), this);
		_previewMovie->setScaledSize(QImageReader(filePath).size().scaled(_previewLabel->size(), Qt::KeepAspectRatio));
		_previewLabel->setMovie(_previewMovie);
		_previewMovie->start();
	} else {
		const QPixmap pixmap(filePath);
		_previewLabel->setPixmap(pixmap.scaled(_previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
	}

	UpdatePreviewInfo();
}

void CItemEditDialog::UpdatePreviewInfo() {
	const QFileInfo fileInfo(_selectedPreviewPath);
	const QImageReader reader(_selectedPreviewPath);
	const QSize imageSize = reader.size();
	const bool animated = PreviewImage::IsAnimatedGif(_selectedPreviewPath);
	const bool fits = fileInfo.size() < WORKSHOP_PREVIEW_MAX_FILE_SIZE;

	QString info = tr("%1 — %2 × %3, %4%5").arg(fileInfo.fileName()).arg(imageSize.width()).arg(imageSize.height()).arg(QLocale().formattedDataSize(fileInfo.size())).arg(animated ? tr(", animated GIF") : QString());
	if (!fits) {
		info += animated ? tr("\nToo large for Steam (limit 1 MB). Shrink the GIF externally — re-encoding would drop the animation.") : tr("\nToo large for Steam (limit 1 MB). Use \"Fit to 1 MB\".");
	}
	_previewInfoLabel->setText(info);
	_previewInfoLabel->setStyleSheet(fits ? QString() : QStringLiteral("color: #ff5a5a;"));
	_fitPreviewButton->setEnabled(!animated);
	UpdateSummary();
}

QString CItemEditDialog::BrowseStartDirectory() const {
	if (!_lastBrowseDirectory.isEmpty()) {
		return _lastBrowseDirectory;
	}
	if (_contentWidget) {
		if (const AddonInfo* addon = _contentWidget->GetSelectedAddon()) {
			return QString::fromStdWString(addon->directory.wstring());
		}
	}
	if (_bspContentWidget && !_bspContentWidget->GetBspPath().empty()) {
		return QString::fromStdWString(_bspContentWidget->GetBspPath().parent_path().wstring());
	}
	const ResolvedGamePaths paths = GamePaths().Resolve(_gameProfile);
	return paths.game.valid ? QString::fromStdWString(paths.game.path.wstring()) : QString();
}

void CItemEditDialog::RememberBrowseDirectory(const QString& filePath) {
	_lastBrowseDirectory = QFileInfo(filePath).absolutePath();
}

void CItemEditDialog::OnBrowsePreviewClicked() {
	const QString filePath = QFileDialog::getOpenFileName(this, tr("Select preview image"), BrowseStartDirectory(), PreviewImage::FileFilter());
	if (filePath.isEmpty()) {
		return;
	}
	RememberBrowseDirectory(filePath);
	ShowPreviewFile(filePath);
}

void CItemEditDialog::OnFitPreviewClicked() {
	if (_selectedPreviewPath.isEmpty()) {
		return;
	}
	const PreviewImage::SizePreset& preset = PreviewImage::SizePresets()[static_cast<size_t>(_previewSizeComboBox->currentIndex())];

	QString errorMessage;
	const std::optional<QString> fittedPath = PreviewImage::FitToLimit(_selectedPreviewPath, preset.size, WORKSHOP_PREVIEW_MAX_FILE_SIZE, errorMessage);
	if (!fittedPath.has_value()) {
		QMessageBox::warning(this, tr("Fit preview"), errorMessage);
		return;
	}
	ShowPreviewFile(*fittedPath);
}

void CItemEditDialog::OnAddPreviewImageClicked() {
	const QStringList filePaths = QFileDialog::getOpenFileNames(this, tr("Select additional preview images"), BrowseStartDirectory(), PreviewImage::FileFilter());
	if (!filePaths.isEmpty()) {
		RememberBrowseDirectory(filePaths.first());
	}
	for (const QString& filePath : filePaths) {
		if (QFileInfo(filePath).size() >= WORKSHOP_PREVIEW_MAX_FILE_SIZE) {
			QMessageBox::warning(this, tr("Additional preview"), tr("%1 is larger than 1 MB and was skipped.").arg(QFileInfo(filePath).fileName()));
			continue;
		}
		AdditionalPreviewEntry entry;
		entry.addition.type = k_EItemPreviewType_Image;
		entry.addition.pathOrVideoId = filePath.toStdString();
		_additionalPreviews.push_back(entry);
	}
	RefreshAdditionalPreviewsList();
}

void CItemEditDialog::OnAddPreviewVideoClicked() {
	bool accepted = false;
	const QString input = QInputDialog::getText(this, tr("Add YouTube video"), tr("YouTube video URL or ID:"), QLineEdit::Normal, QString(), &accepted);
	if (!accepted) {
		return;
	}
	const QString videoId = ExtractYouTubeVideoId(input);
	if (videoId.isEmpty()) {
		QMessageBox::warning(this, tr("Add YouTube video"), tr("Could not recognise a YouTube video id in \"%1\".").arg(input));
		return;
	}
	AdditionalPreviewEntry entry;
	entry.addition.type = k_EItemPreviewType_YouTubeVideo;
	entry.addition.pathOrVideoId = videoId.toStdString();
	_additionalPreviews.push_back(entry);
	RefreshAdditionalPreviewsList();
}

void CItemEditDialog::OnRemovePreviewClicked() {
	const int row = _additionalPreviewsList->currentRow();
	if (row < 0 || static_cast<size_t>(row) >= _additionalPreviews.size()) {
		return;
	}
	AdditionalPreviewEntry& entry = _additionalPreviews[static_cast<size_t>(row)];
	if (entry.existing) {
		entry.markedForRemoval = !entry.markedForRemoval;
	} else {
		_additionalPreviews.erase(_additionalPreviews.begin() + row);
	}
	RefreshAdditionalPreviewsList();
}

void CItemEditDialog::RefreshAdditionalPreviewsList() {
	const int selectedRow = _additionalPreviewsList->currentRow();
	_additionalPreviewsList->clear();
	_additionalPreviewsHintLabel->setText(_additionalPreviews.empty() ? tr("No additional images or videos yet. Add screenshots or a YouTube video to show more of your map. The main preview is on the Text and preview tab.") : tr("Additional media for the Workshop page. Removals take effect when you submit; select an item to undo its removal."));
	for (const AdditionalPreviewEntry& entry : _additionalPreviews) {
		const QString kind = FromStringView(SteamNames::PreviewType(entry.addition.type));
		const QString source = entry.existing ? QString::fromStdString(entry.addition.pathOrVideoId) : QFileInfo(QString::fromStdString(entry.addition.pathOrVideoId)).fileName();
		QString text = QStringLiteral("%1: %2").arg(kind, source);
		if (entry.markedForRemoval) {
			text = tr("[remove] ") + text;
		} else if (!entry.existing) {
			text = tr("[new] ") + text;
		}
		auto* listItem = new QListWidgetItem(text, _additionalPreviewsList);
		listItem->setIcon(QIcon(entry.addition.type == k_EItemPreviewType_Image ? ":/icons/view.svg" : ":/icons/game.svg"));
		listItem->setToolTip(text);
		if (entry.markedForRemoval) {
			listItem->setForeground(QColor(0xff, 0x7a, 0x7a));
		} else if (!entry.existing) {
			listItem->setForeground(QColor(0x66, 0xc0, 0xf4));
		}
	}
	if (!_additionalPreviews.empty()) {
		_additionalPreviewsList->setCurrentRow(std::clamp(selectedRow, 0, static_cast<int>(_additionalPreviews.size()) - 1));
	}
	UpdateSummary();
}

void CItemEditDialog::OnTitleChanged() {
	_titleCounterLabel->setText(QStringLiteral("%1 / %2").arg(_titleEdit->text().size()).arg(WORKSHOP_TITLE_MAX_LENGTH));
	UpdateSummary();
}

void CItemEditDialog::OnDescriptionChanged() {
	const qsizetype length = _descriptionEdit->toPlainText().toUtf8().size();
	_descriptionCounterLabel->setText(QStringLiteral("%1 / %2 bytes").arg(length).arg(WORKSHOP_DESCRIPTION_MAX_LENGTH));
	_descriptionCounterLabel->setStyleSheet(static_cast<size_t>(length) > WORKSHOP_DESCRIPTION_MAX_LENGTH ? QStringLiteral("color: #ff5a5a;") : QString());
	UpdateSummary();
}

std::vector<std::string> CItemEditDialog::CollectTags() const {
	std::vector<std::string> tags;
	auto pushUnique = [&tags](const QString& tag) {
		const QString trimmed = tag.trimmed();
		if (trimmed.isEmpty()) {
			return;
		}
		for (const std::string& existing : tags) {
			if (QString::fromStdString(existing).compare(trimmed, Qt::CaseInsensitive) == 0) {
				return;
			}
		}
		tags.push_back(trimmed.toStdString());
	};

	for (const QCheckBox* checkBox : _requiredTagCheckBoxes) {
		if (checkBox->isChecked()) {
			pushUnique(checkBox->property(TAG_PROPERTY).toString());
		}
	}
	for (const QCheckBox* checkBox : _groupTagCheckBoxes) {
		if (checkBox->isChecked()) {
			pushUnique(checkBox->property(TAG_PROPERTY).toString());
		}
	}
	for (const QString& customTag : _customTagsEdit->text().split(',', Qt::SkipEmptyParts)) {
		pushUnique(customTag);
	}
	return tags;
}

void CItemEditDialog::OnSubmitClicked() {
	if (_busy) {
		return;
	}

	WorkshopUpdateRequest request;
	request.publishedFileId = _item.publishedFileId;

	const std::string newTitle = _titleEdit->text().trimmed().toStdString();
	if (newTitle.empty()) {
		QMessageBox::warning(this, tr("Edit item"), tr("Title cannot be empty."));
		return;
	}
	if (_isNewSubmission || newTitle != _item.title) {
		request.title = newTitle;
	}

	const std::string newDescription = _descriptionEdit->toPlainText().toStdString();
	if (_descriptionEdit->toPlainText().trimmed().isEmpty()) {
		// Same rule as cs2_workshop_manager: a Workshop page without a description is not accepted.
		_tabs->setCurrentIndex(_pageTabIndex);
		_descriptionEdit->setFocus();
		QMessageBox::warning(this, tr("Edit item"), tr("Write a description first — the Workshop page text cannot be empty."));
		return;
	}
	if (newDescription.size() > WORKSHOP_DESCRIPTION_MAX_LENGTH) {
		QMessageBox::warning(this, tr("Edit item"), tr("Description is longer than %1 bytes.").arg(WORKSHOP_DESCRIPTION_MAX_LENGTH));
		return;
	}
	if (_isNewSubmission || newDescription != NormalizeLineEndings(_item.description)) {
		request.description = newDescription;
	}

	const auto newVisibility = static_cast<ERemoteStoragePublishedFileVisibility>(_visibilityComboBox->currentData().toInt());
	if (_isNewSubmission || newVisibility != _item.visibility) {
		request.visibility = newVisibility;
	}

	// SetItemTags replaces the whole list, so it is sent only when the set actually differs;
	// otherwise every Submit would look like a tag change in the item's history.
	if (TagsChanged()) {
		request.tags = CollectTags();
	}

	if (!_selectedPreviewPath.isEmpty()) {
		request.previewFilePath = _selectedPreviewPath.toStdString();
	}
	for (const AdditionalPreviewEntry& entry : _additionalPreviews) {
		if (entry.existing && entry.markedForRemoval) {
			request.removePreviewIndices.push_back(entry.existingIndex);
		} else if (!entry.existing) {
			request.addPreviews.push_back(entry.addition);
		}
	}

	request.changeNote = _changeNoteEdit->toPlainText().trimmed().toStdString();
	if (request.changeNote.size() > WORKSHOP_CHANGE_NOTE_MAX_LENGTH) {
		QMessageBox::warning(this, tr("Edit item"), tr("Change note is longer than %1 bytes.").arg(WORKSHOP_CHANGE_NOTE_MAX_LENGTH));
		return;
	}
	if (request.changeNote.empty() && IsChangeNoteRequired()) {
		_tabs->setCurrentIndex(_changeNoteTabIndex);
		_changeNoteEdit->setFocus();
		QMessageBox::information(this, tr("Update"), tr("Write a change note first: what is new in this version of the map. Subscribers see it in the item's change history."));
		return;
	}

	if (_isNewSubmission) {
		SubmitNewItem(std::move(request));
		return;
	}
	if (IsReplacingContent()) {
		SubmitReupload(std::move(request));
		return;
	}
	if (!HasMetadataChanges(request)) {
		// Like cs2_workshop_manager: an update that changes nothing is refused, change note or not.
		QMessageBox::information(this, tr("Edit item"), request.changeNote.empty() ? tr("Nothing has changed. Edit the text, tags or media, or tick \"Replace the map content\".") : tr("Only the change note is filled in. A change note needs a change to go with it — edit the text, tags or media, or replace the map content."));
		return;
	}

	QString errorMessage;
	if (!_workshopService->SubmitUpdate(request, errorMessage)) {
		QMessageBox::warning(this, tr("Edit item"), errorMessage);
		return;
	}

	SetBusy(true);
	_progressTimer.start();
}

bool CItemEditDialog::FillContentJob(PublishJob& job, QString& errorMessage) {
	job.profile = &_gameProfile;
	job.displayTitle = _titleEdit->text().trimmed().toStdString();

	if (_bspContentWidget) {
		if (!_bspContentWidget->IsReadyToUpload(errorMessage)) {
			return false;
		}
		job.bspPath = _bspContentWidget->GetBspPath();
		return true;
	}

	_contentWidget->RefreshManifest();
	if (!_contentWidget->GetManifest().errorMessage.empty()) {
		errorMessage = tr("Cannot prepare the addon files:\n\n%1").arg(QString::fromStdString(_contentWidget->GetManifest().errorMessage));
		return false;
	}
	const AddonInfo* addon = _contentWidget->GetSelectedAddon();
	if (!addon || !_contentWidget->HasFilesToPack()) {
		errorMessage = tr("Select an addon folder that has files to pack.");
		return false;
	}
	if (_contentWidget->ExceedsSizeLimit()) {
		errorMessage = tr("The content is %1 — the Workshop limit is %2. Exclude folders in Upload paths or trim the addon.").arg(QLocale().formattedDataSize(static_cast<qint64>(_contentWidget->GetManifest().totalSize)), QLocale().formattedDataSize(static_cast<qint64>(WORKSHOP_CONTENT_MAX_SIZE)));
		return false;
	}
	job.addon = *addon;
	job.manifest = _contentWidget->GetManifest();
	job.publishedRoot = _publishContext.publishedRoot;
	return true;
}

void CItemEditDialog::SubmitReupload(WorkshopUpdateRequest request) {
	PublishJob job;
	job.kind = PublishJob::PUBLISH_REUPLOAD;
	QString errorMessage;
	if (!FillContentJob(job, errorMessage)) {
		QMessageBox::warning(this, tr("Update"), errorMessage);
		return;
	}

	if (job.addon.publishedFileId.has_value() && *job.addon.publishedFileId != _item.publishedFileId) {
		const QMessageBox::StandardButton answer = QMessageBox::question(this, tr("Update"), tr("Addon \"%1\" was published as a different item (%2). Upload it over \"%3\" (%4) anyway?").arg(QString::fromStdString(job.addon.name)).arg(*job.addon.publishedFileId).arg(QString::fromStdString(_item.title)).arg(_item.publishedFileId), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if (answer != QMessageBox::Yes) {
			return;
		}
	}

	job.request = std::move(request);
	if (!_publishPipeline->Start(std::move(job), errorMessage)) {
		QMessageBox::warning(this, tr("Update"), errorMessage);
		return;
	}
	SetBusy(true);
}

void CItemEditDialog::SubmitNewItem(WorkshopUpdateRequest request) {
	PublishJob job;
	job.kind = PublishJob::PUBLISH_NEW;
	QString errorMessage;
	if (!FillContentJob(job, errorMessage)) {
		QMessageBox::warning(this, tr("New Submission"), errorMessage);
		return;
	}

	// Without a preview the item looks broken both on the workshop and in the in-game browser; Valve requires one too.
	if (!request.previewFilePath.has_value()) {
		QMessageBox::warning(this, tr("New Submission"), tr("Choose a preview image — Steam Workshop items need one."));
		return;
	}
	job.request = std::move(request);

	if (!_publishPipeline->Start(std::move(job), errorMessage)) {
		QMessageBox::warning(this, tr("New Submission"), errorMessage);
		return;
	}
	SetBusy(true);
}

void CItemEditDialog::OnMapInspected(const BspMapInfo& info) {
	if (_isNewSubmission) {
		const QString suggestedTitle = QString::fromStdWString(_bspContentWidget->GetBspPath().stem().wstring());
		if (_titleEdit->text().isEmpty() || _titleEdit->text() == _defaultTitle) {
			_titleEdit->setText(suggestedTitle);
		}
		_defaultTitle = suggestedTitle;
		ApplySuggestedTags(info.suggestedTags);
	}
	UpdateSummary();
}

void CItemEditDialog::ApplySuggestedTags(const std::vector<std::string>& tags) {
	QStringList customTags;
	for (const std::string& tag : tags) {
		bool known = false;
		for (QCheckBox* checkBox : _groupTagCheckBoxes) {
			if (checkBox->property(TAG_PROPERTY).toString().compare(QString::fromStdString(tag), Qt::CaseInsensitive) == 0) {
				checkBox->setChecked(true);
				known = true;
			}
		}
		if (!known) {
			customTags.append(QString::fromStdString(tag));
		}
	}
	for (QCheckBox* checkBox : _groupTagCheckBoxes) {
		const QString tag = checkBox->property(TAG_PROPERTY).toString();
		const bool suggested = std::ranges::any_of(tags, [&tag](const std::string& candidate) { return QString::fromStdString(candidate).compare(tag, Qt::CaseInsensitive) == 0; });
		if (!suggested) {
			checkBox->setChecked(false);
		}
	}
	if (!customTags.isEmpty()) {
		_customTagsEdit->setText(customTags.join(QStringLiteral(", ")));
	}
}

void CItemEditDialog::OnAddonSelectionChanged() {
	UpdateSummary();
	if (!_contentWidget || !_isNewSubmission) {
		return;
	}
	const AddonInfo* addon = _contentWidget->GetSelectedAddon();
	if (!addon) {
		return;
	}
	const QString suggestedTitle = addon->publishedTitle.has_value() ? QString::fromStdString(*addon->publishedTitle) : QString::fromStdString(addon->name);
	if (_titleEdit->text().isEmpty() || _titleEdit->text() == _defaultTitle) {
		_titleEdit->setText(suggestedTitle);
	}
	_defaultTitle = suggestedTitle;
}

void CItemEditDialog::OnPipelineStageChanged(const QString& stageText) {
	if (!_busy) {
		return;
	}
	_statusLabel->setText(stageText);
	_progressBar->setValue(0);
}

void CItemEditDialog::OnPipelinePackProgress(int percent, const QString& detail) {
	if (!_busy) {
		return;
	}
	_progressBar->setValue(percent);
	_statusLabel->setText(tr("Packing: %1").arg(detail));
}

void CItemEditDialog::OnPipelineUploadStarted() {
	if (!_busy) {
		return;
	}
	_progressBar->setValue(0);
	_progressTimer.start();
}

void CItemEditDialog::OnPipelineFinished(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement) {
	if (!_busy) {
		return;
	}
	_progressTimer.stop();
	SetBusy(false);
	_publishedFileId = publishedFileId;

	if (userNeedsToAcceptLegalAgreement) {
		QMessageBox::information(this, tr("Workshop"), tr("Item %1 is on Steam, but Steam requires you to accept the Workshop legal agreement before it becomes visible. The item page will open now.").arg(publishedFileId));
		QDesktopServices::openUrl(QUrl(QString::fromLatin1(LEGAL_AGREEMENT_URL).arg(publishedFileId)));
	}
	accept();
}

void CItemEditDialog::OnPipelineFailed(const QString& errorMessage) {
	if (!_busy) {
		return;
	}
	_progressTimer.stop();
	SetBusy(false);
	QMessageBox::warning(this, _isNewSubmission ? tr("New Submission") : tr("Update"), errorMessage);
}

// Escape, X and Cancel land here. Mid-upload it asks first; CPublishPipeline::Cancel says what can be stopped.
void CItemEditDialog::reject() {
	if (!_busy) {
		QDialog::reject();
		return;
	}
	const bool pipelineRunning = _publishPipeline->IsRunning();
	const QString question = pipelineRunning ? tr("Cancel the upload?%1%1Files not yet sent are discarded. If Steam has already accepted the update, it may still be applied.%1A new item created for this upload is deleted.").arg(QStringLiteral("\n")) : tr("Steam is already applying the changes and cannot be stopped.%1%1Close the window anyway?").arg(QStringLiteral("\n"));
	if (QMessageBox::question(this, tr("Cancel"), question, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
		return;
	}
	_progressTimer.stop();
	if (pipelineRunning) {
		_publishPipeline->Cancel();
	}
	_busy = false;
	QDialog::reject();
}

void CItemEditDialog::OnUpdateProgressTick() {
	const WorkshopUpdateProgress progress = _workshopService->GetUpdateProgress();
	_statusLabel->setText(FromStringView(SteamNames::ItemUpdateStatus(progress.status)));
	if (progress.bytesTotal > 0) {
		_progressBar->setValue(static_cast<int>(progress.bytesProcessed * 100 / progress.bytesTotal));
	}
}

void CItemEditDialog::OnUpdateSubmitted(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement) {
	// When content is replaced, the pipeline drives the same SubmitUpdate and has already closed the dialog via Finished.
	if (!_busy || publishedFileId != _item.publishedFileId || IsReplacingContent()) {
		return;
	}
	_progressTimer.stop();
	SetBusy(false);

	if (userNeedsToAcceptLegalAgreement) {
		// Until the agreement is accepted Steam keeps the item hidden — take the user to the page.
		QMessageBox::information(this, tr("Edit item"), tr("Changes submitted, but Steam requires you to accept the Workshop legal agreement before the item becomes visible. The item page will open now."));
		QDesktopServices::openUrl(QUrl(QString::fromLatin1(LEGAL_AGREEMENT_URL).arg(publishedFileId)));
	}
	accept();
}

void CItemEditDialog::OnUpdateFailed(PublishedFileId_t publishedFileId, const QString& errorMessage) {
	if (!_busy || publishedFileId != _item.publishedFileId || IsReplacingContent()) {
		return;
	}
	_progressTimer.stop();
	SetBusy(false);
	QMessageBox::warning(this, tr("Edit item"), tr("Update failed.\n\n%1").arg(errorMessage));
}

void CItemEditDialog::SetBusy(bool busy) {
	_busy = busy;
	_submitButton->setEnabled(!busy);
	_cancelButton->setText(busy ? tr("Cancel upload") : tr("Cancel"));
	_progressBar->setVisible(busy);
	_progressBar->setValue(0);
	_statusLabel->setText(busy ? tr("Submitting…") : QString());
	if (_contentWidget) {
		_contentWidget->setEnabled(!busy);
	}
	if (_bspContentWidget) {
		_bspContentWidget->setEnabled(!busy);
	}
	for (QWidget* child : findChildren<QWidget*>()) {
		if (child != _progressBar && child != _statusLabel && child != _cancelButton && child != _submitButton) {
			child->setEnabled(!busy);
		}
	}
	if (!busy) {
		_removePreviewButton->setEnabled(_additionalPreviewsList->currentRow() >= 0);
		_fitPreviewButton->setEnabled(!_selectedPreviewPath.isEmpty() && !PreviewImage::IsAnimatedGif(_selectedPreviewPath));
	}
}
