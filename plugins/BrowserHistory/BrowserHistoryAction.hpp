#pragma once
#include <string>
#include "plugins/sdk/CandyPluginSdk.hpp"

class BrowserHistoryAction final : public candy::ActionBase {
public:
	// 完整地址，UTF-8（History 库里本来就是 UTF-8，地址几乎全是 ASCII，比宽字符省一半；Edge 历史里带 token 的
	// 地址平均 1000 多字符，2000 条就是好几 MB）。只在打开 / 复制时转成宽字符，见 BrowserHistoryPlugin.cpp
	std::string url;
};
