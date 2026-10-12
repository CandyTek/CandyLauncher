#pragma once
#include <string>
#include "plugins/sdk/CandyPluginSdk.hpp"
#include "ServicePluginData.hpp"

class ServiceAction final : public candy::ActionBase {
public:
	std::wstring serviceName; // 服务名称（内部名称）
	std::wstring displayName; // 显示名称
	std::wstring status; // 服务状态（运行中、已停止等，已本地化）
	std::wstring startMode; // 启动模式（自动、手动、禁用，已本地化）
	bool isRunning = false; // 是否正在运行

	// 根据当前状态生成副标题；占位符顺序由翻译决定
	void UpdateSubtitle() {
		subtitle = candy::Format(ServiceTr(L"subtitle", L"Status: {0} - Startup: {1} - Name: {2}"),
								{status, startMode, serviceName});
	}
};
