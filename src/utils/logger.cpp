#include "logger.h"

#include <array>
#include <cstdarg>
#include <cstdio>
#include <format>

#ifdef _WIN32
	#include <Windows.h>
	#include <intrin.h>
#endif

std::string logMessagePrefix;
LogMessageLevel logMessageLevel = LOG_DEFAULT;

namespace {

	const std::array<const char*, 6> logMessageLevelNames = {
		"[INFO] ",
		"[WARN] ",
		"[DEBUG] ",
		"[ERROR] ",
		"[TRACE] ",
		"[CRITICAL] "
	};

	LogMessageSink logMessageSink;

	uint8_t GetLogMessageLevelIndex(LogMessageLevel level) {
#ifdef _MSC_VER
		unsigned long index;
		_BitScanForward(&index, static_cast<unsigned long>(level));
		return static_cast<uint8_t>(index);
#else
		return __builtin_ctz(level);
#endif
	}

} // namespace

void SetLogMessageLevel(LogMessageLevel level) {
	logMessageLevel = level;
}

void AddLogMessageLevel(LogMessageLevel level) {
	logMessageLevel |= level;
}

void RemoveLogMessageLevel(LogMessageLevel level) {
	logMessageLevel &= ~level;
}

void SetLogMessagePrefix(const std::string& prefix) {
	if (!prefix.empty()) {
		logMessagePrefix = std::format("[{}] ", prefix);
	} else {
		logMessagePrefix.clear();
	}
}

void SetLogMessageSink(LogMessageSink sink) {
	logMessageSink = std::move(sink);
}

void LogMessage(LogMessageLevel level, const char* format, ...) {
	if (!(logMessageLevel & level) || !format) {
		return;
	}

	std::array<char, 4096> buffer{};

	va_list args;
	va_start(args, format);
	vsnprintf(buffer.data(), buffer.size(), format, args);
	va_end(args);

	const uint8_t index = GetLogMessageLevelIndex(level);
	const std::string line = std::format("{}{}{}", logMessagePrefix, logMessageLevelNames[index], buffer.data());

	fputs(line.c_str(), stderr);
#ifdef _WIN32
	OutputDebugStringA(line.c_str());
#endif

	if (logMessageSink) {
		logMessageSink(level, line);
	}
}
