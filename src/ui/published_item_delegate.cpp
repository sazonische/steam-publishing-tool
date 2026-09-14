#include "ui/published_item_delegate.h"

#include "ui/published_items_model.h"

#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtWidgets/QApplication>

namespace {

	constexpr int ROW_PADDING_LEFT = 16;
	constexpr int ROW_PADDING_RIGHT = 20;
	constexpr int ROW_PADDING_Y = 12;
	constexpr int TITLE_TO_DESCRIPTION_GAP = 7;
	constexpr int DESCRIPTION_TO_TAGS_GAP = 15;
	constexpr int TAG_PADDING_X = 16;
	constexpr int TAG_PADDING_Y = 6;
	constexpr int TAG_SPACING = 6;
	constexpr int MAX_VISIBLE_TAGS = 3;
	constexpr int PREVIEW_TO_TEXT_GAP = 17;
	constexpr int PREVIEW_ASPECT_WIDTH = 16;
	constexpr int PREVIEW_ASPECT_HEIGHT = 9;
	const QColor WARNING_COLOR(0xe0, 0xb0, 0x50);

	QFont TitleFont(const QFont& baseFont) {
		QFont titleFont(baseFont);
		titleFont.setBold(true);
		titleFont.setPointSizeF(baseFont.pointSizeF() + 1);
		return titleFont;
	}

	int TextBlockHeight(const QFont& baseFont) {
		const int lineHeight = QFontMetrics(baseFont).height();
		return QFontMetrics(TitleFont(baseFont)).height() + TITLE_TO_DESCRIPTION_GAP + lineHeight + DESCRIPTION_TO_TAGS_GAP + lineHeight + TAG_PADDING_Y;
	}

} // namespace

CPublishedItemDelegate::CPublishedItemDelegate(QObject* parent) :
	QStyledItemDelegate(parent) {
}

QSize CPublishedItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
	Q_UNUSED(index);
	return QSize(420, TextBlockHeight(option.font) + ROW_PADDING_Y * 2);
}

void CPublishedItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
	QStyleOptionViewItem itemOption(option);
	initStyleOption(&itemOption, index);
	itemOption.text.clear();
	itemOption.icon = QIcon();
	const QStyle* style = option.widget ? option.widget->style() : QApplication::style();
	style->drawControl(QStyle::CE_ItemViewItem, &itemOption, painter, option.widget);

	painter->save();
	painter->setClipRect(option.rect);
	painter->setRenderHint(QPainter::Antialiasing);
	painter->setRenderHint(QPainter::SmoothPixmapTransform);
	const bool selected = option.state.testFlag(QStyle::State_Selected);
	const QColor textColor = option.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
	const QColor titleColor = option.palette.color(selected ? QPalette::HighlightedText : QPalette::BrightText);
	const QRect contentRect = option.rect.adjusted(ROW_PADDING_LEFT, ROW_PADDING_Y, -ROW_PADDING_RIGHT, -ROW_PADDING_Y);

	const int blockHeight = TextBlockHeight(option.font);
	const QRect previewRect(contentRect.left(), contentRect.top(), blockHeight * PREVIEW_ASPECT_WIDTH / PREVIEW_ASPECT_HEIGHT, blockHeight);
	painter->fillRect(previewRect, option.palette.brush(QPalette::Base));
	const QPixmap previewPixmap = index.siblingAtColumn(CPublishedItemsModel::COLUMN_PREVIEW).data(Qt::DecorationRole).value<QPixmap>();
	if (!previewPixmap.isNull()) {
		QRect targetRect(QPoint(0, 0), previewPixmap.size().scaled(previewRect.size(), Qt::KeepAspectRatio));
		targetRect.moveCenter(previewRect.center());
		painter->drawPixmap(targetRect, previewPixmap);
	}

	const int textLeft = previewRect.right() + PREVIEW_TO_TEXT_GAP;
	const int textWidth = std::max(0, contentRect.right() - textLeft + 1);
	const QFont titleFont = TitleFont(option.font);
	const QFontMetrics titleMetrics(titleFont);
	const QFontMetrics textMetrics(option.font);
	const int lineHeight = textMetrics.height();
	painter->setFont(titleFont);
	painter->setPen(titleColor);
	painter->drawText(QRect(textLeft, contentRect.top(), textWidth, titleMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter, titleMetrics.elidedText(index.data().toString(), Qt::ElideRight, textWidth));

	painter->setFont(option.font);
	painter->setPen(textColor);
	const QString description = index.siblingAtColumn(CPublishedItemsModel::COLUMN_DESCRIPTION).data().toString();
	const int descriptionTop = contentRect.top() + titleMetrics.height() + TITLE_TO_DESCRIPTION_GAP;
	painter->drawText(QRect(textLeft, descriptionTop, textWidth, lineHeight), Qt::AlignLeft | Qt::AlignVCenter, textMetrics.elidedText(description, Qt::ElideRight, textWidth));

	const QStringList tags = index.data(CPublishedItemsModel::ROLE_TAGS).toStringList();
	const int tagTop = descriptionTop + lineHeight + DESCRIPTION_TO_TAGS_GAP;
	const int tagHeight = lineHeight + TAG_PADDING_Y;
	int tagLeft = textLeft;
	for (qsizetype tagIndex = 0; tagIndex < tags.size(); ++tagIndex) {
		const int remaining = static_cast<int>(tags.size() - tagIndex);
		const QString overflowText = QStringLiteral("+%1").arg(remaining);
		const int overflowWidth = textMetrics.horizontalAdvance(overflowText) + TAG_PADDING_X;
		const int availableWidth = contentRect.right() - tagLeft + 1;
		const int tagWidth = textMetrics.horizontalAdvance(tags[tagIndex]) + TAG_PADDING_X;
		const bool overflow = tagIndex >= MAX_VISIBLE_TAGS || tagWidth + (remaining > 1 ? overflowWidth + TAG_SPACING : 0) > availableWidth;
		const int width = overflow ? overflowWidth : tagWidth;
		if (width > availableWidth) {
			break;
		}
		const QRect tagRect(tagLeft, tagTop, width, tagHeight);
		painter->setPen(Qt::NoPen);
		painter->setBrush(option.palette.brush(QPalette::AlternateBase));
		painter->drawRoundedRect(tagRect, 3, 3);
		painter->setPen(option.palette.color(QPalette::Text));
		painter->drawText(tagRect, Qt::AlignCenter, overflow ? overflowText : tags[tagIndex]);
		tagLeft += width + TAG_SPACING;
		if (overflow) {
			break;
		}
	}

	const QString statistics = index.data(CPublishedItemsModel::ROLE_STATISTICS_TEXT).toString();
	const int statisticsWidth = textMetrics.horizontalAdvance(statistics);
	int statisticsLeft = contentRect.right() + 1 - statisticsWidth;
	if (!statistics.isEmpty() && statisticsLeft >= tagLeft + TAG_PADDING_X * 2) {
		QColor statisticsColor = textColor;
		statisticsColor.setAlpha(170);
		painter->setPen(statisticsColor);
		painter->drawText(QRect(statisticsLeft, tagTop, statisticsWidth, tagHeight), Qt::AlignRight | Qt::AlignVCenter, statistics);
	} else {
		statisticsLeft = contentRect.right() + 1;
	}

	// The same highlight cs2_workshop_manager gives entries without the game's required tag:
	// the in-game browser will not list this map until the tag is back.
	const QStringList missingTags = index.data(CPublishedItemsModel::ROLE_MISSING_REQUIRED_TAGS).toStringList();
	if (!missingTags.isEmpty()) {
		const QString warningText = tr("Missing tag: %1").arg(missingTags.join(QStringLiteral(", ")));
		const int warningWidth = textMetrics.horizontalAdvance(warningText) + TAG_PADDING_X;
		if (tagLeft + warningWidth <= statisticsLeft - TAG_SPACING) {
			const QRect warningRect(tagLeft, tagTop, warningWidth, tagHeight);
			painter->setPen(QPen(WARNING_COLOR, 1));
			painter->setBrush(Qt::NoBrush);
			painter->drawRoundedRect(warningRect.adjusted(0, 0, -1, -1), 3, 3);
			painter->drawText(warningRect, Qt::AlignCenter, warningText);
		}
	}
	painter->restore();
}
