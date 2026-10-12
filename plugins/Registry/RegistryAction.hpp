#pragma once
#include <string>
#include "plugins/sdk/CandyPluginSdk.hpp"

class RegistryAction final : public candy::ActionBase {
public:
	std::wstring keyPath;        // 注册表完整路径
	std::wstring valueName;      // 值名称（如果有）
	std::wstring valueData;      // 值数据（如果有）
	std::wstring valueType;      // 值类型（REG_SZ, REG_DWORD 等）
	bool hasException = false;   // 是否有访问异常
	std::wstring exceptionMsg;   // 异常信息
	int subKeyCount = 0;         // 子键数量
	int valueCount = 0;          // 值数量
};
