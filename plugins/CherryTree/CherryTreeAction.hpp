#pragma once
#include "plugins/BaseAction.hpp"
#include "CherryTreePluginData.hpp"

class CherryTreeAction final : public BaseAction {
public:
	CherryTreeAction() {
		pluginId = m_pluginId;
	}

	int nodeId = 0;
	std::wstring url;


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

	~CherryTreeAction() override {
	}
};
