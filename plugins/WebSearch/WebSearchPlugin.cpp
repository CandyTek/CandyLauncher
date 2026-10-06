#include "../Plugin.hpp"
#include <memory>

#include "WebSearchAction.hpp"
#include "WebSearchPluginData.hpp"
#include "WebSearchUtil.hpp"
#include "WebSearchHotkeyManager.hpp"
#include "SearchManagerWindow.hpp"
#include "plugins/SmallToolBox/ToolBoxUtil.hpp"
#include "util/StringUtil.hpp"
#include "util/HotkeyUtils.hpp"
#include "util/UrlUtil.hpp"
#include "util/ClipboardUtil.hpp"

// HotkeyEditView.hpp (via SearchManagerWindow.hpp) includes GlobalState.hpp which
// declares extern g_mainHwnd. Provide a local stub — the code path that uses it
// (pref_hotkey_toggle_main_panel) is never reached with our "websearch_engine_hotkey" key.
HWND g_mainHwnd = nullptr;

inline std::vector<std::shared_ptr<BaseAction>> allEngineActions;
inline std::vector<std::shared_ptr<BaseAction>> textArgActions;
inline std::vector<std::shared_ptr<BaseAction>> urlArgActions;

static bool LaunchWebSearchUrl(const std::wstring& url, bool useSubBrowser)
{
    const std::wstring& configuredBrowser = useSubBrowser
                                                ? (g_subbrowser.empty() ? g_browser : g_subbrowser)
                                                : g_browser;

    if (configuredBrowser.empty())
    {
        return reinterpret_cast<INT_PTR>(
            ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL)
        ) > 32;
    }

    return reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", configuredBrowser.c_str(), url.c_str(), nullptr, SW_SHOWNORMAL)
    ) > 32;
}

static bool LaunchWebSearchUrl(const std::string& url, bool useSubBrowser)
{
    return LaunchWebSearchUrl(utf8_to_wide(url), useSubBrowser);
}


class WebSearchPlugin : public IPlugin
{
public:
    WebSearchPlugin() = default;
    ~WebSearchPlugin() override = default;
    ParsedHotkey hkOpenWithSubBrowser;

    std::wstring GetPluginName() const override
    {
        return L"网络搜索";
    }

    std::wstring GetPluginPackageName() const override
    {
        return L"com.candytek.websearchplugin";
    }

    std::wstring GetPluginVersion() const override
    {
        return L"1.0.0";
    }

    std::wstring GetPluginDescription() const override
    {
        return L"使用关键词快速搜索网页";
    }

    bool Initialize(IPluginHost* host) override
    {
        m_host = host;
        return m_host != nullptr;
    }

    void OnPluginIdChange(const uint16_t pluginId) override
    {
        m_pluginId = pluginId;
    }

    std::wstring DefaultSettingJson() override
    {
        return LR"json(
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
			"title": "指定副浏览器路径 (Alt+Enter)",
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
   )json";
    }

    void OnUserSettingsLoadDone() override
    {
        g_browser = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.websearchplugin.mainbrowser").stringValue);
        g_subbrowser = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.websearchplugin.subbrowser").stringValue);
        hkOpenWithSubBrowser = ParseHotkeyString("xx(1)(13)");
    }

    void OnSettingItemExecute(const SettingItem* setting, HWND parentHwnd) override
    {
        if (setting->key == "com.candytek.websearchplugin.manager")
        {
            ShowSearchManagerWindow(parentHwnd);
        }
    }

    void RefreshAllActions() override
    {
        if (!m_host) return;

        Logi(L"WebSearch", L"RefreshAllActions start");
        LoadWebSearchConfig();

        allEngineActions.clear();
        textArgActions.clear();
        urlArgActions.clear();
        
        for (auto& engine : g_searchEngines)
        {
            auto action = std::make_shared<WebSearchAction>();
            action->sourceUrl = engine.url;
            action->title = engine.name + L" (" + engine.key + L")";
            action->subTitle = engine.url;
            action->matchText = m_host->GetTheProcessedMatchingText(engine.key + L" " + engine.name);
            allEngineActions.push_back(action);
            textArgActions.push_back(action);
        }

        {
            std::wstring name = L"转到链接";
            auto action = std::make_shared<WebSearchAction>();
            action->title = name;
            action->custom_action_id = 11;
            action->matchText = m_host->GetTheProcessedMatchingText(name);
            urlArgActions.push_back(action);
        }
        
        {
            std::wstring name = L"尝试打开文本中的链接";
            auto action = std::make_shared<WebSearchAction>();
            action->title = name;
            action->custom_action_id = 12;
            action->matchText = m_host->GetTheProcessedMatchingText(name);
            textArgActions.push_back(action);
        }
        {
            std::wstring name = L"复制内容";
            auto action = std::make_shared<WebSearchAction>();
            action->title = name;
            action->custom_action_id = 13;
            action->matchText = m_host->GetTheProcessedMatchingText(name);
            textArgActions.push_back(action);
            urlArgActions.push_back(action);
        }

        // 暂时还不知道用作什么
        // WS_StartHotkeyThread();

        Logi(L"WebSearch", L"Loaded ", allEngineActions.size(), L" engines");
    }

    std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() override
    {
        return {};
    }

    std::vector<std::shared_ptr<BaseAction>> InterceptInputShowResultsDirectly(const std::wstring& input) override
    {
        if (input.empty()) return {};

        size_t spacePos = input.find(L' ');
        if (spacePos == std::wstring::npos) return {};

        std::wstring key = input.substr(0, spacePos);
        std::wstring query = input.substr(spacePos + 1);

        if (query.empty())
        {
            if (L"textarg" == key)
            {
                return {textArgActions};
            }
            if (L"urlarg" == key)
            {
                return {urlArgActions};
            }
            return {};
        }

        for (auto& engine : g_searchEngines)
        {
            if (engine.key == key)
            {
                std::wstring url = BuildSearchUrl(engine.url, query);
                auto action = std::make_shared<WebSearchAction>();
                action->searchUrl = url;
                action->title = engine.name + L": " + query;
                action->subTitle = url;
                action->matchText = query;
                action->pluginId = m_pluginId;
                return {action};
            }
        }
        if (L"search" == key)
        {
            for (auto& engineAction : allEngineActions)
            {
                auto a = std::dynamic_pointer_cast<WebSearchAction>(engineAction);
                if (!a || a->sourceUrl.empty()) continue;
                a->searchUrl = BuildSearchUrl(a->sourceUrl, query);
                a->subTitle = a->searchUrl;
            }
            return textArgActions;
        }
        return {};
    }

    int OnSendHotKey(std::shared_ptr<BaseAction>& action, const UINT vk, const UINT currentModifiers,
                     const WPARAM wparam) override
    {
        if (hkOpenWithSubBrowser.matches(vk, currentModifiers))
        {
            const bool launched = CustomActionExecute(action, m_host->GetCurrectArgText(), true);
            if (launched) m_host->PluginTaskDone();
            return launched ? 1 : 0;
        }
        return 0;
    }

    bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override
    {
        return CustomActionExecute(action, arg, false);
    }
    
    bool CustomActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg, bool useSubBrowser)
    {
        if (!m_host) return false;
        auto a = std::dynamic_pointer_cast<WebSearchAction>(action);
        if (!a) return false;
        if (a->custom_action_id == 13)
        {
            return SetClipboardText(arg);
        }

        if (a->searchUrl.empty())
        {
            if (a->custom_action_id == 11)
            {
                std::wstring url = arg;
                return LaunchWebSearchUrl(url, useSubBrowser);
            }else if (a->custom_action_id == 12)
            {
                std::string url =getContainsUrl(arg);
                if (!url.empty())
                {
                    return LaunchWebSearchUrl(url, useSubBrowser);
                }
            }
        }
        else
        {
            if (a->custom_action_id == 1)
            {
                std::wstring url = BuildSearchUrl(a->sourceUrl, arg);
                return LaunchWebSearchUrl(url, useSubBrowser);
            }
            else if (a->custom_action_id == 0)
            {
                return LaunchWebSearchUrl(a->searchUrl, useSubBrowser);
            }
        }

        return false;
    }

    void Shutdown() override
    {
        WS_StopHotkeyThread();
        urlArgActions.clear();
        textArgActions.clear();
        allEngineActions.clear();
        g_searchEngines.clear();
        m_host = nullptr;
    }
};

PLUGIN_EXPORT IPlugin* CreatePlugin()
{
    return new WebSearchPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin)
{
    delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion()
{
    return 1;
}
