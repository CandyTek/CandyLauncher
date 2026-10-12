#pragma once
#include <string>
#include "plugins/sdk/CandyPluginSdk.hpp"

class WindowsSearchAction final : public candy::ActionBase {
public:
	std::wstring path; // 文件路径
	std::wstring fileName; // 文件名
};
