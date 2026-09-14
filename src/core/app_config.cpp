#include "core/app_config.h"

#include "core/app_paths.h"

#include <glaze/glaze.hpp>

namespace {

	constexpr const char* CONFIG_FILE_NAME = "config.json";

	QString ToQString(const std::filesystem::path& path) {
		return QString::fromStdWString(path.wstring());
	}

} // namespace

void CAppConfig::SetConfigFilePath(std::filesystem::path configFilePath) {
	_configFilePath = std::move(configFilePath);
}

void CAppConfig::ResolveDefaultConfigFilePath() {
	if (!_configFilePath.empty()) {
		return;
	}

	const QString portableConfigPath = QCoreApplication::applicationDirPath() + "/" + CONFIG_FILE_NAME;
	if (QFile::exists(portableConfigPath)) {
		_configFilePath = std::filesystem::path(portableConfigPath.toStdWString());
		return;
	}

	_configFilePath = std::filesystem::path(AppPaths::ConfigFile().toStdWString());
}

bool CAppConfig::Load() {
	ResolveDefaultConfigFilePath();
	_data = {};

	QFile configFile(ToQString(_configFilePath));
	if (!configFile.exists()) {
		LogMessage(LOG_INFO, "Config %s not found, using defaults\n", PathText::ToUtf8(_configFilePath).c_str());
		return true;
	}
	if (!configFile.open(QIODevice::ReadOnly)) {
		LogMessage(LOG_WARN, "Cannot read config %s: %s\n", PathText::ToUtf8(_configFilePath).c_str(), configFile.errorString().toUtf8().constData());
		return false;
	}

	const QByteArray fileBytes = configFile.readAll();
	const std::string_view json(fileBytes.constData(), static_cast<size_t>(fileBytes.size()));

	// Unknown keys are not an error: a config from a newer version of the tool must still load.
	static constexpr auto readOptions = glz::opts{
		.comments = true,
		.error_on_unknown_keys = false
	};

	AppConfigData loadedData;
	if (auto errorCode = glz::read<readOptions>(loadedData, json); errorCode) {
		const std::string errorText = glz::format_error(errorCode, json);
		LogMessage(LOG_WARN, "Config %s is broken: %s\n", PathText::ToUtf8(_configFilePath).c_str(), errorText.c_str());
		configFile.close();

		const QString brokenPath = ToQString(_configFilePath) + ".broken-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
		if (QFile::rename(ToQString(_configFilePath), brokenPath)) {
			LogMessage(LOG_WARN, "Broken config moved to %s\n", brokenPath.toUtf8().constData());
		}
		return false;
	}

	if (loadedData.version > APP_CONFIG_VERSION) {
		LogMessage(LOG_WARN, "Config version %d is newer than supported %d, reading what we can\n", loadedData.version, APP_CONFIG_VERSION);
	}
	loadedData.version = APP_CONFIG_VERSION;

	_data = std::move(loadedData);
	LogMessage(LOG_INFO, "Config loaded from %s\n", PathText::ToUtf8(_configFilePath).c_str());
	return true;
}

bool CAppConfig::Save() {
	ResolveDefaultConfigFilePath();

	std::string json;
	static constexpr auto writeOptions = glz::opts{.prettify = true};
	if (auto errorCode = glz::write<writeOptions>(_data, json); errorCode) {
		LogMessage(LOG_ERROR, "Cannot serialize config: %s\n", glz::format_error(errorCode, json).c_str());
		return false;
	}

	const QString configPath = ToQString(_configFilePath);
	if (!QDir().mkpath(QFileInfo(configPath).absolutePath())) {
		LogMessage(LOG_ERROR, "Cannot create config directory for %s\n", PathText::ToUtf8(_configFilePath).c_str());
		return false;
	}

	// QSaveFile writes to a temporary file and renames: a crash mid-write cannot corrupt the config.
	QSaveFile saveFile(configPath);
	if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
		LogMessage(LOG_ERROR, "Cannot open config %s for writing: %s\n", PathText::ToUtf8(_configFilePath).c_str(), saveFile.errorString().toUtf8().constData());
		return false;
	}
	saveFile.write(json.data(), static_cast<qint64>(json.size()));
	saveFile.write("\n", 1);
	if (!saveFile.commit()) {
		LogMessage(LOG_ERROR, "Cannot save config %s: %s\n", PathText::ToUtf8(_configFilePath).c_str(), saveFile.errorString().toUtf8().constData());
		return false;
	}
	return true;
}

std::optional<std::string> CAppConfig::GetGameInstallPath(std::string_view gameId) const {
	const auto gameConfigIterator = _data.games.find(std::string(gameId));
	if (gameConfigIterator == _data.games.end()) {
		return std::nullopt;
	}
	return gameConfigIterator->second.installPath;
}

void CAppConfig::SetGameInstallPath(std::string_view gameId, std::optional<std::string> installPath) {
	GameConfigData& gameConfig = _data.games[std::string(gameId)];
	if (!installPath.has_value() || installPath->empty()) {
		gameConfig.installPath.reset();
	} else {
		gameConfig.installPath = std::move(*installPath);
	}
	if (!gameConfig.installPath.has_value() && !gameConfig.uploadRules.has_value()) {
		_data.games.erase(std::string(gameId));
	}
}

const UploadRulesConfig* CAppConfig::GetUploadRules(std::string_view gameId) const {
	const auto gameConfigIterator = _data.games.find(std::string(gameId));
	if (gameConfigIterator == _data.games.end() || !gameConfigIterator->second.uploadRules.has_value()) {
		return nullptr;
	}
	return &*gameConfigIterator->second.uploadRules;
}

UploadRulesConfig& CAppConfig::EnsureUploadRules(std::string_view gameId) {
	GameConfigData& gameConfig = _data.games[std::string(gameId)];
	if (!gameConfig.uploadRules.has_value()) {
		gameConfig.uploadRules.emplace();
	}
	return *gameConfig.uploadRules;
}

void CAppConfig::ResetUploadRules(std::string_view gameId) {
	const auto gameConfigIterator = _data.games.find(std::string(gameId));
	if (gameConfigIterator == _data.games.end()) {
		return;
	}
	gameConfigIterator->second.uploadRules.reset();
	if (!gameConfigIterator->second.installPath.has_value()) {
		_data.games.erase(gameConfigIterator);
	}
}
