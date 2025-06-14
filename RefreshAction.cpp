﻿#pragma once
#include "ActionBase.cpp"
#include "PinyinHelper.h"
#include "RunCommandAction.cpp"
#include "DataKeeper.hpp"
#include <wrl/client.h>
// 不能少
// #include <shellapi.h>

class RefreshAction final : public RunCommandAction
{
public:
	CallbackFunction callback1;

	explicit RefreshAction(
		const CallbackFunction callback
	):callback1(callback)
	{
		const std::wstring justName = L"刷新列表";
		RunCommand = PinyinHelper::GetPinyinLongStr(MyToLower(justName));
		targetFilePath = EXE_FOLDER_PATH + L"\\refresh.ico";
		SetTitle(justName);
		SetSubtitle(L"刷新运行配置");
		SetExecutable(true);

	}

	// 运行命令
	void Invoke() override
	{
		callback1(0);
	}
};
