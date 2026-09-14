#include "steam/steam_api_library.h"

#include "core/app_paths.h"

#include <Windows.h>

namespace SteamApiLibrary {

	namespace {

		constexpr const wchar_t* STEAM_API_DLL_NAME = L"steam_api64.dll";
		constexpr const wchar_t* STEAM_API_RESOURCE_NAME = L"STEAM_API_DLL";

		std::filesystem::path loadedPath;

		std::string FormatLastError(const char* what) {
			const DWORD errorCode = GetLastError();
			return std::format("{} failed (Win32 error {})", what, errorCode);
		}

		bool ReadEmbeddedDll(QByteArray& dllBytes, std::string& errorMessage) {
			const HMODULE moduleHandle = GetModuleHandleW(nullptr);
			const HRSRC resourceHandle = FindResourceW(moduleHandle, STEAM_API_RESOURCE_NAME, RT_RCDATA);
			if (!resourceHandle) {
				errorMessage = FormatLastError("FindResource(STEAM_API_DLL)");
				return false;
			}
			const HGLOBAL resourceData = LoadResource(moduleHandle, resourceHandle);
			const DWORD resourceSize = SizeofResource(moduleHandle, resourceHandle);
			const void* resourceBytes = resourceData ? LockResource(resourceData) : nullptr;
			if (!resourceBytes || resourceSize == 0) {
				errorMessage = FormatLastError("LoadResource(STEAM_API_DLL)");
				return false;
			}
			dllBytes = QByteArray(static_cast<const char*>(resourceBytes), static_cast<qsizetype>(resourceSize));
			return true;
		}

		// Written through QSaveFile: a second instance of the tool may be reading the file right now,
		// and a half-written DLL on disk is worse than the old complete one.
		bool EnsureExtractedDll(const QString& targetPath, const QByteArray& dllBytes, std::string& errorMessage) {
			QFile existingFile(targetPath);
			if (existingFile.exists() && existingFile.size() == dllBytes.size() && existingFile.open(QIODevice::ReadOnly)) {
				if (existingFile.readAll() == dllBytes) {
					return true;
				}
				existingFile.close();
			}

			if (!QDir().mkpath(QFileInfo(targetPath).absolutePath())) {
				errorMessage = std::format("cannot create directory for {}", targetPath.toStdString());
				return false;
			}

			QSaveFile saveFile(targetPath);
			if (!saveFile.open(QIODevice::WriteOnly) || saveFile.write(dllBytes) != dllBytes.size() || !saveFile.commit()) {
				// The file may be locked by another running instance — then the old copy works anyway.
				if (existingFile.exists()) {
					LogMessage(LOG_WARN, "Cannot refresh %s (%s), using the existing copy\n", targetPath.toUtf8().constData(), saveFile.errorString().toUtf8().constData());
					return true;
				}
				errorMessage = std::format("cannot write {}: {}", targetPath.toStdString(), saveFile.errorString().toStdString());
				return false;
			}
			LogMessage(LOG_INFO, "Extracted embedded steam_api64.dll to %s\n", targetPath.toUtf8().constData());
			return true;
		}

		bool LoadFrom(const std::filesystem::path& dllPath, std::string& errorMessage) {
			// LOAD_WITH_ALTERED_SEARCH_PATH: the DLL's dependencies are searched next to it, not next to the exe.
			const HMODULE dllHandle = LoadLibraryExW(dllPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
			if (!dllHandle) {
				errorMessage = FormatLastError(std::format("LoadLibrary({})", PathText::ToUtf8(dllPath)).c_str());
				return false;
			}
			loadedPath = dllPath;
			LogMessage(LOG_INFO, "steam_api64.dll loaded from %s\n", PathText::ToUtf8(dllPath).c_str());
			return true;
		}

	} // namespace

	bool Load(std::string& errorMessage) {
		if (!loadedPath.empty()) {
			return true;
		}

		const std::filesystem::path besideExecutable = std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString()) / STEAM_API_DLL_NAME;
		std::error_code errorCode;
		if (std::filesystem::is_regular_file(besideExecutable, errorCode)) {
			return LoadFrom(besideExecutable, errorMessage);
		}

		QByteArray dllBytes;
		if (!ReadEmbeddedDll(dllBytes, errorMessage)) {
			return false;
		}

		const QString extractedPath = AppPaths::RuntimeDir() + "/" + QString::fromWCharArray(STEAM_API_DLL_NAME);
		if (!EnsureExtractedDll(extractedPath, dllBytes, errorMessage)) {
			return false;
		}
		return LoadFrom(std::filesystem::path(extractedPath.toStdWString()), errorMessage);
	}

	const std::filesystem::path& GetLoadedPath() {
		return loadedPath;
	}

} // namespace SteamApiLibrary
