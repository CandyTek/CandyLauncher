#include "../Plugin.hpp"
#include <memory>

#include "BookmarkAction.hpp"
#include "BookmarkPluginData.hpp"
#include "BookmarkUtil.hpp"
#include "../../util/StringUtil.hpp"
#include "util/LogUtil.hpp"
#include "util/HotkeyUtils.h"

namespace {
std::wstring EscapeHtmlText(const std::wstring& input) {
	std::wstring result;
	result.reserve(input.size());
	for (const wchar_t ch : input) {
		switch (ch) {
		case L'&':
			result += L"&amp;";
			break;
		case L'<':
			result += L"&lt;";
			break;
		case L'>':
			result += L"&gt;";
			break;
		case L'"':
			result += L"&quot;";
			break;
		default:
			result += ch;
			break;
		}
	}
	return result;
}

std::wstring EscapeRtfText(const std::wstring& input) {
	std::wstring result;
	result.reserve(input.size() * 2);
	for (const wchar_t ch : input) {
		if (ch == L'\\' || ch == L'{' || ch == L'}') {
			result += L'\\';
			result += ch;
			continue;
		}
		if (ch == L'\r') {
			continue;
		}
		if (ch == L'\n') {
			result += L"\\line ";
			continue;
		}
		if (ch >= 0 && ch <= 0x7f) {
			result += ch;
			continue;
		}
		result += L"\\u";
		result += std::to_wstring(static_cast<short>(ch));
		result += L"?";
	}
	return result;
}

static bool LaunchBookmarkUrl(const std::wstring& url, const bool useSubBrowser) {
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
}

class BookmarkPlugin : public IPlugin {
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;
	std::wstring startStr = L"bm ";
	ParsedHotkey hkOpenWithSubBrowser;

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
		},
		{
			"key": "com.candytek.bookmarkplugin.mainbrowser",
			"title": "指定主浏览器路径",
			"type": "string",
			"subPage": "plugin",
			"defValue": ""
		},
		{
			"key": "com.candytek.bookmarkplugin.subbrowser",
			"title": "指定副浏览器路径（Alt + Enter）",
			"type": "string",
			"subPage": "plugin",
			"defValue": ""
		}
	]
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		startStr = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.bookmarkplugin.start_str").stringValue);
		isMatchTextUrl = m_host->GetSettingsMap().at("com.candytek.bookmarkplugin.matchtext_url").boolValue;
		g_browser = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.bookmarkplugin.mainbrowser").stringValue);
		g_subbrowser = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.bookmarkplugin.subbrowser").stringValue);
		hkOpenWithSubBrowser = ParseHotkeyString("xx(1)(13)");
		
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

	int OnSendHotKey(const std::shared_ptr<BaseAction> action, const UINT vk, const UINT currentModifiers, const WPARAM wparam) override {
		if (hkOpenWithSubBrowser.matches(vk, currentModifiers)) {
			auto bookmarkAction = std::dynamic_pointer_cast<BookmarkAction>(action);
			if (!m_host || !bookmarkAction || bookmarkAction->url.empty()) return 0;
			LaunchBookmarkUrl(bookmarkAction->url, true);
			m_host->PluginTaskDone();
			return 1;
		}
		return 0;
	}

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		auto action1 = std::dynamic_pointer_cast<BookmarkAction>(action);
		if (!action1) return false;
		if (action1->url.empty()) return false;
		// const std::wstring command = L"start " + action1->url;
		// system(wide_to_utf8(command).c_str());
		return LaunchBookmarkUrl(action1->url, false);
	}
	bool OnItemBeginDrag(const std::shared_ptr<BaseAction>& action, HWND sourceHwnd, POINT screenPt) override {
		auto bookmarkAction = std::dynamic_pointer_cast<BookmarkAction>(action);
		if (!m_host || !bookmarkAction) {
			return false;
		}

		const std::wstring url = bookmarkAction->url;
		if (url.empty()) {
			return false;
		}

		const std::wstring title = bookmarkAction->title.empty() ? url : bookmarkAction->title;
		const std::wstring escapedTitle = EscapeHtmlText(title);
		const std::wstring escapedUrl = EscapeHtmlText(url);

		OleDragDropData dragData;
		dragData.text = title + L"\r\n" + url;
		dragData.url = url;
		dragData.html = L"<a href=\"" + escapedUrl + L"\">" + escapedTitle + L"</a>";
		dragData.rtf = L"{\\rtf1\\ansi\\deff0 {\\field{\\*\\fldinst HYPERLINK \"" +
			EscapeRtfText(url) + L"\"}{\\fldrslt " + EscapeRtfText(title) + L"}}}";

		ConsolePrintln(L"BookmarkPlugin", L"Begin OLE drag drop url=" + url);
		return m_host->BeginOleDragDropData(dragData, sourceHwnd);
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
