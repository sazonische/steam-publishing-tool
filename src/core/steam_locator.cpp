#include "core/steam_locator.h"

#include "utils/keyvalues.h"

#include <chrono>
#include <thread>

namespace SteamLocator {

	namespace {

		constexpr int MANIFEST_READ_ATTEMPTS = 5;
		constexpr int MANIFEST_READ_RETRY_DELAY_MS = 40;

		std::filesystem::path FromQString(const QString& text) {
			return std::filesystem::path(text.toStdWString());
		}

		std::filesystem::path Normalize(const std::filesystem::path& path) {
			std::error_code errorCode;
			// weakly_canonical resolves junctions and `..` without failing on a missing tail.
			std::filesystem::path normalized = std::filesystem::weakly_canonical(path, errorCode);
			if (errorCode) {
				normalized = path.lexically_normal();
			}
			// No trailing separator: otherwise equal paths compare as different.
			if (!normalized.has_filename() && normalized.has_parent_path()) {
				normalized = normalized.parent_path();
			}
			return normalized;
		}

		std::optional<std::filesystem::path> ReadRegistryPath(const QString& registryKey, const QString& valueName) {
			const QSettings registry(registryKey, QSettings::NativeFormat);
			const QString value = registry.value(valueName).toString();
			if (value.isEmpty()) {
				return std::nullopt;
			}
			return Normalize(FromQString(value));
		}

		bool DirectoryExists(const std::filesystem::path& path) {
			std::error_code errorCode;
			return std::filesystem::is_directory(path, errorCode);
		}

		bool FileExists(const std::filesystem::path& path) {
			std::error_code errorCode;
			return std::filesystem::is_regular_file(path, errorCode);
		}

	} // namespace

	std::optional<std::filesystem::path> FindSteamInstallPath() {
		// SteamPath is written at user login and points at the client actually in use —
		// unlike InstallPath in HKLM, which is left over from the first installation.
		if (auto steamPath = ReadRegistryPath(QStringLiteral("HKEY_CURRENT_USER\\Software\\Valve\\Steam"), QStringLiteral("SteamPath"))) {
			std::string problem;
			if (IsSteamInstallPath(*steamPath, problem)) {
				return steamPath;
			}
		}
		if (auto steamPath = ReadRegistryPath(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam"), QStringLiteral("InstallPath"))) {
			std::string problem;
			if (IsSteamInstallPath(*steamPath, problem)) {
				return steamPath;
			}
		}
		return std::nullopt;
	}

	bool IsSteamInstallPath(const std::filesystem::path& steamPath, std::string& problem) {
		if (steamPath.empty()) {
			problem = "path is empty";
			return false;
		}
		if (!DirectoryExists(steamPath)) {
			problem = "directory does not exist";
			return false;
		}
		if (!FileExists(steamPath / "steam.exe")) {
			problem = "steam.exe not found here";
			return false;
		}
		if (!DirectoryExists(steamPath / "steamapps")) {
			problem = "steamapps folder not found here";
			return false;
		}
		problem.clear();
		return true;
	}

	std::vector<std::filesystem::path> FindLibraryFolders(const std::filesystem::path& steamPath) {
		std::vector<std::filesystem::path> libraries;
		auto addLibrary = [&libraries](const std::filesystem::path& libraryRoot) {
			const std::filesystem::path normalized = Normalize(libraryRoot);
			if (!DirectoryExists(normalized / "steamapps")) {
				return;
			}
			if (std::find(libraries.begin(), libraries.end(), normalized) == libraries.end()) {
				libraries.push_back(normalized);
			}
		};

		addLibrary(steamPath);

		std::string errorMessage;
		const std::optional<KeyValuesNode> root = KeyValues::ParseFile(steamPath / "steamapps" / "libraryfolders.vdf", errorMessage);
		if (!root.has_value()) {
			LogMessage(LOG_WARN, "libraryfolders.vdf: %s\n", errorMessage.c_str());
			return libraries;
		}

		const KeyValuesNode* foldersBlock = root->Find("libraryfolders");
		if (!foldersBlock) {
			return libraries;
		}
		for (const KeyValuesNode& entry : foldersBlock->children) {
			// New format: "0" { "path" "D:\\Steam" ... }; old: "1" "D:\\Steam".
			if (entry.isBlock) {
				const std::string_view path = entry.GetString("path");
				if (!path.empty()) {
					addLibrary(std::filesystem::path(QtText::FromStringView(path).toStdWString()));
				}
			} else if (!entry.value.empty() && std::all_of(entry.key.begin(), entry.key.end(), ::isdigit)) {
				addLibrary(std::filesystem::path(QtText::FromStringView(entry.value).toStdWString()));
			}
		}
		return libraries;
	}

	std::optional<std::filesystem::path> FindGameInstallPath(const std::filesystem::path& steamPath, uint32_t appId) {
		return FindGameInstallPath(steamPath, appId, std::string_view());
	}

	std::optional<std::filesystem::path> FindGameInstallPath(const std::filesystem::path& steamPath, uint32_t appId, std::string_view installFolder) {
		const std::string manifestName = std::format("appmanifest_{}.acf", appId);
		const std::vector<std::filesystem::path> libraries = FindLibraryFolders(steamPath);
		for (const std::filesystem::path& library : libraries) {
			const std::filesystem::path manifestPath = library / "steamapps" / manifestName;
			if (!FileExists(manifestPath)) {
				continue;
			}

			// Steam writes LastPlayed into this very file when our own Steamworks session starts, so the
			// first read right after SteamAPI_Init can hit the write and fail; a few short retries settle it.
			std::string errorMessage;
			std::optional<KeyValuesNode> root;
			for (int attempt = 0; attempt < MANIFEST_READ_ATTEMPTS && !root.has_value(); ++attempt) {
				if (attempt > 0) {
					std::this_thread::sleep_for(std::chrono::milliseconds(MANIFEST_READ_RETRY_DELAY_MS));
				}
				root = KeyValues::ParseFile(manifestPath, errorMessage);
			}
			if (!root.has_value()) {
				LogMessage(LOG_WARN, "%s (after %d attempts)\n", errorMessage.c_str(), MANIFEST_READ_ATTEMPTS);
				continue;
			}
			const KeyValuesNode* appState = root->Find("AppState");
			const std::string_view installDir = appState ? appState->GetString("installdir") : std::string_view();
			if (installDir.empty()) {
				continue;
			}

			const std::filesystem::path installPath = Normalize(library / "steamapps" / "common" / std::filesystem::path(QtText::FromStringView(installDir).toStdWString()));
			if (DirectoryExists(installPath)) {
				return installPath;
			}
			// The manifest is still there but the folder was deleted by hand: keep looking in other libraries.
		}

		// No readable manifest anywhere: fall back to the folder name Steam uses for this game.
		if (!installFolder.empty()) {
			for (const std::filesystem::path& library : libraries) {
				const std::filesystem::path installPath = Normalize(library / "steamapps" / "common" / installFolder);
				if (DirectoryExists(installPath)) {
					LogMessage(LOG_INFO, "AppID %u located by folder name: %s\n", appId, PathText::ToUtf8(installPath).c_str());
					return installPath;
				}
			}
		}
		return std::nullopt;
	}

} // namespace SteamLocator
