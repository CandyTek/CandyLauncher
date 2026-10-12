#pragma once
#include "util/LogUtil.hpp"
#include <string>
#include <vector>
#include "plugins/sdk/CandyPluginSdk.hpp"

inline const candy::Host* m_host = nullptr;

inline int maxSearchCount = 30;
inline bool displayHiddenFiles = false;
inline std::vector<std::wstring> excludedPatterns;
