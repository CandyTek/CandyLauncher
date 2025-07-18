#pragma once
#include "../ActionBase.hpp"
#include "../PinyinHelper.h"
#include "../RunCommandAction.hpp"
#include "../DataKeeper.hpp"
#include <wrl/client.h>

#include "ActionNormal.hpp"
#include "../BaseTools.hpp"
// 不能少
// #include <shellapi.h>

class ActionRefresh final : public RunCommandAction
{
public:
	CallbackFunction callback1;

	explicit ActionRefresh(
		const CallbackFunction callback
	): RunCommandAction(L"", L""), callback1(callback)
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
