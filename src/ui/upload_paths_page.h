#pragma once

#include "core/addon_config.h"
#include "core/app_config.h"
#include "core/game_profile.h"

class CUploadPathsPage : public QWidget {
	Q_OBJECT

public:
	explicit CUploadPathsPage(QWidget* parent = nullptr);

	void ApplyToConfig();
	void SelectGame(std::string_view gameId);

private:
	struct GameState {
		const GameProfile* profile{nullptr};
		std::optional<AddonVpkRules> gameInfoRules;
		std::string gameInfoProblem;
		std::optional<UploadRulesConfig> customRules;
	};

	void LoadGameStates();
	GameState& CurrentState();
	void ShowCurrentState();
	void FillList(QListWidget* list, const std::vector<std::string>& entries, bool editable);

	void OnGameChanged(int index);
	void OnModeChanged();
	void OnAddIncludeClicked();
	void OnAddExcludeClicked();
	void OnRemoveClicked(QListWidget* list, std::vector<std::string> UploadRulesConfig::* entries);
	void OnEditRequested(QListWidget* list, std::vector<std::string> UploadRulesConfig::* entries, QListWidgetItem* item);
	void OnSyncClicked();
	void OnResetClicked();

	std::vector<GameState> _states;

	QComboBox* _gameComboBox{nullptr};
	QLabel* _sourceLabel{nullptr};
	QRadioButton* _gameInfoRadio{nullptr};
	QRadioButton* _customRadio{nullptr};
	QListWidget* _includeList{nullptr};
	QListWidget* _excludeList{nullptr};
	QPushButton* _addIncludeButton{nullptr};
	QPushButton* _addExcludeButton{nullptr};
	QPushButton* _removeIncludeButton{nullptr};
	QPushButton* _removeExcludeButton{nullptr};
	QPushButton* _syncButton{nullptr};
	QPushButton* _resetButton{nullptr};
};
