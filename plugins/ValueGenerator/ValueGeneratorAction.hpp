#pragma once
#include "plugins/BaseAction.hpp"
#include "ValueGeneratorPluginData.hpp"

class ValueGeneratorAction final : public BaseAction {
public:
	ValueGeneratorAction() {
		pluginId = m_pluginId;
	}

	// 图标，只要是文件就可以
	std::wstring iconFilePath;
	std::wstring title;
	std::wstring subTitle;
	int iconFilePathIndex = -1;
	std::wstring arg;  // 存储执行时需要的参数
	
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
};
