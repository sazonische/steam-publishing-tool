#pragma once

// The connection to the Steam client on behalf of the chosen game. One session per process: Steamworks
// cannot re-initialise under another AppID, so switching games means restarting the process.
class CSteamSession : public Singleton<CSteamSession> {
public:
	// The AppID goes through the SteamAppId/SteamGameId environment variables before SteamAPI_InitEx —
	// no steam_appid.txt next to the exe, and the game can be chosen at startup.
	bool Init(uint32_t appId, std::string& errorMessage);
	void Shutdown();

	bool IsInitialized() const { return _initialized; }
	ESteamAPIInitResult GetLastInitResult() const { return _lastInitResult; }
	uint32_t GetAppId() const { return _appId; }

	uint32_t GetAccountId() const;
	uint64_t GetSteamId64() const;
	std::string GetPersonaName() const;

	// Call from the main thread on a timer: delivers CCallResult/CCallback.
	void RunCallbacks();

private:
	bool _initialized{false};
	ESteamAPIInitResult _lastInitResult{k_ESteamAPIInitResult_OK};
	uint32_t _appId{0};
};

inline CSteamSession& SteamSession() { return CSteamSession::Instance(); }
