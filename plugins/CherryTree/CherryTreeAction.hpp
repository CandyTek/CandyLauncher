#pragma once
#include <memory>
#include "plugins/BaseAction.hpp"
#include "util/BitmapUtil.hpp"
#include "CherryTreePluginData.hpp"

class CherryTreeAction final : public BaseAction {
public:
	CherryTreeAction() {
		pluginId = m_pluginId;
	}

	int nodeId = 0;
	std::wstring url;
	std::wstring text;


	// 同一语法类型的所有条目共享图标
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

	~CherryTreeAction() override {
	}
};
