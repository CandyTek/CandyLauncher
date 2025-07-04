#pragma once
#include "../PinyinHelper.h"
#include "../RunCommandAction.cpp"
using CallbackFunction = std::function<void()>;

class ActionNormal final : public RunCommandAction
{
public:
	CallbackFunction callback1;

	explicit ActionNormal(
		const std::wstring& justName,
		const std::wstring& description,
		const std::wstring& icon,
		const CallbackFunction& callback
	): callback1(callback)
	{
		RunCommand = PinyinHelper::GetPinyinLongStr(MyToLower(justName));
		targetFilePath = icon;
		SetTitle(justName);
		SetSubtitle(description);
		SetExecutable(true);
	}

	// 运行命令
	void Invoke() override
	{
		if (callback1) callback1();
	}
};
