#pragma once
#include "plugins/BaseAction.hpp"
#include "BrowserHistoryPluginData.hpp"

class BrowserHistoryAction final : public BaseAction {
public:
	BrowserHistoryAction() {
		pluginId = m_pluginId;
	}

	std::wstring url;
	std::wstring visitTime;  // 访问时间
	int visitCount = 0;      // 访问次数

	// 图标，只要是文件就可以
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

	~BrowserHistoryAction() override {
	}
};
