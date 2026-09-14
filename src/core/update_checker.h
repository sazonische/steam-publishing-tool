#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QNetworkAccessManager>

// Asks GitHub for the newest release of this repository and compares it with the running
// version. Only /releases/latest is used: it skips drafts and prereleases, so whatever it
// returns is something a user is meant to install. Nothing is downloaded or run — the user
// gets a link to the release page.
class CUpdateChecker : public QObject {
	Q_OBJECT

public:
	explicit CUpdateChecker(QObject* parent = nullptr);

	void Check();

	// Dotted versions compared component by component and numerically, so 1.3.10 ranks above
	// 1.3.9. Returns <0, 0 or >0 like strcmp; a missing component counts as 0.
	static int CompareVersions(const QString& left, const QString& right);
	static QString ReleasesPageUrl();

Q_SIGNALS:
	void UpdateAvailable(const QString& version, const QString& releaseUrl);
	void UpToDate(const QString& version);
	void CheckFailed(const QString& reason);

private:
	void OnReplyFinished(class QNetworkReply* reply);

	QNetworkAccessManager _network;
	bool _inFlight{false};
};
