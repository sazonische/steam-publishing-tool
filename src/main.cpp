#include "core/addon_config.h"
#include "core/addon_library.h"
#include "core/app_config.h"
#include "core/app_paths.h"
#include "core/bsp_map.h"
#include "core/game_paths.h"
#include "core/game_profile.h"
#include "core/upload_rules.h"
#include "steam/steam_api_library.h"
#include "steam/steam_session.h"
#include "ui/game_select_dialog.h"
#include "ui/main_window.h"
#include "ui/paths_dialog.h"
#include "ui/theme.h"
#include "utils/file_log_sink.h"
#include "vpk/vpk_writer.h"

namespace {

	constexpr int INSTANCE_CONNECT_TIMEOUT_MS = 500;
	constexpr int INSTANCE_RACE_TIMEOUT_MS = 1000;

	// One endpoint per Windows user: a second copy of the tool would fight the first over the
	// same Steam session, so it hands over to the running one instead.
	QString InstanceServerName() {
		const QByteArray userKey = QCryptographicHash::hash(QDir::homePath().toUtf8(), QCryptographicHash::Sha256).toHex().left(16);
		return QStringLiteral("steam-publishing-tool-") + QString::fromLatin1(userKey);
	}

	bool ActivateRunningInstance(const QString& serverName, int timeoutMs) {
		QLocalSocket socket;
		socket.connectToServer(serverName, QIODevice::WriteOnly);
		if (!socket.waitForConnected(timeoutMs)) {
			return false;
		}
		socket.write("activate");
		socket.waitForBytesWritten(timeoutMs);
		socket.disconnectFromServer();
		return true;
	}

	constexpr int STEAM_CALLBACKS_INTERVAL_MS = 50;

	std::optional<std::filesystem::path> OptionalPath(const QCommandLineParser& parser, const QCommandLineOption& option) {
		if (!parser.isSet(option)) {
			return std::nullopt;
		}
		return std::filesystem::path(parser.value(option).toStdWString());
	}

	const GameProfile* ResolveGameProfile(const QCommandLineParser& parser, const QCommandLineOption& gameOption) {
		if (parser.isSet(gameOption)) {
			const std::string gameId = parser.value(gameOption).toStdString();
			const GameProfile* profile = GameProfiles::FindById(gameId);
			if (!profile) {
				LogMessage(LOG_WARN, "Unknown --game value '%s', falling back to dialog\n", gameId.c_str());
			}
			return profile;
		}

		const std::optional<std::string>& rememberedGameId = AppConfig().Data().rememberedGameId;
		if (rememberedGameId.has_value()) {
			if (const GameProfile* profile = GameProfiles::FindById(*rememberedGameId)) {
				return profile;
			}
		}

		return nullptr;
	}

	int PrintPackPreview(const GameProfile& profile, const std::string& addonName, const std::optional<std::filesystem::path>& packOutputDirectory) {
		const ResolvedGamePaths paths = GamePaths().Resolve(profile);
		if (!paths.game.valid) {
			LogMessage(LOG_ERROR, "Game folder not found: %s\n", paths.game.problem.c_str());
			return 2;
		}

		std::string errorMessage;
		const std::optional<EffectiveUploadRules> effectiveRules = UploadRules::Resolve(profile, paths.game.path, errorMessage);
		if (!effectiveRules.has_value()) {
			LogMessage(LOG_ERROR, "Cannot load addon rules: %s\n", errorMessage.c_str());
			return 2;
		}
		const AddonVpkRules& rules = effectiveRules->rules;
		LogMessage(LOG_INFO, "%s rules: %zu include, %zu exclude, required tag '%s'\n", effectiveRules->custom ? "custom" : "gameinfo", rules.includes.size(), rules.excludes.size(), rules.requiredTag.c_str());

		const std::vector<AddonInfo> addons = AddonLibrary::Enumerate(paths.addonsRoot);
		const auto addonIterator = std::ranges::find_if(addons, [&addonName](const AddonInfo& addon) { return addon.name == addonName; });
		if (addonIterator == addons.end()) {
			LogMessage(LOG_ERROR, "Addon '%s' not found in %s (%zu addons)\n", addonName.c_str(), PathText::ToUtf8(paths.addonsRoot).c_str(), addons.size());
			return 2;
		}

		const AddonManifest manifest = AddonLibrary::BuildManifest(addonIterator->directory, rules);
		if (!manifest.errorMessage.empty()) {
			LogMessage(LOG_ERROR, "Cannot read addon files: %s\n", manifest.errorMessage.c_str());
			return 2;
		}
		for (const AddonFileEntry& file : manifest.files) {
			LogMessage(LOG_INFO, "  %12llu  %s\n", static_cast<unsigned long long>(file.size), PathText::ToGenericUtf8(file.relativePath).c_str());
		}
		for (const std::filesystem::path& skipped : manifest.skippedTopLevelEntries) {
			LogMessage(LOG_INFO, "  skipped: %s\n", PathText::ToGenericUtf8(skipped).c_str());
		}
		LogMessage(LOG_INFO, "%zu files, %llu bytes; published id: %llu\n", manifest.files.size(), static_cast<unsigned long long>(manifest.totalSize), static_cast<unsigned long long>(addonIterator->publishedFileId.value_or(0)));

		if (packOutputDirectory.has_value()) {
			const std::optional<VpkWriter::WriteResult> result = VpkWriter::Write(addonIterator->directory, manifest, *packOutputDirectory, addonName, VpkWriter::WriteOptions{}, nullptr, errorMessage);
			if (!result.has_value()) {
				LogMessage(LOG_ERROR, "VPK write failed: %s\n", errorMessage.c_str());
				return 3;
			}
			LogMessage(LOG_INFO, "VPK: %u archive(s), %llu bytes written to %s\n", result->archiveCount, static_cast<unsigned long long>(result->totalBytes), PathText::ToUtf8(*packOutputDirectory).c_str());
		}
		return 0;
	}

	QString SteamInitProblemText(ESteamAPIInitResult result) {
		switch (result) {
			case k_ESteamAPIInitResult_NoSteamClient: return QObject::tr("Steam is not running.");
			case k_ESteamAPIInitResult_VersionMismatch: return QObject::tr("The Steam client is older than the Steamworks SDK built into this tool. Update Steam first.");
			default: return QObject::tr("Steam refused the connection.");
		}
	}

	int PrintBspInfo(const std::filesystem::path& bspPath) {
		std::string errorMessage;
		const std::optional<BspMapInfo> info = BspMap::Inspect(bspPath, errorMessage);
		if (!info.has_value()) {
			LogMessage(LOG_ERROR, "Cannot inspect %s: %s\n", PathText::ToUtf8(bspPath).c_str(), errorMessage.c_str());
			return 2;
		}
		std::string tags;
		for (const std::string& tag : info->suggestedTags) {
			tags += (tags.empty() ? "" : ", ") + tag;
		}
		LogMessage(LOG_INFO, "%s: BSP v%d, %llu bytes, %zu entities, player start %d, coop spawn %d, PTI end relay %d\n", PathText::ToUtf8(bspPath).c_str(), info->version, static_cast<unsigned long long>(info->fileSize), info->entityCount, info->hasPlayerStart ? 1 : 0, info->hasCoopSpawn ? 1 : 0, info->hasPtiEndRelay ? 1 : 0);
		LogMessage(LOG_INFO, "suggested tags: %s\n", tags.c_str());
		return info->version == BspMap::PORTAL2_BSP_VERSION ? 0 : 1;
	}

} // namespace

int main(int argc, char* argv[]) {
	QCoreApplication::setApplicationName("SteamPublishingTool");
	QCoreApplication::setApplicationVersion(APP_VERSION);

	QApplication application(argc, argv);
	Theme::Apply(application);

	SetLogMessagePrefix("SteamPublishingTool");
#ifdef _DEBUG
	AddLogMessageLevel(LOG_DEBUG);
#endif

	QCommandLineParser parser;
	parser.setApplicationDescription("Steam Workshop publishing tool");
	parser.addHelpOption();
	parser.addVersionOption();
	const QCommandLineOption gameOption({"g", "game"}, "Game profile id to start with (cs2, portal2).", "id");
	const QCommandLineOption editOption({"e", "edit"}, "Open the edit dialog for a published file id right after the list loads.", "publishedfileid");
	const QCommandLineOption configOption("config", "Path to config.json (default: <exe>/config.json if present, else ~/.steam-publishing-tool/config.json).", "file");
	const QCommandLineOption steamPathOption("steam-path", "Steam installation folder; overrides settings for this run only.", "dir");
	const QCommandLineOption gamePathOption("game-path", "Game installation folder; overrides settings for this run only.", "dir");
	const QCommandLineOption pathsOption("paths", "Open the Paths dialog before anything else.");
	const QCommandLineOption packPreviewOption("pack-preview", "Print the files gameinfo rules would pack for this addon folder and exit (needs --game).", "addon");
	const QCommandLineOption packOutputOption("pack-output", "With --pack-preview: also write the VPK chunks into this folder (offline, no Steam).", "dir");
	parser.addOption(gameOption);
	parser.addOption(editOption);
	parser.addOption(configOption);
	parser.addOption(steamPathOption);
	parser.addOption(gamePathOption);
	parser.addOption(pathsOption);
	parser.addOption(packPreviewOption);
	parser.addOption(packOutputOption);
	const QCommandLineOption inspectBspOption("inspect-bsp", "Print what the Portal 2 publish dialog would detect in this .bsp and exit (offline, no Steam).", "file");
	parser.addOption(inspectBspOption);
	parser.process(application);

	// Offline commands may run next to an open window; the GUI itself exists once per user.
	const bool commandLineOnly = parser.isSet(inspectBspOption) || parser.isSet(packPreviewOption);
	const QString instanceServerName = InstanceServerName();
	QLocalServer instanceServer;
	if (!commandLineOnly) {
		if (ActivateRunningInstance(instanceServerName, INSTANCE_CONNECT_TIMEOUT_MS)) {
			return 0;
		}
		// A crashed process can leave a stale registration behind; removing it after the failed
		// connection attempt is harmless.
		QLocalServer::removeServer(instanceServerName);
		if (!instanceServer.listen(instanceServerName)) {
			// Two copies racing through startup: the winner owns the endpoint, the loser hands over and exits.
			if (ActivateRunningInstance(instanceServerName, INSTANCE_RACE_TIMEOUT_MS)) {
				return 0;
			}
			QMessageBox::critical(nullptr, QObject::tr("Steam Publishing Tool"), QObject::tr("Steam Publishing Tool is already running or its instance endpoint is unavailable."));
			return 1;
		}
	}

	if (parser.isSet(configOption)) {
		AppConfig().SetConfigFilePath(std::filesystem::path(parser.value(configOption).toStdWString()));
	}
	const bool configLoadedCleanly = AppConfig().Load();

	FileLogSink::Install(std::filesystem::path((AppPaths::LogDir() + "/SteamPublishingTool.log").toStdWString()));
	LogMessage(LOG_INFO, "Version %s, config %s\n", APP_VERSION, PathText::ToUtf8(AppConfig().GetConfigFilePath()).c_str());

	if (!configLoadedCleanly) {
		QMessageBox::warning(nullptr, QObject::tr("Settings"), QObject::tr("The settings file could not be read and was moved aside. Default settings are used.\n\n%1").arg(QString::fromStdWString(AppConfig().GetConfigFilePath().wstring())));
	}

	GamePaths().SetCommandLineSteamPath(OptionalPath(parser, steamPathOption));
	GamePaths().SetCommandLineGamePath(OptionalPath(parser, gamePathOption));

	if (parser.isSet(inspectBspOption)) {
		return PrintBspInfo(std::filesystem::path(parser.value(inspectBspOption).toStdWString()));
	}

	if (parser.isSet(pathsOption)) {
		CPathsDialog pathsDialog;
		pathsDialog.exec();
	}

	const GameProfile* gameProfile = ResolveGameProfile(parser, gameOption);
	if (!gameProfile) {
		CGameSelectDialog gameSelectDialog;
		if (gameSelectDialog.exec() != QDialog::Accepted) {
			return 0;
		}
		gameProfile = gameSelectDialog.GetSelectedGame();
		if (!gameProfile) {
			return 0;
		}
		if (gameSelectDialog.ShouldRememberChoice()) {
			AppConfig().Data().rememberedGameId = std::string(gameProfile->id);
			AppConfig().Save();
		}
	}

	if (parser.isSet(packPreviewOption)) {
		return PrintPackPreview(*gameProfile, parser.value(packPreviewOption).toStdString(), OptionalPath(parser, packOutputOption));
	}

	std::string steamErrorMessage;
	if (!SteamApiLibrary::Load(steamErrorMessage)) {
		QMessageBox::critical(nullptr, QObject::tr("Steam"), QObject::tr("Could not load steam_api64.dll.\n\n%1").arg(QString::fromStdString(steamErrorMessage)));
		return 1;
	}
	while (!SteamSession().Init(gameProfile->appId, steamErrorMessage)) {
		QMessageBox box(QMessageBox::Critical, QObject::tr("Steam"), SteamInitProblemText(SteamSession().GetLastInitResult()), QMessageBox::Retry | QMessageBox::Close);
		box.setInformativeText(QObject::tr("Start the Steam client and log in with the account that owns the Workshop items, then press Retry."));
		box.setDetailedText(QString::fromStdString(steamErrorMessage));
		box.setDefaultButton(QMessageBox::Retry);
		if (box.exec() != QMessageBox::Retry) {
			return 1;
		}
	}

	QTimer steamCallbacksTimer;
	steamCallbacksTimer.setInterval(STEAM_CALLBACKS_INTERVAL_MS);
	QObject::connect(&steamCallbacksTimer, &QTimer::timeout, []() {
		SteamSession().RunCallbacks();
	});
	steamCallbacksTimer.start();

	CMainWindow mainWindow(*gameProfile);
	if (parser.isSet(editOption)) {
		mainWindow.EditItemWhenLoaded(parser.value(editOption).toULongLong());
	}
	QObject::connect(&instanceServer, &QLocalServer::newConnection, &mainWindow, [&instanceServer, &mainWindow]() {
		while (QLocalSocket* socket = instanceServer.nextPendingConnection()) {
			socket->disconnectFromServer();
			socket->deleteLater();
		}
		mainWindow.BringToFront();
	});
	mainWindow.show();

	const int exitCode = application.exec();
	SteamSession().Shutdown();
	return exitCode;
}
