#pragma once
#include "util/LogUtil.hpp"
#include "plugins/sdk/CandyPluginSdk.hpp"
#include "util/FileUtil.hpp"

inline const candy::Host* m_host = nullptr;
inline std::wstring EXE_FOLDER_PATH2 = GetExecutableFolder();
inline std::wstring CONFIG_IDE_PATHS_PATH = EXE_FOLDER_PATH2 + L"\\config_ide_paths.yaml";
