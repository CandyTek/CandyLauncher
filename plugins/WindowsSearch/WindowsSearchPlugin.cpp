#include <windows.h>
#include <memory>
#include <regex>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <searchapi.h>

#include "WindowsSearchAction.hpp"
#include "WindowsSearchPluginData.hpp"
#include "WindowsSearchUtil.hpp"
#include "../../util/StringUtil.hpp"

// 外部声明 CLSID 和 IID
extern const CLSID CLSID_CSearchManager;
extern const IID IID_ISearchManager;

// Windows Search 查询较慢，使用 2.2 的异步查询：OnQuery 把查询交给插件自己的工作线程，
// 完成后通过 PostResults 追加到当前列表；输入变化后旧查询被取消 / 丢弃
class WindowsSearchPlugin : public candy::Plugin {
private:
	// 一次待执行的查询，工作线程只保留最新的一个（旧的必然已过期）
	struct PendingQuery {
		uint64_t queryId = 0;
		std::wstring keyword;
		int maxCount = 30;
		bool displayHidden = false;
		std::vector<std::wstring> excluded;
	};

	// 0 号：拦截输入时显示的占位条目；1 号：工作线程投递的搜索结果。集合 id 不能重复
	candy::TransientSet<WindowsSearchAction> placeholder{0};
	candy::TransientSet<WindowsSearchAction> results{1};
	std::wstring startStr = L"?";
	std::wstring reservedStringPattern = LR"(^[\/\\\$\%]+$|^.*[<>].*$)";

	std::thread workerThread;
	std::mutex queueMutex;
	std::condition_variable queueCv;
	std::optional<PendingQuery> pending;
	bool stopWorker = false;
	std::atomic<uint64_t> cancelledQueryId{0};

	std::shared_ptr<WindowsSearchAction> FindAction(const uint64_t userData) const {
		if (auto action = results.Find(userData)) return action;
		return placeholder.Find(userData);
	}

	// 去掉前缀并校验关键词；返回 false 表示本次输入不需要搜索
	bool ExtractQuery(const std::wstring& input, std::wstring& searchQuery) const {
		if (startStr.empty() || !StartsWith(input, startStr)) return false;

		// 获取搜索关键词（去除前缀），去除前后空格
		searchQuery = MyTrim(input.substr(startStr.size()));
		if (searchQuery.empty()) return false;

		// 检查是否包含保留字符
		try {
			std::wregex regexPattern(reservedStringPattern);
			if (std::regex_match(searchQuery, regexPattern)) {
				Logi(L"WindowsSearch", L"Query contains reserved characters, skipping search");
				return false;
			}
		} catch (...) {
			// 正则表达式错误，继续搜索
		}
		return true;
	}

	// 查询是否已被取消或不再是当前查询
	bool IsStale(const uint64_t queryId) const {
		return cancelledQueryId.load() == queryId || GetHost().GetCurrentQueryId() != queryId;
	}

	// 工作线程：常驻一个 STA，避免每次查询都重新初始化 COM / 加载 OLE DB 提供程序
	void WorkerLoop() {
		const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		for (;;) {
			PendingQuery job;
			{
				std::unique_lock<std::mutex> lock(queueMutex);
				queueCv.wait(lock, [this] { return stopWorker || pending.has_value(); });
				if (stopWorker) break;
				job = std::move(*pending);
				pending.reset();
			}
			try {
				RunQuery(job);
			} catch (...) {
				Loge(L"WindowsSearch", L"Exception in query worker");
			}
		}
		if (SUCCEEDED(hr)) CoUninitialize();
	}

	void RunQuery(const PendingQuery& job) {
		if (IsStale(job.queryId)) return;

		MethodTimerStart(L"WindowsSearch Query");
		auto items = WindowsSearchAPI::Search(job.keyword, job.maxCount, job.displayHidden, L"*", job.excluded);
		MethodTimerEnd(L"WindowsSearch Query");

		// 搜索期间输入可能已经变化，过期的结果不投递
		if (IsStale(job.queryId)) return;
		results.PostResults(GetHost(), job.queryId, items, true, true);
	}

	void StopWorker() {
		{
			std::lock_guard<std::mutex> lock(queueMutex);
			stopWorker = true;
			pending.reset();
		}
		queueCv.notify_all();
		if (workerThread.joinable()) workerThread.join();
	}

public:
	bool Initialize() override {
		m_host = &GetHost();
		workerThread = std::thread([this] { WorkerLoop(); });
		return true;
	}

	void RefreshActions() override {
		Logi(L"WindowsSearch", L"RefreshActions start");

		// Windows Search 插件不需要预加载数据，搜索是实时的
		// 但我们可以测试一下 COM 是否能正常工作
		HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		bool comInitialized = SUCCEEDED(hr);
		if (hr == S_FALSE || hr == RPC_E_CHANGED_MODE) {
			comInitialized = false;
		}

		ISearchManager* testManager = nullptr;
		hr = CoCreateInstance(CLSID_CSearchManager, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, IID_ISearchManager,
							reinterpret_cast<void**>(&testManager));

		if (SUCCEEDED(hr) && testManager) {
			Logi(L"WindowsSearch", L"Windows Search Manager test: SUCCESS");
			testManager->Release();
		} else {
			wchar_t msg[256];
			swprintf_s(msg, 256, L"0x%08X", hr);
			Loge(L"WindowsSearch", L"Windows Search Manager test: FAILED (HRESULT: ", msg, L")");
		}

		if (comInitialized) CoUninitialize();

		Logi(L"WindowsSearch", L"Ready for dynamic searching");
	}

	void OnUserSettingsLoaded() override {
		const candy::Host& host = GetHost();
		startStr = host.GetString(L"com.candytek.windowssearchplugin.start_str", L"?");
		maxSearchCount = static_cast<int>(host.GetInt(L"com.candytek.windowssearchplugin.max_results", 30));
		displayHiddenFiles = host.GetBool(L"com.candytek.windowssearchplugin.display_hidden_files", false);

		// 读取排除模式
		excludedPatterns.clear();
		for (const auto& pattern : host.GetStringArray(L"com.candytek.windowssearchplugin.excluded_patterns")) {
			if (!pattern.empty()) {
				excludedPatterns.push_back(pattern);
			}
		}

		// 2.2 主程序只在输入以激活命令开头时调用 OnInterceptInput / OnQuery；旧主程序上照旧每次都调用
		if (startStr.empty()) host.SetTriggerKeywords({});
		else host.SetTriggerKeywords({startStr});
	}

	void Shutdown() override {
		StopWorker();
		placeholder.Begin();
		results.Begin();
		m_host = nullptr;
	}

	// 主程序只有在拦截结果非空时才接管列表，所以先放一个占位条目，搜索结果随后由 OnQuery 追加
	void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override {
		std::wstring searchQuery;
		if (!ExtractQuery(input, searchQuery)) return;

		auto hint = std::make_shared<WindowsSearchAction>();
		hint->title = L"正在搜索 Windows 索引: " + searchQuery;
		hint->subtitle = L"搜索结果将追加到下方";
		hint->iconPath = LR"(C:\Windows\explorer.exe)";
		sink.AddTransient(placeholder, {hint});
	}

	// 异步查询：交给工作线程，完成后 PostResults
	void OnQuery(const uint64_t queryId, const std::wstring& input, const std::wstring& arg) override {
		std::wstring searchQuery;
		if (!ExtractQuery(input, searchQuery)) return;

		PendingQuery job;
		job.queryId = queryId;
		job.keyword = std::move(searchQuery);
		job.maxCount = maxSearchCount;
		job.displayHidden = displayHiddenFiles;
		job.excluded = excludedPatterns;
		{
			std::lock_guard<std::mutex> lock(queueMutex);
			pending = std::move(job);
		}
		queueCv.notify_one();
	}

	void OnQueryCancel(const uint64_t queryId) override {
		cancelledQueryId = queryId;
		std::lock_guard<std::mutex> lock(queueMutex);
		if (pending && pending->queryId == queryId) pending.reset();
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto action1 = FindAction(userData);
		// 占位条目没有路径，不做任何事
		if (!action1 || action1->path.empty()) return false;

		try {
			// 检查是否是打开设置的 action
			if (action1->path == L"OpenSettingsAction") {
				// 打开 Windows 设置 -> 搜索 -> Windows 搜索
				// 使用 ms-settings URI scheme
				ShellExecuteW(NULL, L"open", L"ms-settings:cortana-windowssearch", NULL, NULL, SW_SHOWNORMAL);
				return true;
			}

			// 打开文件或文件夹
			ShellExecuteW(NULL, L"open", action1->path.c_str(), NULL, NULL, SW_SHOWNORMAL);
		} catch (...) {
			return false;
		}

		return true;
	}

	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override {
		const auto action1 = results.Find(userData);
		if (!action1 || action1->path.empty() || action1->path == L"OpenSettingsAction") return false;
		text = action1->path;
		return true;
	}
};

CANDY_PLUGIN_EXPORT(WindowsSearchPlugin, L"Windows Search", L"Search files and folders with the Windows indexing service", L"1.1.0",
					L"com.candytek.windowssearchplugin", nullptr)
