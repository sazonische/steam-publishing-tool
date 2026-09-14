#pragma once

#include "core/addon_config.h"
#include "core/addon_library.h"
#include "core/game_profile.h"
#include "steam/publish_pipeline.h"
#include "steam/workshop_item.h"
#include "steam/workshop_service.h"
#include "steam/workshop_update.h"
#include "ui/addon_content_widget.h"
#include "ui/bsp_content_widget.h"
#include "ui/preview_image_loader.h"

#include <QtCore/QTimer>
#include <QtGui/QMovie>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTabWidget>

#include <vector>

class CItemEditDialog : public QDialog {
	Q_OBJECT

public:
	enum Mode : uint8_t {
		MODE_NEW = 0,
		MODE_EDIT,
		MODE_REUPLOAD,
	};

	CItemEditDialog(const GameProfile& gameProfile, Mode mode, const WorkshopItem& item, PublishContext publishContext, CPreviewImageLoader* previewImageLoader, CPublishPipeline* publishPipeline, QWidget* parent = nullptr);

	PublishedFileId_t GetPublishedFileId() const { return _publishedFileId; }

private:
	struct AdditionalPreviewEntry {
		bool existing{false};
		uint32_t existingIndex{0};
		WorkshopPreviewAddition addition;
		bool markedForRemoval{false};
	};

	void BuildUi();
	QWidget* BuildHeader();
	QWidget* BuildContentSection();
	QWidget* CreateContentWidget();
	bool IsReplacingContent() const;
	void OnReplaceContentToggled(bool checked);
	void UpdateSummary();
	void UpdateChangeNoteHint();
	bool IsChangeNoteRequired() const;
	QWidget* BuildPreviewSection();
	QWidget* BuildAdditionalPreviewsSection();
	QWidget* BuildMetadataSection();
	QWidget* BuildTagsAndModesSection();
	void OnRequiredTagToggled();

	void ShowCurrentPreview();
	void OnCurrentPreviewLoaded(PublishedFileId_t publishedFileId, const QPixmap& pixmap);
	void ShowPreviewFile(const QString& filePath);
	void OnBrowsePreviewClicked();
	QString BrowseStartDirectory() const;
	void RememberBrowseDirectory(const QString& filePath);
	void OnFitPreviewClicked();
	void UpdatePreviewInfo();

	void OnAddPreviewImageClicked();
	void OnAddPreviewVideoClicked();
	void OnRemovePreviewClicked();
	void RefreshAdditionalPreviewsList();

	void OnTitleChanged();
	void OnDescriptionChanged();
	std::vector<std::string> CollectTags() const;
	bool TagsChanged() const;
	QStringList PageChanges() const;

	void OnSubmitClicked();
	void SubmitNewItem(WorkshopUpdateRequest request);
	void SubmitReupload(WorkshopUpdateRequest request);
	bool FillContentJob(PublishJob& job, QString& errorMessage);
	void OnAddonSelectionChanged();
	void OnMapInspected(const BspMapInfo& info);
	void ApplySuggestedTags(const std::vector<std::string>& tags);
	void OnPipelineStageChanged(const QString& stageText);
	void OnPipelinePackProgress(int percent, const QString& detail);
	void OnPipelineUploadStarted();
	void OnPipelineFinished(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement);
	void OnPipelineFailed(const QString& errorMessage);
	void OnUpdateProgressTick();
	void OnUpdateSubmitted(PublishedFileId_t publishedFileId, bool userNeedsToAcceptLegalAgreement);
	void OnUpdateFailed(PublishedFileId_t publishedFileId, const QString& errorMessage);
	void SetBusy(bool busy);
	void reject() override;

	const GameProfile& _gameProfile;
	const WorkshopItem _item;
	CWorkshopService* _workshopService{nullptr};
	CPreviewImageLoader* _previewImageLoader{nullptr};

	Mode _mode{MODE_EDIT};
	bool _isNewSubmission{false};
	PublishContext _publishContext;
	CPublishPipeline* _publishPipeline{nullptr};
	bool _contentAvailable{false};
	CAddonContentWidget* _contentWidget{nullptr};
	CBspContentWidget* _bspContentWidget{nullptr};
	QCheckBox* _replaceContentCheckBox{nullptr};
	QWidget* _contentContainer{nullptr};
	QLabel* _summaryLabel{nullptr};
	QString _defaultTitle;
	PublishedFileId_t _publishedFileId{0};

	QLabel* _previewLabel{nullptr};
	QLabel* _previewInfoLabel{nullptr};
	QComboBox* _previewSizeComboBox{nullptr};
	QPushButton* _fitPreviewButton{nullptr};
	QMovie* _previewMovie{nullptr};
	QString _selectedPreviewPath;
	QString _lastBrowseDirectory;

	QListWidget* _additionalPreviewsList{nullptr};
	QLabel* _additionalPreviewsHintLabel{nullptr};
	QPushButton* _removePreviewButton{nullptr};
	std::vector<AdditionalPreviewEntry> _additionalPreviews;

	QLineEdit* _titleEdit{nullptr};
	QLabel* _titleCounterLabel{nullptr};
	QPlainTextEdit* _descriptionEdit{nullptr};
	QLabel* _descriptionCounterLabel{nullptr};
	QTabWidget* _tabs{nullptr};
	int _pageTabIndex{-1};
	int _changeNoteTabIndex{-1};
	QLabel* _changeNoteHintLabel{nullptr};
	QPlainTextEdit* _changeNoteEdit{nullptr};
	QComboBox* _visibilityComboBox{nullptr};

	std::vector<QCheckBox*> _requiredTagCheckBoxes;
	QLabel* _requiredTagWarningLabel{nullptr};
	std::vector<QCheckBox*> _groupTagCheckBoxes;
	QLineEdit* _customTagsEdit{nullptr};

	QProgressBar* _progressBar{nullptr};
	QLabel* _statusLabel{nullptr};
	QPushButton* _submitButton{nullptr};
	QPushButton* _cancelButton{nullptr};
	QTimer _progressTimer;
	bool _busy{false};
};
