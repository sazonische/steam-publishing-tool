#pragma once

#include "steam/workshop_item.h"

#include <QtCore/QAbstractTableModel>
#include <QtCore/QHash>
#include <QtGui/QPixmap>

#include <vector>

class CPublishedItemsModel : public QAbstractTableModel {
	Q_OBJECT

public:
	enum Column : int {
		COLUMN_PREVIEW = 0,
		COLUMN_TITLE,
		COLUMN_TAGS,
		COLUMN_DESCRIPTION,
		COLUMN_VISIBILITY,
		COLUMN_UPDATED,
		COLUMN_CREATED,
		COLUMN_SIZE,
		COLUMN_PUBLISHED_ID,
		COLUMN_COUNT
	};

	enum Role : int {
		ROLE_PUBLISHED_FILE_ID = Qt::UserRole + 1,
		ROLE_SORT_VALUE,
		ROLE_TAGS,
		ROLE_MISSING_REQUIRED_TAGS,
		ROLE_STATISTICS_TEXT,
	};

	static constexpr int PREVIEW_WIDTH = 256;
	static constexpr int PREVIEW_HEIGHT = 144;

	explicit CPublishedItemsModel(QObject* parent = nullptr);

	void SetItems(std::vector<WorkshopItem> items);
	void RemoveItem(PublishedFileId_t publishedFileId);
	void SetPreviewPixmap(PublishedFileId_t publishedFileId, const QPixmap& pixmap);
	// Tags the game needs to list the item in its in-game browser (gameinfo HighlightEntriesMissingRequiredTag).
	void SetRequiredTags(std::vector<std::string> requiredTags);

	const WorkshopItem* FindItem(PublishedFileId_t publishedFileId) const;
	const std::vector<WorkshopItem>& GetItems() const { return _items; }

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
	QVariant GetDisplayValue(const WorkshopItem& item, Column column) const;
	QVariant GetSortValue(const WorkshopItem& item, Column column) const;
	static QString ShortenDescription(const std::string& description);
	static QString FormatStatistics(const WorkshopItem& item);

	std::vector<WorkshopItem> _items;
	std::vector<std::string> _requiredTags;
	QHash<PublishedFileId_t, QPixmap> _previewByFileId;
	QPixmap _placeholderPreview;
};
