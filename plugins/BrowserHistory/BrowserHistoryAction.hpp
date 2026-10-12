#pragma once
#include <string>
#include "plugins/sdk/CandyPluginSdk.hpp"

class BrowserHistoryAction final : public candy::ActionBase {
public:
	std::wstring url;
	std::wstring visitTime;  // 访问时间
	int visitCount = 0;      // 访问次数
};
