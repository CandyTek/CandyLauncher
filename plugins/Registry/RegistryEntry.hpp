#pragma once
#include <windows.h>
#include <string>
#include <memory>

// 注册表条目类，封装注册表键或值的信息
class RegistryEntry {
public:
	std::wstring keyPath;        // 完整键路径
	HKEY key = nullptr;          // 注册表键句柄：预定义根键（不拥有）或 RegOpenKeyEx 打开的键（由 keyOwner 关闭）
	std::wstring valueName;      // 值名称（空表示是键本身）
	std::wstring valueData;      // 值数据
	std::wstring valueType;      // 值类型
	bool hasException = false;   // 是否有异常
	std::wstring exceptionMsg;   // 异常信息
	int subKeyCount = 0;         // 子键数量
	int valueCount = 0;          // 值数量
	// 打开的键句柄的所有权。同一个键下的多个值条目共享同一个 owner，最后一个条目销毁时才 RegCloseKey；
	// 预定义根键没有 owner。以前条目不关句柄，每次按键搜索都泄漏一批句柄
	std::shared_ptr<void> keyOwner;

	// 把 RegOpenKeyEx 打开的句柄交给 shared_ptr 管理
	static std::shared_ptr<void> OwnKey(HKEY opened) {
		return std::shared_ptr<void>(static_cast<void*>(opened), [](void* handle) {
			if (handle) RegCloseKey(static_cast<HKEY>(handle));
		});
	}

	// 构造函数 - 用于错误情况
	RegistryEntry(const std::wstring& path, const std::wstring& exception)
		: keyPath(path), hasException(true), exceptionMsg(exception) {}

	// 构造函数 - 用于注册表键；owned 为 true 表示 k 是 RegOpenKeyEx 打开的句柄，由条目负责关闭
	RegistryEntry(HKEY k, const std::wstring& path, int subKeys, int values, bool owned = false)
		: keyPath(path), key(k), subKeyCount(subKeys), valueCount(values), keyOwner(owned ? OwnKey(k) : nullptr) {}

	// 构造函数 - 用于注册表值；与所属键条目共享句柄所有权
	RegistryEntry(const RegistryEntry& keyEntry, const std::wstring& valName,
				const std::wstring& valData, const std::wstring& valType)
		: keyPath(keyEntry.keyPath), key(keyEntry.key), valueName(valName), valueData(valData), valueType(valType),
		keyOwner(keyEntry.keyOwner) {}

	// 禁止复制构造和赋值
	RegistryEntry(const RegistryEntry&) = delete;
	RegistryEntry& operator=(const RegistryEntry&) = delete;

	// 允许移动构造和赋值
	RegistryEntry(RegistryEntry&& other) noexcept = default;
	RegistryEntry& operator=(RegistryEntry&& other) noexcept = default;

	~RegistryEntry() = default;

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
