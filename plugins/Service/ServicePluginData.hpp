#pragma once
#include "plugins/Plugin.hpp"
#include "util/TaskWorker.hpp"

inline IPluginHost* m_host = nullptr;

inline uint16_t m_pluginId = 65535;
inline bool isMatchTextServiceName = true;
inline TaskWorker g_worker;
