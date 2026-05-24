#pragma once
#include "plugins/BaseAction.hpp"
#include "JbPluginData.hpp"

class JbAction final : public BaseAction {
public:
	JbAction() {
		pluginId = m_pluginId;
	}

	// 图标，只要是文件就可以
	std::wstring iconFilePath;
	std::wstring title;
	std::wstring subTitle;
	int iconFilePathIndex = -1;

	// Additional data for JetBrains workspace
	std::wstring projectPath; // Full path to the project
	std::wstring ideName; // Name of the IDE

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

	~JbAction() override {
	}
};
