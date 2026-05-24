#pragma once
#include "plugins/Plugin.hpp"
#include "util/FileUtil.hpp"

inline IPluginHost* m_host = nullptr;
inline uint16_t m_pluginId= 65535;
inline std::wstring EXE_FOLDER_PATH2 = GetExecutableFolder();
