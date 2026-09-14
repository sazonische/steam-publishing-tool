#pragma once

#include <QtCore/QDir>
#include <QtCore/QString>

namespace AppPaths {

	[[nodiscard]] inline QString AppHome() { return QDir::homePath() + "/.steam-publishing-tool"; }

	[[nodiscard]] inline QString ConfigFile() { return QDir(AppHome()).filePath("config.json"); }

	[[nodiscard]] inline QString LogDir() { return QDir(AppHome()).filePath("logs"); }

	[[nodiscard]] inline QString PreviewCacheDir() { return QDir(AppHome()).filePath("cache/previews"); }

	[[nodiscard]] inline QString TempDir() { return QDir(AppHome()).filePath("temp"); }

	// steam_api64.dll unpacked from the exe.
	[[nodiscard]] inline QString RuntimeDir() { return QDir(AppHome()).filePath("runtime"); }

} // namespace AppPaths
