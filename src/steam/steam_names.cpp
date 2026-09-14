#include "steam/steam_names.h"

#include <array>

namespace SteamNames {

	std::string_view Result(EResult result) {
		switch (result) {
			case k_EResultOK: return "OK";
			case k_EResultFail: return "Fail";
			case k_EResultNoConnection: return "NoConnection";
			case k_EResultInvalidPassword: return "InvalidPassword";
			case k_EResultLoggedInElsewhere: return "LoggedInElsewhere";
			case k_EResultInvalidProtocolVer: return "InvalidProtocolVer";
			case k_EResultInvalidParam: return "InvalidParam";
			case k_EResultFileNotFound: return "FileNotFound";
			case k_EResultBusy: return "Busy";
			case k_EResultInvalidState: return "InvalidState";
			case k_EResultInvalidName: return "InvalidName";
			case k_EResultInvalidEmail: return "InvalidEmail";
			case k_EResultDuplicateName: return "DuplicateName";
			case k_EResultAccessDenied: return "AccessDenied";
			case k_EResultTimeout: return "Timeout";
			case k_EResultBanned: return "Banned";
			case k_EResultAccountNotFound: return "AccountNotFound";
			case k_EResultInvalidSteamID: return "InvalidSteamID";
			case k_EResultServiceUnavailable: return "ServiceUnavailable";
			case k_EResultNotLoggedOn: return "NotLoggedOn";
			case k_EResultPending: return "Pending";
			case k_EResultEncryptionFailure: return "EncryptionFailure";
			case k_EResultInsufficientPrivilege: return "InsufficientPrivilege";
			case k_EResultLimitExceeded: return "LimitExceeded";
			case k_EResultRevoked: return "Revoked";
			case k_EResultExpired: return "Expired";
			case k_EResultAlreadyRedeemed: return "AlreadyRedeemed";
			case k_EResultDuplicateRequest: return "DuplicateRequest";
			case k_EResultAlreadyOwned: return "AlreadyOwned";
			case k_EResultIPNotFound: return "IPNotFound";
			case k_EResultPersistFailed: return "PersistFailed";
			case k_EResultLockingFailed: return "LockingFailed";
			case k_EResultLogonSessionReplaced: return "LogonSessionReplaced";
			case k_EResultConnectFailed: return "ConnectFailed";
			case k_EResultHandshakeFailed: return "HandshakeFailed";
			case k_EResultIOFailure: return "IOFailure";
			case k_EResultRemoteDisconnect: return "RemoteDisconnect";
			case k_EResultShoppingCartNotFound: return "ShoppingCartNotFound";
			case k_EResultBlocked: return "Blocked";
			case k_EResultIgnored: return "Ignored";
			case k_EResultNoMatch: return "NoMatch";
			case k_EResultAccountDisabled: return "AccountDisabled";
			case k_EResultServiceReadOnly: return "ServiceReadOnly";
			case k_EResultAccountNotFeatured: return "AccountNotFeatured";
			case k_EResultAdministratorOK: return "AdministratorOK";
			case k_EResultContentVersion: return "ContentVersion";
			case k_EResultTryAnotherCM: return "TryAnotherCM";
			case k_EResultPasswordRequiredToKickSession: return "PasswordRequiredToKickSession";
			case k_EResultAlreadyLoggedInElsewhere: return "AlreadyLoggedInElsewhere";
			case k_EResultSuspended: return "Suspended";
			case k_EResultCancelled: return "Cancelled";
			case k_EResultDataCorruption: return "DataCorruption";
			case k_EResultDiskFull: return "DiskFull";
			case k_EResultRemoteCallFailed: return "RemoteCallFailed";
			case k_EResultPasswordUnset: return "PasswordUnset";
			case k_EResultExternalAccountUnlinked: return "ExternalAccountUnlinked";
			case k_EResultPSNTicketInvalid: return "PSNTicketInvalid";
			case k_EResultExternalAccountAlreadyLinked: return "ExternalAccountAlreadyLinked";
			case k_EResultRemoteFileConflict: return "RemoteFileConflict";
			case k_EResultIllegalPassword: return "IllegalPassword";
			case k_EResultSameAsPreviousValue: return "SameAsPreviousValue";
			case k_EResultAccountLogonDenied: return "AccountLogonDenied";
			case k_EResultCannotUseOldPassword: return "CannotUseOldPassword";
			case k_EResultInvalidLoginAuthCode: return "InvalidLoginAuthCode";
			case k_EResultAccountLogonDeniedNoMail: return "AccountLogonDeniedNoMail";
			case k_EResultHardwareNotCapableOfIPT: return "HardwareNotCapableOfIPT";
			case k_EResultIPTInitError: return "IPTInitError";
			case k_EResultParentalControlRestricted: return "ParentalControlRestricted";
			case k_EResultFacebookQueryError: return "FacebookQueryError";
			case k_EResultExpiredLoginAuthCode: return "ExpiredLoginAuthCode";
			case k_EResultIPLoginRestrictionFailed: return "IPLoginRestrictionFailed";
			case k_EResultAccountLockedDown: return "AccountLockedDown";
			case k_EResultAccountLogonDeniedVerifiedEmailRequired: return "AccountLogonDeniedVerifiedEmailRequired";
			case k_EResultNoMatchingURL: return "NoMatchingURL";
			case k_EResultBadResponse: return "BadResponse";
			case k_EResultRequirePasswordReEntry: return "RequirePasswordReEntry";
			case k_EResultValueOutOfRange: return "ValueOutOfRange";
			case k_EResultUnexpectedError: return "UnexpectedError";
			case k_EResultDisabled: return "Disabled";
			case k_EResultInvalidCEGSubmission: return "InvalidCEGSubmission";
			case k_EResultRestrictedDevice: return "RestrictedDevice";
			case k_EResultRegionLocked: return "RegionLocked";
			case k_EResultRateLimitExceeded: return "RateLimitExceeded";
			case k_EResultAccountLoginDeniedNeedTwoFactor: return "AccountLoginDeniedNeedTwoFactor";
			case k_EResultItemDeleted: return "ItemDeleted";
			case k_EResultAccountLoginDeniedThrottle: return "AccountLoginDeniedThrottle";
			case k_EResultTwoFactorCodeMismatch: return "TwoFactorCodeMismatch";
			case k_EResultTwoFactorActivationCodeMismatch: return "TwoFactorActivationCodeMismatch";
			case k_EResultAccountAssociatedToMultiplePartners: return "AccountAssociatedToMultiplePartners";
			case k_EResultNotModified: return "NotModified";
			case k_EResultNoMobileDevice: return "NoMobileDevice";
			case k_EResultTimeNotSynced: return "TimeNotSynced";
			case k_EResultSmsCodeFailed: return "SmsCodeFailed";
			case k_EResultAccountLimitExceeded: return "AccountLimitExceeded";
			case k_EResultAccountActivityLimitExceeded: return "AccountActivityLimitExceeded";
			case k_EResultPhoneActivityLimitExceeded: return "PhoneActivityLimitExceeded";
			case k_EResultRefundToWallet: return "RefundToWallet";
			case k_EResultEmailSendFailure: return "EmailSendFailure";
			case k_EResultNotSettled: return "NotSettled";
			case k_EResultNeedCaptcha: return "NeedCaptcha";
			case k_EResultGSLTDenied: return "GSLTDenied";
			case k_EResultGSOwnerDenied: return "GSOwnerDenied";
			case k_EResultInvalidItemType: return "InvalidItemType";
			case k_EResultIPBanned: return "IPBanned";
			case k_EResultGSLTExpired: return "GSLTExpired";
			case k_EResultInsufficientFunds: return "InsufficientFunds";
			case k_EResultTooManyPending: return "TooManyPending";
			case k_EResultNoSiteLicensesFound: return "NoSiteLicensesFound";
			case k_EResultWGNetworkSendExceeded: return "WGNetworkSendExceeded";
			case k_EResultAccountNotFriends: return "AccountNotFriends";
			case k_EResultLimitedUserAccount: return "LimitedUserAccount";
			case k_EResultCantRemoveItem: return "CantRemoveItem";
			case k_EResultAccountDeleted: return "AccountDeleted";
			case k_EResultExistingUserCancelledLicense: return "ExistingUserCancelledLicense";
			case k_EResultCommunityCooldown: return "CommunityCooldown";
			case k_EResultNoLauncherSpecified: return "NoLauncherSpecified";
			case k_EResultMustAgreeToSSA: return "MustAgreeToSSA";
			case k_EResultLauncherMigrated: return "LauncherMigrated";
			case k_EResultSteamRealmMismatch: return "SteamRealmMismatch";
			case k_EResultInvalidSignature: return "InvalidSignature";
			case k_EResultParseFailure: return "ParseFailure";
			case k_EResultNoVerifiedPhone: return "NoVerifiedPhone";
			case k_EResultInsufficientBattery: return "InsufficientBattery";
			case k_EResultChargerRequired: return "ChargerRequired";
			case k_EResultCachedCredentialInvalid: return "CachedCredentialInvalid";
			case K_EResultPhoneNumberIsVOIP: return "PhoneNumberIsVOIP";
			case k_EResultNotSupported: return "NotSupported";
			case k_EResultFamilySizeLimitExceeded: return "FamilySizeLimitExceeded";
			case k_EResultOfflineAppCacheInvalid: return "OfflineAppCacheInvalid";
			default: return "Unknown";
		}
	}

	std::string_view Visibility(ERemoteStoragePublishedFileVisibility visibility) {
		switch (visibility) {
			case k_ERemoteStoragePublishedFileVisibilityPublic: return "Public";
			case k_ERemoteStoragePublishedFileVisibilityFriendsOnly: return "Friends Only";
			case k_ERemoteStoragePublishedFileVisibilityPrivate: return "Private";
			case k_ERemoteStoragePublishedFileVisibilityUnlisted: return "Unlisted";
			default: return "Unknown";
		}
	}

	std::string_view ItemUpdateStatus(EItemUpdateStatus status) {
		switch (status) {
			case k_EItemUpdateStatusInvalid: return "Invalid";
			case k_EItemUpdateStatusPreparingConfig: return "Preparing config";
			case k_EItemUpdateStatusPreparingContent: return "Preparing content";
			case k_EItemUpdateStatusUploadingContent: return "Uploading content";
			case k_EItemUpdateStatusUploadingPreviewFile: return "Uploading preview";
			case k_EItemUpdateStatusCommittingChanges: return "Committing changes";
			default: return "Unknown";
		}
	}

	std::string_view PreviewType(EItemPreviewType previewType) {
		switch (previewType) {
			case k_EItemPreviewType_Image: return "Image";
			case k_EItemPreviewType_YouTubeVideo: return "YouTube";
			case k_EItemPreviewType_Sketchfab: return "Sketchfab";
			case k_EItemPreviewType_EnvironmentMap_HorizontalCross: return "EnvMap (cross)";
			case k_EItemPreviewType_EnvironmentMap_LatLong: return "EnvMap (lat-long)";
			case k_EItemPreviewType_Clip: return "Clip";
			default: return "Unknown";
		}
	}

	std::span<const WorkshopLanguage> WorkshopLanguages() {
		static constexpr std::array<WorkshopLanguage, 29> LANGUAGES{{
			{"english", "English"},
			{"russian", "Russian"},
			{"ukrainian", "Ukrainian"},
			{"german", "German"},
			{"french", "French"},
			{"spanish", "Spanish"},
			{"latam", "Spanish (Latin America)"},
			{"italian", "Italian"},
			{"portuguese", "Portuguese"},
			{"brazilian", "Portuguese (Brazil)"},
			{"polish", "Polish"},
			{"czech", "Czech"},
			{"hungarian", "Hungarian"},
			{"romanian", "Romanian"},
			{"bulgarian", "Bulgarian"},
			{"greek", "Greek"},
			{"turkish", "Turkish"},
			{"dutch", "Dutch"},
			{"danish", "Danish"},
			{"swedish", "Swedish"},
			{"norwegian", "Norwegian"},
			{"finnish", "Finnish"},
			{"schinese", "Chinese (Simplified)"},
			{"tchinese", "Chinese (Traditional)"},
			{"japanese", "Japanese"},
			{"koreana", "Korean"},
			{"thai", "Thai"},
			{"vietnamese", "Vietnamese"},
			{"indonesian", "Indonesian"},
		}};
		return LANGUAGES;
	}

	std::string_view LanguageDisplayName(std::string_view apiName) {
		for (const WorkshopLanguage& language : WorkshopLanguages()) {
			if (language.apiName == apiName) {
				return language.displayName;
			}
		}
		return apiName;
	}

} // namespace SteamNames
