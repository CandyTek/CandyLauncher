#pragma once
#include <memory>
#include "plugins/BaseAction.hpp"
#include "util/BitmapUtil.hpp"
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

	// 同一软件的所有条目共享图标
	std::shared_ptr<LazySysImageIndex> icon;
	std::wstring title;
	std::wstring subTitle;

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

	~VisualStudioAction() override {
	}
};
