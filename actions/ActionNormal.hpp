#pragma once

#include "../AppTools.hpp"
#include "../PinyinHelper.h"
#include "../RunCommandAction.hpp"
using CallbackFunction = std::function<void()>;

class ActionNormal final : public RunCommandAction
{
public:
	CallbackFunction callback1;

	explicit ActionNormal(
		const std::wstring& justName,
		const std::wstring& description,
		const std::wstring& icon,
		CallbackFunction  callback
	): RunCommandAction(justName, description), callback1(std::move(callback))
	{
		targetFilePath = icon;
		SetSubtitle(description);
	}

	explicit ActionNormal(
		const std::wstring& justName,
		const std::wstring& description,
		const int nResID,
		CallbackFunction  callback
	): RunCommandAction(justName, description), callback1(std::move(callback))
	{
		hIcon = LoadIconAsBitmap(g_hInst,nResID,48,48);
		itemType = UWP_APP;
		SetSubtitle(description);
	}

	// 运行命令
	void Invoke() override
	{
		if (callback1) callback1();
	}
};
