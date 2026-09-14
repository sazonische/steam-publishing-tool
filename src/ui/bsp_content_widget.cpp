#include "ui/bsp_content_widget.h"

CBspContentWidget::CBspContentWidget(std::filesystem::path defaultMapsFolder, QWidget* parent) :
	QWidget(parent),
	_defaultMapsFolder(std::move(defaultMapsFolder)) {
	auto* groupBox = new QGroupBox(tr("Content"), this);

	_pathEdit = new QLineEdit(groupBox);
	_pathEdit->setPlaceholderText(tr("Compiled map (.bsp)"));
	_browseButton = new QPushButton(tr("Browse…"), groupBox);

	_infoLabel = new QLabel(groupBox);
	_infoLabel->setObjectName("hintLabel");
	_infoLabel->setWordWrap(true);
	_infoLabel->setTextFormat(Qt::PlainText);

	_warningLabel = new QLabel(groupBox);
	_warningLabel->setWordWrap(true);
	_warningLabel->setTextFormat(Qt::PlainText);
	_warningLabel->setStyleSheet("color: #e0b050;");
	_warningLabel->setVisible(false);

	_allowWithoutPtiCheckBox = new QCheckBox(tr("Upload anyway, the map is not meant for the in-game queue"), groupBox);
	_allowWithoutPtiCheckBox->setVisible(false);

	auto* rowLayout = new QHBoxLayout();
	rowLayout->addWidget(new QLabel(tr("Map file"), groupBox));
	rowLayout->addWidget(_pathEdit, 1);
	rowLayout->addWidget(_browseButton);

	auto* groupLayout = new QVBoxLayout(groupBox);
	groupLayout->addLayout(rowLayout);
	groupLayout->addWidget(_infoLabel);
	groupLayout->addWidget(_warningLabel);
	groupLayout->addWidget(_allowWithoutPtiCheckBox);

	auto* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->addWidget(groupBox);

	connect(_browseButton, &QPushButton::clicked, this, &CBspContentWidget::OnBrowseClicked);
	connect(_pathEdit, &QLineEdit::editingFinished, this, &CBspContentWidget::OnPathEdited);

	Inspect();
}

void CBspContentWidget::SetBspPath(const std::filesystem::path& bspPath) {
	_pathEdit->setText(QDir::toNativeSeparators(QString::fromStdWString(bspPath.wstring())));
	Inspect();
}

std::filesystem::path CBspContentWidget::GetBspPath() const {
	const QString text = _pathEdit->text().trimmed();
	return text.isEmpty() ? std::filesystem::path() : std::filesystem::path(QDir::fromNativeSeparators(text).toStdWString());
}

bool CBspContentWidget::IsReadyToUpload(QString& errorMessage) const {
	if (GetBspPath().empty()) {
		errorMessage = tr("Choose the compiled .bsp file of the map.");
		return false;
	}
	if (!_mapInfo.has_value()) {
		errorMessage = tr("The map file could not be read: %1").arg(_inspectError);
		return false;
	}
	if (_mapInfo->version != BspMap::PORTAL2_BSP_VERSION) {
		errorMessage = tr("BSP version %1 is not a Portal 2 map (expected %2).").arg(_mapInfo->version).arg(BspMap::PORTAL2_BSP_VERSION);
		return false;
	}
	if (!_mapInfo->hasPtiEndRelay && !_allowWithoutPtiCheckBox->isChecked()) {
		errorMessage = tr("The map has no @relay_pti_level_end. Add the PTI end relay or tick \"Upload anyway\".");
		return false;
	}
	return true;
}

void CBspContentWidget::OnBrowseClicked() {
	QString startDirectory = _pathEdit->text().isEmpty() ? QString::fromStdWString(_defaultMapsFolder.wstring()) : QFileInfo(_pathEdit->text()).absolutePath();
	const QString filePath = QFileDialog::getOpenFileName(this, tr("Select the compiled map"), startDirectory, tr("Source maps (*.bsp);;All files (*.*)"));
	if (filePath.isEmpty()) {
		return;
	}
	SetBspPath(std::filesystem::path(filePath.toStdWString()));
}

void CBspContentWidget::OnPathEdited() {
	Inspect();
}

void CBspContentWidget::Inspect() {
	_mapInfo.reset();
	_inspectError.clear();
	_warningLabel->setVisible(false);
	_allowWithoutPtiCheckBox->setVisible(false);

	const std::filesystem::path bspPath = GetBspPath();
	if (bspPath.empty()) {
		_infoLabel->setText(tr("Pick the .bsp compiled by Hammer. Portal 2 keeps them in portal2/maps."));
		_infoLabel->setStyleSheet(QString());
		return;
	}

	std::string errorMessage;
	_mapInfo = BspMap::Inspect(bspPath, errorMessage);
	if (!_mapInfo.has_value()) {
		_inspectError = QString::fromStdString(errorMessage);
		_infoLabel->setText(tr("Cannot read the map: %1").arg(_inspectError));
		_infoLabel->setStyleSheet("color: #ff5a5a;");
		return;
	}

	const BspMapInfo& info = *_mapInfo;
	QStringList tags;
	for (const std::string& tag : info.suggestedTags) {
		tags.append(QString::fromStdString(tag));
	}
	const QString mode = info.hasCoopSpawn ? tr("Cooperative") : info.hasPlayerStart ? tr("Singleplayer") :
																					   tr("no player spawn found");
	_infoLabel->setText(tr("BSP v%1, %2, %3 entities · %4 · PTI end relay: %5 · tags: %6").arg(info.version).arg(QLocale().formattedDataSize(static_cast<qint64>(info.fileSize))).arg(info.entityCount).arg(mode).arg(info.hasPtiEndRelay ? tr("yes") : tr("no")).arg(tags.join(QStringLiteral(", "))));
	_infoLabel->setStyleSheet(info.version == BspMap::PORTAL2_BSP_VERSION ? QString() : QStringLiteral("color: #ff5a5a;"));

	if (info.version != BspMap::PORTAL2_BSP_VERSION) {
		_warningLabel->setText(tr("BSP version %1 — Portal 2 maps are version %2. Either the file is corrupt or it was compiled for another game.").arg(info.version).arg(BspMap::PORTAL2_BSP_VERSION));
		_warningLabel->setVisible(true);
	} else if (!info.hasPtiEndRelay) {
		// Without the relay the game does not count a completion from the Community Test Chambers queue.
		_warningLabel->setText(tr("No @relay_pti_level_end in the map. Players who open it from the in-game queue will not be able to finish it — add the PTI end instance in Hammer."));
		_warningLabel->setVisible(true);
		_allowWithoutPtiCheckBox->setVisible(true);
	}

	Q_EMIT MapInspected(info);
}
