#include "ui/preview_image_loader.h"

#include "core/app_paths.h"

namespace {

	constexpr const char* PUBLISHED_FILE_ID_PROPERTY = "publishedFileId";
	constexpr qint64 DISK_CACHE_SIZE_BYTES = 256LL * 1024 * 1024;

} // namespace

CPreviewImageLoader::CPreviewImageLoader(QObject* parent) :
	QObject(parent) {
	auto* diskCache = new QNetworkDiskCache(this);
	diskCache->setCacheDirectory(AppPaths::PreviewCacheDir());
	diskCache->setMaximumCacheSize(DISK_CACHE_SIZE_BYTES);
	_networkAccessManager.setCache(diskCache);
	_networkAccessManager.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

	connect(&_networkAccessManager, &QNetworkAccessManager::finished, this, &CPreviewImageLoader::OnReplyFinished);
}

void CPreviewImageLoader::Request(PublishedFileId_t publishedFileId, const QUrl& previewUrl) {
	if (!previewUrl.isValid() || previewUrl.isEmpty()) {
		return;
	}

	const auto cachedPixmapIterator = _pixmapByFileId.constFind(publishedFileId);
	if (cachedPixmapIterator != _pixmapByFileId.constEnd()) {
		Q_EMIT PreviewLoaded(publishedFileId, cachedPixmapIterator.value());
		return;
	}

	QNetworkRequest request(previewUrl);
	request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
	QNetworkReply* reply = _networkAccessManager.get(request);
	reply->setProperty(PUBLISHED_FILE_ID_PROPERTY, QVariant::fromValue<qulonglong>(publishedFileId));
}

const QByteArray* CPreviewImageLoader::GetRawImageData(PublishedFileId_t publishedFileId) const {
	const auto rawImageDataIterator = _rawImageDataByFileId.constFind(publishedFileId);
	if (rawImageDataIterator == _rawImageDataByFileId.constEnd()) {
		return nullptr;
	}
	return &rawImageDataIterator.value();
}

void CPreviewImageLoader::OnReplyFinished(QNetworkReply* reply) {
	reply->deleteLater();

	const auto publishedFileId = static_cast<PublishedFileId_t>(reply->property(PUBLISHED_FILE_ID_PROPERTY).toULongLong());
	if (reply->error() != QNetworkReply::NoError) {
		LogMessage(LOG_WARN, "Preview download failed for %llu: %s\n", publishedFileId, reply->errorString().toUtf8().constData());
		return;
	}

	const QByteArray rawImageData = reply->readAll();
	const QImage image = QImage::fromData(rawImageData);
	if (image.isNull()) {
		LogMessage(LOG_WARN, "Preview for %llu is not a decodable image (%lld bytes)\n", publishedFileId, static_cast<long long>(rawImageData.size()));
		return;
	}

	const QPixmap pixmap = QPixmap::fromImage(image);
	_rawImageDataByFileId.insert(publishedFileId, rawImageData);
	_pixmapByFileId.insert(publishedFileId, pixmap);
	Q_EMIT PreviewLoaded(publishedFileId, pixmap);
}
