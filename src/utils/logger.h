#pragma once

/**
 * LogMessageLevel defines the severity and purpose of each log message type.
 * These values are bit flags, so they can be combined using bitwise OR.
 *
 * LOG_NONE     — disables all logging.
 * LOG_INFO     — general informational messages about normal operation.
 * LOG_WARN     — abnormal or unexpected events that are not fatal.
 * LOG_DEBUG    — debugging information useful during development and testing.
 * LOG_ERROR    — an error occurred; the operation failed but the application continues.
 * LOG_TRACE    — extremely detailed information; function-level tracing.
 * LOG_CRITICAL — unrecoverable errors that threaten the application's ability to continue.
 * LOG_ALL      — enables all logging.
 * LOG_DEFAULT  — enables all logging except for LOG_TRACE and LOG_DEBUG.
 */
enum LogMessageLevel : uint8_t {
	LOG_NONE = 0,
	LOG_INFO = 1 << 0,
	LOG_WARN = 1 << 1,
	LOG_DEBUG = 1 << 2,
	LOG_ERROR = 1 << 3,
	LOG_TRACE = 1 << 4,
	LOG_CRITICAL = 1 << 5,

	LOG_ALL = LOG_INFO | LOG_WARN | LOG_DEBUG | LOG_ERROR | LOG_TRACE | LOG_CRITICAL,
	LOG_DEFAULT = LOG_INFO | LOG_WARN | LOG_ERROR | LOG_CRITICAL
};

constexpr LogMessageLevel operator|(LogMessageLevel lhs, LogMessageLevel rhs) {
	return static_cast<LogMessageLevel>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

constexpr LogMessageLevel operator&(LogMessageLevel lhs, LogMessageLevel rhs) {
	return static_cast<LogMessageLevel>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

constexpr LogMessageLevel operator~(LogMessageLevel val) {
	return static_cast<LogMessageLevel>(~static_cast<uint8_t>(val));
}

constexpr LogMessageLevel& operator|=(LogMessageLevel& lhs, LogMessageLevel rhs) {
	return lhs = lhs | rhs;
}

constexpr LogMessageLevel& operator&=(LogMessageLevel& lhs, LogMessageLevel rhs) {
	return lhs = lhs & rhs;
}

using LogMessageSink = std::function<void(LogMessageLevel level, std::string_view message)>;

extern std::string logMessagePrefix;
extern LogMessageLevel logMessageLevel;

void SetLogMessageLevel(LogMessageLevel level);
void AddLogMessageLevel(LogMessageLevel level);
void RemoveLogMessageLevel(LogMessageLevel level);

/**
 * Sets a prefix that is prepended to all log messages. The prefix is formatted
 * as "[<prefix>] ". If the prefix string is empty, the prefix is cleared.
 */
void SetLogMessagePrefix(const std::string& prefix);

/**
 * Sets an additional sink that receives every logged message after it was written
 * to stderr and the debugger output. Pass an empty function to remove the sink.
 */
void SetLogMessageSink(LogMessageSink sink);

/**
 * Logs a formatted message at the specified log level using printf-style
 * formatting. The message is prefixed with the current logMessagePrefix and
 * the textual name of the log level (e.g., "[INFO] ").
 */
void LogMessage(LogMessageLevel level, const char* format, ...);
