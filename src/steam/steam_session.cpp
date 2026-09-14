#include "steam/steam_session.h"

#include <steam/steam_api.h>

#include <QtCore/QByteArray>
#include <QtCore/qglobal.h>

bool CSteamSession::Init(uint32_t appId, std::string& errorMessage) {
	if (_initialized) {
		if (_appId == appId) {
			return true;
		}
		errorMessage = std::format("Steam already initialized for AppID {}", _appId);
		return false;
	}

	const QByteArray appIdText = QByteArray::number(appId);
	qputenv("SteamAppId", appIdText);
	qputenv("SteamGameId", appIdText);

	SteamErrMsg steamErrorMessage{};
	const ESteamAPIInitResult initResult = SteamAPI_InitEx(&steamErrorMessage);
	_lastInitResult = initResult;
	if (initResult != k_ESteamAPIInitResult_OK) {
		errorMessage = std::format("SteamAPI_InitEx failed ({}): {}", static_cast<int>(initResult), steamErrorMessage);
		LogMessage(LOG_ERROR, "%s\n", errorMessage.c_str());
		return false;
	}

	_initialized = true;
	_appId = appId;

	LogMessage(LOG_INFO, "Steam initialized: AppID %u, user %s (%u)\n", appId, GetPersonaName().c_str(), GetAccountId());
	return true;
}

void CSteamSession::Shutdown() {
	if (!_initialized) {
		return;
	}
	SteamAPI_Shutdown();
	_initialized = false;
	_appId = 0;
}

uint32_t CSteamSession::GetAccountId() const {
	if (!_initialized) {
		return 0;
	}
	return SteamUser()->GetSteamID().GetAccountID();
}

uint64_t CSteamSession::GetSteamId64() const {
	if (!_initialized) {
		return 0;
	}
	return SteamUser()->GetSteamID().ConvertToUint64();
}

std::string CSteamSession::GetPersonaName() const {
	if (!_initialized) {
		return {};
	}
	return SteamFriends()->GetPersonaName();
}

void CSteamSession::RunCallbacks() {
	if (!_initialized) {
		return;
	}
	SteamAPI_RunCallbacks();
}
