#pragma once

#include "PluginData.hpp"
#include <string>
#include <ShlObj.h>

class CalcAction final : public BaseAction {
public:
	CalcAction() {
		pluginId = m_pluginId;
	}

	// 图标，只要是文件就可以
	std::wstring title;
	std::wstring subTitle;
	int8_t id = 0;
	int iconFilePathIndex = -1;

	std::wstring& getTitle() override {
		return title;
	}

	std::wstring& getSubTitle() override {
		return subTitle;
	}

	std::wstring& getIconFilePath() override {
		static std::wstring path = LR"(c:\Windows\System32\calc.exe)";
		return path;
	}

	int getIconFilePathIndex() override {
		return iconFilePathIndex;
	}

	HBITMAP getIconBitmap() override {
		return nullptr;
	}


	~CalcAction() override {
	}
};
