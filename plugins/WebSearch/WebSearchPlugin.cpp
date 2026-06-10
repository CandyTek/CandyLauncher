#include "../Plugin.hpp"
#include <memory>

#include "WebSearchAction.hpp"
#include "WebSearchPluginData.hpp"
#include "WebSearchUtil.hpp"
#include "WebSearchHotkeyManager.hpp"
#include "SearchManagerWindow.hpp"
#include "util/StringUtil.hpp"
#include "util/HotkeyUtils.h"

// HotkeyEditView.hpp (via SearchManagerWindow.hpp) includes GlobalState.hpp which
// declares extern g_mainHwnd. Provide a local stub — the code path that uses it
// (pref_hotkey_toggle_main_panel) is never reached with our "websearch_engine_hotkey" key.
HWND g_mainHwnd = nullptr;

inline std::vector<std::shared_ptr<BaseAction>> allEngineActions;

static bool LaunchWebSearchUrl(const std::wstring& url, bool useSubBrowser) {
	const std::wstring& configuredBrowser = useSubBrowser
		? (g_subbrowser.empty() ? g_browser : g_subbrowser)
		: g_browser;

	if (configuredBrowser.empty()) {
		return reinterpret_cast<INT_PTR>(
			ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL)
		) > 32;
	}

	return reinterpret_cast<INT_PTR>(
		ShellExecuteW(nullptr, L"open", configuredBrowser.c_str(), url.c_str(), nullptr, SW_SHOWNORMAL)
	) > 32;
}

class WebSearchPlugin : public IPlugin {
public:
	WebSearchPlugin() = default;
	~WebSearchPlugin() override = default;
	ParsedHotkey hkOpenWithSubBrowser;

	std::wstring GetPluginName() const override {
		return L"网络搜索";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.websearchplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"使用关键词快速搜索网页";
	}

	bool Initialize(IPluginHost* host) override {
		m_host = host;
		return m_host != nullptr;
	}

	void OnPluginIdChange(const uint16_t pluginId) override {
		m_pluginId = pluginId;
	}

	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.websearchplugin.mainbrowser",
			"title": "指定主浏览器路径",
			"type": "string",
			"subPage": "plugin",
			"defValue": ""
		},
		{
			"key": "com.candytek.websearchplugin.subbrowser",
			"title": "指定副浏览器路径（Alt + Enter）",
			"type": "string",
			"subPage": "plugin",
			"defValue": ""
		},
		{
			"key": "com.candytek.websearchplugin.manager",
			"title": "搜索管理器",
			"type": "button",
			"subPage": "plugin",
			"defValue": ""
		}
	]
}
   )";
	}

	void OnUserSettingsLoadDone() override {
		g_browser = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.websearchplugin.mainbrowser").stringValue);
		g_subbrowser = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.websearchplugin.subbrowser").stringValue);
		hkOpenWithSubBrowser = ParseHotkeyString("xx(1)(13)");
	}

	void OnSettingItemExecute(const SettingItem* setting, HWND parentHwnd) override {
		if (setting->key == "com.candytek.websearchplugin.manager") {
			ShowSearchManagerWindow(parentHwnd);
		}
	}

	void RefreshAllActions() override {
		if (!m_host) return;

		ConsolePrintln(L"WebSearch Plugin", L"RefreshAllActions start");
		LoadWebSearchConfig();

		allEngineActions.clear();
		for (auto& engine : g_searchEngines) {
			auto action = std::make_shared<WebSearchAction>();
			action->searchUrl = L"";
			action->title = engine.name + L" (" + engine.key + L")";
			action->subTitle = engine.url;
			action->matchText = m_host->GetTheProcessedMatchingText(engine.key + L" " + engine.name);
			allEngineActions.push_back(action);
		}
		// 暂时还不知道用作什么
		// WS_StartHotkeyThread();

		ConsolePrintln(L"WebSearch Plugin", L"Loaded " + std::to_wstring(allEngineActions.size()) + L" engines");
	}

	std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() override {
		return allEngineActions;
	}

	std::vector<std::shared_ptr<BaseAction>> InterceptInputShowResultsDirectly(const std::wstring& input) override {
		if (input.empty()) return {};

		size_t spacePos = input.find(L' ');
		if (spacePos == std::wstring::npos) return {};

		std::wstring key = input.substr(0, spacePos);
		std::wstring query = input.substr(spacePos + 1);

		if (query.empty()) return {};

		for (auto& engine : g_searchEngines) {
			if (engine.key == key) {
				std::wstring url = BuildSearchUrl(engine.url, query);
				auto action = std::make_shared<WebSearchAction>();
				action->searchUrl = url;
				action->title = engine.name + L": " + query;
				action->subTitle = url;
				action->matchText = query;
				action->pluginId = m_pluginId;
				return { action };
			}
		}

		return {};
	}
	
	int OnSendHotKey(const std::shared_ptr<BaseAction> action, const UINT vk, const UINT currentModifiers, const WPARAM wparam) override {
		if (hkOpenWithSubBrowser.matches(vk, currentModifiers)) {
			auto a = std::dynamic_pointer_cast<WebSearchAction>(action);
			if (!a || a->searchUrl.empty()) return false;
			LaunchWebSearchUrl(a->searchUrl, true);
			m_host->PluginTaskDone();
			return 1;
		}
		return 0;
	}

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		auto a = std::dynamic_pointer_cast<WebSearchAction>(action);
		if (!a || a->searchUrl.empty()) return false;

		return LaunchWebSearchUrl(a->searchUrl, false);
	}

	void Shutdown() override {
		WS_StopHotkeyThread();
		allEngineActions.clear();
		g_searchEngines.clear();
		m_host = nullptr;
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new WebSearchPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}


