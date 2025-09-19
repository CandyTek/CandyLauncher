#pragma once

#include "ActionBase.hpp"
#include "../AppTools.hpp"
#include "../util/PinyinHelper.h"
using CallbackFunction = std::function<void()>;

class ActionNormal final : public ActionBase
{
public:
	CallbackFunction callback1;
	std::wstring iconFilePath;
	std::wstring title;
	std::wstring subTitle;
	int iconFilePathIndex = -1;
	HBITMAP iconBitmap = nullptr;

	explicit ActionNormal(
		const std::wstring& title2,
		const std::wstring& subTitle2,
		const std::wstring& icon,
		CallbackFunction callback
	): ActionBase(), callback1(std::move(callback))
	{
		title = title2;
		subTitle = subTitle2;
		iconFilePath = icon;
		matchText = (PinyinHelper::GetPinyinLongStr(MyToLower(title2)));
	}

	explicit ActionNormal(
		const std::wstring& title2,
		const std::wstring& subTitle2,
		const int nResID,
		CallbackFunction callback
	): ActionBase(), callback1(std::move(callback))
	{
		iconBitmap = LoadIconAsBitmap(g_hInst, nResID, 48, 48);
		// itemType = UWP_APP;
		title = title2;
		subTitle = subTitle2;
		matchText = (PinyinHelper::GetPinyinLongStr(MyToLower(title2)));
	}

	// 运行命令
	void Invoke()
	{
		if (callback1) callback1();
	}

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

};
