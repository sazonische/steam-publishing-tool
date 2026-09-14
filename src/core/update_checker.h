#pragma once

// Compares the running version with GitHub /releases/latest (no drafts, no prereleases). Only links to the release page.
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
