#pragma once
#include "plugins/BaseAction.hpp"
#include "ServicePluginData.hpp"

class ServiceAction final : public BaseAction {
public:
	ServiceAction() {
		pluginId = m_pluginId;
	}

	std::wstring serviceName; // 服务名称（内部名称）
	std::wstring displayName; // 显示名称
	std::wstring status; // 服务状态（运行中、已停止等）
	std::wstring startMode; // 启动模式（自动、手动、禁用）
	bool isRunning = false; // 是否正在运行

	// 图标
	std::wstring iconFilePath;
	std::wstring title;
	std::wstring subTitle;
	int iconFilePathIndex = -1;

	std::wstring& getTitle() override {
		return title;
	}

	std::wstring& getSubTitle() override {
		return subTitle;
	}

	std::wstring& getIconFilePath() override {
		return iconFilePath;
	}

	int getIconFilePathIndex() override {
		return iconFilePathIndex;
	}

	HBITMAP getIconBitmap() override {
		return nullptr;
	}

	~ServiceAction() override {
	}
};
