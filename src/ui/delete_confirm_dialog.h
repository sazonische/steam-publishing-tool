#pragma once

#include "steam/workshop_item.h"

#include <QtGui/QPixmap>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QPushButton>

class CDeleteConfirmDialog : public QDialog {
	Q_OBJECT

public:
	CDeleteConfirmDialog(const WorkshopItem& item, const QPixmap& previewPixmap, QWidget* parent = nullptr);

private:
	void OnAcknowledgeToggled(bool checked);

	QCheckBox* _acknowledgeCheckBox{nullptr};
	QPushButton* _deleteButton{nullptr};
};
