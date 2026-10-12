#pragma once
#include <string>

#include "FeatureLaunchPluginData.hpp"
#include "../../util/BitmapUtil.hpp"

// 每一项对应一个主程序命令（见 CL_HostApi::invoke_app_command），回车时由主程序执行
class FeatureLaunchAction final : public candy::ActionBase {
public:
	std::wstring commandKey;

	FeatureLaunchAction(const std::wstring& title2, const std::wstring& subTitle2, const int shell32IconIndex,
						const std::wstring& command) {
		title = title2;
		subtitle = subTitle2;
		commandKey = command;
		stableKey = command;
		iconBitmap = LoadShell32IconAsBitmap(shell32IconIndex);
	}
};
