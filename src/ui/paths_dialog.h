#pragma once

#include "core/game_paths.h"
#include "core/game_profile.h"
#include "ui/upload_paths_page.h"

class CPathsDialog : public QDialog {
	Q_OBJECT

public:
	enum Tab : int {
		TAB_FOLDERS = 0,
		TAB_UPLOAD_PATHS,
	};

	explicit CPathsDialog(QWidget* parent = nullptr);

	void ShowTab(Tab tab);

Q_SIGNALS:
	void PathsChanged();

private:
	struct PathRow {
		const GameProfile* profile{nullptr};
		QLineEdit* pathEdit{nullptr};
		QPushButton* browseButton{nullptr};
		QPushButton* resetButton{nullptr};
		QLabel* statusLabel{nullptr};
		std::optional<std::string> manualPath;
	};

	void AddRow(const QString& title, const GameProfile* profile, std::optional<std::string> manualPath, class QVBoxLayout* layout);
	void RefreshRow(PathRow& row);
	void OnBrowseClicked(PathRow& row);
	void OnResetClicked(PathRow& row);
	void OnPathEdited(PathRow& row);
	void OnSaveClicked();

	std::vector<PathRow> _rows;
	QLabel* _configPathLabel{nullptr};
	QTabWidget* _tabWidget{nullptr};
	CUploadPathsPage* _uploadPathsPage{nullptr};
};
