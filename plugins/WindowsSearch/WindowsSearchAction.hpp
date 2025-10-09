#pragma once
#include "plugins/BaseAction.hpp"
#include "WindowsSearchPluginData.hpp"

class WindowsSearchAction final : public BaseAction {
public:
	WindowsSearchAction() {
		pluginId = m_pluginId;
	}

	std::wstring path; // 文件路径
	std::wstring fileName; // 文件名

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

	~WindowsSearchAction() override {
	}
};
