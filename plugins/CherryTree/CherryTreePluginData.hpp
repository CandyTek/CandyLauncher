#pragma once
#include "util/LogUtil.hpp"
#include "plugins/sdk/CandyPluginSdk.hpp"
#include "util/FileUtil.hpp"

inline const candy::Host* m_host = nullptr;

// 是否将笔记内容拼接到匹配文本中
inline bool isMatchTextContent = false;
inline std::wstring EXE_FOLDER_PATH2 = GetExecutableFolder();
inline std::wstring ICON_FOLDER_PATH = EXE_FOLDER_PATH2 + L"\\plugins\\CTIcons\\";
