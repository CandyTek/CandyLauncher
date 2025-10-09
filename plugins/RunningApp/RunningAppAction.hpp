#pragma once
#include <string>
#include <ShlObj.h>

#include "util/BaseTools.hpp"
#include "PluginData.hpp"


class RunningAppAction final : public BaseAction
{
public:
	RunningAppAction() {
		pluginId = m_pluginId;
	}

	// 图标，只要是文件就可以
	std::wstring iconFilePath;
	std::wstring title;
	std::wstring subTitle;
	std::wstring runningAppHwnd;
	int iconFilePathIndex = -1;

	std::wstring& getTitle() override
	{
		return title;
	}

	std::wstring& getSubTitle() override
	{
		return subTitle;
	}

	std::wstring& getIconFilePath() override
	{
		return iconFilePath;
	}

	int getIconFilePathIndex() override
	{
		return iconFilePathIndex;
	}

	HBITMAP getIconBitmap() override
	{
		return nullptr;
	}

	void Invoke() const
	{
		try
		{
			showCurrectWindowSimple(reinterpret_cast<HWND>((static_cast<uintptr_t>(std::stoull(runningAppHwnd)))));
		}
		catch (...)
		{
		}

	}
	

	~RunningAppAction() override
	{
	}
};
