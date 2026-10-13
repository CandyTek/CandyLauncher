#include <windows.h>
#include <memory>

#include "WebSearchAction.hpp"
#include "WebSearchPluginData.hpp"
#include "WebSearchUtil.hpp"
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

inline std::vector<std::shared_ptr<WebSearchAction>> allEngineActions;
inline std::vector<std::shared_ptr<WebSearchAction>> textArgActions;
inline std::vector<std::shared_ptr<WebSearchAction>> urlArgActions;

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


class WebSearchPlugin : public candy::Plugin
{
public:
	WebSearchPlugin() = default;
	~WebSearchPlugin() override = default;
	ParsedHotkey hkOpenWithSubBrowser;

private:
	// 所有条目都随输入动态生成（"gg 关键词" / search / textarg / urlarg），只为本次输入存在，不进入任何集合
	candy::TransientSet<WebSearchAction> results{0};

	// 触发关键字：各搜索引擎的 key + 空格，以及 search / textarg / urlarg
	void UpdateTriggerKeywords() const
	{
		std::vector<std::wstring> keywords = {L"search ", L"textarg ", L"urlarg "};
		for (const auto& engine : g_searchEngines)
		{
			if (!engine.key.empty()) keywords.push_back(engine.key + L" ");
		}
		GetHost().SetTriggerKeywords(keywords);
	}

	// 根据当前的搜索引擎列表重建条目
	void BuildActions()
	{
		allEngineActions.clear();
		textArgActions.clear();
		urlArgActions.clear();

		for (auto& engine : g_searchEngines)
		{
			// 文本参数条目：执行时用 arg 现场拼 URL（custom_action_id 1），不缓存任何具体查询
			auto action = std::make_shared<WebSearchAction>();
			action->sourceUrl = engine.url;
			action->custom_action_id = 1;
			action->title = engine.name + L" (" + engine.key + L")";
			action->subtitle = engine.url;
			action->stableKey = engine.url;
			allEngineActions.push_back(action);
			textArgActions.push_back(action);
		}

		{
			std::wstring name = L"转到链接";
			auto action = std::make_shared<WebSearchAction>();
			action->title = name;
			action->custom_action_id = 11;
			action->stableKey = L"websearch_open_url";
			urlArgActions.push_back(action);
		}

		{
			std::wstring name = L"尝试打开文本中的链接";
			auto action = std::make_shared<WebSearchAction>();
			action->title = name;
			action->custom_action_id = 12;
			action->stableKey = L"websearch_open_url_in_text";
			textArgActions.push_back(action);
		}
		{
			std::wstring name = L"复制内容";
			auto action = std::make_shared<WebSearchAction>();
			action->title = name;
			action->custom_action_id = 13;
			action->stableKey = L"websearch_copy";
			textArgActions.push_back(action);
			urlArgActions.push_back(action);
		}

		Logi(L"WebSearch", L"Loaded ", allEngineActions.size(), L" engines");
	}

public:
	bool Initialize() override
	{
		m_host = &GetHost();
		// 搜索管理器保存后同步条目和触发关键字（管理器窗口只在 UI 线程运行）
		g_onSearchEnginesSaved = [this]
		{
			BuildActions();
			UpdateTriggerKeywords();
		};
		return true;
	}

	void OnUserSettingsLoaded() override
	{
		const candy::Host& host = GetHost();
		g_browser = host.GetString(L"com.candytek.websearchplugin.mainbrowser");
		g_subbrowser = host.GetString(L"com.candytek.websearchplugin.subbrowser");
		hkOpenWithSubBrowser = ParseHotkeyString("xx(1)(13)");
	}

	void OnSettingItemExecute(const std::wstring& key, HWND parentHwnd) override
	{
		if (key == L"com.candytek.websearchplugin.manager")
		{
			ShowSearchManagerWindow(parentHwnd);
		}
	}

	void RefreshActions() override
	{
		Logi(L"WebSearch", L"RefreshActions start");
		LoadWebSearchConfig();
		InitWebSearchHotkeys();
		BuildActions();
		UpdateTriggerKeywords();
	}

	void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override
	{
		size_t spacePos = input.find(L' ');
		if (spacePos == std::wstring::npos) return;

		std::wstring key = input.substr(0, spacePos);
		std::wstring query = input.substr(spacePos + 1);

		if (query.empty())
		{
			if (L"textarg" == key)
			{
				sink.AddTransient(results, textArgActions);
			}
			else if (L"urlarg" == key)
			{
				sink.AddTransient(results, urlArgActions);
			}
			return;
		}

		for (auto& engine : g_searchEngines)
		{
			if (engine.key == key)
			{
				std::wstring url = BuildSearchUrl(engine.url, query);
				auto action = std::make_shared<WebSearchAction>();
				action->searchUrl = url;
				action->title = engine.name + L": " + query;
				action->subtitle = url;
				action->stableKey = url;
				sink.AddTransient(results, {action});
				return;
			}
		}
		if (L"search" == key)
		{
			// 每次输入新建条目（custom_action_id 0，URL 已拼好），不能改写 textArgActions 里共享的引擎条目，
			// 否则之后 "textarg " 会打开上一次 search 的 URL
			std::vector<std::shared_ptr<WebSearchAction>> items;
			for (const auto& engine : allEngineActions)
			{
				if (!engine || engine->sourceUrl.empty()) continue;
				auto action = std::make_shared<WebSearchAction>();
				action->sourceUrl = engine->sourceUrl;
				action->searchUrl = BuildSearchUrl(engine->sourceUrl, query);
				action->title = engine->title;
				action->subtitle = action->searchUrl;
				action->stableKey = engine->stableKey;
				items.push_back(action);
			}
			for (const auto& a : textArgActions)
			{
				if (a && a->sourceUrl.empty()) items.push_back(a);
			}
			sink.AddTransient(results, items);
		}
	}

	bool OnHotKey(const uint64_t userData, const UINT vk, const UINT currentModifiers, const WPARAM wparam) override
	{
		if (hkOpenWithSubBrowser.matches(vk, currentModifiers))
		{
			const auto a = results.Find(userData);
			if (!a) return false;
			const bool launched = CustomActionExecute(a, GetHost().GetActionArg(), true);
			if (launched) GetHost().TaskDone();
			return launched;
		}
		return false;
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override
	{
		const auto a = results.Find(userData);
		if (!a) return false;
		return CustomActionExecute(a, arg, false);
	}

	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override
	{
		const auto a = results.Find(userData);
		if (!a) return false;
		const std::wstring& url = a->searchUrl.empty() ? a->sourceUrl : a->searchUrl;
		if (url.empty()) return false;
		text = url;
		return true;
	}

	bool CustomActionExecute(const std::shared_ptr<WebSearchAction>& a, const std::wstring& arg, bool useSubBrowser)
	{
		if (!a) return false;
		if (a->custom_action_id == 13)
		{
			return SetClipboardText(arg);
		}

		switch (a->custom_action_id)
		{
		case 11:
			return LaunchWebSearchUrl(arg, useSubBrowser);
		case 12:
			{
				const std::string url = getContainsUrl(arg);
				return !url.empty() && LaunchWebSearchUrl(url, useSubBrowser);
			}
		case 1:
			// 文本参数：用 arg 拼引擎 URL
			if (a->sourceUrl.empty()) return false;
			return LaunchWebSearchUrl(BuildSearchUrl(a->sourceUrl, arg), useSubBrowser);
		case 0:
			// "gg 关键词" / "search 关键词"：URL 已在拦截输入时拼好
			if (a->searchUrl.empty()) return false;
			return LaunchWebSearchUrl(a->searchUrl, useSubBrowser);
		default:
			return false;
		}
	}

	void Shutdown() override
	{
		g_onSearchEnginesSaved = nullptr;
		g_hotkeyMgr.Clear();
		results.Begin();
		urlArgActions.clear();
		textArgActions.clear();
		allEngineActions.clear();
		g_searchEngines.clear();
		m_host = nullptr;
	}
};

CANDY_PLUGIN_EXPORT(WebSearchPlugin, L"Web Search", L"Search the web with keyword shortcuts", L"1.1.0", L"com.candytek.websearchplugin",
					nullptr)
