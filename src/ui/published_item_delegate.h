#pragma once

#include <QtWidgets/QStyledItemDelegate>

class CPublishedItemDelegate : public QStyledItemDelegate {
public:
	explicit CPublishedItemDelegate(QObject* parent = nullptr);

	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
