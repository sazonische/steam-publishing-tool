#pragma once

namespace FileLogSink {
	bool Install(const std::filesystem::path& logFilePath);
	const std::filesystem::path& GetLogFilePath();
} // namespace FileLogSink
