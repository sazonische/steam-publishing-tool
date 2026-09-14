#include "core/addon_config.h"

#include "utils/keyvalues.h"

#include <QtCore/QString>

#include <algorithm>
#include <format>

namespace AddonConfig {

	namespace {

		// Path comparison the way the Windows file system does it: case-insensitive, one kind of slash.
		std::string NormalizeRelativePath(std::string_view path) {
			std::string normalized = QString::fromUtf8(path.data(), static_cast<qsizetype>(path.size())).toLower().replace('\\', '/').toStdString();
			while (normalized.starts_with("./")) {
				normalized.erase(0, 2);
			}
			while (!normalized.empty() && normalized.back() == '/') {
				normalized.pop_back();
			}
			return normalized;
		}

		std::string NormalizeFilesystemPath(const std::filesystem::path& path) {
			const std::string utf8 = QString::fromStdWString(path.generic_wstring()).toStdString();
			return NormalizeRelativePath(std::string_view(utf8));
		}

		bool MatchesRule(const std::string& normalizedPath, std::string_view rule) {
			const std::string normalizedRule = NormalizeRelativePath(rule);
			if (normalizedRule.empty()) {
				return false;
			}
			if (normalizedPath == normalizedRule) {
				return true;
			}
			return normalizedPath.size() > normalizedRule.size() && normalizedPath.starts_with(normalizedRule) && normalizedPath[normalizedRule.size()] == '/';
		}

	} // namespace

	std::optional<AddonVpkRules> Load(const GameProfile& profile, const std::filesystem::path& gamePath, std::string& errorMessage) {
		if (profile.gameInfoPath.empty()) {
			errorMessage = std::format("{} has no gameinfo path configured", profile.displayName);
			return std::nullopt;
		}

		const std::filesystem::path gameInfoPath = gamePath / std::filesystem::path(std::string(profile.gameInfoPath));
		const std::optional<KeyValuesNode> root = KeyValues::ParseFile(gameInfoPath, errorMessage);
		if (!root.has_value()) {
			return std::nullopt;
		}

		const KeyValuesNode* gameInfo = root->Find("GameInfo");
		const KeyValuesNode* addonConfig = gameInfo ? gameInfo->Find("AddonConfig") : nullptr;
		const KeyValuesNode* vpkDirectories = addonConfig ? addonConfig->Find("VpkDirectories") : nullptr;
		if (!vpkDirectories) {
			errorMessage = std::format("{} has no AddonConfig/VpkDirectories block", gameInfoPath.string());
			return std::nullopt;
		}

		AddonVpkRules rules;
		for (const KeyValuesNode& entry : vpkDirectories->children) {
			if (entry.isBlock || entry.value.empty()) {
				continue;
			}
			const QString key = QString::fromStdString(entry.key).toLower();
			if (key == "include") {
				rules.includes.push_back(entry.value);
			} else if (key == "exclude") {
				rules.excludes.push_back(entry.value);
			}
		}

		if (const KeyValuesNode* workshopManager = gameInfo->Find("CS2WorkshopManager")) {
			rules.requiredTag = workshopManager->GetString("RequiredTag");
		}

		if (rules.includes.empty()) {
			errorMessage = std::format("{}: VpkDirectories has no include entries", gameInfoPath.string());
			return std::nullopt;
		}
		return rules;
	}

	bool IsPathPacked(const AddonVpkRules& rules, const std::filesystem::path& relativePath) {
		const std::string normalizedPath = NormalizeFilesystemPath(relativePath);
		if (normalizedPath.empty()) {
			return false;
		}
		for (const std::string& exclude : rules.excludes) {
			if (MatchesRule(normalizedPath, exclude)) {
				return false;
			}
		}
		return std::ranges::any_of(rules.includes, [&normalizedPath](const std::string& include) {
			return MatchesRule(normalizedPath, include);
		});
	}

} // namespace AddonConfig
