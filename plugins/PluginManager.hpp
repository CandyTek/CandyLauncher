#pragma once
#include <map>
#include <utility>
#include <vector>
#include <memory>
#include <functional>
#include <filesystem>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <windows.h>

#include "PluginActionProxy.hpp"
#include "PluginI18n.hpp"
#include "BaseLaunchAction.hpp"
#include "common/I18n.hpp"
#include "Everything.h"
#include "../util/BaseTools.hpp"
#include "util/FileUtil.hpp"
#include "util/json.hpp"
#include "manager/MatchTextBuilder.hpp"
#include "manager/TextMatchHelper.hpp"
#include "manager/UsageHistory.hpp"
#include "util/PinyinHelper.hpp"
#include "util/MainTools.hpp"
#include "util/MyToastUtil.hpp"
#include "util/OleFileDragDrop.hpp"
#include "util/FileSystemTraverser.hpp"
#include "util/ParallelUtil.hpp"
#include "util/SvgUtil.hpp"
#include "model/FileInfo.hpp"
#include "model/TraverseOptions.hpp"

// refresh_actions on a worker thread (CL_PLUGIN_REFRESH_ANY_THREAD). A refresh requested while one is
// running is coalesced into a single follow-up run.
struct PluginRefreshWorker {
	std::thread thread;
	std::mutex mutex;
	bool running = false;
	bool pending = false;
};

struct PluginInfo {
	HMODULE handle = nullptr;
	const CL_PluginApi* api = nullptr;
	void* self = nullptr;
	std::unique_ptr<CL_PluginContext> context;
	std::unique_ptr<PluginRefreshWorker> refreshWorker;
	std::wstring name;
	std::wstring description;
	std::wstring version;
	std::wstring pkgName;
	std::wstring filePath;
	// The file actually passed to LoadLibrary: filePath, or its shadow copy when enabled.
	std::wstring loadedPath;
	std::wstring defaultSettingJson;
	uint16_t pluginId = 65535;
	uint32_t apiMinor = 0;
	uint32_t apiFlags = 0;
	bool loaded = false;
	uint64_t loadTimeMs = 0;
	uint64_t refreshTimeMs = 0;
	int32_t priority = 0;
	// Last time a slow-callback warning was written for this plugin (rate limit).
	uint64_t lastSlowLogTick = 0;
	// "<pkg>.*" values as of the last NotifyUserSettingsLoadDone, for on_setting_changed.
	std::map<std::string, std::string> settingSnapshot;
};

struct PluginCatalogInfo {
	std::wstring name;
	std::wstring description;
	std::wstring version;
	std::wstring pkgName;
	std::wstring filePath;
	std::wstring defaultSettingJson;
	// Informational, from the manifest; the live keywords come from set_trigger_keywords.
	std::vector<std::wstring> keywords;
	// Manifest "min_windows_version" ("7" / "8" / "8.1" / "10" / "11" or a kernel version like "6.1"); empty = Windows 7+.
	std::wstring minWindowsVersion;
	// Non-empty when the manifest says this plugin cannot run here (host api / Windows version); never loaded then.
	std::wstring unsupportedReason;
	bool enabled = true;
	bool fromManifest = false;
	std::filesystem::file_time_type fileTime{};
	// 2.3 string tables (lang/<stem>/*.json + manifest "strings"), all languages; null when the plugin has none.
	std::shared_ptr<const PluginStringTable> strings;

	// name / description in the current UI language ("plugin.name" / "plugin.description" keys), else the raw ones.
	std::wstring DisplayName() const { return PluginI18n::Localize(strings, PluginI18n::KEY_PLUGIN_NAME, name); }
	std::wstring DisplayDescription() const {
		return PluginI18n::Localize(strings, PluginI18n::KEY_PLUGIN_DESCRIPTION, description);
	}
};

inline std::unordered_map<uint16_t, PluginInfo> m_plugins = {};
inline std::vector<PluginCatalogInfo> g_pluginCatalog = {};
inline uint16_t pluginCount = 0;
static std::wstring TAG = L"PluginManager";
constexpr const wchar_t* PLUGIN_BROWSER_PACKAGE_NAME = L"com.candytek.pluginbrowserplugin";
constexpr const wchar_t* PLUGIN_MANIFEST_SUFFIX = L".plugin.json";
constexpr const wchar_t* PLUGIN_SHADOW_DIR_NAME = L".shadow";
// A per-keystroke plugin callback slower than this is logged (at most once per 5 s per plugin).
constexpr uint64_t PLUGIN_SLOW_CALLBACK_MS = 50;

// Calls into the plugin only if the (optional) entry point exists in the plugin's table.
#define CL_PLUGIN_HAS(info, field) ((info).api && CL_HAS_FIELD((info).api, CL_PluginApi, field) && (info).api->field)

class PluginManager {
private:
	std::function<void()> m_onActionsChanged;
	std::function<void()> m_beforePluginUnload;
	// (queryId, actions, CL_RESULTS_* flags) on the UI thread, see HandleQueryResultsMessage.
	std::function<void(uint64_t, std::vector<std::shared_ptr<BaseAction>>&&, int32_t)> m_onQueryResults;
	// Plugins appeared / disappeared / were replaced on disk; the settings list must be rebuilt.
	std::function<void()> m_onCatalogChanged;
	bool m_suppressNotifications = false;

	// Loaded plugins ordered by priority (desc) then load order; every dispatch walks this.
	std::vector<uint16_t> m_dispatchOrder;
	// Plugins that received the current on_query, for on_query_cancel.
	std::vector<uint16_t> m_queryRecipients;

	struct PendingResults {
		uint64_t queryId = 0;
		std::vector<std::shared_ptr<BaseAction>> actions;
		int32_t flags = 0; // CL_RESULTS_*
	};

public:
	// What on_intercept_input produced for one input.
	struct InterceptResult {
		std::vector<std::shared_ptr<BaseAction>> actions;
		// A plugin asked to show `actions` (even none) instead of running the normal search.
		bool tookOver = false;
	};

private:

	std::mutex m_resultsMutex;
	std::vector<PendingResults> m_pendingResults;
	std::atomic<bool> m_resultsPosted{false};
	std::atomic<bool> m_actionsChangedPosted{false};
	std::atomic<bool> m_dirChangedPosted{false};

	// Combined global-search list of every loaded plugin, rebuilt only when s_actionsVersion moved.
	std::vector<std::shared_ptr<BaseAction>> m_combinedActions;
	uint64_t m_combinedVersion = 0;

	std::wstring m_pluginDir;
	// UI language the string tables were last resolved for (SyncLanguageFromSettings).
	std::wstring m_language;
	bool m_languageSynced = false;
	bool m_shadowCopy = false;
	uint16_t m_nextPluginId = 0;

	// Plugin folder watcher (hot install / update).
	std::thread m_watchThread;
	HANDLE m_watchStopEvent = nullptr;

	// Bumped whenever a set, a priority or the loaded plugin list changes. Any thread.
	inline static std::atomic<uint64_t> s_actionsVersion{1};
	// Id of the input currently shown; results for older ids are dropped.
	inline static std::atomic<uint64_t> s_currentQueryId{0};
	// Everything SDK keeps one global query / result state per process.
	inline static std::mutex s_everythingMutex;

public:
	PluginManager() = default;

	~PluginManager() {
		StopPluginDirWatcher();
		UnloadAllPlugins();
	}

	static uint64_t ActionsVersion() { return s_actionsVersion.load(std::memory_order_acquire); }
	static void BumpActionsVersion() { s_actionsVersion.fetch_add(1, std::memory_order_acq_rel); }
	static uint64_t CurrentQueryId() { return s_currentQueryId.load(std::memory_order_acquire); }

	void TraverseFilesForEverythingSDK(
		const std::wstring& folderPath2,
		const TraverseOptions& options,
		std::function<void(const std::wstring& name,
							const std::wstring& fullPath,
							const std::wstring& parent,
							const std::wstring& ext)>&& callback) {
		namespace fs = std::filesystem;
		const std::wstring folderPath = ExpandEnvironmentVariables(folderPath2, EXE_FOLDER_PATH);
		if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) return;
		std::wstring rootDirectory = fs::path(folderPath).lexically_normal().make_preferred().wstring();
		while (rootDirectory.size() > 3 && rootDirectory.back() == L'\\') rootDirectory.pop_back();
		std::wstring rootPrefix = rootDirectory;
		if (!rootPrefix.empty() && rootPrefix.back() != L'\\') rootPrefix += L'\\';
		auto belongsToFolder = [&](const fs::path& path) {
			const std::wstring parent = path.parent_path().lexically_normal().make_preferred().wstring();
			if (_wcsicmp(parent.c_str(), rootDirectory.c_str()) == 0) return true;
			return options.recursive && parent.size() >= rootPrefix.size() &&
				_wcsnicmp(parent.c_str(), rootPrefix.c_str(), rootPrefix.size()) == 0;
		};

		auto addFile = [&](const fs::path& path) {
			std::wstring filename = path.stem().wstring();
			if (shouldExclude(options, path.filename().wstring())) return;

			if (const auto it = options.renameMap.find(filename); it != options.renameMap.end()) filename = it->second;

			// 这里不限定 callback 的参数，可以传多种信息
			callback(
				filename, // 逻辑名（被 rename 过）
				path.wstring(), // 完整路径
				path.parent_path().wstring(), // 父目录
				path.extension().wstring() // 扩展名
			);
		};


		// 构造更高效的搜索查询
		// 例如: parent:"C:\ProgramData\Microsoft\Windows\Start Menu\Programs" ext:exe;lnk
		std::wstringstream search_query;
		if (options.indexFilesOnly) {
			search_query << L"file: ";
		}
		if (options.recursive) {
			// 带结尾反斜杠的路径只匹配子孙项，Everything 查询明显快于不带反斜杠的形式
			search_query << L"\"" << rootPrefix << L"\" ";
		} else {
			// 如果不递归，使用 parent: 函数更精确、更高效
			search_query << L"parent:\"" << folderPath << L"\" ";
		}

		// 使用 ext: 过滤器，比正则表达式更简单快速
		if (!options.extensions.empty()) {
			search_query << L"ext:";
			for (size_t i = 0; i < options.extensions.size(); ++i) {
				std::wstring ext = options.extensions[i];
				// 去掉可能存在的点
				if (!ext.empty() && ext[0] == L'.') ext.erase(0, 1);
				search_query << ext;
				if (i < options.extensions.size() - 1) search_query << L";";
			}
		}

		Logi(L"Everything", L"Executing optimized Everything search: ", search_query.str());

		// 使用 Everything SDK；SDK 的查询状态是进程级的，多个插件线程不能同时查询
		std::lock_guard<std::mutex> everythingLock(s_everythingMutex);
		Everything_SetSearchW(search_query.str().c_str());
		const ULONGLONG queryStart = GetTickCount64();
		if (!Everything_QueryW(TRUE)) {
			Loge(L"Everything", L"Query failed; scanning folder: ", folderPath);
			TraverseFiles(folderPath, options, EXE_FOLDER_PATH, callback);
			return;
		}
		const ULONGLONG queryDone = GetTickCount64();

		DWORD numResults = Everything_GetNumResults();
		std::vector<wchar_t> fullPath(32768);

		for (DWORD i = 0; i < numResults; i++) {
			const DWORD length = Everything_GetResultFullPathNameW(i, fullPath.data(), static_cast<DWORD>(fullPath.size()));
			if (length == 0 || length >= fullPath.size()) continue;
			const fs::path resultPath(fullPath.data());
			if (belongsToFolder(resultPath)) addFile(resultPath);
		}
		Logi(L"Everything", L"Folder query ms=", queryDone - queryStart,
			L" results=", numResults, L" callbacks ms=", GetTickCount64() - queryDone);
	}

	// 搜索可能的目标文件
	std::vector<FileInfo> SearchPossibleTargets(const std::wstring& fileName, uint64_t resultCount) {
		std::vector<FileInfo> candidates;

		// 构建搜索查询：搜索以fileName开头的.exe文件
		std::wstring searchQuery = fileName;

		std::lock_guard<std::mutex> everythingLock(s_everythingMutex);
		Everything_SetSearchW(searchQuery.c_str());
		Everything_QueryW(TRUE);

		DWORD numResults = Everything_GetNumResults();

		for (DWORD i = 0; i < numResults && i < resultCount; i++) {
			// 限制结果数量避免太多
			wchar_t fullPath[MAX_PATH];
			Everything_GetResultFullPathNameW(i, fullPath, MAX_PATH);

			// 检查文件是否存在
			if (GetFileAttributesW(fullPath) != INVALID_FILE_ATTRIBUTES) {
				FileInfo candidate;
				candidate.file_path = fullPath;
				candidate.label = candidate.file_path.stem().wstring();
				candidates.push_back(candidate);
			}
		}
		return candidates;
	}


	static std::wstring GetTheProcessedMatchingText(const std::wstring& source) {
		// 小写 + 清理 + 拼音变体，见 manager/MatchTextBuilder.hpp
		return BuildProcessedMatchText(source).text;
	}

	bool LoadPlugin(const std::wstring& dllPath) {
		std::wstring pluginName = GetPluginNameFromPathW(dllPath);

		PluginInfo info;
		info.filePath = dllPath;
		info.name = pluginName;
		info.pluginId = m_nextPluginId;

		if (LoadSinglePlugin(dllPath, info)) {
			++m_nextPluginId;
			const uint16_t pluginId = info.pluginId;
			m_plugins[pluginId] = std::move(info);
			RebuildDispatchOrder();
			BumpActionsVersion();
			return true;
		}

		Loge(TAG, L"LoadSinglePlugin failed for: ", pluginName);
		return false;
	}

	bool IsPluginEnabledByPackageName(const std::wstring& packageName) const {
		if (packageName.empty()) {
			return false;
		}

		for (const auto& catalogInfo : g_pluginCatalog) {
			if (catalogInfo.pkgName == packageName) {
				return catalogInfo.enabled;
			}
		}

		return FindLoadedPluginByPackageName(packageName) != nullptr;
	}

	// Plugin ids are never reused, so settings items and proxies keep pointing at the right plugin.
	bool UnloadPlugin(const uint16_t pluginId) {
		if (m_plugins.find(pluginId) != m_plugins.end()) {
			NotifyBeforePluginUnload();
			UnloadSinglePlugin(m_plugins[pluginId]);
			m_plugins.erase(pluginId);
			RebuildDispatchOrder();
			BumpActionsVersion();
			NotifyActionsChanged();
			return true;
		}
		return false;
	}

	void OnMainWindowShowNotifi(bool isShow) {
		for (const uint16_t pluginId : m_dispatchOrder) {
			PluginInfo& info = m_plugins[pluginId];
			if (info.loaded && CL_PLUGIN_HAS(info, on_main_window_show)) {
				info.api->on_main_window_show(info.self, isShow ? 1 : 0);
			}
		}
	}

	void LoadAllPlugins(const std::wstring& pluginDir) {
		Logi(TAG, L"LoadAllPlugins start");
		g_pluginCatalog.clear();
		m_pluginDir = pluginDir;
		// InitializeI18n runs in the settings pass after the plugins are loaded; the first
		// SyncLanguageFromSettings corrects this initial value before anything is published.
		m_language = g_uiLanguageCode;
		m_languageSynced = false;
		PluginI18n::SetLanguage(m_language);

		if (!std::filesystem::exists(pluginDir)) {
			Logw(TAG, L"Plugin directory does not exist: ", pluginDir);
			return;
		}
		if (!std::filesystem::is_directory(pluginDir)) {
			Logw(TAG, L"Plugin path is not a directory: ", pluginDir);
			return;
		}

		const nlohmann::json userConfig = LoadUserPluginConfig();
		m_shadowCopy = ReadShadowCopySetting(userConfig);
		if (m_shadowCopy) PrepareShadowDir();

		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(pluginDir)) {
			if (entry.is_regular_file() && entry.path().extension() == ".dll") {
				AddPluginFromDisk(entry.path().wstring(), userConfig);
			}
		}

		Logi(TAG, L"LoadAllPlugins complete, total plugins loaded: ", m_plugins.size());

		StartPluginDirWatcher();
		NotifyActionsChanged();
	}


	void UnloadAllPlugins() {
		if (!m_plugins.empty()) {
			NotifyBeforePluginUnload();
		}
		for (auto& pair : m_plugins) {
			UnloadSinglePlugin(pair.second);
		}
		m_plugins.clear();
		m_dispatchOrder.clear();
		m_queryRecipients.clear();
		m_combinedActions.clear();
		BumpActionsVersion();
		allActions.clear();
		filteredActions.clear();
	}

	// Plugins flagged CL_PLUGIN_REFRESH_ANY_THREAD refresh on their own worker and report back through
	// notify_actions_changed; the others run here one after another.
	void RefreshAllActions() {
		Logi(TAG, L"RefreshAllActions start, clearing existing actions...");
		m_suppressNotifications = true; // 暂时禁用通知

		for (const uint16_t pluginId : m_dispatchOrder) {
			PluginInfo& info = m_plugins[pluginId];
			if (!info.loaded || !CL_PLUGIN_HAS(info, refresh_actions)) continue;
			if (info.apiFlags & CL_PLUGIN_REFRESH_ANY_THREAD) {
				StartRefreshAsync(info);
				continue;
			}
			RefreshPluginInline(info);
		}

		m_suppressNotifications = false; // 重新启用通知
		NotifyActionsChanged();
	}

	void DispatchUserInput(const std::wstring& input) {
		m_suppressNotifications = true; // 防止在用户输入过程中触发递归刷新
		const std::wstring lowerInput = MyToLower(input);

		for (const uint16_t pluginId : m_dispatchOrder) {
			PluginInfo& info = m_plugins[pluginId];
			if (!info.loaded || !CL_PLUGIN_HAS(info, on_user_input) || !IsRoutedTo(info, lowerInput)) continue;
			const uint64_t start = GetTickCount64();
			info.api->on_user_input(info.self, input.c_str());
			WarnIfSlow(info, L"on_user_input", start);
		}

		m_suppressNotifications = false; // 重新启用通知
	}

	// Starts a new query for `input`: cancels the previous one on every plugin that received it and
	// returns the id results must carry. Called once per key stroke before the dispatches below.
	uint64_t BeginQuery() {
		const uint64_t previous = s_currentQueryId.load(std::memory_order_acquire);
		const uint64_t queryId = s_currentQueryId.fetch_add(1, std::memory_order_acq_rel) + 1;
		for (const uint16_t pluginId : m_queryRecipients) {
			const auto it = m_plugins.find(pluginId);
			if (it == m_plugins.end() || !it->second.loaded || !CL_PLUGIN_HAS(it->second, on_query_cancel)) continue;
			it->second.api->on_query_cancel(it->second.self, previous);
		}
		m_queryRecipients.clear();
		{
			// Batches still queued for the old query will be dropped by id; free them now.
			std::lock_guard<std::mutex> lock(m_resultsMutex);
			m_pendingResults.erase(std::remove_if(m_pendingResults.begin(), m_pendingResults.end(),
												[queryId](const PendingResults& r) { return r.queryId != queryId; }),
									m_pendingResults.end());
		}
		return queryId;
	}

	// Items plugins want shown instead of the normal search for this input.
	// Not intercepted = no items and no take-over.
	InterceptResult DispatchInterceptInput(const std::wstring& input) {
		InterceptResult result;
		const std::wstring lowerInput = MyToLower(input);

		for (const uint16_t pluginId : m_dispatchOrder) {
			PluginInfo& info = m_plugins[pluginId];
			if (!info.loaded || !CL_PLUGIN_HAS(info, on_intercept_input) || !IsRoutedTo(info, lowerInput)) continue;
			CL_ResultSink sink;
			const uint64_t start = GetTickCount64();
			info.api->on_intercept_input(info.self, input.c_str(), &sink);
			WarnIfSlow(info, L"on_intercept_input", start);
			result.tookOver |= sink.takeOver;
			if (!sink.actions.empty()) {
				// 使用 std::make_move_iterator 避免共享指针不必要的引用计数原子操作
				result.actions.insert(
					result.actions.end(),
					std::make_move_iterator(sink.actions.begin()),
					std::make_move_iterator(sink.actions.end())
				);
			}
		}

		return result;
	}

	// Asynchronous search (2.2): plugins answer later through post_results with this query id.
	void DispatchQuery(const uint64_t queryId, const std::wstring& input, const std::wstring& arg) {
		const std::wstring lowerInput = MyToLower(input);
		for (const uint16_t pluginId : m_dispatchOrder) {
			PluginInfo& info = m_plugins[pluginId];
			if (!info.loaded || !CL_PLUGIN_HAS(info, on_query) || !IsRoutedTo(info, lowerInput)) continue;
			const uint64_t start = GetTickCount64();
			info.api->on_query(info.self, queryId, input.c_str(), arg.c_str());
			WarnIfSlow(info, L"on_query", start);
			m_queryRecipients.push_back(pluginId);
		}
	}

	// 下面几个回调里插件可能弹菜单、创建自己的窗口。主窗口是 Per-Monitor 窗口，从它的消息里调用插件时线程处于
	// 主窗口的 DPI 上下文；插件窗口尚未自行适配 DPI，所以切回不感知上下文让系统照旧整体拉伸它们。
	// 带屏幕坐标的回调由调用方在不感知上下文里取坐标（见 CandyLauncher.cpp / EditManager.hpp）
	int DispatchSendHotKey(std::shared_ptr<BaseAction>& action, const UINT vk, const UINT uint, const WPARAM wparam) {
		uint64_t userData = 0;
		PluginInfo* info = ResolveActionOwner(action.get(), userData);
		if (!info || !CL_PLUGIN_HAS(*info, on_hotkey)) {
			return 0;
		}
		Dpi::ThreadContextScope unaware(Dpi::UnawareContext());
		return info->api->on_hotkey(info->self, userData, vk, uint, wparam);
	}

	bool DispatchItemRightClick(const std::shared_ptr<BaseAction>& action, HWND parentHwnd, POINT screenPt) {
		uint64_t userData = 0;
		PluginInfo* info = ResolveActionOwner(action.get(), userData);
		if (!info) {
			Logw(TAG, L"DispatchItemRightClick skipped: action/plugin not found");
			return false;
		}
		Logi(TAG, L"DispatchItemRightClick pluginId=", info->pluginId, L", apiMinor=", info->apiMinor);
		if (!CL_PLUGIN_HAS(*info, on_item_right_click)) {
			return false;
		}
		Dpi::ThreadContextScope unaware(Dpi::UnawareContext());
		return info->api->on_item_right_click(info->self, userData, parentHwnd, screenPt) != 0;
	}

	bool DispatchItemShiftRightClick(const std::shared_ptr<BaseAction>& action, HWND parentHwnd, POINT screenPt) {
		uint64_t userData = 0;
		PluginInfo* info = ResolveActionOwner(action.get(), userData);
		if (!info) {
			Logw(TAG, L"DispatchItemShiftRightClick skipped: action/plugin not found");
			return false;
		}
		Logi(TAG, L"DispatchItemShiftRightClick pluginId=", info->pluginId, L", apiMinor=", info->apiMinor);
		if (!CL_PLUGIN_HAS(*info, on_item_shift_right_click)) {
			return false;
		}
		Dpi::ThreadContextScope unaware(Dpi::UnawareContext());
		return info->api->on_item_shift_right_click(info->self, userData, parentHwnd, screenPt) != 0;
	}

	bool DispatchItemBeginDrag(const std::shared_ptr<BaseAction>& action, HWND sourceHwnd, POINT screenPt) {
		uint64_t userData = 0;
		PluginInfo* info = ResolveActionOwner(action.get(), userData);
		if (!info || !CL_PLUGIN_HAS(*info, on_item_begin_drag)) {
			return false;
		}
		Dpi::ThreadContextScope unaware(Dpi::UnawareContext());
		return info->api->on_item_begin_drag(info->self, userData, sourceHwnd, screenPt) != 0;
	}

	// Text the owning plugin wants copied for `action` (Ctrl+C); false when the plugin does not provide one.
	bool DispatchItemCopyText(const std::shared_ptr<BaseAction>& action, std::wstring& text) {
		uint64_t userData = 0;
		PluginInfo* info = ResolveActionOwner(action.get(), userData);
		if (!info || !CL_PLUGIN_HAS(*info, get_item_copy_text)) {
			return false;
		}
		text.clear();
		return info->api->get_item_copy_text(info->self, userData, [](void* ctx, const wchar_t* str, const size_t len) {
			if (str) static_cast<std::wstring*>(ctx)->assign(str, len);
		}, &text) != 0;
	}

	static bool DispatchActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) {
		if (!action) return false;
		// 执行动作时插件常会打开自己的窗口（索引管理器等），它们还没有自行适配 DPI，见 DispatchSendHotKey 的说明
		Dpi::ThreadContextScope unaware(Dpi::UnawareContext());
		if (!dynamic_cast<PluginActionProxy*>(action.get())) {
			const auto action1 = std::dynamic_pointer_cast<BaseLaunchAction>(action);
			if (!action1) return false;
			RecordUsage(*action1);
			action1->Invoke();
			return true;
		}

		uint64_t userData = 0;
		PluginInfo* info = ResolveActionOwner(action.get(), userData);
		if (!info || !CL_PLUGIN_HAS(*info, on_action_execute)) {
			return false;
		}
		RecordUsage(*action);
		return info->api->on_action_execute(info->self, userData, arg.c_str()) != 0;
	}

	// 使用历史只在开启“按使用频率提升排序”时记录
	static void RecordUsage(const BaseAction& action) {
		if (!pref_usage_history_boost || action.usageKey == 0) return;
		g_usageHistory.Record(action.usageKey);
	}

	static void DispatchSettingItemExecute(const uint16_t pluginId, const std::string& key, HWND parentHwnd) {
		const auto it = m_plugins.find(pluginId);
		if (it == m_plugins.end() || !it->second.loaded || !CL_PLUGIN_HAS(it->second, on_setting_item_execute)) {
			return;
		}
		Dpi::ThreadContextScope unaware(Dpi::UnawareContext());
		it->second.api->on_setting_item_execute(it->second.self, utf8_to_wide(key).c_str(), parentHwnd);
	}

	std::vector<std::wstring> GetLoadedPlugins() const {
		std::vector<std::wstring> result;
		for (const auto& pair : m_plugins) {
			if (pair.second.loaded) {
				result.push_back(pair.second.name);
			}
		}
		return result;
	}

	const std::vector<PluginCatalogInfo>& GetPluginCatalog() const {
		return g_pluginCatalog;
	}

	const PluginInfo* FindLoadedPluginByPackageName(const std::wstring& pkgName) const {
		for (const auto& [pluginId, info] : m_plugins) {
			if (info.pkgName == pkgName) {
				return &info;
			}
		}
		return nullptr;
	}

	bool SetPluginEnabled(uint16_t callerPluginId, const std::wstring& packageName, bool enabled) {
		if (!CanUseRestrictedPluginBrowserApi(callerPluginId) || packageName.empty()) {
			return false;
		}
		if (packageName == PLUGIN_BROWSER_PACKAGE_NAME && !enabled) {
			Logi(TAG, L"Reject disabling plugin browser itself");
			return false;
		}

		// 提前清除插件引用，防止全局列表索引的 action出现内存错误
		filteredActions.clear();
		allActions.clear();
		BumpActionsVersion();
		bool foundCatalog = false;
		for (auto& catalogInfo : g_pluginCatalog) {
			if (catalogInfo.pkgName == packageName) {
				catalogInfo.enabled = enabled;
				foundCatalog = true;
				break;
			}
		}
		if (!foundCatalog) {
			return false;
		}

		SavePluginEnabledState(packageName, enabled);
		{
			std::unique_lock<std::shared_mutex> lock(g_settingsMapMutex);
			auto it = g_settings_map.find(wide_to_utf8(packageName));
			if (it != g_settings_map.end()) {
				it->second.boolValue = enabled;
			} else {
				SettingItem setting;
				setting.key = wide_to_utf8(packageName);
				setting.type = "expandswitch";
				setting.boolValue = enabled;
				g_settings_map[setting.key] = setting;
			}
		}

		SyncPluginsWithSettings();
		NotifyUserSettingsLoadDone();
		// RefreshAllActions(); // TODO:  不要刷新全部，只刷新需要的插件
		return true;
	}

	void ShowResultsDerectly(const std::vector<std::shared_ptr<BaseAction>>& list) {
		// SendMessage(g_listViewHwnd, WM_SETREDRAW, FALSE, 0); // 不要使用暂停重绘，反而会造成列表闪烁
		filteredActions = list;
		ListView_SetItemCountEx(g_listViewHwnd, filteredActions.size(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
		if (!filteredActions.empty()) {
			// 设置第一项为选中状态
			ListView_SetItemState(g_listViewHwnd, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			// 确保第一项可见
			ListView_EnsureVisible(g_listViewHwnd, 0, FALSE);
		}
		InvalidateRect(g_listViewHwnd, nullptr, TRUE);
	}

	void SyncPluginPrioritiesFromSettings() {
		bool changed = false;
		for (auto& [pluginId, info] : m_plugins) {
			if (info.pkgName.empty()) continue;
			const std::string priorityKey = wide_to_utf8(info.pkgName) + ".priority";
			int32_t priority = 0;
			{
				std::shared_lock<std::shared_mutex> lock(g_settingsMapMutex);
				const auto it = g_settings_map.find(priorityKey);
				priority = (it != g_settings_map.end()) ? static_cast<int32_t>(it->second.intValue) : 0;
			}
			if (priority == info.priority && (!info.context || info.context->priority.load() == priority)) continue;
			info.priority = priority;
			changed = true;
			if (!info.context) continue;
			// Proxies carry the priority so the merged list sorts without a lookup; restamp the live sets.
			info.context->priority.store(priority);
			std::lock_guard<std::mutex> lock(info.context->mutex);
			for (auto& [setId, set] : info.context->sets) {
				for (auto& action : set->actions) action->pluginPriority = priority;
			}
		}
		if (changed) {
			RebuildDispatchOrder();
			BumpActionsVersion();
		}
	}

	// ---- 2.3 localization ----

	const PluginCatalogInfo* FindCatalogByFilePath(const std::wstring& filePath) const {
		for (const auto& catalogInfo : g_pluginCatalog) {
			if (catalogInfo.filePath == filePath) return &catalogInfo;
		}
		return nullptr;
	}

	// Builds the flat key -> text view the plugin's get_string reads, for the current language.
	void ResolvePluginStrings(PluginInfo& info) const {
		if (!info.context) return;
		const PluginCatalogInfo* catalogInfo = FindCatalogByFilePath(info.filePath);
		std::shared_ptr<const PluginStringMap> resolved;
		if (catalogInfo && catalogInfo->strings && !catalogInfo->strings->Empty()) {
			resolved = catalogInfo->strings->Resolve(PluginI18n::Language());
			Logi(TAG, L"Strings for ", info.name, L": ", resolved->size(), L" keys, ", catalogInfo->strings->languages.size(),
				L" languages, ui ", PluginI18n::Language(), L", name \"", catalogInfo->DisplayName(), L"\"");
		}
		std::atomic_store(&info.context->strings, resolved);
	}

	// pref_ui_language changed (the settings window re-ran InitializeI18n): re-read every string table so edited
	// translations show up, rebuild each loaded plugin's view, then tell the plugins. The caller refreshes all
	// actions afterwards, which republishes titles in the new language.
	void SyncLanguageFromSettings() {
		const std::wstring language = g_uiLanguageCode;
		// Plugins are loaded before the settings pass that runs InitializeI18n, so the first sync only corrects
		// the language the tables were resolved with; nothing is published yet, so no plugin needs telling.
		const bool firstSync = !m_languageSynced;
		m_languageSynced = true;
		if (language == m_language) return;
		Logi(TAG, firstSync ? L"UI language for plugins: " : L"UI language changed: ", m_language, L" -> ", language);
		m_language = language;
		PluginI18n::SetLanguage(language);
		if (!firstSync) {
			for (auto& catalogInfo : g_pluginCatalog) {
				catalogInfo.strings = PluginI18n::Load(catalogInfo.filePath, ManifestPathFor(catalogInfo.filePath));
			}
		}
		for (const uint16_t pluginId : m_dispatchOrder) {
			PluginInfo& info = m_plugins[pluginId];
			if (!info.loaded) continue;
			ResolvePluginStrings(info);
			if (!firstSync && CL_PLUGIN_HAS(info, on_language_changed)) {
				const uint64_t start = GetTickCount64();
				info.api->on_language_changed(info.self, language.c_str());
				WarnIfSlow(info, L"on_language_changed", start);
			}
		}
	}

	void NotifyUserSettingsLoadDone() {
		SyncLanguageFromSettings();
		SyncPluginPrioritiesFromSettings();
		for (const uint16_t pluginId : m_dispatchOrder) {
			PluginInfo& info = m_plugins[pluginId];
			if (!info.loaded) continue;
			DispatchSettingChanges(info);
			if (CL_PLUGIN_HAS(info, on_user_settings_loaded)) {
				info.api->on_user_settings_loaded(info.self);
			}
		}
	}

	void SyncPluginsWithSettings() {
		bool needsUnload = false;
		for (const auto& catalogInfo : g_pluginCatalog) {
			bool shouldEnable = catalogInfo.enabled;
			if (!catalogInfo.pkgName.empty()) {
				std::shared_lock<std::shared_mutex> lock(g_settingsMapMutex);
				const auto it = g_settings_map.find(wide_to_utf8(catalogInfo.pkgName));
				if (it != g_settings_map.end()) {
					shouldEnable = it->second.boolValue;
				}
			}

			const PluginInfo* loadedPlugin = !catalogInfo.pkgName.empty()
												? FindLoadedPluginByPackageName(catalogInfo.pkgName)
												: FindLoadedPluginByFilePath(catalogInfo.filePath);
			if (!shouldEnable && loadedPlugin) {
				needsUnload = true;
				break;
			}
		}

		if (needsUnload) {
			NotifyBeforePluginUnload();
		}

		for (auto& catalogInfo : g_pluginCatalog) {
			bool shouldEnable = catalogInfo.enabled;
			if (!catalogInfo.pkgName.empty()) {
				std::shared_lock<std::shared_mutex> lock(g_settingsMapMutex);
				const auto it = g_settings_map.find(wide_to_utf8(catalogInfo.pkgName));
				if (it != g_settings_map.end()) {
					shouldEnable = it->second.boolValue;
				}
			}

			const PluginInfo* loadedPlugin = !catalogInfo.pkgName.empty()
												? FindLoadedPluginByPackageName(catalogInfo.pkgName)
												: FindLoadedPluginByFilePath(catalogInfo.filePath);

			if (shouldEnable) {
				if (!loadedPlugin && !catalogInfo.filePath.empty()) {
					const bool loadResult = LoadPlugin(catalogInfo.filePath);
					Logi(TAG, L"SyncPluginsWithSettings load plugin: ", catalogInfo.filePath,
									L", result: ", (loadResult ? L"SUCCESS" : L"FAILED"));
					loadedPlugin = !catalogInfo.pkgName.empty()
										? FindLoadedPluginByPackageName(catalogInfo.pkgName)
										: FindLoadedPluginByFilePath(catalogInfo.filePath);
					if (loadResult && loadedPlugin) {
						RefreshPlugin(m_plugins[loadedPlugin->pluginId]);
					}
				}

				if (loadedPlugin) {
					catalogInfo.enabled = true;
					catalogInfo.name = loadedPlugin->name;
					catalogInfo.version = loadedPlugin->version;
					catalogInfo.pkgName = loadedPlugin->pkgName;
					if (catalogInfo.defaultSettingJson.empty()) {
						catalogInfo.defaultSettingJson = loadedPlugin->defaultSettingJson;
					}
				}
			} else {
				if (loadedPlugin) {
					UnloadPlugin(loadedPlugin->pluginId);
				}
				catalogInfo.enabled = false;
			}
		}
		SyncPluginPrioritiesFromSettings();
	}

	// Appends every global-search item of every loaded plugin, highest priority first. The merged list is
	// cached and only rebuilt after a publish / priority / load change, not on every key stroke.
	void AppendPluginActions(std::vector<std::shared_ptr<BaseAction>>& target) {
		const uint64_t version = ActionsVersion();
		if (version != m_combinedVersion) {
			m_combinedActions.clear();
			for (const uint16_t pluginId : m_dispatchOrder) {
				const PluginInfo& info = m_plugins[pluginId];
				if (!info.loaded || !info.context) continue;
				std::vector<std::shared_ptr<const PluginActionSet>> sets;
				{
					std::lock_guard<std::mutex> lock(info.context->mutex);
					for (const auto& [setId, set] : info.context->sets) {
						if (set->flags & CL_SET_GLOBAL_SEARCH) sets.push_back(set);
					}
				}
				for (const auto& set : sets) {
					m_combinedActions.insert(m_combinedActions.end(), set->actions.begin(), set->actions.end());
				}
			}
			m_combinedVersion = version;
		}
		target.insert(target.end(), m_combinedActions.begin(), m_combinedActions.end());
	}

	void SetActionsChangedCallback(std::function<void()> callback) {
		m_onActionsChanged = callback;
	}

	void SetBeforePluginUnloadCallback(std::function<void()> callback) {
		m_beforePluginUnload = callback;
	}

	void SetQueryResultsCallback(std::function<void(uint64_t, std::vector<std::shared_ptr<BaseAction>>&&, int32_t)> callback) {
		m_onQueryResults = std::move(callback);
	}

	void SetCatalogChangedCallback(std::function<void()> callback) {
		m_onCatalogChanged = std::move(callback);
	}

	// ---- cross-thread requests (any thread) and their UI-thread handlers ----

	// post_results: the batch is queued and one WM_PLUGIN_QUERY_RESULTS is posted per burst.
	bool QueueQueryResults(const uint64_t queryId, std::vector<std::shared_ptr<BaseAction>>&& actions, const int32_t flags) {
		if (queryId != CurrentQueryId()) return false;
		{
			std::lock_guard<std::mutex> lock(m_resultsMutex);
			m_pendingResults.push_back({queryId, std::move(actions), flags});
		}
		if (g_mainHwnd && !m_resultsPosted.exchange(true)) {
			PostMessageW(g_mainHwnd, WM_PLUGIN_QUERY_RESULTS, 0, 0);
		}
		return true;
	}

	void HandleQueryResultsMessage() {
		m_resultsPosted.store(false);
		std::vector<PendingResults> batches;
		{
			std::lock_guard<std::mutex> lock(m_resultsMutex);
			batches.swap(m_pendingResults);
		}
		const uint64_t current = CurrentQueryId();
		for (auto& batch : batches) {
			if (batch.queryId != current || !m_onQueryResults) continue;
			m_onQueryResults(batch.queryId, std::move(batch.actions), batch.flags);
		}
	}

	// request_refresh: the plugin wants refresh_actions run again; marshalled to the UI thread, which
	// starts the worker (CL_PLUGIN_REFRESH_ANY_THREAD) or refreshes inline.
	void RequestPluginRefresh(const uint16_t pluginId) {
		if (g_mainHwnd) PostMessageW(g_mainHwnd, WM_PLUGIN_REFRESH_REQUEST, pluginId, 0);
	}

	void HandleRefreshRequestMessage(const uint16_t pluginId) {
		const auto it = m_plugins.find(pluginId);
		if (it == m_plugins.end() || !it->second.loaded) return;
		RefreshPlugin(it->second);
		// The worker path reports back through RequestActionsChanged when it is done.
		if (!(it->second.apiFlags & CL_PLUGIN_REFRESH_ANY_THREAD)) NotifyActionsChanged();
	}

	// notify_actions_changed and the refresh workers: coalesced into one WM_PLUGIN_ACTIONS_CHANGED.
	void RequestActionsChanged() {
		BumpActionsVersion();
		if (g_mainHwnd && !m_actionsChangedPosted.exchange(true)) {
			PostMessageW(g_mainHwnd, WM_PLUGIN_ACTIONS_CHANGED, 0, 0);
		}
	}

	void HandleActionsChangedMessage() {
		m_actionsChangedPosted.store(false);
		NotifyActionsChanged();
	}

	void HandleDirChangedMessage() {
		m_dirChangedPosted.store(false);
		// Debounce: a copy produces several notifications, reconcile once the writes settle.
		if (g_mainHwnd) SetTimer(g_mainHwnd, TIMER_PLUGIN_DIR_CHANGED, 1000, nullptr);
	}

	// Hot install / update: compares the plugin folder with the catalog and loads, reloads or unloads
	// what changed. Called from the UI thread after the folder watcher settled.
	void ReconcilePluginDir() {
		namespace fs = std::filesystem;
		if (m_pluginDir.empty() || !fs::is_directory(m_pluginDir)) return;
		const nlohmann::json userConfig = LoadUserPluginConfig();
		std::map<std::wstring, fs::file_time_type> onDisk;
		for (const auto& entry : fs::directory_iterator(m_pluginDir)) {
			if (entry.is_regular_file() && entry.path().extension() == ".dll") {
				std::error_code ec;
				onDisk[entry.path().wstring()] = fs::last_write_time(entry.path(), ec);
			}
		}

		bool changed = false;
		// Removed or replaced files.
		for (size_t i = 0; i < g_pluginCatalog.size();) {
			PluginCatalogInfo& catalogInfo = g_pluginCatalog[i];
			const auto disk = onDisk.find(catalogInfo.filePath);
			if (disk == onDisk.end()) {
				Logi(TAG, L"Plugin removed from disk: ", catalogInfo.filePath);
				if (const PluginInfo* loaded = FindLoadedPluginByFilePath(catalogInfo.filePath)) {
					UnloadPlugin(loaded->pluginId);
				}
				g_pluginCatalog.erase(g_pluginCatalog.begin() + static_cast<std::ptrdiff_t>(i));
				changed = true;
				continue;
			}
			if (disk->second != catalogInfo.fileTime) {
				Logi(TAG, L"Plugin changed on disk, reloading: ", catalogInfo.filePath);
				if (const PluginInfo* loaded = FindLoadedPluginByFilePath(catalogInfo.filePath)) {
					UnloadPlugin(loaded->pluginId);
				}
				const std::wstring path = catalogInfo.filePath;
				g_pluginCatalog.erase(g_pluginCatalog.begin() + static_cast<std::ptrdiff_t>(i));
				AddPluginFromDisk(path, userConfig, true);
				changed = true;
				continue;
			}
			++i;
		}
		// New files.
		for (const auto& [path, time] : onDisk) {
			bool known = false;
			for (const auto& catalogInfo : g_pluginCatalog) {
				if (catalogInfo.filePath == path) { known = true; break; }
			}
			if (known) continue;
			Logi(TAG, L"New plugin on disk: ", path);
			AddPluginFromDisk(path, userConfig, true);
			changed = true;
		}

		if (!changed) return;
		if (m_onCatalogChanged) m_onCatalogChanged();
		NotifyActionsChanged();
	}

	HBITMAP LoadResIconAsBitmap(int nResID, int cx, int cy) {
		HICON hIcon = (HICON)LoadImage(g_hInst, MAKEINTRESOURCE(nResID),
										IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR);
		if (!hIcon) return nullptr;

		HDC hDC = GetDC(nullptr);
		HDC hMemDC = CreateCompatibleDC(hDC);

		HBITMAP hBmp = CreateCompatibleBitmap(hDC, cx, cy);
		HGDIOBJ hOld = SelectObject(hMemDC, hBmp);

		DrawIconEx(hMemDC, 0, 0, hIcon, cx, cy, 0, nullptr, DI_NORMAL);

		SelectObject(hMemDC, hOld);
		DeleteDC(hMemDC);
		ReleaseDC(nullptr, hDC);
		DestroyIcon(hIcon);

		return hBmp;
	}

	bool BeginOleDragDropData(const OleDragDropData& data, HWND sourceHwnd) {
		if (sourceHwnd) {
			SetForegroundWindow(sourceHwnd);
		}
		const bool dragHandled = BeginOleDataDragDrop(data);
		const bool shouldHideAfterDrag = sourceHwnd
			&& IsWindow(sourceHwnd)
			&& GetForegroundWindow() != g_mainHwnd
			&& g_settings_map["pref_close_on_dismiss_focus"].stringValue != "null";
		if (shouldHideAfterDrag) {
			HideWindow();
		}
		return dragHandled;
	}

	bool SetPluginSettingValue(uint16_t callerPluginId, const std::string& key, const nlohmann::json& value) {
		const auto pluginIt = m_plugins.find(callerPluginId);
		if (pluginIt == m_plugins.end() || key.empty()) {
			return false;
		}
		// 插件只能修改自己的设置项
		const std::string keyPrefix = wide_to_utf8(pluginIt->second.pkgName) + ".";
		if (key.rfind(keyPrefix, 0) != 0) {
			Loge(TAG, L"SetPluginSettingValue reject key: ", utf8_to_wide(key));
			return false;
		}
		{
			std::unique_lock<std::shared_mutex> lock(g_settingsMapMutex);
			const auto settingIt = g_settings_map.find(key);
			if (settingIt == g_settings_map.end()) {
				Loge(TAG, L"SetPluginSettingValue key not found: ", utf8_to_wide(key));
				return false;
			}
			try {
				settingIt->second.setValue(value);
			} catch (const std::exception& e) {
				Loge(TAG, L"SetPluginSettingValue value type error: ", e.what());
				return false;
			}
			// The plugin changed it itself: no on_setting_changed for this one.
			pluginIt->second.settingSnapshot[key] = SerializeSettingValue(settingIt->second);
		}

		SetSettingValueInList(g_settings_ui_last_save, key, value);
		SetSettingValueInList(g_settings_ui, key, value);
		SaveUserConfigValue(key, value);
		return true;
	}

	bool CanUseRestrictedPluginBrowserApi(uint16_t callerPluginId) const {
		const auto it = m_plugins.find(callerPluginId);
		return it != m_plugins.end() && it->second.pkgName == PLUGIN_BROWSER_PACKAGE_NAME;
	}

private:
	// Finds the loaded plugin that published `action`; null for host actions or items of an unloaded plugin.
	static PluginInfo* ResolveActionOwner(const BaseAction* action, uint64_t& userData) {
		const auto* proxy = dynamic_cast<const PluginActionProxy*>(action);
		if (!proxy) return nullptr;
		const auto it = m_plugins.find(proxy->pluginId);
		if (it == m_plugins.end() || !it->second.loaded || !it->second.context ||
			it->second.context->serial != proxy->ownerSerial) {
			return nullptr;
		}
		userData = proxy->userData;
		return &it->second;
	}

	void RebuildDispatchOrder() {
		m_dispatchOrder.clear();
		for (const auto& [pluginId, info] : m_plugins) {
			if (info.loaded) m_dispatchOrder.push_back(pluginId);
		}
		std::stable_sort(m_dispatchOrder.begin(), m_dispatchOrder.end(), [](const uint16_t a, const uint16_t b) {
			const int32_t pa = m_plugins[a].priority, pb = m_plugins[b].priority;
			return pa != pb ? pa > pb : a < b;
		});
	}

	// 2.2 trigger keywords: no keywords configured = every input.
	static bool IsRoutedTo(const PluginInfo& info, const std::wstring& lowerInput) {
		if (!info.context) return true;
		std::lock_guard<std::mutex> lock(info.context->mutex);
		const CL_PluginContext& ctx = *info.context;
		if (!ctx.keywordsConfigured) return true;
		if (ctx.triggerFlags & CL_TRIGGER_ALSO_GLOBAL) return true;
		for (const auto& keyword : ctx.keywords) {
			if (lowerInput.size() >= keyword.size() && lowerInput.compare(0, keyword.size(), keyword) == 0) return true;
		}
		return false;
	}

	static void WarnIfSlow(PluginInfo& info, const wchar_t* what, const uint64_t startTick) {
		const uint64_t now = GetTickCount64();
		const uint64_t elapsed = now - startTick;
		if (elapsed < PLUGIN_SLOW_CALLBACK_MS || now - info.lastSlowLogTick < 5000) return;
		info.lastSlowLogTick = now;
		Logw(TAG, L"Slow plugin callback: ", info.name, L" ", what, L" took ", elapsed, L" ms");
	}

	void RefreshPluginInline(PluginInfo& info) {
		Logi(TAG, L"Loading actions from plugin: ", info.name);
		const auto refreshStart = std::chrono::steady_clock::now();
		info.api->refresh_actions(info.self);
		info.refreshTimeMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - refreshStart).count());
		Logi(TAG, L"Refresh done: ", info.name, L", ", info.refreshTimeMs, L" ms");
	}

	void RefreshPlugin(PluginInfo& info) {
		if (!info.loaded || !CL_PLUGIN_HAS(info, refresh_actions)) return;
		if (info.apiFlags & CL_PLUGIN_REFRESH_ANY_THREAD) StartRefreshAsync(info);
		else RefreshPluginInline(info);
	}

	void StartRefreshAsync(PluginInfo& info) {
		if (!info.refreshWorker) info.refreshWorker = std::make_unique<PluginRefreshWorker>();
		PluginRefreshWorker& worker = *info.refreshWorker;
		{
			std::lock_guard<std::mutex> lock(worker.mutex);
			if (worker.running) {
				worker.pending = true;
				return;
			}
			worker.running = true;
			worker.pending = false;
		}
		if (worker.thread.joinable()) worker.thread.join();
		// `info` lives in m_plugins until UnloadSinglePlugin, which joins this thread first.
		worker.thread = std::thread([this, &info, &worker] {
			const HRESULT coInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			for (;;) {
				RefreshPluginInline(info);
				RequestActionsChanged();
				std::lock_guard<std::mutex> lock(worker.mutex);
				if (worker.pending) {
					worker.pending = false;
					continue;
				}
				worker.running = false;
				break;
			}
			if (SUCCEEDED(coInit)) CoUninitialize();
		});
	}

	static void JoinRefreshWorker(PluginInfo& info) {
		if (!info.refreshWorker) return;
		if (info.refreshWorker->thread.joinable()) info.refreshWorker->thread.join();
	}

	static std::string SerializeSettingValue(const SettingItem& item) {
		if (item.type == "string" || item.type == "hotkeystring" || item.type == "list") return "s:" + item.stringValue;
		if (item.type == "long") return "i:" + std::to_string(item.intValue);
		if (item.type == "bool" || item.type == "expandswitch") return item.boolValue ? "b:1" : "b:0";
		if (item.type == "double") return "d:" + std::to_string(item.doubleValue);
		if (item.type == "stringArr") {
			std::string joined = "a:";
			for (const auto& value : item.stringArr) joined += value + '\x1f';
			return joined;
		}
		return std::string();
	}

	// on_setting_changed for every "<pkg>.*" value that differs from the previous snapshot.
	static void DispatchSettingChanges(PluginInfo& info) {
		if (info.pkgName.empty()) return;
		const std::string prefix = wide_to_utf8(info.pkgName) + ".";
		std::map<std::string, std::string> current;
		{
			std::shared_lock<std::shared_mutex> lock(g_settingsMapMutex);
			for (const auto& [key, item] : g_settings_map) {
				if (key.rfind(prefix, 0) == 0) current[key] = SerializeSettingValue(item);
			}
		}
		const bool firstTime = info.settingSnapshot.empty();
		if (!firstTime && CL_PLUGIN_HAS(info, on_setting_changed)) {
			for (const auto& [key, value] : current) {
				const auto old = info.settingSnapshot.find(key);
				if (old != info.settingSnapshot.end() && old->second == value) continue;
				info.api->on_setting_changed(info.self, utf8_to_wide(key).c_str());
			}
		}
		info.settingSnapshot = std::move(current);
	}

	static const PluginInfo* FindLoadedPluginByFilePath(const std::wstring& filePath) {
		for (const auto& [pluginId, info] : m_plugins) {
			if (info.filePath == filePath) {
				return &info;
			}
		}
		return nullptr;
	}

	static nlohmann::json LoadUserPluginConfig() {
		const std::string userConfigText = ReadUtf8File(USER_SETTINGS_PATH);
		if (userConfigText.empty()) {
			return {};
		}

		try {
			return nlohmann::json::parse(userConfigText);
		} catch (const std::exception& e) {
			Loge(TAG, L"LoadUserPluginConfig parse error: ", e.what());
			return {};
		}
	}

	static void SavePluginEnabledState(const std::wstring& pkgName, bool enabled) {
		if (pkgName.empty()) {
			return;
		}
		SaveUserConfigValue(wide_to_utf8(pkgName), enabled);
	}

	// 将单个设置值合并写入 user_settings.json
	static void SaveUserConfigValue(const std::string& key, const nlohmann::json& value) {
		nlohmann::json mergedConfig = LoadUserPluginConfig();
		if (!mergedConfig.is_object()) {
			mergedConfig = nlohmann::json::object();
		}
		mergedConfig[key] = value;

		std::ofstream out(std::filesystem::path(USER_SETTINGS_PATH), std::ios::binary);
		if (!out) {
			Loge(TAG, L"SaveUserConfigValue open file failed: ", USER_SETTINGS_PATH);
			return;
		}
		out << mergedConfig.dump(4) << std::endl;
	}

	// 递归更新设置列表中指定 key 的值，保证设置窗口重新打开时显示新值
	static bool SetSettingValueInList(std::vector<SettingItem>& settings, const std::string& key, const nlohmann::json& value) {
		for (auto& setting : settings) {
			if (setting.key == key) {
				try {
					setting.setValue(value);
				} catch (...) {
				}
				return true;
			}
			if ((setting.type == "expand" || setting.type == "expandswitch") &&
				SetSettingValueInList(setting.children, key, value)) {
				return true;
			}
		}
		return false;
	}

	static bool IsPluginEnabledByConfig(const std::wstring& pkgName, const nlohmann::json& userConfig) {
		if (pkgName.empty() || userConfig.is_null() || !userConfig.is_object()) {
			return true;
		}

		const std::string utf8PkgName = wide_to_utf8(pkgName);
		if (!userConfig.contains(utf8PkgName)) {
			return true;
		}

		try {
			return JsonValueToBool(userConfig.at(utf8PkgName));
		} catch (const std::exception& e) {
			Loge(TAG, L"IsPluginEnabledByConfig parse value error: ", e.what());
			return true;
		}
	}

	static bool ReadShadowCopySetting(const nlohmann::json& userConfig) {
		if (!userConfig.is_object() || !userConfig.contains("pref_plugin_shadow_copy")) return false;
		try {
			return JsonValueToBool(userConfig.at("pref_plugin_shadow_copy"));
		} catch (...) {
			return false;
		}
	}

	static std::wstring SafeWString(const wchar_t* value) {
		return value ? std::wstring(value) : std::wstring();
	}

	// ---- catalog / manifest ----

	static std::wstring ManifestPathFor(const std::wstring& dllPath) {
		std::filesystem::path path(dllPath);
		return path.replace_extension().wstring() + PLUGIN_MANIFEST_SUFFIX;
	}

	// Manifest "min_windows_version": "7" / "8" / "8.1" / "10" / "11", or a kernel version "6.1" / "10.0.19041".
	static bool ParseWindowsVersionRequirement(std::string text, DWORD& major, DWORD& minor, DWORD& build) {
		text.erase(std::remove_if(text.begin(), text.end(),
			[](unsigned char c) { return std::isspace(c) || c == '"'; }), text.end());
		major = minor = build = 0;
		if (text == "7") { major = 6; minor = 1; return true; }
		if (text == "8") { major = 6; minor = 2; return true; }
		if (text == "8.1") { major = 6; minor = 3; return true; }
		if (text == "10") { major = 10; return true; }
		if (text == "11") { major = 10; build = 22000; return true; }
		unsigned long parts[3] = {0, 0, 0};
		if (text.empty() || !std::isdigit(static_cast<unsigned char>(text[0]))) return false;
		if (sscanf_s(text.c_str(), "%lu.%lu.%lu", &parts[0], &parts[1], &parts[2]) < 1) return false;
		major = parts[0];
		minor = parts[1];
		build = parts[2];
		return true;
	}

	// True when the running Windows satisfies the manifest requirement; unparsable values are ignored with a warning.
	static bool IsWindowsVersionSatisfied(const std::string& requirement, const std::wstring& dllPath) {
		DWORD needMajor = 0, needMinor = 0, needBuild = 0;
		if (!ParseWindowsVersionRequirement(requirement, needMajor, needMinor, needBuild)) {
			Logw(TAG, L"Unrecognized min_windows_version \"", utf8_to_wide(requirement), L"\", ignored: ", dllPath);
			return true;
		}
		DWORD major = 0, minor = 0, build = 0;
		if (!GetWindowsVersionNumbers(major, minor, build)) return true;
		if (major != needMajor) return major > needMajor;
		if (minor != needMinor) return minor > needMinor;
		return build >= needBuild;
	}

	// "<stem>.plugin.json" next to the DLL describes the plugin without running its code.
	// Requirements the host cannot meet are recorded in unsupportedReason instead of failing (so the DLL is never probed).
	static bool ReadPluginManifest(const std::wstring& dllPath, PluginCatalogInfo& catalogInfo) {
		const std::wstring manifestPath = ManifestPathFor(dllPath);
		std::error_code ec;
		if (!std::filesystem::exists(manifestPath, ec)) return false;
		const std::string text = ReadUtf8File(manifestPath);
		if (text.empty()) return false;
		try {
			const nlohmann::json manifest = nlohmann::json::parse(text);
			if (!manifest.is_object() || !manifest.contains("package_name")) return false;
			catalogInfo.pkgName = utf8_to_wide(manifest.at("package_name").get<std::string>());
			if (catalogInfo.pkgName.empty()) return false;
			if (manifest.contains("name")) catalogInfo.name = utf8_to_wide(manifest.at("name").get<std::string>());
			if (manifest.contains("description")) catalogInfo.description = utf8_to_wide(manifest.at("description").get<std::string>());
			if (manifest.contains("version")) catalogInfo.version = utf8_to_wide(manifest.at("version").get<std::string>());
			if (manifest.contains("default_setting_json")) {
				const nlohmann::json& settings = manifest.at("default_setting_json");
				catalogInfo.defaultSettingJson = utf8_to_wide(settings.is_string() ? settings.get<std::string>() : settings.dump());
			}
			if (manifest.contains("keywords") && manifest.at("keywords").is_array()) {
				for (const auto& keyword : manifest.at("keywords")) {
					if (keyword.is_string()) catalogInfo.keywords.push_back(utf8_to_wide(keyword.get<std::string>()));
				}
			}
			if (manifest.contains("min_host_api_minor")) {
				const auto minor = manifest.at("min_host_api_minor").get<uint32_t>();
				if (minor > CL_API_VERSION_MINOR) {
					catalogInfo.unsupportedReason = L"needs host api 2." + std::to_wstring(minor);
				}
			}
			if (manifest.contains("min_windows_version")) {
				const nlohmann::json& value = manifest.at("min_windows_version");
				const std::string text = value.is_string() ? value.get<std::string>() : value.dump();
				catalogInfo.minWindowsVersion = utf8_to_wide(text);
				if (catalogInfo.unsupportedReason.empty() && !IsWindowsVersionSatisfied(text, dllPath)) {
					catalogInfo.unsupportedReason = L"needs Windows " + catalogInfo.minWindowsVersion + L" or later";
				}
			}
			catalogInfo.fromManifest = true;
			return true;
		} catch (const std::exception& e) {
			Loge(TAG, L"Manifest parse error ", manifestPath, L": ", e.what());
			return false;
		}
	}

	// Reads and validates the plugin table; the DLL must stay loaded while `api` is used.
	static const CL_PluginApi* GetValidatedPluginApi(HMODULE handle, const std::wstring& dllPath) {
		const auto getApi = reinterpret_cast<CL_GetPluginApiFn>(GetProcAddress(handle, CL_PLUGIN_ENTRY_NAME));
		if (!getApi) {
			Logw(TAG, L"Not a CandyLauncher plugin (no CandyPlugin_GetApi export): ", dllPath);
			return nullptr;
		}
		const CL_PluginApi* api = getApi();
		if (!api || !CL_HAS_FIELD(api, CL_PluginApi, destroy)) {
			Logw(TAG, L"Invalid plugin api table: ", dllPath);
			return nullptr;
		}
		if (api->api_major != CL_API_VERSION_MAJOR) {
			Logw(TAG, L"Plugin api major mismatch, plugin=", api->api_major,
				L", host=", CL_API_VERSION_MAJOR, L": ", dllPath);
			return nullptr;
		}
		if (!api->create || !api->destroy || !api->package_name || !*api->package_name) {
			Logw(TAG, L"Plugin api table misses create/destroy/package_name: ", dllPath);
			return nullptr;
		}
		return api;
	}

	static bool ProbePluginCatalogInfo(const std::wstring& dllPath, PluginCatalogInfo& catalogInfo) {
		catalogInfo.filePath = dllPath;
		catalogInfo.name = GetPluginNameFromPathW(dllPath);

		HMODULE handle = LoadLibraryW(dllPath.c_str());
		if (!handle) {
			Logw(TAG, L"Probe LoadLibraryW failed for: ", dllPath, L", Error: ", GetLastError());
			return false;
		}

		const CL_PluginApi* api = GetValidatedPluginApi(handle, dllPath);
		if (!api) {
			FreeLibrary(handle);
			return false;
		}

		if (api->name && *api->name) catalogInfo.name = api->name;
		catalogInfo.description = SafeWString(api->description);
		catalogInfo.version = SafeWString(api->version);
		catalogInfo.pkgName = SafeWString(api->package_name);
		catalogInfo.defaultSettingJson = SafeWString(api->default_setting_json);

		FreeLibrary(handle);
		return true;
	}

	bool IsPackageInCatalog(const std::wstring& pkgName) const {
		for (const auto& catalogInfo : g_pluginCatalog) {
			if (catalogInfo.pkgName == pkgName) return true;
		}
		return false;
	}

	// Catalogs one DLL (manifest first, probe otherwise) and loads it when enabled.
	// `refreshNow` runs refresh_actions right away (hot install); startup refreshes everything later.
	void AddPluginFromDisk(const std::wstring& dllPath, const nlohmann::json& userConfig, const bool refreshNow = false) {
		PluginCatalogInfo catalogInfo;
		catalogInfo.filePath = dllPath;
		catalogInfo.name = GetPluginNameFromPathW(dllPath);
		std::error_code ec;
		catalogInfo.fileTime = std::filesystem::last_write_time(dllPath, ec);
		if (!ReadPluginManifest(dllPath, catalogInfo)) {
			// 清单是必需的：没有清单时退回加载 DLL 探测元数据（没有默认设置以外的本地化信息）
			Logw(TAG, L"Plugin has no manifest (", ManifestPathFor(dllPath), L"), probing the DLL instead");
			if (!ProbePluginCatalogInfo(dllPath, catalogInfo)) {
				// 不是有效的插件（旧版 C++ 接口或 API 主版本不兼容），不加入目录
				return;
			}
		}
		if (!catalogInfo.unsupportedReason.empty()) {
			// 当前系统 / 主程序不满足清单要求：不加入目录也不加载（例如 AirPods 插件需要 Windows 10 的 WinRT 蓝牙）
			Logw(TAG, L"Plugin skipped (", catalogInfo.unsupportedReason, L"): ", dllPath);
			return;
		}
		catalogInfo.strings = PluginI18n::Load(dllPath, ManifestPathFor(dllPath));
		if (IsPackageInCatalog(catalogInfo.pkgName)) {
			Logw(TAG, L"Duplicate package_name ", catalogInfo.pkgName, L", skipped: ", dllPath);
			return;
		}
		catalogInfo.enabled = IsPluginEnabledByConfig(catalogInfo.pkgName, userConfig);
		g_pluginCatalog.push_back(catalogInfo);

		if (!catalogInfo.enabled) {
			Logi(TAG, L"Skip disabled plugin: ", catalogInfo.pkgName);
			return;
		}

		const auto pluginLoadStart = GetTickCount64();
		const bool loadResult = LoadPlugin(dllPath);
		const uint64_t pluginLoadTime = GetTickCount64() - pluginLoadStart;
		Logi(TAG, L"Load result for ", std::filesystem::path(dllPath).filename().wstring(), L": ",
			(loadResult ? L"SUCCESS" : L"FAILED"));
		if (!loadResult) return;
		const PluginInfo* loadedPlugin = FindLoadedPluginByFilePath(dllPath);
		if (!loadedPlugin) return;
		PluginInfo& info = m_plugins[loadedPlugin->pluginId];
		info.loadTimeMs = pluginLoadTime;
		// The manifest is informational; the DLL's table is authoritative once loaded.
		PluginCatalogInfo& stored = g_pluginCatalog.back();
		if (stored.fromManifest && stored.pkgName != info.pkgName) {
			Logw(TAG, L"Manifest package_name ", stored.pkgName, L" differs from DLL ", info.pkgName, L": ", dllPath);
			stored.pkgName = info.pkgName;
		}
		if (stored.defaultSettingJson.empty()) stored.defaultSettingJson = info.defaultSettingJson;
		if (stored.fromManifest && !info.version.empty() && stored.version != info.version) {
			Logw(TAG, L"Manifest version ", stored.version, L" differs from DLL ", info.version, L": ", dllPath);
		}
		if (refreshNow) RefreshPlugin(info);
	}

	// ---- shadow copies (pref_plugin_shadow_copy): the original DLL is never locked, so it can be
	// overwritten in place while the plugin runs; the watcher then reloads it. ----

	std::wstring ShadowDir() const {
		return m_pluginDir + L"\\" + PLUGIN_SHADOW_DIR_NAME;
	}

	void PrepareShadowDir() const {
		namespace fs = std::filesystem;
		std::error_code ec;
		fs::create_directories(ShadowDir(), ec);
		SetFileAttributesW(ShadowDir().c_str(), FILE_ATTRIBUTE_HIDDEN);
		// Copies left behind by a crash; the ones still locked by another instance stay.
		for (const auto& entry : fs::directory_iterator(ShadowDir(), ec)) {
			fs::remove(entry.path(), ec);
		}
		if (const auto addDllDirectory = GetAddDllDirectory()) {
			addDllDirectory(m_pluginDir.c_str());
		} else {
			// Windows 7 without KB2533623: no per-process search list, fall back to the legacy global one.
			SetDllDirectoryW(m_pluginDir.c_str());
		}
	}

	// AddDllDirectory / LOAD_LIBRARY_SEARCH_* exist on Windows 8+ (Windows 7 only with KB2533623).
	// Resolved at runtime so the exe has no static import of them; otherwise Windows 7 refuses to start it
	// with "entry point AddDllDirectory not found in kernel32.dll".
	using AddDllDirectoryFn = PVOID(WINAPI*)(PCWSTR);
	static AddDllDirectoryFn GetAddDllDirectory() {
		static const AddDllDirectoryFn fn = reinterpret_cast<AddDllDirectoryFn>(
			GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "AddDllDirectory"));
		return fn;
	}

	HMODULE LoadPluginLibrary(const std::wstring& dllPath, std::wstring& loadedPath) const {
		loadedPath = dllPath;
		if (!m_shadowCopy) return LoadLibraryW(dllPath.c_str());

		namespace fs = std::filesystem;
		std::error_code ec;
		const auto time = fs::last_write_time(dllPath, ec).time_since_epoch().count();
		wchar_t stamp[32];
		swprintf_s(stamp, L"%llx", static_cast<unsigned long long>(time));
		const std::wstring shadow = ShadowDir() + L"\\" + fs::path(dllPath).stem().wstring() + L"." + stamp + L".dll";
		if (!fs::exists(shadow, ec) && !CopyFileW(dllPath.c_str(), shadow.c_str(), FALSE)) {
			Logw(TAG, L"Shadow copy failed (", GetLastError(), L"), loading in place: ", dllPath);
			return LoadLibraryW(dllPath.c_str());
		}
		loadedPath = shadow;
		// Dependencies next to the original DLL still resolve through AddDllDirectory(m_pluginDir).
		if (GetAddDllDirectory()) {
			return LoadLibraryExW(shadow.c_str(), nullptr,
								LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_USER_DIRS);
		}
		// The LOAD_LIBRARY_SEARCH_* flags are rejected on the same systems; SetDllDirectoryW above covers m_pluginDir.
		return LoadLibraryW(shadow.c_str());
	}

	bool LoadSinglePlugin(const std::wstring& dllPath, PluginInfo& info) {
		Logi(TAG, L"LoadSinglePlugin start: ", info.name);

		info.handle = LoadPluginLibrary(dllPath, info.loadedPath);
		if (!info.handle) {
			Loge(TAG, L"LoadLibraryW failed for: ", dllPath, L", Error: ", GetLastError());
			return false;
		}

		info.api = GetValidatedPluginApi(info.handle, dllPath);
		if (!info.api) {
			FreeLibrary(info.handle);
			info.handle = nullptr;
			return false;
		}
		if (FindLoadedPluginByPackageName(SafeWString(info.api->package_name))) {
			Loge(TAG, L"Duplicate package_name already loaded, rejected: ", dllPath);
			FreeLibrary(info.handle);
			info.handle = nullptr;
			info.api = nullptr;
			return false;
		}

		static std::atomic<uint64_t> nextSerial{1};
		info.apiMinor = info.api->api_minor;
		info.apiFlags = CL_HAS_FIELD(info.api, CL_PluginApi, flags) ? info.api->flags : 0u;
		if (info.api->name && *info.api->name) info.name = info.api->name;
		info.description = SafeWString(info.api->description);
		info.version = SafeWString(info.api->version);
		info.pkgName = SafeWString(info.api->package_name);
		info.defaultSettingJson = SafeWString(info.api->default_setting_json);
		info.context = std::make_unique<CL_PluginContext>();
		info.context->serial = nextSerial++;
		info.context->pluginId = info.pluginId;
		info.context->pkgName = info.pkgName;
		info.context->logTag = info.name;
		ResolvePluginStrings(info);

		info.self = info.api->create(info.context.get(), GetHostApi());
		if (!info.self) {
			Loge(TAG, L"Plugin create failed: ", info.pkgName);
			ReleasePluginContext(info);
			FreeLibrary(info.handle);
			info.handle = nullptr;
			info.api = nullptr;
			return false;
		}
		Logi(TAG, L"Plugin create success: ", info.pkgName, L", api ", info.api->api_major, L".", info.apiMinor,
			(info.apiFlags & CL_PLUGIN_REFRESH_ANY_THREAD) ? L", refresh on worker" : L"");

		info.loaded = true;
		return true;
	}

	// Drops everything the host keeps on behalf of the plugin, especially callbacks into its code.
	static void ReleasePluginContext(PluginInfo& info) {
		if (!info.context) return;
		for (const auto& key : info.context->appCommands) {
			appLaunchActionCallBacks.erase(key);
		}
		info.context.reset();
	}

	void UnloadSinglePlugin(PluginInfo& info) {
		Logi(TAG, L"Unloading plugin: ", info.name);
		// A refresh still running on the worker must finish before the instance goes away.
		JoinRefreshWorker(info);
		m_queryRecipients.erase(std::remove(m_queryRecipients.begin(), m_queryRecipients.end(), info.pluginId),
								m_queryRecipients.end());
		if (info.self && info.api) {
			Logi(TAG, L"Destroying plugin: ", info.name);
			info.api->destroy(info.self);
			info.self = nullptr;
		}
		ReleasePluginContext(info);

		if (info.handle) {
			Logi(TAG, L"Freeing plugin library: ", info.name);
			FreeLibrary(info.handle);
			info.handle = nullptr;
		}
		if (!info.loadedPath.empty() && info.loadedPath != info.filePath) {
			DeleteFileW(info.loadedPath.c_str());
		}

		info.api = nullptr;
		info.loaded = false;
	}

	static std::wstring GetPluginNameFromPathW(const std::wstring& dllPath) {
		std::filesystem::path path(dllPath);
		return path.stem().wstring();
	}

	void NotifyActionsChanged() {
		if (m_suppressNotifications) {
			return; // 如果通知被抑制，直接返回
		}
		if (m_onActionsChanged) {
			m_onActionsChanged();
		}
	}

	void NotifyBeforePluginUnload() {
		if (m_beforePluginUnload) {
			m_beforePluginUnload();
		}
	}

	// ---- plugin folder watcher ----

	void StartPluginDirWatcher() {
		if (m_watchThread.joinable() || m_pluginDir.empty()) return;
		m_watchStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		m_watchThread = std::thread([this] { WatchPluginDir(); });
	}

	void StopPluginDirWatcher() {
		if (m_watchStopEvent) SetEvent(m_watchStopEvent);
		if (m_watchThread.joinable()) m_watchThread.join();
		if (m_watchStopEvent) {
			CloseHandle(m_watchStopEvent);
			m_watchStopEvent = nullptr;
		}
	}

	void WatchPluginDir() {
		const HANDLE dir = CreateFileW(m_pluginDir.c_str(), FILE_LIST_DIRECTORY,
										FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
										FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
		if (dir == INVALID_HANDLE_VALUE) {
			Logw(TAG, L"Plugin dir watcher not started: ", GetLastError());
			return;
		}
		OVERLAPPED overlapped{};
		overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		std::vector<BYTE> buffer(64 * 1024);
		const HANDLE waits[2] = {m_watchStopEvent, overlapped.hEvent};
		for (;;) {
			ResetEvent(overlapped.hEvent);
			if (!ReadDirectoryChangesW(dir, buffer.data(), static_cast<DWORD>(buffer.size()), FALSE,
										FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
										nullptr, &overlapped, nullptr)) {
				break;
			}
			if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) != WAIT_OBJECT_0 + 1) {
				CancelIo(dir);
				break;
			}
			DWORD bytes = 0;
			if (!GetOverlappedResult(dir, &overlapped, &bytes, TRUE)) continue;
			bool relevant = bytes == 0; // 缓冲区溢出时 bytes 为 0，保守地重新扫描
			for (DWORD offset = 0; !relevant && offset < bytes;) {
				const auto* notify = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer.data() + offset);
				const std::wstring name(notify->FileName, notify->FileNameLength / sizeof(wchar_t));
				const std::wstring lower = MyToLower(name);
				relevant = EndsWith(lower, L".dll") || EndsWith(lower, MyToLower(PLUGIN_MANIFEST_SUFFIX));
				if (notify->NextEntryOffset == 0) break;
				offset += notify->NextEntryOffset;
			}
			if (relevant && g_mainHwnd && !m_dirChangedPosted.exchange(true)) {
				PostMessageW(g_mainHwnd, WM_PLUGIN_DIR_CHANGED, 0, 0);
			}
		}
		CloseHandle(overlapped.hEvent);
		CloseHandle(dir);
	}

	static const CL_HostApi* GetHostApi();
};

inline std::unique_ptr<PluginManager> g_pluginManager;

#include "PluginHostApi.hpp"
