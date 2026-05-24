#include "../Plugin.hpp"
#include <memory>
#include <fstream>

#include "BrowserHistoryAction.hpp"
#include "BrowserHistoryPluginData.hpp"
#include "BrowserHistoryUtil.hpp"
#include "../../util/StringUtil.hpp"

class BrowserHistoryPlugin : public IPlugin {
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;
	std::wstring startStr = L"h ";

public:
	BrowserHistoryPlugin() = default;
	~BrowserHistoryPlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"浏览器历史";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.browserhistoryplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"索引浏览器历史记录";
	}

	bool Initialize(IPluginHost* host) override {
		m_host = host;
		return m_host != nullptr;
	}

	void OnPluginIdChange(const uint16_t pluginId) override {
		m_pluginId = pluginId;
	}

	void RefreshAllActions() override {
		if (!m_host) {
			Loge(L"BrowserHistory Plugin", L"RefreshAllActions: m_host is null");
			return;
		}

		ConsolePrintln(L"BrowserHistory Plugin", L"RefreshAllActions start");
		allPluginActions.clear();

		// 获取所有浏览器历史记录
		allPluginActions = GetAllBrowserHistory();

		ConsolePrintln(L"BrowserHistory Plugin", L"Loaded " + std::to_wstring(allPluginActions.size()) + L" history items");
	}

	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.browserhistoryplugin.start_str",
			"title": "直接激活命令（留空表示不需要前缀）",
			"type": "string",
			"subPage": "plugin",
			"defValue": "h "
		},
		{
			"key": "com.candytek.browserhistoryplugin.matchtext_url",
			"type": "bool",
			"title": "搜索匹配历史记录URL",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.browserhistoryplugin.browser_list",
			"type": "stringArr",
			"title": "浏览器历史文件路径",
			"subPage": "plugin",
			"defValue": "edge"
		},
		{
			"key": "com.candytek.browserhistoryplugin.max_results",
			"type": "long",
			"title": "每个浏览器最大历史记录数",
			"subPage": "plugin",
			"defValue": 2000
		}
	]
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		startStr = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.browserhistoryplugin.start_str").stringValue);
		isMatchTextUrl = m_host->GetSettingsMap().at("com.candytek.browserhistoryplugin.matchtext_url").boolValue;
	}

	void Shutdown() override {
		if (m_host) {
			allPluginActions.clear();
		}
		m_host = nullptr;
	}

	std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() override {
		if (!m_host || !startStr.empty()) return {};
		return allPluginActions;
	}

	std::vector<std::shared_ptr<BaseAction>> InterceptInputShowResultsDirectly(const std::wstring& input) override {
		if (!startStr.empty() && StartsWith(input, startStr)) {
			// 单独显示本插件actions
			return m_host->GetSuccessfullyMatchingTextActions(input.substr(startStr.size()), allPluginActions);
		}
		// 由插件系统管理
		return {};
	}

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		auto action1 = std::dynamic_pointer_cast<BrowserHistoryAction>(action);
		if (!action1) return false;

		try {
			// 打开浏览器访问该 URL
			ShellExecuteW(NULL, L"open", (action1->url).c_str(), NULL, NULL, SW_SHOWNORMAL);
		} catch (...) {
			return false;
		}

		return true;
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new BrowserHistoryPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
