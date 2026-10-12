#include <memory>

#include "BookmarkAction.hpp"
#include "BookmarkPluginData.hpp"
#include "BookmarkUtil.hpp"
#include "../../util/StringUtil.hpp"
#include "util/LogUtil.hpp"
#include "util/HotkeyUtils.hpp"

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

static const wchar_t* BOOKMARK_DEFAULT_SETTING_JSON = LR"json(
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
			"title": "指定副浏览器路径 (Alt+Enter)",
			"type": "string",
			"subPage": "plugin",
			"defValue": ""
		}
	]
}

   )json";

class BookmarkPlugin : public candy::Plugin {
private:
	candy::ActionSet<BookmarkAction> bookmarks{0};
	std::wstring startStr = L"bm ";
	ParsedHotkey hkOpenWithSubBrowser;
	ParsedHotkey hkCopyMarkdownLink;

	// 设置了激活命令时，书签只通过激活命令显示，不参与全局搜索
	uint32_t BookmarkSetFlags() const {
		return startStr.empty() ? CL_SET_GLOBAL_SEARCH : 0u;
	}

public:
	bool Initialize() override {
		m_host = &GetHost();
		return true;
	}

	void RefreshActions() override {
		bookmarks.SetFlags(GetHost(), BookmarkSetFlags());
		bookmarks.Publish(GetHost(), GetAllChromiumBookmarks());
	}

	void OnUserSettingsLoaded() override {
		const candy::Host& host = GetHost();
		startStr = host.GetString(L"com.candytek.bookmarkplugin.start_str", L"bm ");
		isMatchTextUrl = host.GetBool(L"com.candytek.bookmarkplugin.matchtext_url", true);
		g_browser = host.GetString(L"com.candytek.bookmarkplugin.mainbrowser");
		g_subbrowser = host.GetString(L"com.candytek.bookmarkplugin.subbrowser");
		hkOpenWithSubBrowser = ParseHotkeyString("xx(1)(13)");
		hkCopyMarkdownLink = ParseHotkeyString("xx(6)(67)"); // Ctrl+Shift+C
		bookmarks.SetFlags(host, BookmarkSetFlags());
		// 2.2 主程序只在输入以激活命令开头时调用 OnInterceptInput；旧主程序上照旧每次都调用
		if (startStr.empty()) host.SetTriggerKeywords({});
		else host.SetTriggerKeywords({startStr});
	}

	void Shutdown() override {
		bookmarks.ClearLocal();
		m_host = nullptr;
	}

	void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override {
		if (!startStr.empty() && StartsWith(input, startStr)) {
			// 单独显示本插件actions
			sink.AddMatches(bookmarks.Id(), input.substr(startStr.size()));
		}
	}

	bool OnHotKey(const uint64_t userData, const UINT vk, const UINT currentModifiers, const WPARAM wparam) override {
		if (hkOpenWithSubBrowser.matches(vk, currentModifiers)) {
			const auto bookmarkAction = bookmarks.Find(userData);
			if (!bookmarkAction || bookmarkAction->url.empty()) return false;
			LaunchBookmarkUrl(bookmarkAction->url, true);
			GetHost().TaskDone();
			return true;
		}
		if (hkCopyMarkdownLink.matches(vk, currentModifiers)) {
			const auto bookmarkAction = bookmarks.Find(userData);
			if (!bookmarkAction || bookmarkAction->url.empty()) return false;
			const std::wstring title = bookmarkAction->title.empty() ? bookmarkAction->url : bookmarkAction->title;
			// 主程序低于 2.1 时没有 set_clipboard_text，返回 false 交给主程序默认处理
			return GetHost().SetClipboardText(L"[" + title + L"](" + bookmarkAction->url + L")");
		}
		return false;
	}

	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override {
		const auto bookmarkAction = bookmarks.Find(userData);
		if (!bookmarkAction || bookmarkAction->url.empty()) return false;
		text = bookmarkAction->url;
		return true;
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto action1 = bookmarks.Find(userData);
		if (!action1 || action1->url.empty()) return false;
		return LaunchBookmarkUrl(action1->url, false);
	}

	bool OnItemBeginDrag(const uint64_t userData, const HWND sourceHwnd, const POINT screenPt) override {
		const auto bookmarkAction = bookmarks.Find(userData);
		if (!bookmarkAction || bookmarkAction->url.empty()) {
			return false;
		}

		const std::wstring& url = bookmarkAction->url;
		const std::wstring title = bookmarkAction->title.empty() ? url : bookmarkAction->title;
		const std::wstring escapedTitle = EscapeHtmlText(title);
		const std::wstring escapedUrl = EscapeHtmlText(url);

		candy::DragData dragData;
		dragData.text = title + L"\r\n" + url;
		dragData.url = url;
		dragData.html = L"<a href=\"" + escapedUrl + L"\">" + escapedTitle + L"</a>";
		dragData.rtf = LR"({\rtf1\ansi\deff0 {\field{\*\fldinst HYPERLINK ")" +
			EscapeRtfText(url) + LR"("}{\fldrslt )" + EscapeRtfText(title) + L"}}}";

		Logi(L"BookmarkPlugin", L"Begin OLE drag drop url=", url);
		return GetHost().BeginDrag(dragData, sourceHwnd);
	}
};

// 读取书签文件只用到 "Any thread" 的主程序服务，可以在工作线程刷新，不阻塞启动
CANDY_PLUGIN_EXPORT_EX(BookmarkPlugin, L"书签", L"索引浏览器书签", L"1.2.0", L"com.candytek.bookmarkplugin",
						BOOKMARK_DEFAULT_SETTING_JSON, CL_PLUGIN_REFRESH_ANY_THREAD)
