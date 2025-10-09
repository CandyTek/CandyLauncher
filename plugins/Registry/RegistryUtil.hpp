#pragma once

#include "RegistryAction.hpp"
#include "RegistryEntry.hpp"
#include <windows.h>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>

#include "util/StringUtil.hpp"
#include "util/LogUtil.hpp"

// 注册表根键映射
static const std::map<std::wstring, HKEY> BaseKeyMap = {
	// 短名称
	{L"HKCR", HKEY_CLASSES_ROOT},
	{L"HKCC", HKEY_CURRENT_CONFIG},
	{L"HKCU", HKEY_CURRENT_USER},
	{L"HKLM", HKEY_LOCAL_MACHINE},
	{L"HKPD", HKEY_PERFORMANCE_DATA},
	{L"HKU", HKEY_USERS},
	// 完整名称
	{L"HKEY_CLASSES_ROOT", HKEY_CLASSES_ROOT},
	{L"HKEY_CURRENT_CONFIG", HKEY_CURRENT_CONFIG},
	{L"HKEY_CURRENT_USER", HKEY_CURRENT_USER},
	{L"HKEY_LOCAL_MACHINE", HKEY_LOCAL_MACHINE},
	{L"HKEY_PERFORMANCE_DATA", HKEY_PERFORMANCE_DATA},
	{L"HKEY_USERS", HKEY_USERS},
};

// 短名称映射
static const std::map<std::wstring, std::wstring> BaseKeyShortNames = {
	{L"HKEY_CLASSES_ROOT", L"HKCR"},
	{L"HKEY_CURRENT_CONFIG", L"HKCC"},
	{L"HKEY_CURRENT_USER", L"HKCU"},
	{L"HKEY_LOCAL_MACHINE", L"HKLM"},
	{L"HKEY_PERFORMANCE_DATA", L"HKPD"},
	{L"HKEY_USERS", L"HKU"},
};

// 获取所有根键
static std::vector<RegistryEntry> GetAllBaseKeys() {
	std::vector<RegistryEntry> result;

	// 添加六个主要的根键
	result.push_back(RegistryEntry(HKEY_CLASSES_ROOT, L"HKEY_CLASSES_ROOT", 0, 0));
	result.push_back(RegistryEntry(HKEY_CURRENT_CONFIG, L"HKEY_CURRENT_CONFIG", 0, 0));
	result.push_back(RegistryEntry(HKEY_CURRENT_USER, L"HKEY_CURRENT_USER", 0, 0));
	result.push_back(RegistryEntry(HKEY_LOCAL_MACHINE, L"HKEY_LOCAL_MACHINE", 0, 0));
	result.push_back(RegistryEntry(HKEY_PERFORMANCE_DATA, L"HKEY_PERFORMANCE_DATA", 0, 0));
	result.push_back(RegistryEntry(HKEY_USERS, L"HKEY_USERS", 0, 0));

	return result;
}

// 从查询中获取根键和子键路径
static std::pair<std::vector<HKEY>, std::wstring> GetRegistryBaseKey(const std::wstring& query) {
	std::vector<HKEY> baseKeys;
	std::wstring subKey;

	if (query.empty()) {
		return {baseKeys, subKey};
	}

	// 分割路径
	size_t firstBackslash = query.find(L'\\');
	std::wstring baseKeyPart = (firstBackslash != std::wstring::npos)
		? query.substr(0, firstBackslash)
		: query;

	if (firstBackslash != std::wstring::npos) {
		subKey = query.substr(firstBackslash + 1);
	}

	// 查找匹配的根键（不区分大小写）
	std::wstring baseKeyPartUpper = baseKeyPart;
	std::transform(baseKeyPartUpper.begin(), baseKeyPartUpper.end(),
				baseKeyPartUpper.begin(), ::towupper);

	for (const auto& [key, hkey] : BaseKeyMap) {
		std::wstring keyUpper = key;
		std::transform(keyUpper.begin(), keyUpper.end(), keyUpper.begin(), ::towupper);

		if (keyUpper.find(baseKeyPartUpper) == 0) {
			// 避免重复添加相同的 HKEY
			if (std::find(baseKeys.begin(), baseKeys.end(), hkey) == baseKeys.end()) {
				baseKeys.push_back(hkey);
			}
		}
	}

	return {baseKeys, subKey};
}

// 获取根键的完整名称
static std::wstring GetBaseKeyName(HKEY key) {
	if (key == HKEY_CLASSES_ROOT) return L"HKEY_CLASSES_ROOT";
	if (key == HKEY_CURRENT_CONFIG) return L"HKEY_CURRENT_CONFIG";
	if (key == HKEY_CURRENT_USER) return L"HKEY_CURRENT_USER";
	if (key == HKEY_LOCAL_MACHINE) return L"HKEY_LOCAL_MACHINE";
	if (key == HKEY_PERFORMANCE_DATA) return L"HKEY_PERFORMANCE_DATA";
	if (key == HKEY_USERS) return L"HKEY_USERS";
	return L"";
}

// 获取注册表键的短名称（用于截断长路径）
static std::wstring GetKeyWithShortBaseKey(const std::wstring& keyPath) {
	for (const auto& [fullName, shortName] : BaseKeyShortNames) {
		if (keyPath.find(fullName) == 0) {
			return shortName + keyPath.substr(fullName.length());
		}
	}
	return keyPath;
}

// 截断文本（从左侧或右侧）
static std::wstring TruncateText(const std::wstring& text, size_t maxLength, bool fromLeft = true) {
	if (text.length() <= maxLength) {
		return text;
	}

	if (fromLeft) {
		// 先尝试使用短名称
		std::wstring shortText = GetKeyWithShortBaseKey(text);
		if (shortText.length() <= maxLength) {
			return shortText;
		}
		// 还是太长，截断
		return L"..." + shortText.substr(shortText.length() - maxLength + 3);
	} else {
		return text.substr(0, maxLength - 3) + L"...";
	}
}

// 获取注册表值的字符串表示
static std::wstring GetValueString(HKEY key, const std::wstring& valueName, DWORD type, size_t maxLength = 1000) {
	DWORD dataSize = 0;
	LONG result = RegQueryValueExW(key, valueName.c_str(), nullptr, nullptr, nullptr, &dataSize);

	if (result != ERROR_SUCCESS || dataSize == 0) {
		return L"";
	}

	std::vector<BYTE> data(dataSize);
	result = RegQueryValueExW(key, valueName.c_str(), nullptr, nullptr, data.data(), &dataSize);

	if (result != ERROR_SUCCESS) {
		return L"";
	}

	std::wostringstream oss;

	switch (type) {
		case REG_DWORD: {
			if (dataSize >= sizeof(DWORD)) {
				DWORD value = *reinterpret_cast<DWORD*>(data.data());
				oss << L"0x" << std::hex << std::uppercase << std::setfill(L'0') << std::setw(8) << value
					<< L" (" << std::dec << value << L")";
			}
			break;
		}
		case REG_QWORD: {
			if (dataSize >= sizeof(ULONGLONG)) {
				ULONGLONG value = *reinterpret_cast<ULONGLONG*>(data.data());
				oss << L"0x" << std::hex << std::uppercase << std::setfill(L'0') << std::setw(16) << value
					<< L" (" << std::dec << value << L")";
			}
			break;
		}
		case REG_BINARY: {
			for (size_t i = 0; i < dataSize && i < 32; i++) {
				if (i > 0) oss << L" ";
				oss << std::hex << std::uppercase << std::setfill(L'0') << std::setw(2) << (int)data[i];
			}
			if (dataSize > 32) oss << L"...";
			break;
		}
		case REG_SZ:
		case REG_EXPAND_SZ:
		case REG_MULTI_SZ: {
			std::wstring str(reinterpret_cast<wchar_t*>(data.data()), dataSize / sizeof(wchar_t));
			// 移除尾部的 null 字符
			while (!str.empty() && str.back() == L'\0') {
				str.pop_back();
			}
			oss << str;
			break;
		}
		default:
			oss << L"(Binary data)";
			break;
	}

	std::wstring valueStr = oss.str();
	if (valueStr.length() > maxLength) {
		return valueStr.substr(0, maxLength - 3) + L"...";
	}
	return valueStr;
}

// 获取注册表值类型的字符串
static std::wstring GetValueTypeName(DWORD type) {
	switch (type) {
		case REG_NONE: return L"REG_NONE";
		case REG_SZ: return L"REG_SZ";
		case REG_EXPAND_SZ: return L"REG_EXPAND_SZ";
		case REG_BINARY: return L"REG_BINARY";
		case REG_DWORD: return L"REG_DWORD";
		case REG_DWORD_BIG_ENDIAN: return L"REG_DWORD_BIG_ENDIAN";
		case REG_LINK: return L"REG_LINK";
		case REG_MULTI_SZ: return L"REG_MULTI_SZ";
		case REG_RESOURCE_LIST: return L"REG_RESOURCE_LIST";
		case REG_QWORD: return L"REG_QWORD";
		default: return L"REG_UNKNOWN";
	}
}

// 查找子键（优化版本：限制最大结果数，避免枚举过多）
static std::vector<RegistryEntry> FindSubKey(HKEY parentKey, const std::wstring& parentPath,
											const std::wstring& searchSubKey) {
	const int maxResultsLimit = maxResults > 0 ? maxResults : 2000;
	std::vector<RegistryEntry> result;

	try {
		// 如果搜索词为空，只返回提示信息，不枚举所有子键
		if (searchSubKey.empty()) {
			// 获取子键总数
			DWORD totalSubKeys = 0;
			RegQueryInfoKeyW(parentKey, nullptr, nullptr, nullptr, &totalSubKeys,
						nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

			// 返回一个提示条目
			std::wstring hint = L"请继续输入子键名称（此键包含 " + std::to_wstring(totalSubKeys) + L" 个子键）";
			result.push_back(RegistryEntry(parentPath, hint));
			return result;
		}

		// 转换搜索词为大写（用于不区分大小写匹配）
		std::wstring searchUpper = searchSubKey;
		std::transform(searchUpper.begin(), searchUpper.end(), searchUpper.begin(), ::towupper);

		// 枚举子键（限制数量）
		DWORD index = 0;
		wchar_t subKeyName[256];
		DWORD subKeyNameSize;
		int matchCount = 0;

		while (matchCount < maxResultsLimit) {
			subKeyNameSize = 256;
			LONG enumResult = RegEnumKeyExW(parentKey, index++, subKeyName, &subKeyNameSize,
										nullptr, nullptr, nullptr, nullptr);

			if (enumResult == ERROR_NO_MORE_ITEMS) {
				break;
			}

			if (enumResult != ERROR_SUCCESS) {
				continue;
			}

			std::wstring subKeyNameStr(subKeyName, subKeyNameSize);

			// 不区分大小写的前缀匹配
			std::wstring subKeyUpper = subKeyNameStr;
			std::transform(subKeyUpper.begin(), subKeyUpper.end(), subKeyUpper.begin(), ::towupper);

			if (subKeyUpper.find(searchUpper) != 0) {
				continue;
			}

			// 找到匹配项
			matchCount++;

			// 如果完全匹配，只返回这一个
			if (subKeyUpper == searchUpper) {
				HKEY subKey;
				std::wstring fullPath = parentPath + L"\\" + subKeyNameStr;
				LONG openResult = RegOpenKeyExW(parentKey, subKeyNameStr.c_str(), 0, KEY_READ, &subKey);

				if (openResult == ERROR_SUCCESS) {
					DWORD subKeyCount = 0, valueCount = 0;
					RegQueryInfoKeyW(subKey, nullptr, nullptr, nullptr, &subKeyCount,
								nullptr, nullptr, &valueCount, nullptr, nullptr, nullptr, nullptr);

					result.clear();  // 清空之前的部分匹配
					result.push_back(RegistryEntry(subKey, fullPath, subKeyCount, valueCount));
					return result;
				} else {
					result.clear();
					result.push_back(RegistryEntry(fullPath, L"Access denied or key not found"));
					return result;
				}
			}

			// 部分匹配，打开键
			HKEY subKey;
			std::wstring fullPath = parentPath + L"\\" + subKeyNameStr;
			LONG openResult = RegOpenKeyExW(parentKey, subKeyNameStr.c_str(), 0, KEY_READ, &subKey);

			if (openResult == ERROR_SUCCESS) {
				DWORD subKeyCount = 0, valueCount = 0;
				RegQueryInfoKeyW(subKey, nullptr, nullptr, nullptr, &subKeyCount,
							nullptr, nullptr, &valueCount, nullptr, nullptr, nullptr, nullptr);

				result.push_back(RegistryEntry(subKey, fullPath, subKeyCount, valueCount));
			} else {
				result.push_back(RegistryEntry(fullPath, L"Access denied"));
			}
		}

		// 如果达到了最大限制，添加提示
		if (matchCount >= maxResultsLimit) {
			std::wstring hint = L"显示前 " + std::to_wstring(maxResultsLimit) + L" 个结果，请输入更多字符以缩小范围";
			result.push_back(RegistryEntry(parentPath + L"\\...", hint));
		}

	} catch (const std::exception& e) {
		result.push_back(RegistryEntry(parentPath, utf8_to_wide(e.what())));
	}

	return result;
}

// 搜索子键路径
static std::vector<RegistryEntry> SearchForSubKey(HKEY baseKey, const std::wstring& subKeyPath) {
	std::wstring basePath = GetBaseKeyName(baseKey);

	if (subKeyPath.empty()) {
		return FindSubKey(baseKey, basePath, L"");
	}

	// 分割子键路径
	std::vector<std::wstring> parts;
	std::wstring current;
	for (wchar_t ch : subKeyPath) {
		if (ch == L'\\') {
			if (!current.empty()) {
				parts.push_back(current);
				current.clear();
			}
		} else {
			current += ch;
		}
	}
	if (!current.empty()) {
		parts.push_back(current);
	}

	HKEY currentKey = baseKey;
	std::wstring currentPath = basePath;
	std::vector<RegistryEntry> result;
	bool needCloseKey = false;

	for (size_t i = 0; i < parts.size(); i++) {
		result = FindSubKey(currentKey, currentPath, parts[i]);

		// 关闭之前打开的键（但不关闭根键）
		if (needCloseKey && currentKey != baseKey) {
			RegCloseKey(currentKey);
			needCloseKey = false;
		}

		if (result.empty()) {
			return result;
		}

		if (result.size() == 1 && i < parts.size() - 1) {
			// 只找到一个，继续深入
			if (result[0].key) {
				currentKey = result[0].key;
				currentPath = result[0].keyPath;
				needCloseKey = true;
			} else {
				// 有异常，返回错误
				return result;
			}
		} else {
			// 找到多个或已到最后一部分
			break;
		}
	}

	return result;
}

// 从注册表键获取所有值
static std::vector<RegistryEntry> GetValuesFromKey(HKEY key, const std::wstring& keyPath,
												const std::wstring& searchValue = L"") {
	std::vector<RegistryEntry> result;

	try {
		DWORD valueCount = 0;
		RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
					&valueCount, nullptr, nullptr, nullptr, nullptr);

		if (valueCount == 0) {
			return result;
		}

		std::vector<std::tuple<std::wstring, DWORD, std::wstring>> values;

		// 枚举所有值
		for (DWORD i = 0; i < valueCount; i++) {
			wchar_t valueName[16384];
			DWORD valueNameSize = 16384;
			DWORD type = 0;

			LONG enumResult = RegEnumValueW(key, i, valueName, &valueNameSize,
										nullptr, &type, nullptr, nullptr);

			if (enumResult != ERROR_SUCCESS) {
				continue;
			}

			std::wstring valueNameStr(valueName, valueNameSize);
			std::wstring valueDataStr = GetValueString(key, valueNameStr, type, 50);

			// 如果有搜索条件，过滤
			if (!searchValue.empty()) {
				std::wstring nameUpper = valueNameStr;
				std::wstring dataUpper = valueDataStr;
				std::wstring searchUpper = searchValue;
				std::transform(nameUpper.begin(), nameUpper.end(), nameUpper.begin(), ::towupper);
				std::transform(dataUpper.begin(), dataUpper.end(), dataUpper.begin(), ::towupper);
				std::transform(searchUpper.begin(), searchUpper.end(), searchUpper.begin(), ::towupper);

				if (nameUpper.find(searchUpper) == std::wstring::npos &&
					dataUpper.find(searchUpper) == std::wstring::npos) {
					continue;
				}
			}

			values.push_back({valueNameStr, type, valueDataStr});
		}

		// 按名称排序
		std::sort(values.begin(), values.end(),
			[](const auto& a, const auto& b) { return std::get<0>(a) < std::get<0>(b); });

		// 创建结果条目
		for (const auto& [valName, type, valData] : values) {
			std::wstring fullValueData = GetValueString(key, valName, type);
			std::wstring typeStr = GetValueTypeName(type);

			result.push_back(RegistryEntry(key, keyPath, valName, fullValueData, typeStr));
		}

	} catch (const std::exception& e) {
		result.push_back(RegistryEntry(keyPath, utf8_to_wide(e.what())));
	}

	return result;
}

// 解析查询，分离键路径和值名称
static bool ParseQuery(const std::wstring& query, std::wstring& keyPart, std::wstring& valuePart) {
	// 查找 "\\" 分隔符（表示要搜索值）
	size_t pos = query.find(L"\\\\");

	if (pos != std::wstring::npos) {
		keyPart = query.substr(0, pos);
		valuePart = query.substr(pos + 2);
		return true;
	}

	keyPart = query;
	valuePart = L"";
	return false;
}

// 打开注册表编辑器并跳转到指定键
static bool OpenRegistryEditor(const std::wstring& keyPath) {
	try {
		// 设置注册表编辑器的最后访问键
		HKEY regEditKey;
		LONG result = RegOpenKeyExW(HKEY_CURRENT_USER,
									L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit",
									0, KEY_WRITE, &regEditKey);

		if (result == ERROR_SUCCESS) {
			RegSetValueExW(regEditKey, L"LastKey", 0, REG_SZ,
						reinterpret_cast<const BYTE*>(keyPath.c_str()),
						static_cast<DWORD>((keyPath.length() + 1) * sizeof(wchar_t)));
			RegCloseKey(regEditKey);
		}

		// 启动注册表编辑器（使用管理员权限）
		SHELLEXECUTEINFOW sei = {sizeof(sei)};
		sei.lpVerb = L"runas";  // 以管理员身份运行
		sei.lpFile = L"regedit.exe";
		sei.lpParameters = L"-m";  // 允许多实例
		sei.nShow = SW_SHOWNORMAL;
		sei.fMask = SEE_MASK_NOCLOSEPROCESS;

		if (!ShellExecuteExW(&sei)) {
			// 如果管理员权限失败，尝试普通权限
			sei.lpVerb = L"open";
			if (!ShellExecuteExW(&sei)) {
				return false;
			}
		}

		return true;
	} catch (...) {
		return false;
	}
}
