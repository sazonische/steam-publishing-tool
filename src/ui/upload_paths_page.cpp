#include "ui/upload_paths_page.h"

#include "core/game_paths.h"
#include "core/upload_rules.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QVBoxLayout>

namespace {

	QString FromStringView(std::string_view text) {
		return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
	}

	QListWidget* BuildRulesList(QWidget* parent) {
		auto* list = new QListWidget(parent);
		list->setSelectionMode(QAbstractItemView::SingleSelection);
		list->setAlternatingRowColors(true);
		list->setToolTip(QObject::tr("Double-click an entry to edit it"));
		return list;
	}

} // namespace

void CUploadPathsPage::SelectGame(std::string_view gameId) {
	for (size_t index = 0; index < _states.size(); ++index) {
		if (_states[index].profile->id == gameId) {
			_gameComboBox->setCurrentIndex(static_cast<int>(index));
			return;
		}
	}
}

CUploadPathsPage::CUploadPathsPage(QWidget* parent) :
	QWidget(parent) {
	auto* hintLabel = new QLabel(tr("Only these folders and files of an addon are packed into the Workshop VPK — the same AddonConfig/VpkDirectories rules the game's own workshop manager follows. Paths are relative to the addon folder; an exclude wins over an include it sits inside."), this);
	hintLabel->setObjectName("hintLabel");
	hintLabel->setWordWrap(true);

	_gameComboBox = new QComboBox(this);
	for (const GameProfile& profile : GameProfiles::All()) {
		_gameComboBox->addItem(FromStringView(profile.displayName));
	}

	_sourceLabel = new QLabel(this);
	_sourceLabel->setObjectName("hintLabel");
	_sourceLabel->setWordWrap(true);
	_sourceLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

	_gameInfoRadio = new QRadioButton(tr("Use the rules from gameinfo (recommended)"), this);
	_customRadio = new QRadioButton(tr("Use my own list — copied from gameinfo when first enabled, new Valve entries are added automatically"), this);

	auto* includeColumn = new QWidget(this);
	auto* includeCaption = new QLabel(tr("Packed into the VPK"), includeColumn);
	includeCaption->setObjectName("sectionLabel");
	_includeList = BuildRulesList(includeColumn);
	_addIncludeButton = new QPushButton(tr("Add…"), includeColumn);
	_removeIncludeButton = new QPushButton(tr("Remove"), includeColumn);
	auto* includeButtons = new QHBoxLayout();
	includeButtons->addWidget(_addIncludeButton);
	includeButtons->addWidget(_removeIncludeButton);
	includeButtons->addStretch();
	auto* includeLayout = new QVBoxLayout(includeColumn);
	includeLayout->setContentsMargins(0, 0, 0, 0);
	includeLayout->setSpacing(6);
	includeLayout->addWidget(includeCaption);
	includeLayout->addWidget(_includeList, 1);
	includeLayout->addLayout(includeButtons);

	auto* excludeColumn = new QWidget(this);
	auto* excludeCaption = new QLabel(tr("Left out"), excludeColumn);
	excludeCaption->setObjectName("sectionLabel");
	_excludeList = BuildRulesList(excludeColumn);
	_addExcludeButton = new QPushButton(tr("Add…"), excludeColumn);
	_removeExcludeButton = new QPushButton(tr("Remove"), excludeColumn);
	auto* excludeButtons = new QHBoxLayout();
	excludeButtons->addWidget(_addExcludeButton);
	excludeButtons->addWidget(_removeExcludeButton);
	excludeButtons->addStretch();
	auto* excludeLayout = new QVBoxLayout(excludeColumn);
	excludeLayout->setContentsMargins(0, 0, 0, 0);
	excludeLayout->setSpacing(6);
	excludeLayout->addWidget(excludeCaption);
	excludeLayout->addWidget(_excludeList, 1);
	excludeLayout->addLayout(excludeButtons);

	auto* listsLayout = new QHBoxLayout();
	listsLayout->setSpacing(24);
	listsLayout->addWidget(includeColumn, 3);
	listsLayout->addWidget(excludeColumn, 2);

	_syncButton = new QPushButton(tr("Sync with gameinfo"), this);
	_syncButton->setToolTip(tr("Add entries that appeared in gameinfo since the last sync. Entries you removed yourself stay removed."));
	_resetButton = new QPushButton(tr("Reset to gameinfo"), this);
	_resetButton->setToolTip(tr("Discard the custom list and copy the current gameinfo rules again"));

	auto* actionsLayout = new QHBoxLayout();
	actionsLayout->addWidget(_syncButton);
	actionsLayout->addWidget(_resetButton);
	actionsLayout->addStretch();

	auto* gameLayout = new QHBoxLayout();
	gameLayout->addWidget(new QLabel(tr("Game"), this));
	gameLayout->addWidget(_gameComboBox, 1);

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 12, 0, 0);
	mainLayout->setSpacing(10);
	mainLayout->addWidget(hintLabel);
	mainLayout->addLayout(gameLayout);
	mainLayout->addWidget(_sourceLabel);
	mainLayout->addWidget(_gameInfoRadio);
	mainLayout->addWidget(_customRadio);
	mainLayout->addLayout(listsLayout, 1);
	mainLayout->addLayout(actionsLayout);

	connect(_gameComboBox, &QComboBox::currentIndexChanged, this, &CUploadPathsPage::OnGameChanged);
	connect(_gameInfoRadio, &QRadioButton::toggled, this, &CUploadPathsPage::OnModeChanged);
	connect(_addIncludeButton, &QPushButton::clicked, this, &CUploadPathsPage::OnAddIncludeClicked);
	connect(_addExcludeButton, &QPushButton::clicked, this, &CUploadPathsPage::OnAddExcludeClicked);
	connect(_removeIncludeButton, &QPushButton::clicked, this, [this]() { OnRemoveClicked(_includeList, &UploadRulesConfig::includes); });
	connect(_removeExcludeButton, &QPushButton::clicked, this, [this]() { OnRemoveClicked(_excludeList, &UploadRulesConfig::excludes); });
	connect(_includeList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) { OnEditRequested(_includeList, &UploadRulesConfig::includes, item); });
	connect(_excludeList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) { OnEditRequested(_excludeList, &UploadRulesConfig::excludes, item); });
	connect(_syncButton, &QPushButton::clicked, this, &CUploadPathsPage::OnSyncClicked);
	connect(_resetButton, &QPushButton::clicked, this, &CUploadPathsPage::OnResetClicked);

	LoadGameStates();
	ShowCurrentState();
}

void CUploadPathsPage::LoadGameStates() {
	_states.clear();
	for (const GameProfile& profile : GameProfiles::All()) {
		GameState state;
		state.profile = &profile;

		const ResolvedGamePaths paths = GamePaths().Resolve(profile);
		if (profile.gameInfoPath.empty()) {
			state.gameInfoProblem = "this game has no addon packing rules";
		} else if (!paths.game.valid) {
			state.gameInfoProblem = paths.game.problem;
		} else {
			std::string errorMessage;
			state.gameInfoRules = AddonConfig::Load(profile, paths.game.path, errorMessage);
			if (!state.gameInfoRules.has_value()) {
				state.gameInfoProblem = errorMessage;
			}
		}

		if (const UploadRulesConfig* configured = AppConfig().GetUploadRules(profile.id)) {
			state.customRules = *configured;
		}
		_states.push_back(std::move(state));
	}
}

CUploadPathsPage::GameState& CUploadPathsPage::CurrentState() {
	return _states[static_cast<size_t>(std::max(0, _gameComboBox->currentIndex()))];
}

void CUploadPathsPage::FillList(QListWidget* list, const std::vector<std::string>& entries, bool editable) {
	list->clear();
	for (const std::string& entry : entries) {
		list->addItem(QString::fromStdString(entry));
	}
	list->setEnabled(editable);
}

void CUploadPathsPage::ShowCurrentState() {
	GameState& state = CurrentState();
	const bool hasGameInfo = state.gameInfoRules.has_value();
	const bool custom = state.customRules.has_value() && state.customRules->useCustomRules;

	if (hasGameInfo) {
		_sourceLabel->setText(tr("gameinfo: %1 include, %2 exclude rules").arg(state.gameInfoRules->includes.size()).arg(state.gameInfoRules->excludes.size()));
		_sourceLabel->setStyleSheet(QString());
	} else {
		_sourceLabel->setText(tr("gameinfo rules unavailable: %1").arg(QString::fromStdString(state.gameInfoProblem)));
		_sourceLabel->setStyleSheet("color: #ff5a5a;");
	}

	_gameInfoRadio->blockSignals(true);
	_customRadio->blockSignals(true);
	_gameInfoRadio->setChecked(!custom);
	_customRadio->setChecked(custom);
	_gameInfoRadio->blockSignals(false);
	_customRadio->blockSignals(false);
	_gameInfoRadio->setEnabled(hasGameInfo || custom);
	_customRadio->setEnabled(hasGameInfo || custom);

	if (custom) {
		FillList(_includeList, state.customRules->includes, true);
		FillList(_excludeList, state.customRules->excludes, true);
	} else if (hasGameInfo) {
		FillList(_includeList, state.gameInfoRules->includes, false);
		FillList(_excludeList, state.gameInfoRules->excludes, false);
	} else {
		FillList(_includeList, {}, false);
		FillList(_excludeList, {}, false);
	}

	for (QPushButton* button : {_addIncludeButton, _addExcludeButton, _removeIncludeButton, _removeExcludeButton, _resetButton}) {
		button->setEnabled(custom);
	}
	_syncButton->setEnabled(custom && hasGameInfo);
}

void CUploadPathsPage::OnGameChanged(int index) {
	Q_UNUSED(index);
	ShowCurrentState();
}

void CUploadPathsPage::OnModeChanged() {
	GameState& state = CurrentState();
	const bool wantCustom = _customRadio->isChecked();

	if (wantCustom && !state.customRules.has_value()) {
		if (!state.gameInfoRules.has_value()) {
			QMessageBox::warning(this, tr("Upload paths"), tr("Cannot start a custom list: gameinfo rules are unavailable (%1).").arg(QString::fromStdString(state.gameInfoProblem)));
			ShowCurrentState();
			return;
		}
		state.customRules.emplace();
		UploadRules::CopyFromGameInfo(*state.customRules, *state.gameInfoRules);
	}
	if (state.customRules.has_value()) {
		state.customRules->useCustomRules = wantCustom;
	}
	ShowCurrentState();
}

void CUploadPathsPage::OnAddIncludeClicked() {
	GameState& state = CurrentState();
	bool accepted = false;
	const QString entry = QInputDialog::getText(this, tr("Add include"), tr("Folder or file relative to the addon root, e.g. panorama/images/custom:"), QLineEdit::Normal, QString(), &accepted).trimmed();
	if (!accepted || entry.isEmpty() || !state.customRules.has_value()) {
		return;
	}
	if (UploadRules::ContainsRule(state.customRules->includes, entry.toStdString())) {
		return;
	}
	state.customRules->includes.push_back(entry.toStdString());
	ShowCurrentState();
}

void CUploadPathsPage::OnAddExcludeClicked() {
	GameState& state = CurrentState();
	bool accepted = false;
	const QString entry = QInputDialog::getText(this, tr("Add exclude"), tr("Folder or file relative to the addon root, e.g. maps/content_examples:"), QLineEdit::Normal, QString(), &accepted).trimmed();
	if (!accepted || entry.isEmpty() || !state.customRules.has_value()) {
		return;
	}
	if (UploadRules::ContainsRule(state.customRules->excludes, entry.toStdString())) {
		return;
	}
	state.customRules->excludes.push_back(entry.toStdString());
	ShowCurrentState();
}

void CUploadPathsPage::OnRemoveClicked(QListWidget* list, std::vector<std::string> UploadRulesConfig::* entries) {
	GameState& state = CurrentState();
	const int row = list->currentRow();
	if (!state.customRules.has_value() || row < 0) {
		return;
	}
	std::vector<std::string>& target = (*state.customRules).*entries;
	if (static_cast<size_t>(row) >= target.size()) {
		return;
	}
	target.erase(target.begin() + row);
	ShowCurrentState();
}

void CUploadPathsPage::OnEditRequested(QListWidget* list, std::vector<std::string> UploadRulesConfig::* entries, QListWidgetItem* item) {
	GameState& state = CurrentState();
	const int row = list->row(item);
	if (!state.customRules.has_value() || row < 0) {
		return;
	}
	std::vector<std::string>& target = (*state.customRules).*entries;
	if (static_cast<size_t>(row) >= target.size()) {
		return;
	}
	bool accepted = false;
	const QString entry = QInputDialog::getText(this, tr("Edit rule"), tr("Folder or file relative to the addon root:"), QLineEdit::Normal, QString::fromStdString(target[static_cast<size_t>(row)]), &accepted).trimmed();
	if (!accepted || entry.isEmpty()) {
		return;
	}
	target[static_cast<size_t>(row)] = entry.toStdString();
	ShowCurrentState();
	list->setCurrentRow(row);
}

void CUploadPathsPage::OnSyncClicked() {
	GameState& state = CurrentState();
	if (!state.customRules.has_value() || !state.gameInfoRules.has_value()) {
		return;
	}
	const std::vector<std::string> added = UploadRules::SyncWithGameInfo(*state.customRules, *state.gameInfoRules);
	ShowCurrentState();

	if (added.empty()) {
		QMessageBox::information(this, tr("Sync with gameinfo"), tr("Nothing new: every gameinfo rule is already known."));
		return;
	}
	QStringList addedText;
	for (const std::string& entry : added) {
		addedText.append(QString::fromStdString(entry));
	}
	QMessageBox::information(this, tr("Sync with gameinfo"), tr("Added %n new rule(s):\n%1", nullptr, static_cast<int>(added.size())).arg(addedText.join('\n')));
}

void CUploadPathsPage::OnResetClicked() {
	GameState& state = CurrentState();
	if (!state.customRules.has_value() || !state.gameInfoRules.has_value()) {
		return;
	}
	UploadRules::CopyFromGameInfo(*state.customRules, *state.gameInfoRules);
	ShowCurrentState();
}

void CUploadPathsPage::ApplyToConfig() {
	for (const GameState& state : _states) {
		if (state.customRules.has_value()) {
			AppConfig().EnsureUploadRules(state.profile->id) = *state.customRules;
		} else {
			AppConfig().ResetUploadRules(state.profile->id);
		}
	}
}
