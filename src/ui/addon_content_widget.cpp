#include "ui/addon_content_widget.h"

#include "core/game_paths.h"
#include "core/upload_rules.h"
#include "steam/workshop_update.h"
#include "ui/upload_paths_page.h"

#include <QtCore/QLocale>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

namespace {

	void AddFileToTree(QTreeWidget* tree, const std::filesystem::path& relativePath, std::optional<uint64_t> size) {
		const QString path = QString::fromStdWString(relativePath.generic_wstring());
		const QStringList parts = path.split('/', Qt::SkipEmptyParts);
		QTreeWidgetItem* parent = tree->invisibleRootItem();
		for (qsizetype index = 0; index < parts.size(); ++index) {
			QTreeWidgetItem* item = nullptr;
			for (int childIndex = 0; childIndex < parent->childCount(); ++childIndex) {
				if (parent->child(childIndex)->text(0) == parts[index]) {
					item = parent->child(childIndex);
					break;
				}
			}
			if (!item) {
				item = new QTreeWidgetItem(parent, {parts[index]});
				if (index + 1 < parts.size()) {
					item->setIcon(0, QIcon(":/icons/folder.svg"));
				}
			}
			item->setToolTip(0, parts.mid(0, index + 1).join('/'));
			if (size.has_value()) {
				const uint64_t total = item->data(1, Qt::UserRole).toULongLong() + *size;
				item->setData(1, Qt::UserRole, QVariant::fromValue<qulonglong>(total));
				item->setText(1, QLocale().formattedDataSize(static_cast<qint64>(total)));
				item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
			}
			parent = item;
		}
	}

} // namespace

CAddonContentWidget::CAddonContentWidget(const GameProfile& gameProfile, std::vector<AddonInfo> addons, AddonVpkRules rules, QWidget* parent) :
	QWidget(parent),
	_addons(std::move(addons)),
	_rules(std::move(rules)),
	_gameProfile(gameProfile) {
	auto* groupBox = new QGroupBox(this);
	groupBox->setObjectName("uploadContent");

	_addonComboBox = new QComboBox(groupBox);
	_addonComboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	_addonComboBox->setMinimumContentsLength(24);
	for (const AddonInfo& addon : _addons) {
		_addonComboBox->addItem(QString::fromStdString(addon.name));
		_addonComboBox->setItemData(_addonComboBox->count() - 1, QString::fromStdWString(addon.directory.wstring()), Qt::ToolTipRole);
	}

	_manifestLabel = new QLabel(groupBox);
	_manifestLabel->setObjectName("hintLabel");
	_manifestLabel->setWordWrap(true);
	_manifestLabel->setTextFormat(Qt::PlainText);
	_rulesLabel = new QLabel(groupBox);
	_rulesLabel->setObjectName("hintLabel");
	_rulesLabel->setTextFormat(Qt::PlainText);

	_publishedWarningLabel = new QLabel(groupBox);
	_publishedWarningLabel->setWordWrap(true);
	_publishedWarningLabel->setTextFormat(Qt::PlainText);
	_publishedWarningLabel->setStyleSheet("color: #e0b050;");
	_publishedWarningLabel->setVisible(false);

	_showFilesButton = new QPushButton(QIcon(":/icons/view.svg"), tr("Review files…"), groupBox);
	auto* uploadPathsButton = new QPushButton(QIcon(":/icons/uploadpaths.svg"), tr("Upload paths…"), groupBox);

	auto* buttonsLayout = new QHBoxLayout();
	buttonsLayout->setSpacing(8);
	buttonsLayout->addWidget(_showFilesButton, 1);
	buttonsLayout->addWidget(uploadPathsButton, 1);
	auto* buttons = new QWidget(groupBox);
	buttons->setLayout(buttonsLayout);
	buttonsLayout->setContentsMargins(16, 0, 0, 0);
	buttons->setFixedWidth(400);

	auto* groupLayout = new QGridLayout(groupBox);
	groupLayout->setContentsMargins(0, 0, 0, 0);
	groupLayout->setHorizontalSpacing(8);
	groupLayout->setVerticalSpacing(6);
	groupLayout->setColumnMinimumWidth(0, 96);
	groupLayout->setColumnStretch(1, 1);
	groupLayout->addWidget(new QLabel(tr("Addon folder"), groupBox), 0, 0);
	groupLayout->addWidget(_addonComboBox, 0, 1);
	groupLayout->addWidget(buttons, 0, 2);
	_contentSizeBar = new CContentSizeBar(groupBox);
	groupLayout->addWidget(_contentSizeBar, 1, 1, 1, 2);
	groupLayout->addWidget(_manifestLabel, 2, 1);
	groupLayout->addWidget(_rulesLabel, 2, 2, Qt::AlignRight);
	groupLayout->addWidget(_publishedWarningLabel, 3, 1, 1, 2);

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 13, 0);
	mainLayout->addWidget(groupBox);

	connect(_addonComboBox, &QComboBox::currentIndexChanged, this, &CAddonContentWidget::OnAddonChanged);
	connect(_showFilesButton, &QPushButton::clicked, this, &CAddonContentWidget::OnShowFilesClicked);
	connect(uploadPathsButton, &QPushButton::clicked, this, &CAddonContentWidget::OnUploadPathsClicked);

	RefreshManifest();
}

void CAddonContentWidget::SelectAddonFor(uint64_t publishedFileId, const std::string& title) {
	_targetPublishedFileId = publishedFileId;
	int matchIndex = -1;
	for (size_t index = 0; index < _addons.size() && matchIndex < 0; ++index) {
		if (publishedFileId != 0 && _addons[index].publishedFileId == publishedFileId) {
			matchIndex = static_cast<int>(index);
		}
	}
	for (size_t index = 0; index < _addons.size() && matchIndex < 0; ++index) {
		const bool titleMatches = _addons[index].publishedTitle.has_value() && *_addons[index].publishedTitle == title;
		const bool nameMatches = QString::fromStdString(_addons[index].name).compare(QString::fromStdString(title), Qt::CaseInsensitive) == 0;
		if (!title.empty() && (titleMatches || nameMatches)) {
			matchIndex = static_cast<int>(index);
		}
	}
	_addonComboBox->setCurrentIndex(matchIndex);
	RefreshManifest();
}

const AddonInfo* CAddonContentWidget::GetSelectedAddon() const {
	const int index = _addonComboBox->currentIndex();
	if (index < 0 || static_cast<size_t>(index) >= _addons.size()) {
		return nullptr;
	}
	return &_addons[static_cast<size_t>(index)];
}

void CAddonContentWidget::OnAddonChanged(int index) {
	Q_UNUSED(index);
	RefreshManifest();
	Q_EMIT SelectionChanged();
}

void CAddonContentWidget::RefreshManifest() {
	const AddonInfo* addon = GetSelectedAddon();
	if (!addon) {
		_manifest = {};
		_contentSizeBar->SetManifest(_manifest);
		_manifestLabel->setText(_addons.empty() ? tr("No addon folders found") : tr("Choose the addon folder to upload."));
		_manifestLabel->setStyleSheet(QString());
		_manifestLabel->setToolTip(QString());
		_rulesLabel->clear();
		_publishedWarningLabel->setVisible(false);
		_showFilesButton->setEnabled(false);
		return;
	}

	const ResolvedGamePaths paths = GamePaths().Resolve(_gameProfile);
	std::string errorMessage;
	const std::optional<EffectiveUploadRules> effectiveRules = UploadRules::Resolve(_gameProfile, paths.game.path, errorMessage);
	if (!effectiveRules) {
		_manifest = {};
		_manifest.errorMessage = errorMessage;
		_rulesLabel->setText(tr("Upload rules unavailable"));
	} else {
		_rules = effectiveRules->rules;
		_rulesLabel->setText(effectiveRules->custom ? tr("Rules: custom upload paths") : tr("Rules: from the game"));
		_rulesLabel->setToolTip(effectiveRules->custom ? tr("Settings: %1\nGame defaults: %2").arg(QString::fromStdWString(AppConfig().GetConfigFilePath().wstring()), QString::fromStdWString(effectiveRules->gameInfoPath.wstring())) : QString::fromStdWString(effectiveRules->gameInfoPath.wstring()));
		_manifest = AddonLibrary::BuildManifest(addon->directory, _rules);
	}
	_contentSizeBar->SetManifest(_manifest);
	_showFilesButton->setEnabled(_manifest.errorMessage.empty());
	_addonComboBox->setToolTip(QString::fromStdWString(addon->directory.wstring()));

	if (!_manifest.errorMessage.empty()) {
		_manifestLabel->setText(tr("Cannot read content. Check the addon folder and upload paths."));
		_manifestLabel->setToolTip(QString::fromStdString(_manifest.errorMessage));
		_manifestLabel->setStyleSheet("color: #ff5a5a;");
	} else if (_manifest.files.empty()) {
		_manifestLabel->setText(tr("Nothing to pack: no files match the upload paths. Compile the map first or check Upload paths."));
		_manifestLabel->setStyleSheet("color: #ff5a5a;");
	} else if (ExceedsSizeLimit()) {
		_manifestLabel->setText(tr("Too large to upload: %1 of the %2 limit. Exclude folders in Upload paths or trim the content.").arg(QLocale().formattedDataSize(static_cast<qint64>(_manifest.totalSize)), QLocale().formattedDataSize(static_cast<qint64>(WORKSHOP_CONTENT_MAX_SIZE))));
		_manifestLabel->setToolTip(tr("Valve's workshop manager stops at the same size. Review files shows what takes the space."));
		_manifestLabel->setStyleSheet("color: #ff5a5a;");
	} else {
		QString text = tr("Ready to upload");
		if (!_manifest.skippedFiles.empty()) {
			text += tr(" · %n excluded", nullptr, static_cast<int>(_manifest.skippedFiles.size()));
		}
		_manifestLabel->setText(text);
		_manifestLabel->setToolTip(tr("Only files in the Included tab are packed (limit %1). Review files to inspect folders and exclusions.").arg(QLocale().formattedDataSize(static_cast<qint64>(WORKSHOP_CONTENT_MAX_SIZE))));
		_manifestLabel->setStyleSheet(QString());
	}

	if (addon->publishedFileId.has_value()) {
		_publishedWarningLabel->setText(_targetPublishedFileId == 0 ? tr("Already published as %1. To update that item, use Re-Upload.").arg(*addon->publishedFileId) : tr("This folder belongs to a different Workshop item (%1). Check the selected addon.").arg(*addon->publishedFileId));
	}
	_publishedWarningLabel->setVisible(addon->publishedFileId.has_value() && *addon->publishedFileId != _targetPublishedFileId);
}

bool CAddonContentWidget::ExceedsSizeLimit() const {
	return _manifest.errorMessage.empty() && _manifest.totalSize > WORKSHOP_CONTENT_MAX_SIZE;
}

void CAddonContentWidget::OnShowFilesClicked() {
	RefreshManifest();
	const AddonInfo* addon = GetSelectedAddon();
	if (!addon || !_manifest.errorMessage.empty()) {
		return;
	}

	QDialog dialog(this);
	dialog.setWindowTitle(tr("Files to pack — %1").arg(QString::fromStdString(addon->name)));
	dialog.resize(820, 580);

	auto* tabs = new QTabWidget(&dialog);
	auto* includedTree = new QTreeWidget(tabs);
	auto* excludedTree = new QTreeWidget(tabs);
	for (QTreeWidget* tree : {includedTree, excludedTree}) {
		tree->setHeaderLabels({tr("Folder / file"), tr("Size")});
		tree->setRootIsDecorated(true);
		tree->setUniformRowHeights(true);
		tree->header()->setStretchLastSection(false);
		tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
		tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	}
	excludedTree->hideColumn(1);
	for (const AddonFileEntry& file : _manifest.files) {
		AddFileToTree(includedTree, file.relativePath, file.size);
	}
	for (const std::filesystem::path& entry : _manifest.skippedFiles) {
		AddFileToTree(excludedTree, entry, std::nullopt);
	}
	includedTree->expandToDepth(0);
	excludedTree->sortItems(0, Qt::AscendingOrder);
	tabs->addTab(includedTree, tr("Included (%1)").arg(_manifest.files.size()));
	tabs->addTab(excludedTree, tr("Excluded (%1)").arg(_manifest.skippedFiles.size()));
	auto* hintLabel = new QLabel(tr("Only Included files are uploaded. Excluded files stay on your computer. Folder sizes include their nested files; empty folders are not stored in VPK."), &dialog);
	hintLabel->setWordWrap(true);
	hintLabel->setObjectName("hintLabel");

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

	auto* layout = new QVBoxLayout(&dialog);
	layout->addWidget(hintLabel);
	layout->addWidget(tabs, 1);
	layout->addWidget(buttons);
	dialog.exec();
}

void CAddonContentWidget::OnUploadPathsClicked() {
	QDialog dialog(this);
	dialog.setWindowTitle(tr("Upload paths"));
	dialog.resize(820, 560);
	auto* page = new CUploadPathsPage(&dialog);
	page->SelectGame(_gameProfile.id);
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
	buttons->button(QDialogButtonBox::Save)->setObjectName("primaryButton");
	auto* layout = new QVBoxLayout(&dialog);
	layout->addWidget(page, 1);
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, page]() {
		const AppConfigData previousConfig = AppConfig().Data();
		page->ApplyToConfig();
		if (!AppConfig().Save()) {
			AppConfig().Data() = previousConfig;
			QMessageBox::warning(&dialog, tr("Upload paths"), tr("Could not save the settings. Check that the settings folder is writable."));
			return;
		}
		dialog.accept();
	});
	if (dialog.exec() == QDialog::Accepted) {
		RefreshManifest();
		Q_EMIT SelectionChanged();
	}
}
