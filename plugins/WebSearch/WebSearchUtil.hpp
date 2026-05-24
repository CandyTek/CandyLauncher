#pragma once

#include "WebSearchPluginData.hpp"
#include "util/FileUtil.hpp"
#include "util/StringUtil.hpp"
#include "util/LogUtil.hpp"
#include "util/json.hpp"

#include <fstream>
#include <filesystem>

using json = nlohmann::json;

static std::wstring UrlEncodeQuery(const std::wstring& input) {
	std::wstring result;
	std::string utf8 = wide_to_utf8(input);
	for (unsigned char c : utf8) {
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			result += static_cast<wchar_t>(c);
		} else if (c == ' ') {
			result += L'+';
		} else {
			wchar_t hex[4];
			swprintf_s(hex, 4, L"%%%02X", c);
			result += hex;
		}
	}
	return result;
}

static std::wstring GetWebSearchConfigPath() {
	return GetExecutableFolder() + L"\\config_websearch_plugin.json";
}

static void CreateDefaultWebSearchConfig(const std::wstring& path) {
	json j;
	j["engines"] = json::array();

	auto addEngine = [&](const std::string& key, const std::string& name, const std::string& url) {
		json e;
		e["key"] = key;
		e["name"] = name;
		e["url"] = url;
		j["engines"].push_back(e);
	};

	addEngine("gg",   "Google",           "https://www.google.com/search?q=%s&oq=hello&sourceid=chrome&ie=UTF-8");
	addEngine("gt",   "Google Translate",  "https://translate.google.com/?hl=en&tab=wT#en/zh-CN/%s");
	addEngine("ii",   "Bing",              "http://www.bing.com/search?q=%s");
	addEngine("bd",   "Baidu",             "https://www.baidu.com/s?wd=%s");
	addEngine("bt",   "Baidu Translation", "http://fanyi.baidu.com/?aldtype=85#en/zh/%s");
	addEngine("gmap", "Google Maps",       "https://www.google.com/maps/search/?api=1&query=%s");
	addEngine("bmap", "Baidu Maps",        "http://api.map.baidu.com/geocoder?address=%s&output=html&src=Chrome");
	addEngine("tao",  "Taobao",            "https://s.taobao.com/search?q=%s");
	addEngine("jd",   "Jingdong",          "https://search.jd.com/Search?keyword=%s&enc=utf-8");
	addEngine("sn",   "Suning",            "https://search.suning.com/%s/");
	addEngine("pr",   "Pronto",            "https://pronto.inside.nsn.com/pronto/problemReportSearch.html?freeTextdropDownID=prId&searchTopText=%s");

	try {
		std::ofstream f(path, std::ios::binary);
		if (f.is_open()) {
			f.put(static_cast<char>(0xEF));
			f.put(static_cast<char>(0xBB));
			f.put(static_cast<char>(0xBF));
			std::string content = j.dump(2);
			f.write(content.c_str(), static_cast<std::streamsize>(content.size()));
		}
	} catch (...) {
		Loge(L"WebSearch Plugin", L"Failed to create default config");
	}
}

static void LoadWebSearchConfig() {
	g_searchEngines.clear();

	std::wstring configPath = GetWebSearchConfigPath();

	if (!std::filesystem::exists(configPath)) {
		CreateDefaultWebSearchConfig(configPath);
	}

	try {
		std::string jsonText = ReadUtf8File(configPath);
		if (jsonText.empty()) return;

		json j = json::parse(jsonText);

		if (j.contains("engines") && j["engines"].is_array()) {
			for (auto& engine : j["engines"]) {
				if (!engine.contains("key") || !engine.contains("name") || !engine.contains("url")) continue;
				SearchEngineInfo info;
				info.key  = utf8_to_wide(engine["key"].get<std::string>());
				info.name = utf8_to_wide(engine["name"].get<std::string>());
				info.url  = utf8_to_wide(engine["url"].get<std::string>());
				if (engine.contains("hotkey") && engine["hotkey"].is_string())
					info.hotkey = utf8_to_wide(engine["hotkey"].get<std::string>());
				g_searchEngines.push_back(info);
			}
		}

		ConsolePrintln(L"WebSearch Plugin", L"Loaded " + std::to_wstring(g_searchEngines.size()) + L" search engines");
	} catch (...) {
		Loge(L"WebSearch Plugin", L"Failed to load config");
	}
}

static void SaveWebSearchConfig() {
	std::wstring configPath = GetWebSearchConfigPath();
	try {
		json j;
		j["engines"] = json::array();
		for (auto& info : g_searchEngines) {
			json e;
			e["key"]  = wide_to_utf8(info.key);
			e["name"] = wide_to_utf8(info.name);
			e["url"]  = wide_to_utf8(info.url);
			if (!info.hotkey.empty()) e["hotkey"] = wide_to_utf8(info.hotkey);
			j["engines"].push_back(e);
		}
		std::ofstream f(configPath, std::ios::binary);
		if (f.is_open()) {
			f.put(static_cast<char>(0xEF));
			f.put(static_cast<char>(0xBB));
			f.put(static_cast<char>(0xBF));
			std::string content = j.dump(2);
			f.write(content.c_str(), static_cast<std::streamsize>(content.size()));
		}
		ConsolePrintln(L"WebSearch Plugin", L"Config saved");
	} catch (...) {
		Loge(L"WebSearch Plugin", L"Failed to save config");
	}
}

static std::wstring BuildSearchUrl(const std::wstring& urlTemplate, const std::wstring& query) {
	std::wstring url = urlTemplate;
	std::wstring encoded = UrlEncodeQuery(query);
	size_t pos = url.find(L"%s");
	if (pos != std::wstring::npos) {
		url.replace(pos, 2, encoded);
	}
	return url;
}

