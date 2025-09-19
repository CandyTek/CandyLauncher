//
// Created by Administrator on 2025/10/1.
//
#pragma once

#ifndef FOLDERDATAKEEPER_HPP
#define FOLDERDATAKEEPER_HPP
#include <string>

#include "util/BaseTools.hpp"

#endif //FOLDERDATAKEEPER_HPP

inline std::wstring EXE_FOLDER_PATH2 = GetExecutableFolder();
inline std::wstring RUNNER_CONFIG_PATH2 = EXE_FOLDER_PATH2 + L"\\runner.json";

inline IPluginHost* g_host = nullptr;
