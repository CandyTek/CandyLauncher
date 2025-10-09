#pragma once
#include "plugins/BaseAction.hpp"
#include "VisualStudioPluginData.hpp"

class VisualStudioAction final : public BaseAction {
public:
	VisualStudioAction() {
		pluginId = m_pluginId;
	}

	std::wstring projectPath;       // 项目/解决方案完整路径
	std::wstring visualStudioPath;  // Visual Studio 可执行文件路径
	std::wstring displayName;       // Visual Studio 版本显示名称
	bool isPrerelease = false;      // 是否为预发布版本
	bool isFavorite = false;        // 是否为收藏项目

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

	~VisualStudioAction() override {
	}
};
