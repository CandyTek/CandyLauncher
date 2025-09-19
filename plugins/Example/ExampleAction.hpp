#pragma once
#include <string>
#include <ShlObj.h>

class ExampleAction final : public ActionBase
{
public:
	// 图标，只要是文件就可以
	std::wstring iconFilePath;
	std::wstring title;
	std::wstring subTitle;
	std::wstring id;
	int iconFilePathIndex = -1;
	HBITMAP iconBitmap = nullptr;

	std::wstring getTitle() override
	{
		return title;
	}

	std::wstring getSubTitle() override
	{
		return subTitle;
	}

	std::wstring getIconFilePath() override
	{
		return iconFilePath;
	}

	int getIconFilePathIndex() override
	{
		return iconFilePathIndex;
	}

	HBITMAP getIconBitmap() override
	{
		return iconBitmap;
	}
	

	~ExampleAction() override
	{
		if (iconBitmap)
		{
			DeleteObject(iconBitmap);
			iconBitmap = nullptr;
		}
	}
};
