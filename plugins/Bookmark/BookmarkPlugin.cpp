#include "../Plugin.hpp"
#include <memory>

#include "BookmarkAction.hpp"
#include "BookmarkPluginData.hpp"
#include "BookmarkUtil.hpp"
#include "../../util/StringUtil.hpp"

class BookmarkPlugin : public IPlugin {
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;
	std::wstring startStr = L"bm ";

public:
	BookmarkPlugin() = default;
	~BookmarkPlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"书签";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.bookmarkplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"索引浏览器书签";
	}

	bool Initialize(IPluginHost* host) override {
		m_host = host;
		return m_host != nullptr;
	}
	void OnPluginIdChange(const uint16_t pluginId) override {
		m_pluginId = pluginId;
	}

	void RefreshAllActions() override {
		if (!m_host) return;
		allPluginActions = GetAllChromiumBookmarks();
	}

	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.bookmarkplugin.start_str",
			"title": "直接激活命令",
			"type": "string",
			"subPage": "plugin",
			"defValue": "bm "
		},
		{
			"key": "com.candytek.bookmarkplugin.matchtext_url",
			"type": "bool",
			"title": "搜索匹配书签URL",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.bookmarkplugin.browser_list",
			"type": "stringArr",
			"title": "浏览器书签文件路径",
			"subPage": "plugin",
			"defValue": "chrome"
		}
	]
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		startStr = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.bookmarkplugin.start_str").stringValue);
		isMatchTextUrl = m_host->GetSettingsMap().at("com.candytek.bookmarkplugin.matchtext_url").boolValue;
		
		// auto it = settings_map.find("pref_pinyin_mode");
		// if (it != settings_map.end()) {
		// 	pinyinType = it->second.stringValue;
		// }
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
			return m_host->GetSuccessfullyMatchingTextActions(input.substr(startStr.size()),allPluginActions);
		}
		// 由插件系统管理
		return {};
	}

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		auto action1 = std::dynamic_pointer_cast<BookmarkAction>(action);
		if (!action1) return false;
		try {
			ShellExecuteW(NULL, L"open", (action1->url).c_str(), NULL, NULL, SW_SHOWNORMAL);
		} catch (...) {
			return false;
		}
		// const std::wstring command = L"start " + action1->url;
		// system(wide_to_utf8(command).c_str());
		return true;
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new BookmarkPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
