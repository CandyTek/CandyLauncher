#pragma once
#include "UnitConverterPluginData.hpp"

class UnitConverterAction final : public candy::ActionBase {
public:
	// 回车复制的数值部分（不含单位）
	std::wstring copyText;
};
