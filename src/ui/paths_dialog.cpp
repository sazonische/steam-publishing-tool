#include "ui/paths_dialog.h"

#include "core/app_config.h"
#include "core/steam_locator.h"

namespace {

	QString FromStringView(std::string_view text) {
		return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
	}

	QString ToQString(const std::filesystem::path& path) {
		return QDir::toNativeSeparators(QString::fromStdWString(path.wstring()));
	}

	std::string ToUtf8(const QString& text) {
		return QDir::fromNativeSeparators(text.trimmed()).toStdString();
	}

} // namespace

CPathsDialog::CPathsDialog(QWidget* parent) :
	QDialog(parent) {
	setObjectName("pathsEditor");
	setWindowTitle(tr("Paths"));
	setMinimumSize(860, 640);

	auto* headlineLabel = new QLabel(tr("Paths"), this);
	headlineLabel->setObjectName("titleLabel");
	auto* headerHintLabel = new QLabel(tr("Where Steam and the games live, and which addon folders go into the Workshop VPK. Everything is detected through the Steam client; set a folder by hand only when detection fails — portable Steam, a copy of the game outside Steam, several installations."), this);
	headerHintLabel->setObjectName("hintLabel");
	headerHintLabel->setWordWrap(true);

	auto* foldersPage = new QWidget(this);
	auto* foldersLayout = new QVBoxLayout(foldersPage);
	foldersLayout->setContentsMargins(0, 12, 0, 0);
	foldersLayout->setSpacing(18);
	AddRow(tr("Steam"), nullptr, AppConfig().Data().steamPath, foldersLayout);
	for (const GameProfile& profile : GameProfiles::All()) {
		AddRow(FromStringView(profile.displayName), &profile, AppConfig().GetGameInstallPath(profile.id), foldersLayout);
	}
	foldersLayout->addStretch();

	_uploadPathsPage = new CUploadPathsPage(this);

	_tabWidget = new QTabWidget(this);
	_tabWidget->setObjectName("editorTabs");
	_tabWidget->tabBar()->setObjectName("editorTabBar");
	_tabWidget->tabBar()->setDrawBase(false);
	_tabWidget->addTab(foldersPage, tr("Folders"));
	_tabWidget->addTab(_uploadPathsPage, tr("Upload paths"));

	_configPathLabel = new QLabel(tr("Settings file: %1").arg(ToQString(AppConfig().GetConfigFilePath())), this);
	_configPathLabel->setObjectName("hintLabel");
	_configPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

	auto* cancelButton = new QPushButton(tr("Cancel"), this);
	auto* saveButton = new QPushButton(tr("Save"), this);
	saveButton->setObjectName("primaryButton");
	saveButton->setDefault(true);

	auto* buttonsLayout = new QHBoxLayout();
	buttonsLayout->addWidget(_configPathLabel, 1);
	buttonsLayout->addWidget(cancelButton);
	buttonsLayout->addWidget(saveButton);

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(16, 16, 16, 16);
	mainLayout->setSpacing(12);
	mainLayout->addWidget(headlineLabel);
	mainLayout->addWidget(headerHintLabel);
	mainLayout->addWidget(_tabWidget, 1);
	mainLayout->addLayout(buttonsLayout);

	connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
	connect(saveButton, &QPushButton::clicked, this, &CPathsDialog::OnSaveClicked);

	for (PathRow& row : _rows) {
		RefreshRow(row);
	}
}

void CPathsDialog::AddRow(const QString& title, const GameProfile* profile, std::optional<std::string> manualPath, QVBoxLayout* layout) {
	PathRow row;
	row.profile = profile;
	row.manualPath = std::move(manualPath);
	row.pathEdit = new QLineEdit(this);
	row.pathEdit->setPlaceholderText(tr("detecting…"));
	row.browseButton = new QPushButton(tr("Browse…"), this);
	row.resetButton = new QPushButton(tr("Auto"), this);
	row.resetButton->setToolTip(tr("Forget the manual path and detect it through Steam"));
	row.statusLabel = new QLabel(this);
	row.statusLabel->setObjectName("hintLabel");
	row.statusLabel->setWordWrap(true);

	auto* titleLabel = new QLabel(title, this);
	titleLabel->setObjectName("sectionLabel");

	auto* fieldLayout = new QHBoxLayout();
	fieldLayout->setSpacing(8);
	fieldLayout->addWidget(row.pathEdit, 1);
	fieldLayout->addWidget(row.browseButton);
	fieldLayout->addWidget(row.resetButton);

	auto* blockLayout = new QVBoxLayout();
	blockLayout->setSpacing(4);
	blockLayout->addWidget(titleLabel);
	blockLayout->addLayout(fieldLayout);
	blockLayout->addWidget(row.statusLabel);
	layout->addLayout(blockLayout);

	_rows.push_back(row);
	const size_t rowIndex = _rows.size() - 1;
	// An index, not a reference: the vector is still growing and a reference would dangle.
	connect(row.browseButton, &QPushButton::clicked, this, [this, rowIndex]() { OnBrowseClicked(_rows[rowIndex]); });
	connect(row.resetButton, &QPushButton::clicked, this, [this, rowIndex]() { OnResetClicked(_rows[rowIndex]); });
	connect(row.pathEdit, &QLineEdit::editingFinished, this, [this, rowIndex]() { OnPathEdited(_rows[rowIndex]); });
}

void CPathsDialog::RefreshRow(PathRow& row) {
	const bool isManual = row.manualPath.has_value();
	row.resetButton->setEnabled(isManual);

	std::filesystem::path shownPath;
	std::string problem;
	bool valid = false;
	QString sourceText;

	if (isManual) {
		shownPath = std::filesystem::path(QString::fromStdString(*row.manualPath).toStdWString());
		if (row.profile) {
			valid = CGamePaths::ValidateGamePath(*row.profile, shownPath, problem);
		} else {
			valid = SteamLocator::IsSteamInstallPath(shownPath, problem);
		}
		sourceText = tr("manual");
	} else if (row.profile) {
		const PathRow& steamRow = _rows.front();
		std::string steamProblem;
		std::filesystem::path steamPath;
		if (steamRow.manualPath.has_value()) {
			steamPath = std::filesystem::path(QString::fromStdString(*steamRow.manualPath).toStdWString());
		} else if (const auto detectedSteamPath = SteamLocator::FindSteamInstallPath()) {
			steamPath = *detectedSteamPath;
		}
		if (steamPath.empty() || !SteamLocator::IsSteamInstallPath(steamPath, steamProblem)) {
			problem = "Steam path is unknown";
		} else if (const auto detectedGamePath = SteamLocator::FindGameInstallPath(steamPath, row.profile->appId, row.profile->installFolder)) {
			shownPath = *detectedGamePath;
			valid = CGamePaths::ValidateGamePath(*row.profile, shownPath, problem);
		} else {
			problem = std::format("AppID {} is not installed in any Steam library", row.profile->appId);
		}
		sourceText = tr("detected via Steam");
	} else {
		if (const auto detectedSteamPath = SteamLocator::FindSteamInstallPath()) {
			shownPath = *detectedSteamPath;
			valid = SteamLocator::IsSteamInstallPath(shownPath, problem);
		} else {
			problem = "Steam installation not found in the registry";
		}
		sourceText = tr("detected via registry");
	}

	row.pathEdit->blockSignals(true);
	row.pathEdit->setText(shownPath.empty() ? QString() : ToQString(shownPath));
	row.pathEdit->blockSignals(false);
	row.pathEdit->setPlaceholderText(isManual ? tr("enter a folder") : tr("not found — use Browse… to set it manually"));

	if (valid) {
		row.statusLabel->setText(tr("✓ %1").arg(sourceText));
		row.statusLabel->setStyleSheet("color: #5cc063;");
	} else {
		row.statusLabel->setText(tr("✗ %1: %2").arg(sourceText, QString::fromStdString(problem)));
		row.statusLabel->setStyleSheet("color: #ff5a5a;");
	}
}

void CPathsDialog::OnBrowseClicked(PathRow& row) {
	const QString startDirectory = row.pathEdit->text().isEmpty() ? QString() : row.pathEdit->text();
	const QString title = row.profile ? tr("Select the %1 installation folder").arg(FromStringView(row.profile->displayName)) : tr("Select the Steam folder (contains steam.exe)");
	const QString selectedDirectory = QFileDialog::getExistingDirectory(this, title, startDirectory);
	if (selectedDirectory.isEmpty()) {
		return;
	}
	row.manualPath = ToUtf8(selectedDirectory);
	for (PathRow& eachRow : _rows) {
		RefreshRow(eachRow);
	}
}

void CPathsDialog::OnResetClicked(PathRow& row) {
	row.manualPath.reset();
	for (PathRow& eachRow : _rows) {
		RefreshRow(eachRow);
	}
}

void CPathsDialog::OnPathEdited(PathRow& row) {
	const QString text = row.pathEdit->text().trimmed();
	if (text.isEmpty()) {
		row.manualPath.reset();
	} else {
		row.manualPath = ToUtf8(text);
	}
	for (PathRow& eachRow : _rows) {
		RefreshRow(eachRow);
	}
}

void CPathsDialog::ShowTab(Tab tab) {
	_tabWidget->setCurrentIndex(static_cast<int>(tab));
}

void CPathsDialog::OnSaveClicked() {
	_uploadPathsPage->ApplyToConfig();
	for (const PathRow& row : _rows) {
		if (row.profile) {
			AppConfig().SetGameInstallPath(row.profile->id, row.manualPath);
		} else {
			AppConfig().Data().steamPath = row.manualPath;
		}
	}
	if (!AppConfig().Save()) {
		QMessageBox::warning(this, tr("Paths"), tr("Could not write the settings file:\n%1").arg(ToQString(AppConfig().GetConfigFilePath())));
		return;
	}
	Q_EMIT PathsChanged();
	accept();
}
