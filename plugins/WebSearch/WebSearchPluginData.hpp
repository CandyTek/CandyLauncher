#pragma once
#include "plugins/Plugin.hpp"
#include <vector>
#include <string>

inline IPluginHost* m_host = nullptr;
inline uint16_t m_pluginId = 65535;

struct SearchEngineInfo {
	std::wstring key;
	std::wstring name;
	std::wstring url;
	std::wstring hotkey; // optional global hotkey, e.g. "Alt+G"
};

inline std::vector<SearchEngineInfo> g_searchEngines;
inline std::wstring g_browser; // empty = use default browser

