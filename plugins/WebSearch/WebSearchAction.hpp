#pragma once
#include "plugins/BaseAction.hpp"
#include "WebSearchPluginData.hpp"

class WebSearchAction final : public BaseAction {
public:
	WebSearchAction() {
		pluginId = m_pluginId;
	}

	std::wstring searchUrl;
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
		return empty;
	}

	int getIconFilePathIndex() override {
		return -1;
	}

	HBITMAP getIconBitmap() override {
		return nullptr;
	}

	~WebSearchAction() override {
	}
};


