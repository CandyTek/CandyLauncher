#pragma once
// Host side of plugin localization (CL_HostApi::get_ui_language / get_string). Design: docs/DOC_PLUGIN_I18N.md.
//
// Each plugin may ship string tables: "plugins/lang/<dll stem>/<language>.json" files (one per language) and/or
// a "strings" object in its "<dll stem>.plugin.json" manifest ({ "<language>": { key: text } }). The host loads
// every language once per plugin (PluginStringTable) and resolves a flat key -> text map for the current UI
// language (Resolve), which get_string reads lock free from CL_PluginContext.

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "util/FileUtil.hpp"
#include "util/LogUtil.hpp"
#include "util/StringUtil.hpp"
#include "util/json.hpp"

using PluginStringMap = std::unordered_map<std::wstring, std::wstring>;

namespace PluginI18n {

constexpr const wchar_t* LANG_DIR_NAME = L"lang";
// Reserved keys that localize the catalog metadata without loading the DLL.
constexpr const wchar_t* KEY_PLUGIN_NAME = L"plugin.name";
constexpr const wchar_t* KEY_PLUGIN_DESCRIPTION = L"plugin.description";

inline std::wstring NormalizeCode(const std::wstring& code) {
	std::wstring result = MyToLower(code);
	std::replace(result.begin(), result.end(), L'_', L'-');
	return result;
}

inline std::wstring PrimaryOf(const std::wstring& normalizedCode) {
	const size_t dash = normalizedCode.find(L'-');
	return dash == std::wstring::npos ? normalizedCode : normalizedCode.substr(0, dash);
}

// Flattens nested objects with '.', so {"action": {"hello": "Hi"}} becomes "action.hello". Non-string leaves are ignored.
inline void MergeJsonStrings(PluginStringMap& into, const nlohmann::json& object, const std::wstring& prefix = L"") {
	if (!object.is_object()) return;
	for (const auto& [key, value] : object.items()) {
		const std::wstring fullKey = prefix.empty() ? utf8_to_wide(key) : prefix + L"." + utf8_to_wide(key);
		if (value.is_string()) {
			into[fullKey] = utf8_to_wide(value.get<std::string>());
		} else if (value.is_object()) {
			MergeJsonStrings(into, value, fullKey);
		}
	}
}

}

// All languages of one plugin. Immutable once built; shared between the catalog entry and the loaded plugin.
struct PluginStringTable {
	// normalized language code -> key -> text. Ordered so the "first table" fallback is deterministic.
	std::map<std::wstring, PluginStringMap> languages;

	bool Empty() const { return languages.empty(); }

	// Lookup chain for `language`: exact code, same primary language, en-US, any English, then any table.
	std::vector<const PluginStringMap*> Chain(const std::wstring& language) const {
		std::vector<const PluginStringMap*> chain;
		auto add = [&](const PluginStringMap* table) {
			if (table && std::find(chain.begin(), chain.end(), table) == chain.end()) chain.push_back(table);
		};
		auto find = [&](const std::wstring& code) -> const PluginStringMap* {
			const auto it = languages.find(code);
			return it == languages.end() ? nullptr : &it->second;
		};
		const std::wstring wanted = PluginI18n::NormalizeCode(language);
		add(find(wanted));
		const std::wstring primary = PluginI18n::PrimaryOf(wanted);
		for (const auto& [code, table] : languages) {
			if (PluginI18n::PrimaryOf(code) == primary) add(&table);
		}
		add(find(L"en-us"));
		for (const auto& [code, table] : languages) {
			if (PluginI18n::PrimaryOf(code) == L"en") add(&table);
		}
		for (const auto& [code, table] : languages) add(&table);
		return chain;
	}

	// Single lookup without building the merged map (catalog name / description).
	bool Find(const std::wstring& language, const std::wstring& key, std::wstring& out) const {
		for (const PluginStringMap* table : Chain(language)) {
			const auto it = table->find(key);
			if (it != table->end()) {
				out = it->second;
				return true;
			}
		}
		return false;
	}

	// Flat key -> text view for `language` with the fallback chain applied.
	std::shared_ptr<const PluginStringMap> Resolve(const std::wstring& language) const {
		auto result = std::make_shared<PluginStringMap>();
		const auto chain = Chain(language);
		// Lowest priority first so higher priority tables overwrite.
		for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
			for (const auto& [key, text] : **it) (*result)[key] = text;
		}
		return result;
	}
};

namespace PluginI18n {

inline std::shared_mutex g_languageMutex;
// Mirror of g_uiLanguageCode that worker threads may read (g_uiLanguageCode itself is only touched on the UI thread).
inline std::wstring g_language = L"zh-CN";

inline void SetLanguage(const std::wstring& code) {
	std::unique_lock<std::shared_mutex> lock(g_languageMutex);
	g_language = code;
}

inline std::wstring Language() {
	std::shared_lock<std::shared_mutex> lock(g_languageMutex);
	return g_language;
}

// "<plugin dir>\lang\<dll stem>"
inline std::wstring LangDirFor(const std::wstring& dllPath) {
	const std::filesystem::path path(dllPath);
	return (path.parent_path() / LANG_DIR_NAME / path.stem()).wstring();
}

// Loads every "<language>.json" in the plugin's lang dir on top of the manifest's "strings" object (files win).
// Re-reads the manifest itself so a language switch can reload edited translations without re-cataloging.
inline std::shared_ptr<const PluginStringTable> Load(const std::wstring& dllPath, const std::wstring& manifestPath) {
	auto table = std::make_shared<PluginStringTable>();
	std::error_code ec;
	if (!manifestPath.empty() && std::filesystem::exists(manifestPath, ec)) {
		try {
			const nlohmann::json manifest = nlohmann::json::parse(ReadUtf8File(manifestPath));
			if (manifest.is_object() && manifest.contains("strings") && manifest.at("strings").is_object()) {
				for (const auto& [code, object] : manifest.at("strings").items()) {
					MergeJsonStrings(table->languages[NormalizeCode(utf8_to_wide(code))], object);
				}
			}
		} catch (const std::exception& e) {
			Loge(L"PluginI18n", L"Bad manifest strings ", manifestPath, L": ", e.what());
		}
	}
	const std::wstring dir = LangDirFor(dllPath);
	if (std::filesystem::is_directory(dir, ec)) {
		for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
			if (!entry.is_regular_file(ec) || MyToLower(entry.path().extension().wstring()) != L".json") continue;
			const std::string text = ReadUtf8File(entry.path().wstring());
			if (text.empty()) continue;
			try {
				const nlohmann::json object = nlohmann::json::parse(text);
				MergeJsonStrings(table->languages[NormalizeCode(entry.path().stem().wstring())], object);
			} catch (const std::exception& e) {
				Loge(L"PluginI18n", L"Bad string table ", entry.path().wstring(), L": ", e.what());
			}
		}
	}
	return table;
}

// Text for `key` in the current language, or `fallback`.
inline std::wstring Localize(const std::shared_ptr<const PluginStringTable>& table, const wchar_t* key,
							const std::wstring& fallback) {
	std::wstring text;
	if (table && table->Find(Language(), key, text)) return text;
	return fallback;
}

}
