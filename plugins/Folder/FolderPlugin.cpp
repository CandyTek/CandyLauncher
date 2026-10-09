#include "../Plugin.hpp"
#include <windows.h>
#include <memory>


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
#include "util/ThreadPool.hpp"
#include <mutex>
#include <atomic>

#include "util/HotkeyUtils.hpp"

inline bool IS_SHOW_INDEX_MANAGER_WINDOW = false;

// 任务队列系统
constexpr const char* OPEN_FOLDER_INDEXED_MANAGER_CALLBACK_KEY = "openFolderIndexedManager";
inline std::queue<std::function<void()>> g_taskQueue;
inline std::mutex g_taskQueueMutex;
inline std::condition_variable g_taskQueueCV;
// 用于插件索引系统正在运行的窗口
inline std::thread g_pluginIndexedRunningAppsThread;
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
	if (g_pluginIndexedRunningAppsThread.joinable()) {
		g_workerShouldStop = true;
		g_taskQueueCV.notify_all();
		g_pluginIndexedRunningAppsThread.join();
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

class FolderPlugin : public IPlugin {
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;
	std::shared_ptr<AutomationEditorAction> automationEditorAction;
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

public:
	FolderPlugin() = default;
	~FolderPlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"文件夹";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.folderplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"文件夹";
	}


	bool Initialize(IPluginHost* host) override {
		g_host = host;
		if (!g_host) return false;
		EnsureAutomationActionIndex();
		g_host->RegisterAppLaunchActionCallback(OPEN_FOLDER_INDEXED_MANAGER_CALLBACK_KEY, []() {
			ShowIndexedManagerWindow(nullptr);
		});
		g_refreshFolderPlugin = [this]() {
			RefreshAllActions();
		};
		g_workerShouldStop = false;
		g_pluginIndexedRunningAppsThread = std::thread(WorkerThreadFunction);
		return true;
	}

	void OnPluginIdChange(const uint16_t pluginId) override {
		m_pluginId = pluginId;
		if (automationEditorAction) automationEditorAction->pluginId = pluginId;
	}


	void Shutdown() override {
		if (g_host) {
			g_host->UnregisterAppLaunchActionCallback(OPEN_FOLDER_INDEXED_MANAGER_CALLBACK_KEY);
			stopThreadPluginRunningApps();
		}
		g_host = nullptr;
	}


	std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() override {
		if (!g_host) return {};
		auto actions = allPluginActions;
		if (automationEditorAction) actions.push_back(automationEditorAction);
		return actions;
	}

	static void doActionAddIconIndex(std::vector<std::shared_ptr<FileAction>>& shareds) {
		if (shareds.empty()) {
			return;
		}
		// 普通扩展名共享系统图标；只为有独立图标的文件访问 Shell。
		std::unordered_map<std::wstring, int> sharedIcons;
		std::vector<std::shared_ptr<FileAction>> individualIcons;
		individualIcons.reserve(shareds.size());
		for (const auto& action : shareds) {
			if (action->iconFilePathIndex >= 0) continue;
			const std::wstring extension = std::filesystem::path(action->getIconFilePath()).extension().wstring();
			if (extension.empty() || _wcsicmp(extension.c_str(), L".exe") == 0 ||
				_wcsicmp(extension.c_str(), L".lnk") == 0 ||
				_wcsicmp(extension.c_str(), L".ico") == 0 ||
				_wcsicmp(extension.c_str(), L".url") == 0) {
				individualIcons.push_back(action);
				continue;
			}
			std::wstring key = extension;
			std::transform(key.begin(), key.end(), key.begin(), towlower);
			auto [it, inserted] = sharedIcons.try_emplace(key, -1);
			if (inserted) it->second = GetSysImageIndex2(L"candylauncher_file" + key);
			action->iconFilePathIndex = it->second;
		}
		if (individualIcons.empty()) return;
		// 确定要使用的线程数，通常基于硬件核心数
		// hardware_concurrency() 可能返回0，所以至少保证1个线程
		size_t numThreads = std::min<size_t>(std::thread::hardware_concurrency(), individualIcons.size());
		if (numThreads == 0) {
			numThreads = 1;
		}
		ThreadPool poolThread(numThreads);

		std::vector<std::future<void>> futures;
		std::atomic<ULONGLONG> shortcutMs{0}, executableMs{0}, otherMs{0};
		std::atomic<size_t> shortcutCount{0}, executableCount{0}, otherCount{0};
		const ULONGLONG iconStart = GetTickCount64();
		const size_t totalSize = individualIcons.size();
		const size_t chunkSize = (totalSize + numThreads - 1) / numThreads;

		// 创建并分发任务给多个线程
		for (unsigned int i = 0; i < numThreads; ++i) {
			const size_t start_index = i * chunkSize;
			if (start_index >= totalSize) break;
			const size_t end_index = std::min(start_index + chunkSize, totalSize);

			// 获取该分块的起始和结束迭代器
			auto start_it = individualIcons.begin() + start_index;
			auto end_it = individualIcons.begin() + end_index;

			// 注意：按值捕获迭代器，按引用捕获 shareds (如果只是读写成员，甚至不需要捕获整个容器)
			futures.push_back(poolThread.enqueue([start_it, end_it, &shortcutMs, &executableMs, &otherMs,
				&shortcutCount, &executableCount, &otherCount]() {
				for (auto it = start_it; it != end_it; ++it) {
					auto& action = *it;
					const ULONGLONG begin = GetTickCount64();
					action->iconFilePathIndex = GetSysImageIndex(action->getIconFilePath());
					const ULONGLONG elapsed = GetTickCount64() - begin;
					const auto& path = action->getIconFilePath();
					if (EndsWithIgnoreCase(path, L".lnk")) { shortcutMs += elapsed; ++shortcutCount; }
					else if (EndsWithIgnoreCase(path, L".exe")) { executableMs += elapsed; ++executableCount; }
					else { otherMs += elapsed; ++otherCount; }
				}
			}));
		}
		for (auto& f : futures) {
			f.get();
		}
		// 必须做一轮查询，做一个兜底，因为多线程调用GetSysImageIndex ，会有几率得到 index 为0的图标
		for (const std::shared_ptr<FileAction>& action : individualIcons) {
			if (action->iconFilePathIndex == 0) {
				action->iconFilePathIndex = GetSysImageIndex(action->getIconFilePath());
			}
		}
		Logi(L"FolderPlugin", L"icon timing wall=", GetTickCount64() - iconStart,
			L"ms shortcut=", shortcutCount.load(), L"/", shortcutMs.load(),
			L"ms exe=", executableCount.load(), L"/", executableMs.load(),
			L"ms other=", otherCount.load(), L"/", otherMs.load(), L"ms");
	}

	static void doActionAddIconIndexWithoutMultithreading(const std::vector<std::shared_ptr<FileAction>>& shareds) {
		if (shareds.empty()) {
			return;
		}
		for (const std::shared_ptr<FileAction>& action : shareds) {
			if (action->iconFilePathIndex >= 0) continue;
			action->iconFilePathIndex = GetSysImageIndex(action->getIconFilePath());
		}
	}


	void RefreshAllActionsBackup() {
		allPluginActions.clear();
		const bool allowDuplicateItems = g_host->GetSettingsMap().at("com.candytek.folderplugin.allow_duplicate_items").boolValue;
		std::unordered_set<std::wstring> indexedNames;
		auto pushAction = [&](std::vector<std::shared_ptr<FileAction>>& actions, const std::shared_ptr<FileAction>& action) {
			if (!action) return;
			if (!allowDuplicateItems) {
				const std::wstring normalizedTitle = NormalizeActionTitleForDedup(action->getTitle());
				if (indexedNames.find(normalizedTitle) != indexedNames.end()) return;
				indexedNames.insert(normalizedTitle);
			}
			actions.push_back(action);
		};
		auto pushAction2 = [&](std::vector<std::shared_ptr<BaseAction>>& actions, const std::shared_ptr<FileAction>& action) {
			if (!action) return;
			if (!allowDuplicateItems) {
				const std::wstring normalizedTitle = NormalizeActionTitleForDedup(action->getTitle());
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
				if (g_host->GetSettingsMap().at("pref_use_everything_sdk_index").boolValue) {
					g_host->TraverseFilesForEverythingSDK(traverseOptions1.folder, traverseOptions1, [&](const std::wstring& name,
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
				if (g_host->GetSettingsMap().at("com.candytek.folderplugin.index_folderpath_itself").boolValue) {
					std::wstring folderPath = ShortToLongPathWithEnvironment(
						ExpandEnvironmentVariables(traverseOptions1.folder, EXE_FOLDER_PATH2));
					const auto action = std::make_shared<FileAction>(traverseOptions1.name, folderPath, false, folderPath);
					pushAction(tempActions, action);
				}
			} else if (traverseOptions1.type == L"path" && !isPathAdded &&
				g_host->GetSettingsMap().at("com.candytek.folderplugin.envpath_apps").boolValue) {
				TraversePATHExecutables2([&](const std::wstring& name,
											const std::wstring& fullPath,
											const std::wstring& parent,
											const std::wstring& ext) {
					const auto action = std::make_shared<FileAction>(name, fullPath, false, parent);
					pushAction(tempActions, action);
				}, traverseOptions1, EXE_FOLDER_PATH2);

				isPathAdded = true;
			} else if (traverseOptions1.type == L"uwp" && !isUwpAdded &&
				g_host->GetSettingsMap().at("com.candytek.folderplugin.uwp_apps").boolValue) {
				std::vector<std::shared_ptr<BaseAction>> uwpActions;
				traverseUwpApps(uwpActions, traverseOptions1);
				for (const auto& baseAction : uwpActions) {
					if (auto fileAction = std::dynamic_pointer_cast<FileAction>(baseAction)) pushAction2(allPluginActions, fileAction);
				}
				isUwpAdded = true;
			} else if (traverseOptions1.type == L"control_panel" || traverseOptions1.type == L"windows_settings") {
				for (const auto& item : GetSystemSettingsItems(traverseOptions1)) {
					auto action = std::make_shared<FileAction>(item.name, item.target);
					action->iconFilePathIndex = item.iconIndex;
					pushAction(tempActions, action);
				}
			}
		}

		doActionAddIconIndex(tempActions);
		allPluginActions.insert(allPluginActions.end(),
								std::make_move_iterator(tempActions.begin()),
								std::make_move_iterator(tempActions.end()));

		TraverseOptions emptyTraverseOptions;
		TraverseOptions defaultOptions = CreateDefaultPathTraverseOptions();
		if (!isUwpAdded && g_host->GetSettingsMap().at("com.candytek.folderplugin.uwp_apps").boolValue) {
			std::vector<std::shared_ptr<BaseAction>> uwpActions;
			traverseUwpApps(uwpActions, emptyTraverseOptions);
			for (const auto& baseAction : uwpActions) {
				auto fileAction = std::dynamic_pointer_cast<FileAction>(baseAction);
				if (fileAction) pushAction2(allPluginActions, fileAction);
			}
		} else if (!isPathAdded && g_host->GetSettingsMap().at("com.candytek.folderplugin.envpath_apps").boolValue) {
			// TODO: 解决这个崩溃问题
			TraversePATHExecutables2([&](const std::wstring& name,
										const std::wstring& fullPath,
										const std::wstring& parent,
										const std::wstring& ext) {
				const auto action = std::make_shared<FileAction>(
					name, fullPath, false, parent
				);
				action->iconFilePathIndex = GetSysImageIndex(action->getIconFilePath());
				pushAction2(allPluginActions, action);
			}, defaultOptions, EXE_FOLDER_PATH2);
		}
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
				if (previousAction != previous.end() && previousAction->second->getTitle() == title) {
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

	static std::vector<std::shared_ptr<BaseAction>> MergeVisibleActions(const std::vector<CachedSource>& sources,
		bool allowDuplicates) {
		std::vector<std::shared_ptr<BaseAction>> result;
		std::unordered_set<std::wstring> titles;
		auto addSource = [&](const CachedSource& source) {
			for (const auto& action : source.actions) {
				if (allowDuplicates || titles.insert(NormalizeActionTitleForDedup(action->getTitle())).second)
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
		if (source.isUwp || source.isSystemSettings) return OriginalDisplayName(source.options, action.getTitle());
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
		allPluginActions = MergeVisibleActions(cachedSources, cachedAllowDuplicates);
		Logi(L"FolderPlugin", kind == IndexEditKind::Exclude ? L"index excluded key=" : L"index renamed key=", key,
			L" title=", newTitle, L" config=", configIndex, L" affected=", affected.size());
		return true;
	}

	void RefreshAllActions() override {
		if (!g_host) return;
		std::lock_guard<std::mutex> lock(cacheMutex);
		const ULONGLONG refreshStart = GetTickCount64();
		const auto& settings = g_host->GetSettingsMap();
		const bool allowDuplicates = settings.at("com.candytek.folderplugin.allow_duplicate_items").boolValue;
		const bool indexRoot = settings.at("com.candytek.folderplugin.index_folderpath_itself").boolValue;
		const bool uwpEnabled = settings.at("com.candytek.folderplugin.uwp_apps").boolValue;
		const bool pathEnabled = settings.at("com.candytek.folderplugin.envpath_apps").boolValue;
		const bool everythingEnabled = settings.at("pref_use_everything_sdk_index").boolValue;
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
					// UWP 条目及拼音匹配文本在工作线程构造，与文件索引并发执行。
					uwpFuture = std::async(std::launch::async, [options]() {
						std::vector<std::shared_ptr<BaseAction>> loaded;
						traverseUwpApps(loaded, options);
						std::vector<std::shared_ptr<FileAction>> actions;
						actions.reserve(loaded.size());
						for (auto& action : loaded)
							actions.push_back(std::static_pointer_cast<FileAction>(action));
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
							action->iconFilePathIndex = item.iconIndex;
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
						g_host->TraverseFilesForEverythingSDK(root, source.options,
							[&](const std::wstring& title, const std::wstring& fullPath,
								const std::wstring&, const std::wstring&) {
								const auto old = previous.find(CachePathKey(fullPath));
								if (old != previous.end() && old->second->getTitle() == title) {
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
			// 系统图标按需提取，避免启动时对全部快捷方式执行 Shell 查询。
			for (const auto& action : newActions)
				if (action->iconFilePathIndex < 0) action->iconIndexOnDemand = true;
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
			allPluginActions = MergeVisibleActions(sources, allowDuplicates);
			const ULONGLONG mergeDone = GetTickCount64();
			Logi(L"FolderPlugin", L"index timing config=", configDone - refreshStart,
				L"ms sources=", sourceDone - sourceStart, L"ms icon setup=", iconsDone - sourceDone,
				L"ms UWP wait=", uwpDone - iconsDone, L"ms merge=", mergeDone - uwpDone,
				L"ms total=", mergeDone - refreshStart,
				L"ms new=", newActions.size(), L" visible=", allPluginActions.size());
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

	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.folderplugin.uwp_apps",
			"title": "索引UWP应用",
			"type": "bool",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.folderplugin.regedit_apps",
			"title": "索引注册表中注册的应用",
			"type": "bool",
			"subPage": "plugin",
			"defValue": false
		},
		{
			"key": "com.candytek.folderplugin.envpath_apps",
			"title": "索引 %PATH% 中的可执行文件",
			"type": "bool",
			"subPage": "plugin",
			"defValue": false
		},
		{
			"key": "com.candytek.folderplugin.show_sendto_shortcut",
			"title": "在右键 \"发送到\" 菜单中显示 \"CandyLauncher 索引文件夹\"",
			"type": "bool",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.folderplugin.allow_duplicate_items",
			"title": "允许项目重复",
			"title_en": "Allow duplicate items",
			"type": "bool",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.folderplugin.run_item_as_admin",
			"title": "以管理员身份运行项目",
			"title_en": "Run item as administrator",
			"type": "bool",
			"subPage": "plugin",
			"defValue": false
		},
		{
			"key": "com.candytek.folderplugin.index_folderpath_itself",
			"title": "索引路径本身",
			"title_en": "The index path itself",
			"type": "bool",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.folderplugin.indexed_manager",
			"title": "索引查看器",
			"type": "button",
			"subPage": "plugin",
			"defValue": ""
		},
		{
			"key": "com.candytek.folderplugin.create_automation_action",
			"title": "创建或编辑自动化动作组",
			"type": "button",
			"subPage": "plugin",
			"defValue": ""
		},
		{
			"key": "com.candytek.folderplugin.hotkey_open_file_location",
			"title": "打开项目文件所在位置",
			"title_en": "Open item file location",
			"type": "hotkeystring",
			"subPage": "plugin",
			"defValue": ""
		},
		{
			"key": "com.candytek.folderplugin.hotkey_open_target_location",
			"title": "打开项目目标所在位置",
			"title_en": "Open item target location",
			"type": "hotkeystring",
			"subPage": "plugin",
			"defValue": ""
		},
		{
			"key": "com.candytek.folderplugin.hotkey_copy_file_path",
			"title": "复制项目文件路径",
			"title_en": "Copy item file path",
			"type": "hotkeystring",
			"subPage": "plugin",
			"defValue": ""
		},
		{
			"key": "com.candytek.folderplugin.hotkey_copy_target_path",
			"title": "复制快捷方式目标路径",
			"title_en": "Copy shortcut target path",
			"type": "hotkeystring",
			"subPage": "plugin",
			"defValue": ""
		},
		{
			"key": "com.candytek.folderplugin.open_with_clipboard_params",
			"title": "打开附带剪贴板参数",
			"title_en": "Open with clipboard parameters",
			"type": "hotkeystring",
			"subPage": "plugin",
			"defValue": ""
		},
		{
			"key": "com.candytek.folderplugin.hotkey_run_item_as_admin",
			"title": "以管理员身份运行项目",
			"title_en": "Run item as administrator",
			"type": "hotkeystring",
			"subPage": "plugin",
			"defValue": ""
		}

	]
}

   )";
	}


	void OnUserSettingsLoadDone() override {
		if (!g_host) return;
		g_host->RegisterAppLaunchActionCallback(OPEN_FOLDER_INDEXED_MANAGER_CALLBACK_KEY, []() {
			ShowIndexedManagerWindow(nullptr);
		});

		bool pref_indexed_apps_show_sendto_shortcut = g_host->GetSettingsMap().at("com.candytek.folderplugin.show_sendto_shortcut").
															boolValue;

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
		}

		const auto& settings = g_host->GetSettingsMap();
		hkOpenFileLocation = ParseHotkeyString(settings.at("com.candytek.folderplugin.hotkey_open_file_location").stringValue);
		hkOpenTargetLocation = ParseHotkeyString(settings.at("com.candytek.folderplugin.hotkey_open_target_location").stringValue);
		hkCopyFilePath = ParseHotkeyString(settings.at("com.candytek.folderplugin.hotkey_copy_file_path").stringValue);
		hkCopyTargetPath = ParseHotkeyString(settings.at("com.candytek.folderplugin.hotkey_copy_target_path").stringValue);
		hkOpenWithClipboard = ParseHotkeyString(settings.at("com.candytek.folderplugin.open_with_clipboard_params").stringValue);
		hkRunAsAdmin = ParseHotkeyString(settings.at("com.candytek.folderplugin.hotkey_run_item_as_admin").stringValue);
		automationEditorAction = std::make_shared<AutomationEditorAction>();

#if defined(DEBUG) || defined(_DEBUG) || defined(REL_WITH_DEB_INFO_DEBUG) 
		if (IS_SHOW_INDEX_MANAGER_WINDOW)
		{
			ShowIndexedManagerWindow(nullptr);
		}
#endif

	}


	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!g_host) return false;
		// 执行自定义行为
		if (std::dynamic_pointer_cast<AutomationEditorAction>(action)) {
			ShowAutomationActionWindow(nullptr);
			return true;
		}
		auto fileAction = std::dynamic_pointer_cast<FileAction>(action);
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

	int OnSendHotKey(std::shared_ptr<BaseAction>& action, const UINT vk, const UINT currentModifiers, const WPARAM wparam) override {
		auto it = std::dynamic_pointer_cast<FileAction>(action);
		if (!it) return 0;

		if (hkOpenFileLocation.matches(vk, currentModifiers)) {
			it->InvokeOpenFolder();
			return 1;
		}
		if (hkOpenTargetLocation.matches(vk, currentModifiers)) {
			it->InvokeOpenGoalFolder();
			return 1;
		}
		if (hkCopyFilePath.matches(vk, currentModifiers)) {
			const std::wstring& path = it->GetTargetPath();
			if (!path.empty()) CopyTextToClipboard(nullptr, path);
			return 1;
		}
		if (hkCopyTargetPath.matches(vk, currentModifiers)) {
			const std::wstring path = SaveGetShortcutTarget(it->GetTargetPath());
			if (!path.empty()) CopyTextToClipboard(nullptr, path);
			return 1;
		}
		if (hkOpenWithClipboard.matches(vk, currentModifiers)) {
			it->InvokeWithTargetClipBoard();
			return 1;
		}
		if (hkRunAsAdmin.matches(vk, currentModifiers)) {
			it->InvokeWithTarget(nullptr, true);
			return 1;
		}

		return 0;
	}


	void OnSettingItemExecute(const SettingItem* setting, HWND parentHwnd) override {
		if (setting->key == "com.candytek.folderplugin.indexed_manager") {
			ShowIndexedManagerWindow(parentHwnd);
		} else if (setting->key == "com.candytek.folderplugin.create_automation_action") {
			ShowAutomationActionWindow(parentHwnd);
		}
	}

	bool OnItemRightClick(const std::shared_ptr<BaseAction>& action, HWND parentHwnd, POINT screenPt) override {
		Logi(L"FolderPlugin", L"OnItemRightClick entered");
		auto fileAction = std::dynamic_pointer_cast<FileAction>(action);
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
	static void RefreshVisibleList(const std::wstring& inputText) {
		g_host->ChangeEditTextText(inputText);
	}

	void ExcludeIndexItem(const std::shared_ptr<FileAction>& action, HWND parentHwnd) {
		std::wstring error;
		if (!ApplyIndexEdit(action, IndexEditKind::Exclude, L"", error)) {
			Loge(L"FolderPlugin", L"exclude index failed: ", error);
			MessageBoxW(parentHwnd, error.c_str(), L"排除该索引", MB_OK | MB_ICONWARNING);
			return;
		}
		RefreshVisibleList(std::wstring(g_host->GetEditTextText()));
	}

	void RenameIndexItem(const std::shared_ptr<FileAction>& action, HWND parentHwnd) {
		const std::wstring renameKey = GetIndexRenameKey(action);
		if (renameKey.empty()) return;
		// 对话框获得焦点时主窗口可能随失焦隐藏并清空输入，先保存输入，结束后恢复
		const std::wstring inputText = g_host->GetEditTextText();
		std::wstring newTitle;
		const std::wstring caption = L"重命名该索引 (原名: " + renameKey + L")";
		const bool confirmed = ShowRenameDialog(parentHwnd, action->getTitle(), newTitle, caption.c_str());
		g_host->MyShowWindow(SW_SHOW, true);
		if (!confirmed || newTitle == action->getTitle()) {
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

	bool OnItemShiftRightClick(const std::shared_ptr<BaseAction>& action, HWND parentHwnd, POINT screenPt) override {
		Logi(L"FolderPlugin", L"OnItemShiftRightClick entered");
		auto fileAction = std::dynamic_pointer_cast<FileAction>(action);
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
		case IDM_COPY_PATH: CopyTextToClipboard(parentHwnd, fileAction->GetTargetPath());
			break;
		case IDM_COPY_TARGET_PATH: CopyTextToClipboard(parentHwnd, SaveGetShortcutTargetAndReturn(fileAction->GetTargetPath()));
			break;
		default: break;
		}
		return true;
	}

	bool OnItemBeginDrag(const std::shared_ptr<BaseAction>& action, HWND sourceHwnd, POINT screenPt) override {
		auto fileAction = std::dynamic_pointer_cast<FileAction>(action);
		if (!g_host || !fileAction) {
			return false;
		}

		const std::wstring targetPath = fileAction->GetTargetPath();
		if (targetPath.empty()) {
			return false;
		}

		Logi(L"FolderPlugin", L"Begin OLE drag drop path=", targetPath);
		return g_host->BeginOleDragDropFiles({targetPath}, sourceHwnd);
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new FolderPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}


// case TRAY_MENU_ID_EDIT_CONFIG: // 编辑 JSON 配置文件
// 	{
// 		wchar_t path[MAX_PATH];
// 		wcsncpy_s(path, RUNNER_CONFIG_PATH.c_str(), MAX_PATH - 1);
// 		ShellExecute(nullptr, L"open", L"notepad.exe", path, nullptr, SW_SHOW);
// 	}
// 	break;
// 	ShellExecute(nullptr, L"open", RUNNER_CONFIG_PATH.c_str(), nullptr, nullptr, SW_SHOW);
