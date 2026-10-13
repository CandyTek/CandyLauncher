#include <windows.h>
#include <memory>

#include "FolderPluginData.hpp"
#include "../../util/MainTools.hpp"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <algorithm>
#include <cwctype>
#include <future>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>

#include "FileAction.hpp"
#include "IndexManagerWindow.hpp"
#include "AutomationActionWindow.hpp"
#include "AutomationActionModel.hpp"
#include "AutomationEditorAction.hpp"
#include "ContextMenuHelper.hpp"
#include "FileHelper.hpp"
#include "FolderPluginConfigUtils.hpp"
#include "SystemSettingsIndex.hpp"
#include "../../util/BitmapUtil.hpp"
#include "../../util/FileSystemTraverser.hpp"
#include "../../util/FileUtil.hpp"
#include <mutex>
#include <atomic>

#include "util/HotkeyUtils.hpp"

inline bool IS_SHOW_INDEX_MANAGER_WINDOW = false;

// 注册给主程序的应用命令，常用跳转插件通过 InvokeAppCommand 打开索引管理器
constexpr const wchar_t* OPEN_FOLDER_INDEXED_MANAGER_CALLBACK_KEY = L"openFolderIndexedManager";
// 任务队列系统
inline std::queue<std::function<void()>> g_taskQueue;
inline std::mutex g_taskQueueMutex;
inline std::condition_variable g_taskQueueCV;
// 索引工作线程。进程经 ExitProcess 退出（如托盘退出、崩溃）时不会调用 Shutdown，
// DLL 卸载时全局 std::thread 若仍 joinable 会触发 std::terminate → abort；此时其他线程已被系统结束，只能 detach。
struct DetachOnExitThread {
	std::thread thread;

	~DetachOnExitThread() {
		if (thread.joinable()) thread.detach();
	}
};

inline DetachOnExitThread g_pluginIndexedRunningAppsThread;
inline std::atomic<bool> g_workerShouldStop{false};


// 工作线程函数
inline void WorkerThreadFunction() {
	while (!g_workerShouldStop) {
		std::unique_lock<std::mutex> lock(g_taskQueueMutex);
		g_taskQueueCV.wait(lock, []() {
			return !g_taskQueue.empty() || g_workerShouldStop;
		});

		if (g_workerShouldStop) break;

		if (!g_taskQueue.empty()) {
			auto task = g_taskQueue.front();
			g_taskQueue.pop();
			lock.unlock();

			try {
				task();
			} catch (...) {
				// 捕获任务执行中的异常，防止线程崩溃
			}
		}
	}
}

// 向工作线程提交任务
inline void SubmitTask(std::function<void()> task) {
	{
		std::lock_guard<std::mutex> lock(g_taskQueueMutex);
		g_taskQueue.push(std::move(task));
	}
	g_taskQueueCV.notify_one();
}


inline void stopThreadPluginRunningApps() {
	if (g_pluginIndexedRunningAppsThread.thread.joinable()) {
		g_workerShouldStop = true;
		g_taskQueueCV.notify_all();
		g_pluginIndexedRunningAppsThread.thread.join();
	}
}

inline const wchar_t* kLinkName = L"CandyLauncher 索引文件夹.lnk";
inline const wchar_t* kLinkDesc = L"CandyLauncher 索引文件夹";

inline bool InstallSendToEntry(const std::wstring& exePath) {
	HRESULT hr = CoInitialize(nullptr);
	bool needUninit = SUCCEEDED(hr);

	PWSTR sendto = nullptr;
	std::wstring linkPath;
	bool ok = false;

	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SendTo, 0, nullptr, &sendto))) {
		linkPath.assign(sendto);
		if (!linkPath.empty() && linkPath.back() != L'\\') linkPath += L'\\';
		linkPath += kLinkName;

		IShellLinkW* psl = nullptr;
		hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
							IID_PPV_ARGS(&psl));
		if (SUCCEEDED(hr)) {
			psl->SetPath(exePath.c_str()); // 目标：CreateShortcut.exe
			psl->SetDescription(kLinkDesc);
			// 这里不需要设置 "%1"；SendTo 会自动把所选文件路径附加为参数
			// 如需自定义前缀参数，可： psl->SetArguments(L"--mode index");

			IPersistFile* ppf = nullptr;
			hr = psl->QueryInterface(IID_PPV_ARGS(&ppf));
			if (SUCCEEDED(hr)) {
				hr = ppf->Save(linkPath.c_str(), TRUE);
				ppf->Release();
				ok = SUCCEEDED(hr);
			}
			psl->Release();
		}
		CoTaskMemFree(sendto);
	}

	if (needUninit) CoUninitialize();
	return ok;
}

inline bool DeleteSendToEntry(const std::wstring& shortcutName) {
	HRESULT hr = CoInitialize(nullptr);
	bool needUninit = SUCCEEDED(hr);

	bool ok = false;
	PWSTR sendto = nullptr;

	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SendTo, 0, nullptr, &sendto))) {
		std::wstring linkPath(sendto);
		if (!linkPath.empty() && linkPath.back() != L'\\') linkPath += L'\\';
		linkPath += shortcutName; // 例如 L"CandyLauncher 索引文件夹.lnk"

		// 尝试删除文件
		if (DeleteFileW(linkPath.c_str())) {
			ok = true;
		} else {
			// 如果不存在也视为成功
			ok = (GetLastError() == ERROR_FILE_NOT_FOUND);
		}

		CoTaskMemFree(sendto);
	}

	if (needUninit) CoUninitialize();

	return ok;
}

static std::wstring NormalizeActionTitleForDedup(const std::wstring& title) {
	std::wstring normalized = title;
	std::transform(normalized.begin(), normalized.end(), normalized.begin(), towlower);
	return normalized;
}

class FolderPlugin : public candy::Plugin {
private:
	// 0 号集合：全部索引项（文件、UWP、%PATH%、系统设置）；1 号集合：自动化动作组编辑器入口。都参与全局搜索
	candy::ActionSet<FileAction> fileActions{0};
	candy::ActionSet<AutomationEditorAction> automationEditor{1};
	struct DirectorySnapshot {
		std::filesystem::file_time_type modified;
		std::vector<std::wstring> children;
		std::vector<std::shared_ptr<FileAction>> actions;
	};
	struct CachedSource {
		TraverseOptions options;
		std::vector<std::wstring> roots;
		std::unordered_map<std::wstring, DirectorySnapshot> directories;
		std::vector<std::shared_ptr<FileAction>> actions;
		bool isUwp = false;
		bool isSystemSettings = false;
		bool explicitUwp = false;
		bool indexRoot = false;
		std::shared_ptr<FileAction> rootAction;
		// 在 config_folder_plugin.json 数组中的下标；默认补充的 UWP/PATH 源为 -1
		int configIndex = -1;
	};
	// 右键排除/重命名时，用于把索引项映射回配置中的排除名称或重命名源
	struct IndexEditTarget {
		size_t sourceIndex = 0;
		std::wstring excludeKey;
		std::wstring renameKey;
	};
	enum class IndexEditKind { Exclude, Rename };
	std::mutex cacheMutex;
	std::vector<CachedSource> cachedSources;
	std::string cachedConfig;
	std::wstring cachedPath;
	bool cacheReady = false;
	bool cachedAllowDuplicates = false;
	bool cachedIndexRoot = false;
	bool cachedUwpEnabled = false;
	bool cachedPathEnabled = false;
	bool cachedEverythingEnabled = false;
	ParsedHotkey hkOpenFileLocation;
	ParsedHotkey hkOpenTargetLocation;
	ParsedHotkey hkCopyFilePath;
	ParsedHotkey hkCopyTargetPath;
	ParsedHotkey hkOpenWithClipboard;
	ParsedHotkey hkRunAsAdmin;

	// 应用命令回调：C 函数指针 + ctx，由主程序在插件卸载时自动移除
	static void CL_CALL OpenIndexedManagerCommand(void*) {
		ShowIndexedManagerWindow(nullptr);
	}

public:
	FolderPlugin() = default;
	~FolderPlugin() override = default;

	bool Initialize() override {
		m_host = &GetHost();
		g_folderWork.Reset();
		EnsureAutomationActionIndex();
		GetHost().RegisterAppCommand(OPEN_FOLDER_INDEXED_MANAGER_CALLBACK_KEY, OpenIndexedManagerCommand, nullptr);
		// 索引管理器 / 自动化编辑器保存后在它们自己的线程上调用：发布集合后通知主程序重建可见列表
		g_refreshFolderPlugin = [this]() {
			RefreshActions();
			GetHost().NotifyActionsChanged();
		};
		// 给已 joinable 的 std::thread 赋值会 std::terminate，先停掉上一次的线程
		stopThreadPluginRunningApps();
		g_workerShouldStop = false;
		g_pluginIndexedRunningAppsThread.thread = std::thread(WorkerThreadFunction);
		return true;
	}


	// 主程序在 UI 线程上调用，之后 delete 本实例并 FreeLibrary：
	// 这里必须让所有引用本 DLL 代码的东西（窗口过程、后台线程、持有 this 的回调）都先结束
	void Shutdown() override {
		if (m_host) {
			GetHost().UnregisterAppCommand(OPEN_FOLDER_INDEXED_MANAGER_CALLBACK_KEY);
			stopThreadPluginRunningApps();
		}
		// 先关窗口：编辑器保存会再投递刷新线程，关掉后不会再有新任务
		CloseAutomationActionWindow();
		CloseIndexManagerWindows();
		// 取消自动化 delay 等待，等启动 / 自动化 / 保存后刷新线程全部结束（刷新回调持有 this）
		g_folderWork.Shutdown();
		g_refreshFolderPlugin = nullptr;
		fileActions.ClearLocal();
		automationEditor.ClearLocal();
		m_host = nullptr;
	}


	// 系统图标由主程序按 iconPath 延迟解析，这里不再预先查询 Shell
	void RefreshAllActionsBackup() {
		const candy::Host& host = GetHost();
		std::vector<std::shared_ptr<FileAction>> allPluginActions;
		const bool allowDuplicateItems = host.GetBool(L"com.candytek.folderplugin.allow_duplicate_items", true);
		const bool everythingEnabled = host.GetBool(L"pref_use_everything_sdk_index", false);
		const bool indexRoot = host.GetBool(L"com.candytek.folderplugin.index_folderpath_itself", true);
		const bool uwpEnabled = host.GetBool(L"com.candytek.folderplugin.uwp_apps", true);
		const bool pathEnabled = host.GetBool(L"com.candytek.folderplugin.envpath_apps", false);
		std::unordered_set<std::wstring> indexedNames;
		auto pushAction = [&](std::vector<std::shared_ptr<FileAction>>& actions, const std::shared_ptr<FileAction>& action) {
			if (!action) return;
			if (!allowDuplicateItems) {
				const std::wstring normalizedTitle = NormalizeActionTitleForDedup(action->title);
				if (indexedNames.find(normalizedTitle) != indexedNames.end()) return;
				indexedNames.insert(normalizedTitle);
			}
			actions.push_back(action);
		};
		// 遍历运行中的窗口并添加到列表
		bool isPathAdded = false;
		bool isUwpAdded = false;


		std::vector<std::shared_ptr<FileAction>> tempActions;

		std::vector<TraverseOptions> runnerConfigs = ParseRunnerConfig();
		for (TraverseOptions traverseOptions1 : runnerConfigs) {
			if (traverseOptions1.type == L"folder" || traverseOptions1.type.empty()) {
				traverseOptions1.folder = GetCurrentFolderPath(traverseOptions1);
				if (everythingEnabled) {
					host.TraverseFiles(MakeHostTraverseOptions(traverseOptions1, traverseOptions1.folder),
						[&](const std::wstring& name,
							const std::wstring& fullPath,
							const std::wstring& parent,
							const std::wstring& ext) {
							const auto action = std::make_shared<FileAction>(name, fullPath, false, parent);
							pushAction(tempActions, action);
						});
				} else {
					TraverseFiles(traverseOptions1.folder, traverseOptions1, EXE_FOLDER_PATH2, [&](const std::wstring& name,
								const std::wstring& fullPath,
								const std::wstring& parent,
								const std::wstring& ext) {
									const auto action = std::make_shared<FileAction>(name, fullPath, false, parent);
									pushAction(tempActions, action);
								});
				}
				if (indexRoot) {
					std::wstring folderPath = ShortToLongPathWithEnvironment(
						ExpandEnvironmentVariables(traverseOptions1.folder, EXE_FOLDER_PATH2));
					const auto action = std::make_shared<FileAction>(traverseOptions1.name, folderPath, false, folderPath);
					pushAction(tempActions, action);
				}
			} else if (traverseOptions1.type == L"path" && !isPathAdded && pathEnabled) {
				TraversePATHExecutables2([&](const std::wstring& name,
											const std::wstring& fullPath,
											const std::wstring& parent,
											const std::wstring& ext) {
					const auto action = std::make_shared<FileAction>(name, fullPath, false, parent);
					pushAction(tempActions, action);
				}, traverseOptions1, EXE_FOLDER_PATH2);

				isPathAdded = true;
			} else if (traverseOptions1.type == L"uwp" && !isUwpAdded && uwpEnabled) {
				std::vector<std::shared_ptr<FileAction>> uwpActions;
				traverseUwpApps(uwpActions, traverseOptions1);
				for (const auto& fileAction : uwpActions) pushAction(allPluginActions, fileAction);
				isUwpAdded = true;
			} else if (traverseOptions1.type == L"control_panel" || traverseOptions1.type == L"windows_settings") {
				for (const auto& item : GetSystemSettingsItems(traverseOptions1)) {
					auto action = std::make_shared<FileAction>(item.name, item.target);
					action->iconIndex = item.iconIndex;
					pushAction(tempActions, action);
				}
			}
		}

		allPluginActions.insert(allPluginActions.end(),
								std::make_move_iterator(tempActions.begin()),
								std::make_move_iterator(tempActions.end()));

		TraverseOptions emptyTraverseOptions;
		TraverseOptions defaultOptions = CreateDefaultPathTraverseOptions();
		if (!isUwpAdded && uwpEnabled) {
			std::vector<std::shared_ptr<FileAction>> uwpActions;
			traverseUwpApps(uwpActions, emptyTraverseOptions);
			for (const auto& fileAction : uwpActions) pushAction(allPluginActions, fileAction);
		} else if (!isPathAdded && pathEnabled) {
			// TODO: 解决这个崩溃问题
			TraversePATHExecutables2([&](const std::wstring& name,
										const std::wstring& fullPath,
										const std::wstring& parent,
										const std::wstring& ext) {
				const auto action = std::make_shared<FileAction>(
					name, fullPath, false, parent
				);
				pushAction(allPluginActions, action);
			}, defaultOptions, EXE_FOLDER_PATH2);
		}
		fileActions.Publish(host, std::move(allPluginActions));
	}

	// 目录修改时间只反映直接子项的增删；递归配置需要逐个检查已知子目录。
	static std::wstring CachePathKey(const std::wstring& path) {
		std::wstring key = std::filesystem::path(path).lexically_normal().wstring();
		std::transform(key.begin(), key.end(), key.begin(), towlower);
		return key;
	}

	static void RefreshDirectory(CachedSource& source, const std::wstring& path,
		std::unordered_map<std::wstring, DirectorySnapshot>& next,
		std::vector<std::shared_ptr<FileAction>>& newActions) {
		namespace fs = std::filesystem;
		std::error_code error;
		const fs::file_status status = fs::status(path, error);
		// 配置的目录不存在时视为空目录，与 Everything 索引路径的行为一致。
		if (status.type() == fs::file_type::not_found) return;
		if (error) throw fs::filesystem_error("Cannot inspect indexed directory", fs::path(path), error);
		if (!fs::is_directory(status)) return;
		const auto modified = fs::last_write_time(path, error);
		if (error) throw fs::filesystem_error("Cannot read directory date", fs::path(path), error);
		const std::wstring key = CachePathKey(path);
		const auto old = source.directories.find(key);
		DirectorySnapshot snapshot;
		if (old != source.directories.end() && old->second.modified == modified) {
			snapshot = old->second;
		} else {
			snapshot.modified = modified;
			std::unordered_map<std::wstring, std::shared_ptr<FileAction>> previous;
			if (old != source.directories.end()) {
				for (const auto& action : old->second.actions)
					previous.emplace(CachePathKey(action->GetTargetPath()), action);
			}
			for (fs::directory_iterator it(path, error), end; it != end && !error; it.increment(error)) {
				const auto& entry = *it;
				const fs::path entryPath = entry.path();
				const bool isDirectory = entry.is_directory(error);
				if (error) break;
				const bool isFile = entry.is_regular_file(error);
				if (error) break;
				if (isDirectory && source.options.recursive && !entry.is_symlink(error))
					snapshot.children.push_back(entryPath.wstring());
				if (error) break;
				if (!isFile && (!isDirectory || source.options.indexFilesOnly)) continue;
				if (isFile && !source.options.extensions.empty() &&
					std::none_of(source.options.extensions.begin(), source.options.extensions.end(),
						[&](const std::wstring& ext) { return _wcsicmp(ext.c_str(), entryPath.extension().c_str()) == 0; })) continue;
				if (shouldExclude(source.options, entryPath.filename().wstring())) continue;
				std::wstring title = entryPath.stem().wstring();
				if (const auto renamed = source.options.renameMap.find(title); renamed != source.options.renameMap.end())
					title = renamed->second;
				const auto previousAction = previous.find(CachePathKey(entryPath.wstring()));
				if (previousAction != previous.end() && previousAction->second->title == title) {
					snapshot.actions.push_back(previousAction->second);
				} else {
					auto action = std::make_shared<FileAction>(title, entryPath.wstring(), false, entryPath.parent_path().wstring());
					newActions.push_back(action);
					snapshot.actions.push_back(std::move(action));
				}
			}
			if (error) throw fs::filesystem_error("Cannot enumerate indexed directory", fs::path(path), error);
		}
		const auto children = snapshot.children;
		next.emplace(key, std::move(snapshot));
		for (const auto& child : children) RefreshDirectory(source, child, next, newActions);
	}

	static std::vector<std::shared_ptr<FileAction>> MergeVisibleActions(const std::vector<CachedSource>& sources,
		bool allowDuplicates) {
		std::vector<std::shared_ptr<FileAction>> result;
		std::unordered_set<std::wstring> titles;
		auto addSource = [&](const CachedSource& source) {
			for (const auto& action : source.actions) {
				if (allowDuplicates || titles.insert(NormalizeActionTitleForDedup(action->title)).second)
					result.push_back(action);
			}
		};
		// 原实现先插入显式 UWP，再插入文件，最后插入默认 UWP/PATH。
		for (const auto& source : sources)
			if (source.explicitUwp) addSource(source);
		for (const auto& source : sources)
			if (!source.isUwp) addSource(source);
		for (const auto& source : sources)
			if (source.isUwp && !source.explicitUwp) addSource(source);
		return result;
	}

	// UWP/系统设置按显示名排除和重命名，标题可能已被重命名，需反查原名
	static std::wstring OriginalDisplayName(const TraverseOptions& options, const std::wstring& title) {
		for (const auto& [source, target] : options.renameMap)
			if (target == title && source != title) return source;
		return title;
	}

	// 与各遍历器保持一致：文件按完整文件名排除，按不含扩展名的文件名重命名
	static std::wstring IndexKeyOf(const CachedSource& source, FileAction& action, IndexEditKind kind) {
		if (source.isUwp || source.isSystemSettings) return OriginalDisplayName(source.options, action.title);
		const std::filesystem::path path(action.GetTargetPath());
		return kind == IndexEditKind::Exclude ? path.filename().wstring() : path.stem().wstring();
	}

	// 调用方需持有 cacheMutex
	bool FindIndexEditTarget(const std::shared_ptr<FileAction>& action, IndexEditTarget& target) {
		if (!cacheReady || !action) return false;
		for (size_t i = 0; i < cachedSources.size(); ++i) {
			const auto& source = cachedSources[i];
			// "索引路径本身" 生成的根目录项不受排除/重命名规则影响
			if (source.rootAction == action) return false;
			if (std::find(source.actions.begin(), source.actions.end(), action) == source.actions.end()) continue;
			target.sourceIndex = i;
			target.excludeKey = IndexKeyOf(source, *action, IndexEditKind::Exclude);
			target.renameKey = IndexKeyOf(source, *action, IndexEditKind::Rename);
			return !target.excludeKey.empty() && !target.renameKey.empty();
		}
		return false;
	}

	bool CanEditIndex(const std::shared_ptr<FileAction>& action) {
		std::lock_guard<std::mutex> lock(cacheMutex);
		IndexEditTarget target;
		return FindIndexEditTarget(action, target);
	}

	std::wstring GetIndexRenameKey(const std::shared_ptr<FileAction>& action) {
		std::lock_guard<std::mutex> lock(cacheMutex);
		IndexEditTarget target;
		return FindIndexEditTarget(action, target) ? target.renameKey : L"";
	}

	// 默认补充的 UWP/PATH 源在配置中没有对应项，首次编辑时追加一项使其成为显式配置
	static nlohmann::json CreateConfigItemForDefaultSource(const CachedSource& source) {
		return {
			{"name", source.isUwp ? "UWP 应用" : "%PATH% 环境变量"},
			{"type", source.isUwp ? "uwp" : "path"},
			{"folder", ""},
			{"is_contain_subfolder", false},
			{"index_files_only", true},
			{"exclude_words", WideVectorToUtf8Vector(source.options.excludeWords)},
			{"excludes", WideVectorToUtf8Vector(source.options.excludeNames)},
			{"rename_sources", WideVectorToUtf8Vector(source.options.renameSources)},
			{"rename_targets", WideVectorToUtf8Vector(source.options.renameTargets)},
			{"exts", WideVectorToUtf8Vector(source.options.extensions)}
		};
	}

	static nlohmann::json& EnsureJsonArray(nlohmann::json& item, const char* key) {
		if (!item.contains(key) || !item[key].is_array()) item[key] = nlohmann::json::array();
		return item[key];
	}

	// 只有 rename_sources 与 rename_targets 成对的部分生效；新名称与原名相同时删除该映射
	static void SetRenameMapping(nlohmann::json& item, const std::string& source, const std::string& target) {
		auto& sources = EnsureJsonArray(item, "rename_sources");
		auto& targets = EnsureJsonArray(item, "rename_targets");
		const size_t count = std::min(sources.size(), targets.size());
		bool found = false;
		for (size_t i = count; i-- > 0;) {
			if (!sources[i].is_string() || sources[i].get<std::string>() != source) continue;
			found = true;
			if (source == target) {
				sources.erase(sources.begin() + static_cast<std::ptrdiff_t>(i));
				targets.erase(targets.begin() + static_cast<std::ptrdiff_t>(i));
			} else {
				targets[i] = target;
			}
		}
		if (found || source == target) return;
		sources.insert(sources.begin() + static_cast<std::ptrdiff_t>(count), source);
		targets.insert(targets.begin() + static_cast<std::ptrdiff_t>(count), target);
	}

	// 写入配置并就地更新缓存，避免整表重建索引；下次刷新时缓存与配置一致，仍走增量路径
	bool ApplyIndexEdit(const std::shared_ptr<FileAction>& action, IndexEditKind kind, const std::wstring& newTitle,
		std::wstring& error) {
		std::lock_guard<std::mutex> lock(cacheMutex);
		IndexEditTarget target;
		if (!FindIndexEditTarget(action, target)) {
			error = L"该项不属于任何索引配置";
			return false;
		}
		const std::string current = ReadUtf8File(RUNNER_CONFIG_PATH2);
		if (current != cachedConfig) {
			error = L"配置文件已在外部被修改，请等待索引刷新后重试";
			return false;
		}
		CachedSource& source = cachedSources[target.sourceIndex];
		nlohmann::json config;
		try {
			config = current.empty() ? nlohmann::json::array() : nlohmann::json::parse(current);
		} catch (const std::exception& e) {
			error = L"配置文件解析失败: " + utf8_to_wide(e.what());
			return false;
		}
		if (!config.is_array()) {
			error = L"配置文件格式错误";
			return false;
		}
		int configIndex = source.configIndex;
		if (configIndex < 0) {
			config.push_back(CreateConfigItemForDefaultSource(source));
			configIndex = static_cast<int>(config.size()) - 1;
		}
		if (configIndex >= static_cast<int>(config.size()) || !config[configIndex].is_object()) {
			error = L"找不到对应的索引配置";
			return false;
		}
		auto& item = config[configIndex];
		const std::wstring& key = kind == IndexEditKind::Exclude ? target.excludeKey : target.renameKey;
		if (kind == IndexEditKind::Exclude) {
			auto& excludes = EnsureJsonArray(item, "excludes");
			const std::string utf8Key = wide_to_utf8(key);
			if (std::find(excludes.begin(), excludes.end(), utf8Key) == excludes.end()) excludes.push_back(utf8Key);
		} else {
			SetRenameMapping(item, wide_to_utf8(key), wide_to_utf8(newTitle));
		}
		// 规则更新前先按旧规则找出受影响的项（同名文件、同名 UWP 等会一起变化）
		std::unordered_set<FileAction*> affected;
		for (const auto& candidate : source.actions)
			if (candidate != source.rootAction && IndexKeyOf(source, *candidate, kind) == key) affected.insert(candidate.get());
		if (!WriteRunnerConfigJson(config)) {
			error = L"写入配置文件失败";
			return false;
		}
		const TraverseOptions updated = getTraverseOptions(item);
		source.options.excludeNames = updated.excludeNames;
		source.options.renameSources = updated.renameSources;
		source.options.renameTargets = updated.renameTargets;
		source.options.renameMap = updated.renameMap;
		source.configIndex = configIndex;
		if (source.isUwp) source.explicitUwp = true;
		cachedConfig = ReadUtf8File(RUNNER_CONFIG_PATH2);
		if (kind == IndexEditKind::Exclude) {
			auto removeAffected = [&](std::vector<std::shared_ptr<FileAction>>& actions) {
				actions.erase(std::remove_if(actions.begin(), actions.end(),
					[&](const std::shared_ptr<FileAction>& candidate) { return affected.count(candidate.get()) != 0; }),
					actions.end());
			};
			removeAffected(source.actions);
			for (auto& [path, snapshot] : source.directories) removeAffected(snapshot.actions);
		} else {
			for (auto* candidate : affected) candidate->SetTitle(newTitle);
		}
		// 主程序持有的是副本，排除/重命名后必须重新发布
		fileActions.Publish(GetHost(), MergeVisibleActions(cachedSources, cachedAllowDuplicates));
		Logi(L"FolderPlugin", kind == IndexEditKind::Exclude ? L"index excluded key=" : L"index renamed key=", key,
			L" title=", newTitle, L" config=", configIndex, L" affected=", affected.size());
		return true;
	}

	// 带 CL_PLUGIN_REFRESH_ANY_THREAD，由主程序在工作线程上调用；这里只用 "Any thread" 的主程序服务
	void RefreshActions() override {
		if (!m_host) return;
		std::lock_guard<std::mutex> lock(cacheMutex);
		const candy::Host& host = GetHost();
		automationEditor.Publish(host, {std::make_shared<AutomationEditorAction>()});
		const ULONGLONG refreshStart = GetTickCount64();
		const bool allowDuplicates = host.GetBool(L"com.candytek.folderplugin.allow_duplicate_items", true);
		const bool indexRoot = host.GetBool(L"com.candytek.folderplugin.index_folderpath_itself", true);
		const bool uwpEnabled = host.GetBool(L"com.candytek.folderplugin.uwp_apps", true);
		const bool pathEnabled = host.GetBool(L"com.candytek.folderplugin.envpath_apps", false);
		const bool everythingEnabled = host.GetBool(L"pref_use_everything_sdk_index", false);
		const std::string config = ReadUtf8File(RUNNER_CONFIG_PATH2);
		DWORD pathLength = GetEnvironmentVariableW(L"PATH", nullptr, 0);
		std::wstring environmentPath(pathLength ? pathLength : 1, L'\0');
		if (pathLength) {
			GetEnvironmentVariableW(L"PATH", environmentPath.data(), pathLength);
			environmentPath.resize(wcslen(environmentPath.c_str()));
		} else environmentPath.clear();

		try {
			std::vector<TraverseOptions> configs = ParseRunnerConfig();
			const ULONGLONG configDone = GetTickCount64();
			std::vector<CachedSource> sources;
			bool explicitUwp = false, explicitPath = false;
			for (size_t configIndex = 0; configIndex < configs.size(); ++configIndex) {
				auto options = configs[configIndex];
				CachedSource source;
				source.configIndex = static_cast<int>(configIndex);
				if (options.type == L"folder" || options.type.empty()) {
					options.folder = GetCurrentFolderPath(options);
					source.options = options;
					source.roots.push_back(ExpandEnvironmentVariables(options.folder, EXE_FOLDER_PATH2));
					source.indexRoot = indexRoot;
					sources.push_back(std::move(source));
				} else if (options.type == L"path" && pathEnabled && !explicitPath) {
					options.recursive = false;
					source.options = options;
					source.roots = GetPATHDirectories();
					sources.push_back(std::move(source));
					explicitPath = true;
				} else if (options.type == L"uwp" && uwpEnabled && !explicitUwp) {
					source.options = options;
					source.isUwp = true;
					source.explicitUwp = true;
					sources.push_back(std::move(source));
					explicitUwp = true;
				} else if (options.type == L"control_panel" || options.type == L"windows_settings") {
					source.options = options;
					source.isSystemSettings = true;
					sources.push_back(std::move(source));
				}
			}
			if (!explicitUwp && uwpEnabled) {
				CachedSource source;
				source.isUwp = true;
				sources.push_back(std::move(source));
			} else if (!explicitPath && pathEnabled) {
				CachedSource source;
				source.options = CreateDefaultPathTraverseOptions();
				source.roots = GetPATHDirectories();
				sources.push_back(std::move(source));
			}
			bool rebuild = !cacheReady || config != cachedConfig || environmentPath != cachedPath ||
				allowDuplicates != cachedAllowDuplicates || indexRoot != cachedIndexRoot ||
				uwpEnabled != cachedUwpEnabled || pathEnabled != cachedPathEnabled ||
				everythingEnabled != cachedEverythingEnabled || sources.size() != cachedSources.size();
			if (!rebuild) {
				for (size_t i = 0; i < sources.size(); ++i) {
					if (sources[i].roots != cachedSources[i].roots || sources[i].isUwp != cachedSources[i].isUwp ||
						sources[i].isSystemSettings != cachedSources[i].isSystemSettings) {
						rebuild = true;
						break;
					}
				}
			}
			if (!rebuild) sources = std::move(cachedSources);
			std::future<std::vector<std::shared_ptr<FileAction>>> uwpFuture;
			if (rebuild) {
				for (const auto& source : sources) {
					if (!source.isUwp) continue;
					const TraverseOptions options = source.options;
					// UWP 条目（含图标位图）在工作线程构造，与文件索引并发执行。
					uwpFuture = std::async(std::launch::async, [options]() {
						std::vector<std::shared_ptr<FileAction>> actions;
						traverseUwpApps(actions, options);
						return actions;
					});
					break;
				}
			}
			// 控制面板/系统设置通过 Shell 枚举，耗时较长，同样与文件索引并发执行。
			std::vector<std::pair<size_t, std::future<std::vector<std::shared_ptr<FileAction>>>>> settingsFutures;
			if (rebuild) {
				for (size_t i = 0; i < sources.size(); ++i) {
					if (!sources[i].isSystemSettings) continue;
					const TraverseOptions options = sources[i].options;
					settingsFutures.emplace_back(i, std::async(std::launch::async, [options]() {
						const ULONGLONG start = GetTickCount64();
						std::vector<std::shared_ptr<FileAction>> actions;
						for (const auto& item : GetSystemSettingsItems(options)) {
							auto action = std::make_shared<FileAction>(item.name, item.target);
							action->iconIndex = item.iconIndex;
							actions.push_back(std::move(action));
						}
						Logi(L"FolderPlugin", L"index source=", options.type, L" ms=", GetTickCount64() - start,
							L" count=", actions.size());
						return actions;
					}));
				}
			}
			std::vector<std::shared_ptr<FileAction>> newActions;
			const ULONGLONG sourceStart = GetTickCount64();
			for (auto& source : sources) {
				const ULONGLONG oneSourceStart = GetTickCount64();
				if (source.isUwp) {
					continue;
				}
				if (source.isSystemSettings) {
					continue;
				}
				if (everythingEnabled && !source.roots.empty() && source.options.type != L"path") {
					std::unordered_map<std::wstring, std::shared_ptr<FileAction>> previous;
					previous.reserve(source.actions.size());
					for (const auto& action : source.actions)
						previous.emplace(CachePathKey(action->GetTargetPath()), action);
					std::vector<std::shared_ptr<FileAction>> actions;
					for (const auto& root : source.roots) {
						host.TraverseFiles(MakeHostTraverseOptions(source.options, root),
							[&](const std::wstring& title, const std::wstring& fullPath,
								const std::wstring&, const std::wstring&) {
								const auto old = previous.find(CachePathKey(fullPath));
								if (old != previous.end() && old->second->title == title) {
									actions.push_back(old->second);
								} else {
									auto action = std::make_shared<FileAction>(title, fullPath);
									newActions.push_back(action);
									actions.push_back(std::move(action));
								}
							});
					}
					if (source.indexRoot) {
						if (!source.rootAction) {
							const std::wstring rootPath = ShortToLongPathWithEnvironment(source.roots.front());
							source.rootAction = std::make_shared<FileAction>(source.options.name, rootPath, false, rootPath);
							newActions.push_back(source.rootAction);
						}
						actions.push_back(source.rootAction);
					}
					source.directories.clear();
					source.actions = std::move(actions);
					Logi(L"FolderPlugin", L"index source=Everything root=", source.roots.front(),
						L" ms=", GetTickCount64() - oneSourceStart, L" count=", source.actions.size());
					continue;
				}
				std::unordered_map<std::wstring, DirectorySnapshot> next;
				std::vector<std::shared_ptr<FileAction>> actions;
				for (const auto& root : source.roots) {
					RefreshDirectory(source, root, next, newActions);
				}
				for (const auto& root : source.roots) {
					std::function<void(const std::wstring&)> collect = [&](const std::wstring& directory) {
						const auto found = next.find(CachePathKey(directory));
						if (found == next.end()) return;
						actions.insert(actions.end(), found->second.actions.begin(), found->second.actions.end());
						for (const auto& child : found->second.children) collect(child);
					};
					collect(root);
				}
				if (source.indexRoot) {
					if (!source.rootAction) {
						const std::wstring rootPath = ShortToLongPathWithEnvironment(source.roots.front());
						source.rootAction = std::make_shared<FileAction>(source.options.name, rootPath, false, rootPath);
						newActions.push_back(source.rootAction);
					}
					actions.push_back(source.rootAction);
				}
				source.directories = std::move(next);
				source.actions = std::move(actions);
				Logi(L"FolderPlugin", L"index source=filesystem root=", source.roots.empty() ? L"" : source.roots.front(),
					L" ms=", GetTickCount64() - oneSourceStart, L" count=", source.actions.size());
			}
			for (auto& [index, future] : settingsFutures) {
				sources[index].actions = future.get();
				newActions.insert(newActions.end(), sources[index].actions.begin(), sources[index].actions.end());
			}
			const ULONGLONG sourceDone = GetTickCount64();
			// 系统图标由主程序按 iconPath 按需提取，避免启动时对全部快捷方式执行 Shell 查询。
			const ULONGLONG iconsDone = GetTickCount64();
			if (uwpFuture.valid()) {
				const ULONGLONG waitStart = GetTickCount64();
				auto uwpActions = uwpFuture.get();
				for (auto& source : sources) {
					if (!source.isUwp) continue;
					source.actions = std::move(uwpActions);
					Logi(L"FolderPlugin", L"index source=UWP count=", source.actions.size(),
						L" wait ms=", GetTickCount64() - waitStart);
				}
			}
			const ULONGLONG uwpDone = GetTickCount64();
			auto visible = MergeVisibleActions(sources, allowDuplicates);
			const size_t visibleCount = visible.size();
			// 发布后主程序复制条目并处理匹配文本（可在任意线程调用）
			fileActions.Publish(host, std::move(visible));
			const ULONGLONG mergeDone = GetTickCount64();
			Logi(L"FolderPlugin", L"index timing config=", configDone - refreshStart,
				L"ms sources=", sourceDone - sourceStart, L"ms icon setup=", iconsDone - sourceDone,
				L"ms UWP wait=", uwpDone - iconsDone, L"ms merge+publish=", mergeDone - uwpDone,
				L"ms total=", mergeDone - refreshStart,
				L"ms new=", newActions.size(), L" visible=", visibleCount);
			cachedSources = std::move(sources);
			cachedConfig = config;
			cachedPath = std::move(environmentPath);
			cachedAllowDuplicates = allowDuplicates;
			cachedIndexRoot = indexRoot;
			cachedUwpEnabled = uwpEnabled;
			cachedPathEnabled = pathEnabled;
			cachedEverythingEnabled = everythingEnabled;
			cacheReady = true;
		} catch (const std::exception& e) {
			Loge(L"FolderPlugin", L"Incremental refresh failed; rebuilding: ", e.what());
			cacheReady = false;
			RefreshAllActionsBackup();
		}
	}

	void OnUserSettingsLoaded() override {
		if (!m_host) return;
		const candy::Host& host = GetHost();
		host.RegisterAppCommand(OPEN_FOLDER_INDEXED_MANAGER_CALLBACK_KEY, OpenIndexedManagerCommand, nullptr);

		bool pref_indexed_apps_show_sendto_shortcut = host.GetBool(L"com.candytek.folderplugin.show_sendto_shortcut", true);

		PWSTR sendto = nullptr;
		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SendTo, 0, nullptr, &sendto))) {
			// 需要编译CreateShortcut 文件夹里的win32 程序，然后把exe放在CandyLauncher.exe 同一个目录下
			std::wstring createShortcutExePath;
			createShortcutExePath.append(GetExecutableFolder()).append(L"\\CreateShortcut.exe");
			std::wstring sendToShortcutName;
			sendToShortcutName.append(sendto).append(L"\\").append(kLinkName);

			bool sendtoShortcutExists = PathFileExistsW(sendToShortcutName.c_str());

			if (pref_indexed_apps_show_sendto_shortcut) {
				if (!sendtoShortcutExists) {
					// 创建快捷方式
					InstallSendToEntry(createShortcutExePath);
				}
			} else {
				if (sendtoShortcutExists) {
					bool result = DeleteSendToEntry(kLinkName);
					// ShowErrorMsgBox(L"删除快捷方式" + result);
				}
			}
			CoTaskMemFree(sendto);
		}

		hkOpenFileLocation = ParseHotkeyString(host.GetString(L"com.candytek.folderplugin.hotkey_open_file_location"));
		hkOpenTargetLocation = ParseHotkeyString(host.GetString(L"com.candytek.folderplugin.hotkey_open_target_location"));
		hkCopyFilePath = ParseHotkeyString(host.GetString(L"com.candytek.folderplugin.hotkey_copy_file_path"));
		hkCopyTargetPath = ParseHotkeyString(host.GetString(L"com.candytek.folderplugin.hotkey_copy_target_path"));
		hkOpenWithClipboard = ParseHotkeyString(host.GetString(L"com.candytek.folderplugin.open_with_clipboard_params"));
		hkRunAsAdmin = ParseHotkeyString(host.GetString(L"com.candytek.folderplugin.hotkey_run_item_as_admin"));

#if defined(DEBUG) || defined(_DEBUG) || defined(REL_WITH_DEB_INFO_DEBUG)
		if (IS_SHOW_INDEX_MANAGER_WINDOW)
		{
			ShowIndexedManagerWindow(nullptr);
		}
#endif

	}


	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		if (!m_host) return false;
		// 执行自定义行为
		if (automationEditor.Find(userData)) {
			ShowAutomationActionWindow(nullptr);
			return true;
		}
		const auto fileAction = fileActions.Find(userData);
		if (!fileAction) return false;
		// 执行自动化功能
		const std::filesystem::path target(fileAction->GetTargetPath());
		const std::filesystem::path automationFolder = std::filesystem::path(EXE_FOLDER_PATH2) / L"plugins\\AutomationActions";
		if (_wcsicmp(target.extension().c_str(), L".json") == 0 &&
			_wcsicmp(target.parent_path().lexically_normal().c_str(), automationFolder.lexically_normal().c_str()) == 0) {
			RunAutomationDocument(target.wstring());
			return true;
		}
		// 执行文件
		fileAction->Invoke();
		return true;
	}

	bool OnHotKey(const uint64_t userData, const UINT vk, const UINT currentModifiers, const WPARAM wparam) override {
		const auto it = fileActions.Find(userData);
		if (!it) return false;

		if (hkOpenFileLocation.matches(vk, currentModifiers)) {
			it->InvokeOpenFolder();
			return true;
		}
		if (hkOpenTargetLocation.matches(vk, currentModifiers)) {
			it->InvokeOpenGoalFolder();
			return true;
		}
		if (hkCopyFilePath.matches(vk, currentModifiers)) {
			const std::wstring& path = it->GetTargetPath();
			if (!path.empty()) GetHost().SetClipboardText(path);
			return true;
		}
		if (hkCopyTargetPath.matches(vk, currentModifiers)) {
			const std::wstring path = SaveGetShortcutTarget(it->GetTargetPath());
			if (!path.empty()) GetHost().SetClipboardText(path);
			return true;
		}
		if (hkOpenWithClipboard.matches(vk, currentModifiers)) {
			it->InvokeWithTargetClipBoard();
			return true;
		}
		if (hkRunAsAdmin.matches(vk, currentModifiers)) {
			it->InvokeWithTarget(nullptr, true);
			return true;
		}

		return false;
	}

	// 输入框无选中文本时 Ctrl+C 复制条目路径
	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override {
		const auto fileAction = fileActions.Find(userData);
		if (!fileAction || fileAction->GetTargetPath().empty()) return false;
		text = fileAction->GetTargetPath();
		return true;
	}


	void OnSettingItemExecute(const std::wstring& key, HWND parentHwnd) override {
		if (key == L"com.candytek.folderplugin.indexed_manager") {
			ShowIndexedManagerWindow(parentHwnd);
		} else if (key == L"com.candytek.folderplugin.create_automation_action") {
			ShowAutomationActionWindow(parentHwnd);
		}
	}

	bool OnItemRightClick(const uint64_t userData, HWND parentHwnd, POINT screenPt) override {
		Logi(L"FolderPlugin", L"OnItemRightClick entered");
		const auto fileAction = fileActions.Find(userData);
		if (!fileAction) {
			Logw(L"FolderPlugin", L"OnItemRightClick skipped: action is not FileAction");
			return false;
		}
		Logi(L"FolderPlugin", L"ShowShellContextMenu path=", fileAction->GetTargetPath());
		ShowShellContextMenu(parentHwnd, fileAction->GetTargetPath(), screenPt);
		Logi(L"FolderPlugin", L"ShowShellContextMenu returned");
		return true;
	}

	// 重新按当前输入筛选，让主列表立即反映排除/重命名结果
	void RefreshVisibleList(const std::wstring& inputText) const {
		GetHost().SetEditText(inputText);
	}

	void ExcludeIndexItem(const std::shared_ptr<FileAction>& action, HWND parentHwnd) {
		std::wstring error;
		if (!ApplyIndexEdit(action, IndexEditKind::Exclude, L"", error)) {
			Loge(L"FolderPlugin", L"exclude index failed: ", error);
			MessageBoxW(parentHwnd, error.c_str(), L"排除该索引", MB_OK | MB_ICONWARNING);
			return;
		}
		RefreshVisibleList(GetHost().GetEditText());
	}

	void RenameIndexItem(const std::shared_ptr<FileAction>& action, HWND parentHwnd) {
		const std::wstring renameKey = GetIndexRenameKey(action);
		if (renameKey.empty()) return;
		// 对话框获得焦点时主窗口可能随失焦隐藏并清空输入，先保存输入，结束后恢复
		const std::wstring inputText = GetHost().GetEditText();
		std::wstring newTitle;
		const std::wstring caption = L"重命名该索引 (原名: " + renameKey + L")";
		const bool confirmed = ShowRenameDialog(parentHwnd, action->title, newTitle, caption.c_str());
		GetHost().ShowMainWindow(SW_SHOW, true);
		if (!confirmed || newTitle == action->title) {
			RefreshVisibleList(inputText);
			return;
		}
		std::wstring error;
		if (!ApplyIndexEdit(action, IndexEditKind::Rename, newTitle, error)) {
			Loge(L"FolderPlugin", L"rename index failed: ", error);
			MessageBoxW(parentHwnd, error.c_str(), L"重命名该索引", MB_OK | MB_ICONWARNING);
		}
		RefreshVisibleList(inputText);
	}

	bool OnItemShiftRightClick(const uint64_t userData, HWND parentHwnd, POINT screenPt) override {
		Logi(L"FolderPlugin", L"OnItemShiftRightClick entered");
		const auto fileAction = fileActions.Find(userData);
		if (!fileAction) {
			Logw(L"FolderPlugin", L"OnItemShiftRightClick skipped: action is not FileAction");
			return false;
		}
		Logi(L"FolderPlugin", L"ShowMyContextMenu path=", fileAction->GetTargetPath());
		const UINT cmd = ShowMyContextMenu(parentHwnd, fileAction->GetTargetPath(), screenPt, CanEditIndex(fileAction));
		Logi(L"FolderPlugin", L"ShowMyContextMenu cmd=", cmd);
		switch (cmd) {
		case IDM_REMOVE_ITEM: ExcludeIndexItem(fileAction, parentHwnd);
			break;
		case IDM_CONTEXT_MENU_RENAME_ITEM: RenameIndexItem(fileAction, parentHwnd);
			break;
		case IDM_RUN_AS_ADMIN: fileAction->InvokeWithTarget(nullptr, true);
			break;
		case IDM_OPEN_IN_CONSOLE: OpenConsoleHere(SaveGetShortcutTargetAndReturn(fileAction->GetTargetPath()));
			break;
		case IDM_KILL_PROCESS: KillProcessByImagePath(SaveGetShortcutTargetAndReturn(fileAction->GetTargetPath()));
			break;
		case IDM_COPY_PATH: GetHost().SetClipboardText(fileAction->GetTargetPath());
			break;
		case IDM_COPY_TARGET_PATH: GetHost().SetClipboardText(SaveGetShortcutTargetAndReturn(fileAction->GetTargetPath()));
			break;
		default: break;
		}
		return true;
	}

	bool OnItemBeginDrag(const uint64_t userData, const HWND sourceHwnd, const POINT screenPt) override {
		const auto fileAction = fileActions.Find(userData);
		if (!m_host || !fileAction) {
			return false;
		}

		const std::wstring targetPath = fileAction->GetTargetPath();
		if (targetPath.empty()) {
			return false;
		}

		Logi(L"FolderPlugin", L"Begin OLE drag drop path=", targetPath);
		candy::DragData dragData;
		dragData.filePaths = {targetPath};
		return GetHost().BeginDrag(dragData, sourceHwnd);
	}
};

// 索引刷新只用 "Any thread" 的主程序服务（集合发布、设置读取、traverse_files、日志），
// 所以允许主程序在工作线程上调用 RefreshActions，启动时不再串行等待文件夹索引
CANDY_PLUGIN_EXPORT_EX(FolderPlugin, L"Folders", L"Index configured folders, %PATH% and UWP apps", L"1.1.0", L"com.candytek.folderplugin",
						nullptr, CL_PLUGIN_REFRESH_ANY_THREAD)


// case TRAY_MENU_ID_EDIT_CONFIG: // 编辑 JSON 配置文件
// 	{
// 		wchar_t path[MAX_PATH];
// 		wcsncpy_s(path, RUNNER_CONFIG_PATH.c_str(), MAX_PATH - 1);
// 		ShellExecute(nullptr, L"open", L"notepad.exe", path, nullptr, SW_SHOW);
// 	}
// 	break;
// 	ShellExecute(nullptr, L"open", RUNNER_CONFIG_PATH.c_str(), nullptr, nullptr, SW_SHOW);
