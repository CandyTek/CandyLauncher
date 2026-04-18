//
// Created by Administrator on 2025/10/1.
//
#pragma once

#include <string>
#include <functional>

#include "util/BaseTools.hpp"
#include "util/FileUtil.hpp"


inline std::wstring EXE_FOLDER_PATH2 = GetExecutableFolder();
inline std::wstring RUNNER_CONFIG_PATH2 = EXE_FOLDER_PATH2 + L"\\plugins\\config_folder_plugin.json";

inline IPluginHost* g_host = nullptr;

inline uint16_t m_pluginId= 65535;

inline std::function<void()> g_refreshFolderPlugin = nullptr;
