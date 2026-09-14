#pragma once

// Prepares previews for the Steam Workshop limits: < 1 MB per file, any of JPG/PNG/GIF.
// GIFs are never re-encoded — that would drop the animation they are uploaded for.
namespace PreviewImage {
	struct SizePreset {
		QString title;
		QSize size; // an invalid QSize keeps the original size
	};

	const std::vector<SizePreset>& SizePresets();

	bool IsAnimatedGif(const QString& filePath);
	QString FileFilter();

	// Re-encodes the image as JPEG at targetSize (KeepAspectRatio) and lowers the quality until the
	// file fits in maxBytes. The result goes to the temp folder; nullopt — it did not fit.
	std::optional<QString> FitToLimit(const QString& sourcePath, const QSize& targetSize, qint64 maxBytes, QString& errorMessage);
} // namespace PreviewImage
