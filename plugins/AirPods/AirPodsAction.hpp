#pragma once

#include "AirPodsPluginData.hpp"

class AirPodsAction final : public BaseAction {
public:
	AirPodsAction() {
		pluginId = g_airPodsPluginId;
	}

	std::wstring title;
	std::wstring subTitle;
	std::wstring iconFilePath;
	std::wstring details;
	int iconIndex = -1;

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
		return iconIndex;
	}

	HBITMAP getIconBitmap() override {
		return nullptr;
	}
};
