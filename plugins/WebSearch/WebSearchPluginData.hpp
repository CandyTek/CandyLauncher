#pragma once
#include "util/LogUtil.hpp"
#include "plugins/sdk/CandyPluginSdk.hpp"
#include <functional>
#include <vector>
#include <string>

inline const candy::Host* m_host = nullptr;

struct SearchEngineInfo {
	std::wstring key;
	std::wstring name;
	std::wstring url;
	std::wstring hotkey; // optional global hotkey, e.g. "Alt+G"
};

inline std::vector<SearchEngineInfo> g_searchEngines;
inline std::wstring g_browser; // empty = use default browser
inline std::wstring g_subbrowser; // empty = use default browser
// 搜索管理器保存配置后回调插件，重建条目和触发关键字
inline std::function<void()> g_onSearchEnginesSaved;
