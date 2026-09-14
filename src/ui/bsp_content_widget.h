#pragma once

#include "core/bsp_map.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

#include <filesystem>
#include <optional>

// Content for Source 1 games (Portal 2): a single BSP. Shows what could be learned about the map and
// warns about a missing PTI relay — like p2-publishing-tool, with an "upload anyway" checkbox.
class CBspContentWidget : public QWidget {
	Q_OBJECT

public:
	CBspContentWidget(std::filesystem::path defaultMapsFolder, QWidget* parent = nullptr);

	void SetBspPath(const std::filesystem::path& bspPath);
	std::filesystem::path GetBspPath() const;
	const std::optional<BspMapInfo>& GetMapInfo() const { return _mapInfo; }

	bool IsReadyToUpload(QString& errorMessage) const;

Q_SIGNALS:
	void MapInspected(const BspMapInfo& info);

private:
	void OnBrowseClicked();
	void OnPathEdited();
	void Inspect();

	std::filesystem::path _defaultMapsFolder;
	std::optional<BspMapInfo> _mapInfo;
	QString _inspectError;

	QLineEdit* _pathEdit{nullptr};
	QPushButton* _browseButton{nullptr};
	QLabel* _infoLabel{nullptr};
	QLabel* _warningLabel{nullptr};
	QCheckBox* _allowWithoutPtiCheckBox{nullptr};
};
