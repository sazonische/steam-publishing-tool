#include "ui/delete_confirm_dialog.h"

namespace {

	constexpr int PREVIEW_WIDTH = 160;
	constexpr int PREVIEW_HEIGHT = 90;

} // namespace

CDeleteConfirmDialog::CDeleteConfirmDialog(const WorkshopItem& item, const QPixmap& previewPixmap, QWidget* parent) :
	QDialog(parent) {
	setWindowTitle(tr("Delete workshop item"));
	setMinimumWidth(520);

	auto* titleLabel = new QLabel(tr("Delete this item from the Steam Workshop?"), this);
	titleLabel->setObjectName("titleLabel");

	auto* previewLabel = new QLabel(this);
	previewLabel->setFixedSize(PREVIEW_WIDTH, PREVIEW_HEIGHT);
	previewLabel->setAlignment(Qt::AlignCenter);
	previewLabel->setStyleSheet("background-color: #262627; border: 1px solid #1a1a1a;");
	if (!previewPixmap.isNull()) {
		previewLabel->setPixmap(previewPixmap.scaled(previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
	}

	auto* itemLabel = new QLabel(
		tr("<b>%1</b><br>ID %2<br>%3, published %4")
			.arg(QString::fromStdString(item.title).toHtmlEscaped())
			.arg(item.publishedFileId)
			.arg(QLocale().formattedDataSize(static_cast<qint64>(item.totalFilesSize)))
			.arg(QLocale().toString(QDateTime::fromSecsSinceEpoch(item.timeCreated), QLocale::ShortFormat)),
		this
	);
	itemLabel->setTextFormat(Qt::RichText);
	itemLabel->setWordWrap(true);

	auto* itemLayout = new QHBoxLayout();
	itemLayout->setSpacing(12);
	itemLayout->addWidget(previewLabel);
	itemLayout->addWidget(itemLabel, 1);

	auto* warningLabel = new QLabel(tr("Subscribers lose access immediately, the Workshop page and its ratings, comments and statistics are gone, and the ID cannot be reused. Steam has no undo for this."), this);
	warningLabel->setWordWrap(true);
	warningLabel->setStyleSheet("color: #ff5a5a;");

	_acknowledgeCheckBox = new QCheckBox(tr("I understand this cannot be undone"), this);

	auto* cancelButton = new QPushButton(tr("Cancel"), this);
	cancelButton->setDefault(true);

	_deleteButton = new QPushButton(tr("Delete"), this);
	_deleteButton->setObjectName("dangerButton");
	_deleteButton->setEnabled(false);

	auto* buttonsLayout = new QHBoxLayout();
	buttonsLayout->addStretch();
	buttonsLayout->addWidget(cancelButton);
	buttonsLayout->addWidget(_deleteButton);

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(20, 20, 20, 20);
	mainLayout->setSpacing(12);
	mainLayout->addWidget(titleLabel);
	mainLayout->addLayout(itemLayout);
	mainLayout->addWidget(warningLabel);
	mainLayout->addWidget(_acknowledgeCheckBox);
	mainLayout->addLayout(buttonsLayout);

	connect(_acknowledgeCheckBox, &QCheckBox::toggled, this, &CDeleteConfirmDialog::OnAcknowledgeToggled);
	connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
	connect(_deleteButton, &QPushButton::clicked, this, &QDialog::accept);
}

void CDeleteConfirmDialog::OnAcknowledgeToggled(bool checked) {
	_deleteButton->setEnabled(checked);
	_deleteButton->setDefault(checked);
}
