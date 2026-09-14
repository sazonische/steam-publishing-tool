#include "core/upload_rules.h"

namespace UploadRules {

	namespace {

		QString NormalizeRule(std::string_view rule) {
			QString normalized = QtText::FromStringView(rule).trimmed().replace('\\', '/').toLower();
			while (normalized.endsWith('/')) {
				normalized.chop(1);
			}
			return normalized;
		}

		std::vector<std::string> MergeNewEntries(std::vector<std::string>& target, std::vector<std::string>& seen, const std::vector<std::string>& source) {
			std::vector<std::string> added;
			for (const std::string& entry : source) {
				const bool alreadySeen = ContainsRule(seen, entry);
				if (!alreadySeen) {
					seen.push_back(entry);
				}
				if (!alreadySeen && !ContainsRule(target, entry)) {
					target.push_back(entry);
					added.push_back(entry);
				}
			}
			return added;
		}

	} // namespace

	bool ContainsRule(const std::vector<std::string>& rules, std::string_view rule) {
		const QString wanted = NormalizeRule(rule);
		return std::ranges::any_of(rules, [&wanted](const std::string& existing) {
			return NormalizeRule(existing) == wanted;
		});
	}

	void CopyFromGameInfo(UploadRulesConfig& config, const AddonVpkRules& gameInfoRules) {
		config.includes = gameInfoRules.includes;
		config.excludes = gameInfoRules.excludes;
		config.seenGameInfoIncludes = gameInfoRules.includes;
		config.seenGameInfoExcludes = gameInfoRules.excludes;
	}

	std::vector<std::string> SyncWithGameInfo(UploadRulesConfig& config, const AddonVpkRules& gameInfoRules) {
		std::vector<std::string> added = MergeNewEntries(config.includes, config.seenGameInfoIncludes, gameInfoRules.includes);
		std::vector<std::string> addedExcludes = MergeNewEntries(config.excludes, config.seenGameInfoExcludes, gameInfoRules.excludes);
		added.append_range(addedExcludes);
		return added;
	}

	std::optional<EffectiveUploadRules> Resolve(const GameProfile& profile, const std::filesystem::path& gamePath, std::string& errorMessage) {
		const std::optional<AddonVpkRules> gameInfoRules = AddonConfig::Load(profile, gamePath, errorMessage);
		if (!gameInfoRules.has_value()) {
			return std::nullopt;
		}

		EffectiveUploadRules effective;
		effective.gameInfoPath = gamePath / profile.gameInfoPath;

		const UploadRulesConfig* userRules = AppConfig().GetUploadRules(profile.id);
		if (!userRules || !userRules->useCustomRules) {
			effective.rules = *gameInfoRules;
			return effective;
		}

		// Valve extends VpkDirectories now and then (new panorama folders and the like) — without
		// auto-sync the user's list would silently fall behind and maps would upload without files.
		UploadRulesConfig& mutableRules = AppConfig().EnsureUploadRules(profile.id);
		effective.autoAdded = SyncWithGameInfo(mutableRules, *gameInfoRules);
		if (!effective.autoAdded.empty()) {
			for (const std::string& entry : effective.autoAdded) {
				LogMessage(LOG_INFO, "Upload rules for %s: added '%s' from gameinfo\n", std::string(profile.id).c_str(), entry.c_str());
			}
			AppConfig().Save();
		}

		effective.custom = true;
		effective.rules.includes = mutableRules.includes;
		effective.rules.excludes = mutableRules.excludes;
		effective.rules.requiredTag = gameInfoRules->requiredTag;
		return effective;
	}

} // namespace UploadRules
