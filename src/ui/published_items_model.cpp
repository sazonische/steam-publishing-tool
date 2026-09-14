#include "ui/published_items_model.h"

#include "core/required_tags.h"

#include "steam/steam_names.h"

namespace {

	constexpr int DESCRIPTION_PREVIEW_MAX_CHARS = 600;

	QPixmap BuildPlaceholderPreview(int width, int height) {
		QPixmap pixmap(width, height);
		pixmap.fill(QColor(38, 38, 39));

		QPainter painter(&pixmap);
		QFont font = painter.font();
		font.setPointSizeF(font.pointSizeF() * 1.8);
		painter.setFont(font);
		painter.setPen(QColor(152, 152, 153));
		painter.drawRect(0, 0, width - 1, height - 1);
		painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("no preview"));
		return pixmap;
	}

	QString FormatTimestamp(uint32_t unixTime) {
		if (unixTime == 0) {
			return {};
		}
		return QLocale().toString(QDateTime::fromSecsSinceEpoch(unixTime).date(), QLocale::ShortFormat);
	}

} // namespace

CPublishedItemsModel::CPublishedItemsModel(QObject* parent) :
	QAbstractTableModel(parent),
	_placeholderPreview(BuildPlaceholderPreview(PREVIEW_WIDTH, PREVIEW_HEIGHT)) {
}

void CPublishedItemsModel::SetItems(std::vector<WorkshopItem> items) {
	beginResetModel();
	_items = std::move(items);
	endResetModel();
}

void CPublishedItemsModel::RemoveItem(PublishedFileId_t publishedFileId) {
	for (size_t row = 0; row < _items.size(); ++row) {
		if (_items[row].publishedFileId != publishedFileId) {
			continue;
		}
		beginRemoveRows(QModelIndex(), static_cast<int>(row), static_cast<int>(row));
		_items.erase(_items.begin() + static_cast<std::ptrdiff_t>(row));
		_previewByFileId.remove(publishedFileId);
		endRemoveRows();
		return;
	}
}

void CPublishedItemsModel::SetRequiredTags(std::vector<std::string> requiredTags) {
	_requiredTags = std::move(requiredTags);
	if (!_items.empty()) {
		Q_EMIT dataChanged(index(0, COLUMN_TITLE), index(static_cast<int>(_items.size()) - 1, COLUMN_TITLE), {ROLE_MISSING_REQUIRED_TAGS, Qt::ToolTipRole});
	}
}

void CPublishedItemsModel::SetPreviewPixmap(PublishedFileId_t publishedFileId, const QPixmap& pixmap) {
	_previewByFileId.insert(publishedFileId, pixmap.scaled(PREVIEW_WIDTH, PREVIEW_HEIGHT, Qt::KeepAspectRatio, Qt::SmoothTransformation));

	for (size_t row = 0; row < _items.size(); ++row) {
		if (_items[row].publishedFileId == publishedFileId) {
			const QModelIndex previewIndex = index(static_cast<int>(row), COLUMN_PREVIEW);
			Q_EMIT dataChanged(previewIndex, previewIndex, {Qt::DecorationRole});
			const QModelIndex titleIndex = index(static_cast<int>(row), COLUMN_TITLE);
			Q_EMIT dataChanged(titleIndex, titleIndex, {Qt::DecorationRole});
			return;
		}
	}
}

const WorkshopItem* CPublishedItemsModel::FindItem(PublishedFileId_t publishedFileId) const {
	for (const WorkshopItem& item : _items) {
		if (item.publishedFileId == publishedFileId) {
			return &item;
		}
	}
	return nullptr;
}

int CPublishedItemsModel::rowCount(const QModelIndex& parent) const {
	if (parent.isValid()) {
		return 0;
	}
	return static_cast<int>(_items.size());
}

int CPublishedItemsModel::columnCount(const QModelIndex& parent) const {
	if (parent.isValid()) {
		return 0;
	}
	return COLUMN_COUNT;
}

QVariant CPublishedItemsModel::data(const QModelIndex& index, int role) const {
	if (!index.isValid() || index.row() < 0 || static_cast<size_t>(index.row()) >= _items.size()) {
		return {};
	}

	const WorkshopItem& item = _items[static_cast<size_t>(index.row())];
	const auto column = static_cast<Column>(index.column());

	switch (role) {
		case Qt::DisplayRole:
			return GetDisplayValue(item, column);
		case Qt::DecorationRole: {
			if (column != COLUMN_PREVIEW) {
				return {};
			}
			const auto previewIterator = _previewByFileId.constFind(item.publishedFileId);
			return previewIterator != _previewByFileId.constEnd() ? previewIterator.value() : _placeholderPreview;
		}
		case Qt::ToolTipRole:
			if (column == COLUMN_TITLE) {
				QString toolTip = QStringLiteral("<b>%1</b><br>%2<br><br>%3<br>%4<br>%5")
									  .arg(QString::fromStdString(item.title).toHtmlEscaped(), ShortenDescription(item.description).toHtmlEscaped(), tr("Tags: %1").arg(GetDisplayValue(item, COLUMN_TAGS).toString()).toHtmlEscaped(), tr("Created: %1").arg(FormatTimestamp(item.timeCreated)).toHtmlEscaped(), tr("Published ID: %1").arg(item.publishedFileId));
				toolTip += QStringLiteral("<br>%1").arg(FormatStatistics(item).toHtmlEscaped());
				const std::vector<std::string> missingTags = RequiredTags::Missing(_requiredTags, item.tags);
				if (!missingTags.empty()) {
					QStringList missing;
					for (const std::string& tag : missingTags) {
						missing.append(QString::fromStdString(tag));
					}
					toolTip += QStringLiteral("<br><br><span style=\"color:#e0b050\">%1</span>").arg(tr("Missing required tags: %1. The game hides the item from its in-game browser until they are added (Edit → Tags).").arg(missing.join(QStringLiteral(", "))).toHtmlEscaped());
				}
				return toolTip;
			}
			if (column == COLUMN_UPDATED || column == COLUMN_CREATED) {
				const uint32_t timestamp = column == COLUMN_UPDATED ? item.timeUpdated : item.timeCreated;
				return timestamp ? QLocale().toString(QDateTime::fromSecsSinceEpoch(timestamp), QLocale::LongFormat) : QString();
			}
			if (column == COLUMN_DESCRIPTION) {
				return QString::fromStdString(item.description);
			}
			if (column == COLUMN_TAGS) {
				return GetDisplayValue(item, column);
			}
			return {};
		case Qt::TextAlignmentRole:
			if (column == COLUMN_UPDATED || column == COLUMN_CREATED || column == COLUMN_SIZE || column == COLUMN_PUBLISHED_ID || column == COLUMN_VISIBILITY) {
				return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
			}
			return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
		case ROLE_PUBLISHED_FILE_ID:
			return QVariant::fromValue<qulonglong>(item.publishedFileId);
		case ROLE_SORT_VALUE:
			return GetSortValue(item, column);
		case ROLE_TAGS: {
			QStringList tags;
			for (const std::string& tag : item.tags) {
				tags.append(QString::fromStdString(tag));
			}
			return tags;
		}
		case ROLE_STATISTICS_TEXT:
			return FormatStatistics(item);
		case ROLE_MISSING_REQUIRED_TAGS: {
			QStringList missing;
			for (const std::string& tag : RequiredTags::Missing(_requiredTags, item.tags)) {
				missing.append(QString::fromStdString(tag));
			}
			return missing;
		}
		default:
			return {};
	}
}

QVariant CPublishedItemsModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
		return {};
	}

	switch (static_cast<Column>(section)) {
		case COLUMN_PREVIEW: return QString();
		case COLUMN_TITLE: return tr("Workshop item");
		case COLUMN_TAGS: return tr("Tags");
		case COLUMN_DESCRIPTION: return tr("Description");
		case COLUMN_VISIBILITY: return tr("Visibility");
		case COLUMN_UPDATED: return tr("Last Updated");
		case COLUMN_CREATED: return tr("Date Created");
		case COLUMN_SIZE: return tr("Size");
		case COLUMN_PUBLISHED_ID: return tr("Published ID");
		default: return {};
	}
}

QVariant CPublishedItemsModel::GetDisplayValue(const WorkshopItem& item, Column column) const {
	switch (column) {
		case COLUMN_TITLE:
			return QString::fromStdString(item.title);
		case COLUMN_TAGS: {
			QStringList tags;
			tags.reserve(static_cast<qsizetype>(item.tags.size()));
			for (const std::string& tag : item.tags) {
				tags.append(QString::fromStdString(tag));
			}
			return tags.join(QStringLiteral(", "));
		}
		case COLUMN_DESCRIPTION:
			return ShortenDescription(item.description);
		case COLUMN_VISIBILITY:
			return QString::fromUtf8(SteamNames::Visibility(item.visibility).data());
		case COLUMN_UPDATED:
			return FormatTimestamp(item.timeUpdated);
		case COLUMN_CREATED:
			return FormatTimestamp(item.timeCreated);
		case COLUMN_SIZE:
			return QLocale().formattedDataSize(static_cast<qint64>(item.totalFilesSize), 2, QLocale::DataSizeTraditionalFormat);
		case COLUMN_PUBLISHED_ID:
			return QString::number(item.publishedFileId);
		default:
			return {};
	}
}

QVariant CPublishedItemsModel::GetSortValue(const WorkshopItem& item, Column column) const {
	switch (column) {
		case COLUMN_UPDATED: return QVariant::fromValue<uint>(item.timeUpdated);
		case COLUMN_CREATED: return QVariant::fromValue<uint>(item.timeCreated);
		case COLUMN_SIZE: return QVariant::fromValue<qulonglong>(item.totalFilesSize);
		case COLUMN_PUBLISHED_ID: return QVariant::fromValue<qulonglong>(item.publishedFileId);
		case COLUMN_VISIBILITY: return static_cast<int>(item.visibility);
		default: return GetDisplayValue(item, column);
	}
}

QString CPublishedItemsModel::FormatStatistics(const WorkshopItem& item) {
	const QLocale locale;
	const auto count = [&locale](uint64_t value, const QString& singular, const QString& plural) {
		return QStringLiteral("%1 %2").arg(locale.toString(static_cast<qulonglong>(value)), value == 1 ? singular : plural);
	};
	return QStringLiteral("%1 · %2 · %3 · %4").arg(count(item.subscriptions, tr("subscriber"), tr("subscribers")), count(item.favorites, tr("favorite"), tr("favorites")), count(item.websiteViews, tr("view"), tr("views")), tr("%1 up / %2 down").arg(locale.toString(item.votesUp), locale.toString(item.votesDown)));
}

QString CPublishedItemsModel::ShortenDescription(const std::string& description) {
	QString text = QString::fromStdString(description).trimmed();

	// The list shows plain text; the original Steam BBCode stays in the editor.
	static const QRegularExpression mediaExpression(QStringLiteral(R"(\[(img|previewyoutube)(?:=[^\]]*)?\][\s\S]*?\[/\1\])"), QRegularExpression::CaseInsensitiveOption);
	static const QRegularExpression markupExpression(QStringLiteral(R"(\[/?(?:h[1-6]|b|i|u|strike|spoiler|noparse|url|quote|code|list|olist|table|tr|td|th|hr|\*)(?:=[^\]]*)?\])"), QRegularExpression::CaseInsensitiveOption);
	text.remove(mediaExpression);
	text.replace(markupExpression, QStringLiteral(" "));
	text = text.simplified();
	if (text.size() > DESCRIPTION_PREVIEW_MAX_CHARS) {
		text = text.left(DESCRIPTION_PREVIEW_MAX_CHARS) + QStringLiteral("…");
	}
	return text;
}
