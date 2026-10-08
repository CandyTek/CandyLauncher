#pragma once
#include <memory>
#include "util/BitmapUtil.hpp"
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

	// 同一浏览器的所有条目共享图标
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

	~BrowserHistoryAction() override {
	}
};
