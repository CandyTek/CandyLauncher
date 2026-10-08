#pragma once
#include <memory>
#include "plugins/BaseAction.hpp"
#include "util/BitmapUtil.hpp"
#include "VscPluginData.hpp"

class VscAction final : public BaseAction {
public:
	VscAction() {
		pluginId = m_pluginId;
	}

	// 同一软件的所有条目共享图标
	std::shared_ptr<LazySysImageIndex> icon;
	std::wstring title;
	std::wstring subTitle;

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
		static std::wstring empty;
		return icon ? icon->path : empty;
	}

	int getIconFilePathIndex() override {
		return icon ? icon->get() : -1;
	}

	HBITMAP getIconBitmap() override {
		return nullptr;
	}
};
