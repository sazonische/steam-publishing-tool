#include "utils/file_log_sink.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <mutex>

namespace FileLogSink {

	namespace {

		constexpr qint64 MAX_LOG_FILE_SIZE = 5LL * 1024 * 1024;

		std::filesystem::path logFilePath;
		QFile logFile;
		std::mutex logMutex;

	} // namespace

	bool Install(const std::filesystem::path& filePath) {
		const QString qtPath = QString::fromStdWString(filePath.wstring());
		if (!QDir().mkpath(QFileInfo(qtPath).absolutePath())) {
			return false;
		}

		QIODevice::OpenMode openMode = QIODevice::WriteOnly | QIODevice::Text;
		openMode |= QFileInfo(qtPath).size() > MAX_LOG_FILE_SIZE ? QIODevice::Truncate : QIODevice::Append;

		logFile.setFileName(qtPath);
		if (!logFile.open(openMode)) {
			return false;
		}
		logFilePath = filePath;

		SetLogMessageSink([](LogMessageLevel, std::string_view message) {
			const std::lock_guard<std::mutex> lock(logMutex);
			const QByteArray timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz ").toUtf8();
			logFile.write(timestamp);
			logFile.write(message.data(), static_cast<qint64>(message.size()));
			logFile.flush();
		});

		LogMessage(LOG_INFO, "---- session started ----\n");
		return true;
	}

	const std::filesystem::path& GetLogFilePath() {
		return logFilePath;
	}

} // namespace FileLogSink
