#include "ui/game_select_dialog.h"

#include "core/game_paths.h"
#include "ui/paths_dialog.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

namespace {

	constexpr int GAME_ID_ROLE = Qt::UserRole + 1;

} // namespace

CGameSelectDialog::CGameSelectDialog(QWidget* parent) :
	QDialog(parent) {
	setWindowTitle(tr("Steam Publishing Tool"));
	setMinimumWidth(620);

	auto* titleLabel = new QLabel(tr("Select a game"), this);
	titleLabel->setObjectName("titleLabel");

	auto* hintLabel = new QLabel(tr("The tool connects to Steam as the selected game and shows its Workshop items."), this);
	hintLabel->setObjectName("hintLabel");
	hintLabel->setWordWrap(true);

	_gamesListWidget = new QListWidget(this);
	PopulateGames();

	auto* pathsButton = new QPushButton(tr("Paths…"), this);
	pathsButton->setToolTip(tr("Steam and game folders"));

	_rememberChoiceCheckBox = new QCheckBox(tr("Remember my choice and skip this dialog"), this);

	_continueButton = new QPushButton(tr("Continue"), this);
	_continueButton->setObjectName("primaryButton");
	_continueButton->setDefault(true);

	auto* cancelButton = new QPushButton(tr("Quit"), this);

	auto* versionLabel = new QLabel(tr("Steam Publishing Tool %1").arg(QLatin1String(APP_VERSION)), this);
	versionLabel->setObjectName("hintLabel");

	auto* buttonsLayout = new QHBoxLayout();
	buttonsLayout->addWidget(pathsButton);
	buttonsLayout->addSpacing(12);
	buttonsLayout->addWidget(versionLabel);
	buttonsLayout->addStretch();
	buttonsLayout->addWidget(cancelButton);
	buttonsLayout->addWidget(_continueButton);

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(20, 20, 20, 20);
	mainLayout->setSpacing(12);
	mainLayout->addWidget(titleLabel);
	mainLayout->addWidget(hintLabel);
	mainLayout->addWidget(_gamesListWidget, 1);
	mainLayout->addWidget(_rememberChoiceCheckBox);
	mainLayout->addLayout(buttonsLayout);

	connect(_gamesListWidget, &QListWidget::currentRowChanged, this, &CGameSelectDialog::OnSelectionChanged);
	connect(_gamesListWidget, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
	connect(_continueButton, &QPushButton::clicked, this, &QDialog::accept);
	connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
	connect(pathsButton, &QPushButton::clicked, this, &CGameSelectDialog::OnPathsClicked);

	OnSelectionChanged();
}

void CGameSelectDialog::PopulateGames() {
	const QString previousGameId = _gamesListWidget->currentItem() ? _gamesListWidget->currentItem()->data(GAME_ID_ROLE).toString() : QString();
	_gamesListWidget->clear();

	for (const GameProfile& profile : GameProfiles::All()) {
		const ResolvedGamePaths paths = GamePaths().Resolve(profile);
		const QString pathLine = paths.game.valid ? QDir::toNativeSeparators(QString::fromStdWString(paths.game.path.wstring())) : tr("not found: %1").arg(QString::fromStdString(paths.game.problem));

		auto* gameItem = new QListWidgetItem(QStringLiteral("%1   (AppID %2)\n%3").arg(QString::fromUtf8(profile.displayName.data(), static_cast<qsizetype>(profile.displayName.size()))).arg(profile.appId).arg(pathLine));
		gameItem->setData(GAME_ID_ROLE, QString::fromUtf8(profile.id.data(), static_cast<qsizetype>(profile.id.size())));
		if (!paths.game.valid) {
			gameItem->setForeground(QColor(0x8f, 0x98, 0xa0));
		}
		_gamesListWidget->addItem(gameItem);
	}

	if (!previousGameId.isEmpty()) {
		SetCurrentGame(previousGameId.toStdString());
	} else if (_gamesListWidget->count() > 0) {
		_gamesListWidget->setCurrentRow(0);
	}
}

void CGameSelectDialog::OnPathsClicked() {
	CPathsDialog pathsDialog(this);
	connect(&pathsDialog, &CPathsDialog::PathsChanged, this, &CGameSelectDialog::PopulateGames);
	pathsDialog.exec();
}

void CGameSelectDialog::SetCurrentGame(std::string_view gameId) {
	const QString wantedGameId = QString::fromUtf8(gameId.data(), static_cast<qsizetype>(gameId.size()));
	for (int row = 0; row < _gamesListWidget->count(); ++row) {
		if (_gamesListWidget->item(row)->data(GAME_ID_ROLE).toString() == wantedGameId) {
			_gamesListWidget->setCurrentRow(row);
			return;
		}
	}
}

const GameProfile* CGameSelectDialog::GetSelectedGame() const {
	const QListWidgetItem* currentItem = _gamesListWidget->currentItem();
	if (!currentItem) {
		return nullptr;
	}
	const std::string gameId = currentItem->data(GAME_ID_ROLE).toString().toStdString();
	return GameProfiles::FindById(gameId);
}

void CGameSelectDialog::OnSelectionChanged() {
	_continueButton->setEnabled(GetSelectedGame() != nullptr);
}
