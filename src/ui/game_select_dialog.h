#pragma once

#include "core/game_profile.h"

class CGameSelectDialog : public QDialog {
	Q_OBJECT

public:
	explicit CGameSelectDialog(QWidget* parent = nullptr);

	void SetCurrentGame(std::string_view gameId);
	const GameProfile* GetSelectedGame() const;
	bool ShouldRememberChoice() const { return _rememberChoiceCheckBox->isChecked(); }

private:
	void PopulateGames();
	void OnSelectionChanged();
	void OnPathsClicked();

	QListWidget* _gamesListWidget{nullptr};
	QCheckBox* _rememberChoiceCheckBox{nullptr};
	QPushButton* _continueButton{nullptr};
};
