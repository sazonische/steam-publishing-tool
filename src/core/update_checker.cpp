#include "core/update_checker.h"

namespace {

	constexpr int REQUEST_TIMEOUT_MS = 10000;

} // namespace

CUpdateChecker::CUpdateChecker(QObject* parent) :
	QObject(parent) {
}

QString CUpdateChecker::ReleasesPageUrl() {
	return QStringLiteral("https://github.com/%1/releases/latest").arg(QLatin1String(APP_GITHUB_REPOSITORY));
}

void CUpdateChecker::Check() {
	if (_inFlight) {
		return;
	}
	QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(QLatin1String(APP_GITHUB_REPOSITORY))));
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	// GitHub answers 403 to requests without a User-Agent; naming ourselves also keeps their logs honest.
	request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SteamPublishingTool/%1").arg(QCoreApplication::applicationVersion()));
	request.setRawHeader("Accept", "application/vnd.github+json");
	request.setTransferTimeout(REQUEST_TIMEOUT_MS);

	_inFlight = true;
	QNetworkReply* reply = _network.get(request);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() { OnReplyFinished(reply); });
}

void CUpdateChecker::OnReplyFinished(QNetworkReply* reply) {
	_inFlight = false;
	const QByteArray body = reply->readAll();
	const QNetworkReply::NetworkError status = reply->error();
	const QString networkError = reply->errorString();
	reply->deleteLater();

	if (status != QNetworkReply::NoError || body.isEmpty()) {
		const QString reason = status != QNetworkReply::NoError ? networkError : tr("empty response");
		LogMessage(LOG_WARN, "Update check skipped: %s\n", reason.toUtf8().constData());
		Q_EMIT CheckFailed(reason);
		return;
	}

	const QJsonObject release = QJsonDocument::fromJson(body).object();
	const QString tag = release.value(QStringLiteral("tag_name")).toString().trimmed();
	const QString latest = tag.startsWith(QLatin1Char('v'), Qt::CaseInsensitive) ? tag.mid(1) : tag;
	if (latest.isEmpty()) {
		LogMessage(LOG_WARN, "Update check skipped: the latest release carries no tag name\n");
		Q_EMIT CheckFailed(tr("the latest release has no tag name"));
		return;
	}

	const QString current = QCoreApplication::applicationVersion();
	if (CompareVersions(latest, current) <= 0) {
		LogMessage(LOG_INFO, "Update check: %s is the newest release, running %s\n", latest.toUtf8().constData(), current.toUtf8().constData());
		Q_EMIT UpToDate(latest);
		return;
	}

	QString url = release.value(QStringLiteral("html_url")).toString();
	if (url.isEmpty()) {
		url = ReleasesPageUrl();
	}
	LogMessage(LOG_INFO, "Update check: %s is available, running %s\n", latest.toUtf8().constData(), current.toUtf8().constData());
	Q_EMIT UpdateAvailable(latest, url);
}

int CUpdateChecker::CompareVersions(const QString& left, const QString& right) {
	const QStringList leftParts = left.split(QLatin1Char('.'));
	const QStringList rightParts = right.split(QLatin1Char('.'));
	const qsizetype components = std::max(leftParts.size(), rightParts.size());
	for (qsizetype index = 0; index < components; ++index) {
		const int leftValue = leftParts.value(index).toInt();
		const int rightValue = rightParts.value(index).toInt();
		if (leftValue != rightValue) {
			return leftValue < rightValue ? -1 : 1;
		}
	}
	return 0;
}
