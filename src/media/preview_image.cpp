#include "media/preview_image.h"

#include "core/app_paths.h"

#include <QtCore/QBuffer>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtGui/QImage>
#include <QtGui/QImageReader>

namespace PreviewImage {

	namespace {

		constexpr int JPEG_QUALITY_START = 92;
		constexpr int JPEG_QUALITY_MIN = 35;
		constexpr int JPEG_QUALITY_STEP = 6;

	} // namespace

	const std::vector<SizePreset>& SizePresets() {
		static const std::vector<SizePreset> presets = {
			{QStringLiteral("Original size"), QSize()},
			{QStringLiteral("1920 × 1080"), QSize(1920, 1080)},
			{QStringLiteral("1280 × 720"), QSize(1280, 720)},
			{QStringLiteral("960 × 540"), QSize(960, 540)},
			{QStringLiteral("640 × 360"), QSize(640, 360)},
			{QStringLiteral("512 × 512"), QSize(512, 512)},
		};
		return presets;
	}

	bool IsAnimatedGif(const QString& filePath) {
		QImageReader reader(filePath);
		return reader.format().compare("gif", Qt::CaseInsensitive) == 0 && reader.supportsAnimation() && reader.imageCount() > 1;
	}

	QString FileFilter() {
		return QStringLiteral("Images (*.png *.jpg *.jpeg *.gif *.bmp *.webp);;All files (*.*)");
	}

	std::optional<QString> FitToLimit(const QString& sourcePath, const QSize& targetSize, qint64 maxBytes, QString& errorMessage) {
		QImageReader reader(sourcePath);
		reader.setAutoTransform(true);
		QImage image = reader.read();
		if (image.isNull()) {
			errorMessage = QStringLiteral("Cannot decode image: %1").arg(reader.errorString());
			return std::nullopt;
		}

		if (targetSize.isValid() && (image.width() > targetSize.width() || image.height() > targetSize.height())) {
			image = image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		}
		// JPEG has no alpha: Steam composites preview transparency onto a dark background anyway.
		if (image.hasAlphaChannel()) {
			QImage opaqueImage(image.size(), QImage::Format_RGB32);
			opaqueImage.fill(QColor(0x1b, 0x28, 0x38));
			QPainter painter(&opaqueImage);
			painter.drawImage(0, 0, image);
			painter.end();
			image = opaqueImage;
		}

		QByteArray encoded;
		for (int quality = JPEG_QUALITY_START; quality >= JPEG_QUALITY_MIN; quality -= JPEG_QUALITY_STEP) {
			encoded.clear();
			QBuffer buffer(&encoded);
			buffer.open(QIODevice::WriteOnly);
			if (!image.save(&buffer, "JPEG", quality)) {
				errorMessage = QStringLiteral("JPEG encoding failed");
				return std::nullopt;
			}
			if (encoded.size() < maxBytes) {
				break;
			}
		}
		if (encoded.size() >= maxBytes) {
			errorMessage = QStringLiteral("Image is still %1 at lowest quality — pick a smaller size").arg(QLocale().formattedDataSize(encoded.size()));
			return std::nullopt;
		}

		const QString outputDirectoryPath = AppPaths::TempDir();
		if (!QDir().mkpath(outputDirectoryPath)) {
			errorMessage = QStringLiteral("Cannot create temp directory %1").arg(outputDirectoryPath);
			return std::nullopt;
		}

		const QString outputPath = QStringLiteral("%1/%2_%3x%4.jpg").arg(outputDirectoryPath, QFileInfo(sourcePath).completeBaseName()).arg(image.width()).arg(image.height());
		QFile outputFile(outputPath);
		if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate) || outputFile.write(encoded) != encoded.size()) {
			errorMessage = QStringLiteral("Cannot write %1").arg(outputPath);
			return std::nullopt;
		}
		outputFile.close();

		LogMessage(LOG_INFO, "Preview fitted: %s -> %s (%lld bytes)\n", sourcePath.toUtf8().constData(), outputPath.toUtf8().constData(), static_cast<long long>(encoded.size()));
		return outputPath;
	}

} // namespace PreviewImage
