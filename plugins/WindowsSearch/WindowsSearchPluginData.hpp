#pragma once
#include "plugins/Plugin.hpp"

inline IPluginHost* m_host = nullptr;

inline uint16_t m_pluginId = 65535;
inline bool isMatchTextUrl = true;
inline int maxSearchCount = 30;
inline bool displayHiddenFiles = false;
inline std::vector<std::wstring> excludedPatterns;
