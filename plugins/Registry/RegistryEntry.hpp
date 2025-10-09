#pragma once
#include <windows.h>
#include <string>
#include <memory>

// 注册表条目类，封装注册表键或值的信息
class RegistryEntry {
public:
	std::wstring keyPath;        // 完整键路径
	HKEY key = nullptr;          // 注册表键句柄（如果成功打开）
	std::wstring valueName;      // 值名称（空表示是键本身）
	std::wstring valueData;      // 值数据
	std::wstring valueType;      // 值类型
	bool hasException = false;   // 是否有异常
	std::wstring exceptionMsg;   // 异常信息
	int subKeyCount = 0;         // 子键数量
	int valueCount = 0;          // 值数量

	// 构造函数 - 用于错误情况
	RegistryEntry(const std::wstring& path, const std::wstring& exception)
		: keyPath(path), hasException(true), exceptionMsg(exception) {}

	// 构造函数 - 用于注册表键
	RegistryEntry(HKEY k, const std::wstring& path, int subKeys, int values)
		: key(k), keyPath(path), subKeyCount(subKeys), valueCount(values) {}

	// 构造函数 - 用于注册表值
	RegistryEntry(HKEY k, const std::wstring& path, const std::wstring& valName,
				const std::wstring& valData, const std::wstring& valType)
		: key(k), keyPath(path), valueName(valName), valueData(valData), valueType(valType) {}

	// 禁止复制构造和赋值
	RegistryEntry(const RegistryEntry&) = delete;
	RegistryEntry& operator=(const RegistryEntry&) = delete;

	// 允许移动构造和赋值
	RegistryEntry(RegistryEntry&& other) noexcept
		: keyPath(std::move(other.keyPath)),
		key(other.key),
		valueName(std::move(other.valueName)),
		valueData(std::move(other.valueData)),
		valueType(std::move(other.valueType)),
		hasException(other.hasException),
		exceptionMsg(std::move(other.exceptionMsg)),
		subKeyCount(other.subKeyCount),
		valueCount(other.valueCount) {
		other.key = nullptr;
	}

	RegistryEntry& operator=(RegistryEntry&& other) noexcept {
		if (this != &other) {
			if (key) RegCloseKey(key);
			keyPath = std::move(other.keyPath);
			key = other.key;
			valueName = std::move(other.valueName);
			valueData = std::move(other.valueData);
			valueType = std::move(other.valueType);
			hasException = other.hasException;
			exceptionMsg = std::move(other.exceptionMsg);
			subKeyCount = other.subKeyCount;
			valueCount = other.valueCount;
			other.key = nullptr;
		}
		return *this;
	}

	~RegistryEntry() {
		// 注意：这里不关闭 key，因为它可能是预定义的根键（如 HKEY_LOCAL_MACHINE）
		// 只有通过 RegOpenKeyEx 打开的键才需要关闭，会在工具函数中处理
	}

	// 获取注册表键路径
	std::wstring GetRegistryKey() const {
		return keyPath;
	}

	// 获取带值名称的完整路径
	std::wstring GetValueNameWithKey() const {
		if (valueName.empty()) {
			return keyPath;
		}
		return keyPath + L"\\\\" + valueName;
	}

	// 获取值数据
	std::wstring GetValueData() const {
		return valueData;
	}
};
