#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <utils/singleton.h>

constexpr int APP_CONFIG_VERSION = 1;

// Own packing rules instead of AddonConfig/VpkDirectories from gameinfo.gi. The seen* lists remember
// which gameinfo entries we have already seen: new Valve entries are added automatically, while
// what the user removed on purpose does not come back on sync.
struct UploadRulesConfig {
	bool useCustomRules{false};
	std::vector<std::string> includes;
	std::vector<std::string> excludes;
	std::vector<std::string> seenGameInfoIncludes;
	std::vector<std::string> seenGameInfoExcludes;
};

// Settings of one game. An empty installPath means the path is detected through Steam.
struct GameConfigData {
	std::optional<std::string> installPath;
	std::optional<UploadRulesConfig> uploadRules;
};

// config.json in the user's settings folder. Only what the user set is stored:
// detected paths are not saved, otherwise the config goes stale after moving a game.
struct AppConfigData {
	int version{APP_CONFIG_VERSION};
	std::optional<std::string> rememberedGameId;
	std::optional<std::string> steamPath;
	std::optional<std::string> skippedUpdateVersion; // release the user chose not to be reminded about
	std::string workshopLanguage{"english"};		 // Steam API language for item texts, read and written (Steam defaults to the client UI language)
	std::map<std::string, GameConfigData> games;
};

class CAppConfig : public Singleton<CAppConfig> {
public:
	// Default: <exe>/config.json when it exists (portable mode), otherwise ~/.steam-publishing-tool/config.json.
	void SetConfigFilePath(std::filesystem::path configFilePath);
	const std::filesystem::path& GetConfigFilePath() const { return _configFilePath; }

	// false — the file existed but did not parse: it was renamed to *.broken-<time> and defaults are used.
	bool Load();
	bool Save();

	AppConfigData& Data() { return _data; }
	const AppConfigData& Data() const { return _data; }

	std::optional<std::string> GetGameInstallPath(std::string_view gameId) const;
	void SetGameInstallPath(std::string_view gameId, std::optional<std::string> installPath);

	const UploadRulesConfig* GetUploadRules(std::string_view gameId) const;
	UploadRulesConfig& EnsureUploadRules(std::string_view gameId);
	void ResetUploadRules(std::string_view gameId);

private:
	void ResolveDefaultConfigFilePath();

	std::filesystem::path _configFilePath;
	AppConfigData _data;
};

inline CAppConfig& AppConfig() { return CAppConfig::Instance(); }
