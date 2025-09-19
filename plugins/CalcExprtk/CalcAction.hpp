#pragma once
#include <string>
#include <ShlObj.h>

class CalcAction final : public ActionBase
{
public:
	// 图标，只要是文件就可以
	std::wstring title;
	std::wstring subTitle;
	int8_t id;
	int iconFilePathIndex = -1;

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
		return LR"(c:\Windows\System32\calc.exe)";
	}

	int getIconFilePathIndex() override
	{
		return iconFilePathIndex;
	}

	HBITMAP getIconBitmap() override
	{
		return nullptr;
	}
	

	~CalcAction() override
	{
	}
};
