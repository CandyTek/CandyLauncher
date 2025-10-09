#pragma once

#include <string>

#include "../util/json.hpp"

// 转换为布尔值的函数
inline bool JsonValueToBool(const nlohmann::json& v) {
	if (v.is_boolean()) return v.get<bool>();
	if (v.is_number_integer()) return v.get<int>() != 0; // 兼容 0/1
	if (v.is_string()) {
		std::string s = v.get<std::string>();
		std::transform(s.begin(), s.end(), s.begin(), ::tolower);
		return (s == "true" || s == "1" || s == "yes" || s == "on");
	}
	throw std::runtime_error("not a boolean-like value");
}

// 配置项结构
struct SettingItem {
	std::string key;
	std::string type;
	std::string title;
	uint8_t subPageIndex = 0; // 使用字节存储页面索引，节省内存
	std::vector<std::string> entries;
	std::vector<std::string> entryValues;
	std::vector<SettingItem> children; // 用于expand类型的子项
	bool isExpanded = false; // expand类型的展开状态
	uint16_t pluginId = 65535;

	// 基元配置值，方便程序使用
	int64_t intValue = 0;
	bool boolValue = false;
	double doubleValue = 0.0;
	std::string stringValue;
	std::vector<std::string> stringArr;

	void setValue(const nlohmann::json& _value) {
		// value = _value;
		if (type == "string" || type == "hotkeystring" || type == "list") {
			stringValue = _value.get<std::string>();
		} else if (type == "long") {
			intValue = _value.get<int64_t>();
		} else if (type == "bool" || type == "expandswitch") {
			boolValue = JsonValueToBool(_value);
		} else if (type == "double") {
			doubleValue = _value.get<double>();
		} else if (type == "stringArr") {
			stringArr.clear();
			for (const nlohmann::basic_json<>& s : _value) {
				stringArr.push_back(s.get<std::string>());
			}
		}
	}

	// private:
	// nlohmann::json value;
};
