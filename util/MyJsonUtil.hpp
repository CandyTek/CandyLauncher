#include <codecvt>
#include <string>

#include "json.hpp"
#include "common/GlobalState.hpp"

inline std::string makeCompactJsonWithoutBraces(const std::string& path) {
	// 构造 JSON
	nlohmann::json j;
	j["arg"] = path;

	// 生成紧凑的 JSON 字符串
	std::string json_str = j.dump();

	return json_str;
}

inline std::string makeCompactJsonWithoutBraces(const std::wstring& path) {
	return makeCompactJsonWithoutBraces(wide_to_utf8(path));
}


// 解析 settings.json 程序配置文本
inline std::vector<SettingItem> ParseConfig(const std::string& configStr, uint16_t pluginId) {
	// 不用做错误处理，这个基础设置json一定要是好的
	nlohmann::json j = nlohmann::json::parse(configStr);
	std::vector<SettingItem> settings;

	std::function<void(const nlohmann::json&, std::vector<SettingItem>&)> parseItems =
		[&](const nlohmann::json& items, std::vector<SettingItem>& targetSettings) {
			for (const nlohmann::basic_json<>& item : items) {
				SettingItem setting;
				setting.key = item["key"].get<std::string>();
				setting.type = item["type"].get<std::string>();
				setting.title = item["title"].get<std::string>();
				setting.pluginId = pluginId;
				setting.subPageIndex = GetTabIndexForSetting(item["subPage"].get<std::string>());

				// 如果存在 entries，就解析；否则设为空
				if (item.contains("entries")) {
					setting.entries = item["entries"].get<std::vector<std::string>>();
				}

				if (item.contains("entryValues")) {
					setting.entryValues = item["entryValues"].get<std::vector<std::string>>();
				}

				if (item.contains("defValue")) {
					setting.setValue(item["defValue"]); // 保持原始 JSON 类型，灵活性更高
				}

				// 处理expand类型的children
				if ((setting.type == "expand" || setting.type == "expandswitch") && item.contains("children")) {
					parseItems(item["children"], setting.children);
				}

				targetSettings.push_back(setting);
			}
	};

	parseItems(j["prefList"], settings);
	return settings;
}
