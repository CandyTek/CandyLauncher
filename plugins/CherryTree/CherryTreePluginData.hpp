#pragma once
#include "plugins/Plugin.hpp"
#include "util/FileUtil.hpp"

inline IPluginHost* m_host = nullptr;

inline uint16_t m_pluginId= 65535;
// 是否将笔记内容拼接到匹配文本中
inline bool isMatchTextContent = false;
inline std::wstring EXE_FOLDER_PATH2 = GetExecutableFolder();
inline std::wstring ICON_FOLDER_PATH = EXE_FOLDER_PATH2 + L"\\plugins\\CTIcons\\";
