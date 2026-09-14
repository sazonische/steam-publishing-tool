#pragma once

#include "core/addon_config.h"
#include "core/addon_library.h"
#include "ui/content_size_bar.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

#include <vector>

class CAddonContentWidget : public QWidget {
	Q_OBJECT

public:
	CAddonContentWidget(const GameProfile& gameProfile, std::vector<AddonInfo> addons, AddonVpkRules rules, QWidget* parent = nullptr);

	void SelectAddonFor(uint64_t publishedFileId, const std::string& title);
	const AddonInfo* GetSelectedAddon() const;
	const AddonManifest& GetManifest() const { return _manifest; }
	bool HasFilesToPack() const { return _manifest.errorMessage.empty() && !_manifest.files.empty(); }
	bool ExceedsSizeLimit() const;
	void RefreshManifest();

Q_SIGNALS:
	void SelectionChanged();

private:
	void OnAddonChanged(int index);
	void OnShowFilesClicked();
	void OnUploadPathsClicked();

	std::vector<AddonInfo> _addons;
	AddonVpkRules _rules;
	AddonManifest _manifest;
	const GameProfile& _gameProfile;
	uint64_t _targetPublishedFileId{0};

	QComboBox* _addonComboBox{nullptr};
	QLabel* _manifestLabel{nullptr};
	CContentSizeBar* _contentSizeBar{nullptr};
	QLabel* _rulesLabel{nullptr};
	QLabel* _publishedWarningLabel{nullptr};
	QPushButton* _showFilesButton{nullptr};
};
