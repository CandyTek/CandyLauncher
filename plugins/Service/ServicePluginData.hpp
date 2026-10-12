#pragma once
#include "util/LogUtil.hpp"
#include "plugins/sdk/CandyPluginSdk.hpp"
#include "util/TaskWorker.hpp"

inline const candy::Host* m_host = nullptr;

inline bool isMatchTextServiceName = true;
inline TaskWorker g_worker;

// 2.3 本地化：翻译在清单 ServicePlugin.plugin.json 的 "strings" 里，fallback 是没有任何翻译时的默认文本（任意线程可用）
inline std::wstring ServiceTr(const wchar_t* key, const wchar_t* fallback) {
	return m_host ? m_host->Tr(key, fallback) : fallback;
}
