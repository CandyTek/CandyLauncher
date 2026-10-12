#pragma once
#include "ValueGeneratorPluginData.hpp"

class ValueGeneratorAction final : public candy::ActionBase {
public:
	std::wstring arg;  // 存储执行时需要的参数
	// 直接计算命令的计算结果，Ctrl+C 复制
	std::wstring result;
};
