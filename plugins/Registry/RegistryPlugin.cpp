#include "../Plugin.hpp"
#include <memory>
#include <vector>

#include "RegistryAction.hpp"
#include "RegistryPluginData.hpp"
#include "RegistryUtil.hpp"
#include "../../util/StringUtil.hpp"
#include "../../util/BitmapUtil.hpp"

class RegistryPlugin : public IPlugin {
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;
	std::wstring startStr = L": ";
	std::wstring iconPath;

public:
	RegistryPlugin() = default;
	~RegistryPlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"注册表导航";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.registryplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"Windows 注册表导航";
	}

	bool Initialize(IPluginHost* host) override {
		m_host = host;
		iconPath = LR"(C:\Windows\regedit.exe)";
		return m_host != nullptr;
	}

	void OnPluginIdChange(const uint16_t pluginId) override {
		m_pluginId = pluginId;
	}

	void RefreshAllActions() override {
		if (!m_host) {
			Loge(L"Registry Plugin", L"RefreshAllActions: m_host is null");
			return;
		}

		ConsolePrintln(L"Registry Plugin", L"RefreshAllActions start");
		allPluginActions.clear();

		// 注册表插件不需要在启动时预加载所有内容
		// 只在用户输入时动态搜索

		ConsolePrintln(L"Registry Plugin", L"Registry plugin initialized");
	}

	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.registryplugin.start_str",
			"title": "直接激活命令（默认使用 ': ' 前缀）",
			"type": "string",
			"subPage": "plugin",
			"defValue": ": "
		},
		{
			"key": "com.candytek.registryplugin.max_results",
			"type": "long",
			"title": "最大结果数",
			"subPage": "plugin",
			"defValue": 2000
		}
	]
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		startStr = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.registryplugin.start_str").stringValue);
		maxResults = static_cast<int>(m_host->GetSettingsMap().at("com.candytek.registryplugin.max_results").intValue);
	}

	void Shutdown() override {
		if (m_host) {
			allPluginActions.clear();
		}
		m_host = nullptr;
	}

	std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() override {
		// 注册表插件需要前缀触发，不参与全局匹配
		return {};
	}

	std::vector<std::shared_ptr<BaseAction>> InterceptInputShowResultsDirectly(const std::wstring& input) override {
		if (!m_host || startStr.empty()) return {};

		if (!MyStartsWith2(input, startStr)) {
			return {};
		}
		MethodTimerStart();

		// 移除前缀
		std::wstring query = input.substr(startStr.size());

		// 如果查询为空，显示所有根键
		if (query.empty()) {
			return ConvertRegistryEntriesToActions(GetAllBaseKeys());
		}

		// 解析查询
		std::wstring keyPart, valuePart;
		bool searchForValue = ParseQuery(query, keyPart, valuePart);

		// 获取根键
		auto [baseKeys, subKey] = GetRegistryBaseKey(keyPart);

		if (baseKeys.empty()) {
			// 没有找到匹配的根键，显示所有根键
			return ConvertRegistryEntriesToActions(GetAllBaseKeys());
		}

		if (baseKeys.size() == 1) {
			// 只找到一个根键，搜索子键
			auto entries = SearchForSubKey(baseKeys[0], subKey);

			// 如果搜索值名称且只找到一个键
			if (searchForValue && entries.size() == 1 && entries[0].key) {
				auto valueEntries = GetValuesFromKey(entries[0].key, entries[0].keyPath, valuePart);
				return ConvertRegistryEntriesToActions(valueEntries, true);
			}
			MethodTimerEnd(L"InterceptInputShowResultsDirectly");

			return ConvertRegistryEntriesToActions(entries);
		} else {
			// 找到多个根键，显示它们
			std::vector<RegistryEntry> entries;
			for (HKEY key : baseKeys) {
				entries.push_back(RegistryEntry(key, GetBaseKeyName(key), 0, 0));
			}
			return ConvertRegistryEntriesToActions(entries);
		}
	}

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		auto action1 = std::dynamic_pointer_cast<RegistryAction>(action);
		if (!action1) return false;

		try {
			// 打开注册表编辑器并跳转到该键
			if (!OpenRegistryEditor(action1->keyPath)) {
				ConsolePrintln(L"Registry Plugin", L"Failed to open registry editor");
				return false;
			}
		} catch (...) {
			return false;
		}

		return true;
	}

private:
	// 将 RegistryEntry 转换为 RegistryAction
	std::vector<std::shared_ptr<BaseAction>> ConvertRegistryEntriesToActions(
		const std::vector<RegistryEntry>& entries, bool isValue = false) {
		std::vector<std::shared_ptr<BaseAction>> result;
		int iconIndex = GetSysImageIndex(iconPath);
		ConsolePrintln(L"RegistryPlugin", L"列表数量：" + std::to_wstring(entries.size()));

		for (const auto& entry : entries) {
			auto action = std::make_shared<RegistryAction>();

			action->keyPath = entry.keyPath;
			action->valueName = entry.valueName;
			action->valueData = entry.valueData;
			action->valueType = entry.valueType;
			action->hasException = entry.hasException;
			action->exceptionMsg = entry.exceptionMsg;
			action->subKeyCount = entry.subKeyCount;
			action->valueCount = entry.valueCount;

			action->iconFilePath = iconPath;
			action->iconFilePathIndex = iconIndex;

			if (isValue) {
				// 这是一个注册表值
				std::wstring displayName = entry.valueName.empty() ? L"(Default)" : entry.valueName;
				action->title = TruncateText(displayName, 100, true);

				// 子标题显示类型和值
				std::wstring valuePreview = GetValueString(entry.key, entry.valueName,
															entry.valueType == L"REG_SZ"
																? REG_SZ
																: entry.valueType == L"REG_DWORD"
																? REG_DWORD
																: entry.valueType == L"REG_QWORD"
																? REG_QWORD
																: entry.valueType == L"REG_BINARY"
																? REG_BINARY
																: entry.valueType == L"REG_EXPAND_SZ"
																? REG_EXPAND_SZ
																: entry.valueType == L"REG_MULTI_SZ"
																? REG_MULTI_SZ
																: REG_NONE, 50);

				action->subTitle = L"Type: " + entry.valueType + L" - Value: " + valuePreview;
			} else if (entry.hasException) {
				// 有异常的键
				action->title = TruncateText(entry.keyPath, 100, true);
				action->subTitle = TruncateText(entry.exceptionMsg, 150, false);
			} else {
				// 普通注册表键
				action->title = TruncateText(entry.keyPath, 100, true);
				action->subTitle = L"子键: " + std::to_wstring(entry.subKeyCount) +
					L" - 值: " + std::to_wstring(entry.valueCount);
			}

			// 设置匹配文本
			action->matchText = action->title;
			// action->matchText = m_host->GetTheProcessedMatchingText(action->title);

			result.push_back(action);
		}
		return result;
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new RegistryPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
