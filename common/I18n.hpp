#pragma once

#include <windows.h>
#include <string>
#include <vector>

#include "../util/json.hpp"
#include "../util/StringUtil.hpp"
#include "GlobalState.hpp"

inline HMODULE g_hLangRes = nullptr;
inline std::wstring g_uiLanguageCode = L"zh-CN";

inline std::wstring DetectSystemUiLanguageCode() {
	const LANGID uiLang = GetUserDefaultUILanguage();
	switch (PRIMARYLANGID(uiLang)) {
	case LANG_CHINESE:
		return L"zh-CN";
	default:
		return L"en-US";
	}
}

inline std::wstring NormalizeUiLanguageCode(const std::wstring& code) {
	if (code.empty()) {
		return DetectSystemUiLanguageCode();
	}
	if (code == L"zh" || code == L"zh-CN" || code == L"zh_CN") {
		return L"zh-CN";
	}
	if (code == L"en" || code == L"en-US" || code == L"en_US") {
		return L"en-US";
	}
	return DetectSystemUiLanguageCode();
}

inline std::wstring NormalizeUiLanguageCode(const std::string& code) {
	return NormalizeUiLanguageCode(utf8_to_wide(code));
}

inline std::wstring ResolveUiLanguageCode(const nlohmann::json* userConfig = nullptr) {
	if (userConfig && userConfig->contains("pref_ui_language")) {
		try {
			return NormalizeUiLanguageCode((*userConfig)["pref_ui_language"].get<std::string>());
		} catch (...) {
		}
	}
	return DetectSystemUiLanguageCode();
}

inline std::string GetCurrentUiLanguageSuffix() {
	return g_uiLanguageCode.rfind(L"zh", 0) == 0 ? "zh" : "en";
}

inline HINSTANCE GetLangResourceModule() {
	return g_hLangRes ? g_hLangRes : g_hInst;
}

inline bool LoadLanguageResourceDll(const std::wstring& languageCode) {
	const std::wstring normalized = NormalizeUiLanguageCode(languageCode);
	const std::wstring dllPath = EXE_FOLDER_PATH + L"\\lang\\" + normalized + L".dll";
	HMODULE hModule = LoadLibraryExW(
		dllPath.c_str(),
		nullptr,
		LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
	if (!hModule) {
		return false;
	}
	if (g_hLangRes) {
		FreeLibrary(g_hLangRes);
	}
	g_hLangRes = hModule;
	g_uiLanguageCode = normalized;
	return true;
}

inline void InitializeI18n(const nlohmann::json* userConfig = nullptr) {
	const std::wstring preferredCode = ResolveUiLanguageCode(userConfig);
	if (LoadLanguageResourceDll(preferredCode)) {
		return;
	}
	if (preferredCode != L"en-US" && LoadLanguageResourceDll(L"en-US")) {
		return;
	}
	if (preferredCode != L"zh-CN") {
		LoadLanguageResourceDll(L"zh-CN");
	}
}

inline std::wstring LoadI18nString(const UINT id, const std::wstring& fallback = L"") {
	wchar_t buffer[512] = {};
	const int len = LoadStringW(GetLangResourceModule(), id, buffer, _countof(buffer));
	if (len <= 0) {
		return fallback;
	}
	return std::wstring(buffer, len);
}

inline std::string GetLocalizedJsonString(const nlohmann::json& item, const char* baseKey) {
	const std::string localizedKey = std::string(baseKey) + "_" + GetCurrentUiLanguageSuffix();
	if (item.contains(localizedKey) && item[localizedKey].is_string()) {
		return item[localizedKey].get<std::string>();
	}
	if (item.contains(baseKey) && item[baseKey].is_string()) {
		return item[baseKey].get<std::string>();
	}
	return "";
}

inline std::vector<std::string> GetLocalizedJsonStringArray(const nlohmann::json& item, const char* baseKey) {
	const std::string localizedKey = std::string(baseKey) + "_" + GetCurrentUiLanguageSuffix();
	if (item.contains(localizedKey) && item[localizedKey].is_array()) {
		return item[localizedKey].get<std::vector<std::string>>();
	}
	if (item.contains(baseKey) && item[baseKey].is_array()) {
		return item[baseKey].get<std::vector<std::string>>();
	}
	return {};
}
