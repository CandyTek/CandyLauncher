#pragma once
#include "plugins/BaseAction.hpp"
#include "VscPluginData.hpp"

class VscAction final : public BaseAction {
public:
	VscAction() {
		pluginId = m_pluginId;
	}

	// 图标，只要是文件就可以
	std::wstring iconFilePath;
	std::wstring title;
	std::wstring subTitle;
	int iconFilePathIndex = -1;

	// Additional data for workspace
	std::wstring projectPath; // Full path to the project
	std::wstring originalUri; // Original VSCode URI for launching
	bool isWorkspaceFile = false; // Whether it's a .code-workspace file

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
