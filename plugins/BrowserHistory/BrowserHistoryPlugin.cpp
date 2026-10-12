#include <windows.h>
#include <memory>
#include <thread>

#include "BrowserHistoryAction.hpp"
#include "BrowserHistoryPluginData.hpp"
#include "BrowserHistoryUtil.hpp"
#include "../../util/StringUtil.hpp"

class BrowserHistoryPlugin : public candy::Plugin {
private:
	candy::ActionSet<BrowserHistoryAction> history{0};
	std::wstring startStr = L"h ";
	// 设置项变化后在这个线程上重建索引
	std::thread refreshThread;

	// 设置了激活命令时，历史记录只通过激活命令显示，不参与全局搜索
	uint32_t HistorySetFlags() const {
		return startStr.empty() ? CL_SET_GLOBAL_SEARCH : 0u;
	}

	// 读取全部浏览器历史并发布，可在工作线程调用
	void RefreshHistory() {
		auto items = GetAllBrowserHistory();
		Logi(L"BrowserHistory", L"Loaded ", items.size(), L" history items");
		history.Publish(GetHost(), std::move(items));
	}

	void JoinRefreshThread() {
		if (refreshThread.joinable()) refreshThread.join();
	}

public:
	bool Initialize() override {
		m_host = &GetHost();
		return true;
	}

	void RefreshActions() override {
		Logi(L"BrowserHistory", L"RefreshActions start");
		history.SetFlags(GetHost(), HistorySetFlags());
		RefreshHistory();
	}

	void OnUserSettingsLoaded() override {
		const candy::Host& host = GetHost();
		startStr = host.GetString(L"com.candytek.browserhistoryplugin.start_str", L"h ");
		isMatchTextUrl = host.GetBool(L"com.candytek.browserhistoryplugin.matchtext_url", true);
		history.SetFlags(host, HistorySetFlags());
		// 2.2 主程序只在输入以激活命令开头时调用 OnInterceptInput；旧主程序上照旧每次都调用
		if (startStr.empty()) host.SetTriggerKeywords({});
		else host.SetTriggerKeywords({startStr});
	}

	// 影响索引内容的设置变化后在后台重建索引，再通知主程序刷新列表
	void OnSettingChanged(const std::wstring& key) override {
		if (key != L"com.candytek.browserhistoryplugin.matchtext_url" &&
			key != L"com.candytek.browserhistoryplugin.browser_list" &&
			key != L"com.candytek.browserhistoryplugin.max_results") {
			return;
		}
		isMatchTextUrl = GetHost().GetBool(L"com.candytek.browserhistoryplugin.matchtext_url", true);
		JoinRefreshThread();
		refreshThread = std::thread([this] {
			RefreshHistory();
			GetHost().NotifyActionsChanged();
		});
	}

	void Shutdown() override {
		JoinRefreshThread();
		history.ClearLocal();
		m_host = nullptr;
	}

	void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override {
		if (!startStr.empty() && StartsWith(input, startStr)) {
			// 单独显示本插件actions
			sink.AddMatches(history.Id(), input.substr(startStr.size()));
		}
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto action1 = history.Find(userData);
		if (!action1 || action1->url.empty()) return false;

		try {
			// 打开浏览器访问该 URL
			ShellExecuteW(NULL, L"open", (action1->url).c_str(), NULL, NULL, SW_SHOWNORMAL);
		} catch (...) {
			return false;
		}

		return true;
	}

	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override {
		const auto action1 = history.Find(userData);
		if (!action1 || action1->url.empty()) return false;
		text = action1->url;
		return true;
	}
};

// 读取 SQLite 历史库只用到 "Any thread" 的主程序服务，可以在工作线程刷新，不阻塞启动
CANDY_PLUGIN_EXPORT_EX(BrowserHistoryPlugin, L"Browser History", L"Index browser history", L"1.1.0",
						L"com.candytek.browserhistoryplugin", nullptr,
						CL_PLUGIN_REFRESH_ANY_THREAD)
