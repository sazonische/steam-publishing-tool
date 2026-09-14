#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtGui/QPixmap>
#include <QtNetwork/QNetworkAccessManager>

#include <steam/steam_api.h>

// Loads item previews by URL from the Steam CDN with a disk cache. Raw bytes are kept so an
// animated GIF can be shown through QMovie rather than as its first frame only.
class CPreviewImageLoader : public QObject {
	Q_OBJECT

public:
	explicit CPreviewImageLoader(QObject* parent = nullptr);

	void Request(PublishedFileId_t publishedFileId, const QUrl& previewUrl);
	const QByteArray* GetRawImageData(PublishedFileId_t publishedFileId) const;

Q_SIGNALS:
	void PreviewLoaded(PublishedFileId_t publishedFileId, const QPixmap& pixmap);

private:
	void OnReplyFinished(QNetworkReply* reply);

	QNetworkAccessManager _networkAccessManager;
	QHash<PublishedFileId_t, QByteArray> _rawImageDataByFileId;
	QHash<PublishedFileId_t, QPixmap> _pixmapByFileId;
};
