#pragma once

#include "../BaseAction.hpp"
#include "../../util/BitmapUtil.hpp"
#include "PluginBrowserPluginData.hpp"

class PluginBrowserAction final : public BaseAction {
public:
	std::wstring title;
	std::wstring subTitle;
	std::wstring iconFilePath;
	std::wstring packageName;
	bool enabled = false;
	bool canToggle = true;
	int iconFilePathIndex = -1;
	HBITMAP iconBitmap = nullptr;

	PluginBrowserAction() {
		pluginId = g_pluginBrowserId;
	}

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
		return iconBitmap;
	}

	~PluginBrowserAction() override {
		if (iconBitmap) {
			DeleteObject(iconBitmap);
			iconBitmap = nullptr;
		}
	}
};
