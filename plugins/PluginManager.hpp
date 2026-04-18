#pragma once
#include "Plugin.hpp"
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <filesystem>
#include <algorithm>
#include <windows.h>

#include "BaseLaunchAction.hpp"
#include "Everything.h"
#include "../util/BaseTools.hpp"
#include "util/FileUtil.hpp"
#include "util/json.hpp"
#include "manager/TextMatchHelper.hpp"
#include "util/PinyinHelper.hpp"
#include "util/MainTools.hpp"
#include "util/MyToastUtil.hpp"

struct PluginInfo {
	HMODULE handle = nullptr;
	std::unique_ptr<IPlugin> plugin;
	std::wstring name;
	std::wstring version;
	std::wstring pkgName;
	std::wstring filePath;
	uint16_t pluginId = 65535;
	int apiVersion = 1;
	bool loaded = false;
	uint64_t loadTimeMs = 0;
	uint64_t refreshTimeMs = 0;
	int32_t priority = 0;

	CreatePluginFunc createFunc = nullptr;
	DestroyPluginFunc destroyFunc = nullptr;
	GetPluginApiVersionFunc getVersionFunc = nullptr;
};

struct PluginCatalogInfo {
	std::wstring name;
	std::wstring version;
	std::wstring pkgName;
	std::wstring filePath;
	std::wstring defaultSettingJson;
	bool enabled = true;
};

inline std::unordered_map<uint16_t, PluginInfo> m_plugins = {};
inline std::vector<PluginCatalogInfo> g_pluginCatalog = {};
inline uint16_t pluginCount = 0;
static std::wstring TAG = L"PluginManager";

class PluginManager : public IPluginHost {
private:
	std::function<void()> m_onActionsChanged;
	bool m_suppressNotifications = false;

public:
	PluginManager() = default;

	~PluginManager() {
		UnloadAllPlugins();
	}

	void TraverseFilesSimpleForEverythingSDK(
		const std::wstring& folderPath, // 起始目录
		bool recursive, // 是否递归
		const std::vector<std::wstring>& extensions, // 扩展名过滤
		const std::wstring& nameFilter, // 文件名关键字，可为空
		bool isIndexFilesOnly, // 是否只索引文件
		std::function<void(const std::wstring& name,
							const std::wstring& fullPath,
							const std::wstring& parent,
							const std::wstring& ext)> callback) override {
		namespace fs = std::filesystem;
		if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) return;

		std::wstringstream query;
		if (isIndexFilesOnly) {
			query << L"file: ";
		}
		// 指定搜索目录（递归或非递归）
		if (recursive) query << L"\"" << folderPath << L"\"";
		else query << L"parent:\"" << folderPath << L"\"";

		// 拼接扩展名过滤（ext:）
		if (!extensions.empty()) {
			query << L" ext:";
			for (size_t i = 0; i < extensions.size(); ++i) {
				std::wstring ext = extensions[i];
				if (!ext.empty() && ext[0] == L'.') ext.erase(0, 1);
				query << ext;
				if (i < extensions.size() - 1) query << L";";
			}
		}

		// 文件名关键字过滤（匹配部分文件名）
		if (!nameFilter.empty()) {
			query << L" " << nameFilter;
		}

		std::wcout << L"[Everything] Query: " << query.str() << std::endl;

		// 执行 Everything 查询
		Everything_SetSearchW(query.str().c_str());
		Everything_QueryW(TRUE);

		DWORD num = Everything_GetNumResults();
		for (DWORD i = 0; i < num; ++i) {
			wchar_t fullPath[MAX_PATH];
			Everything_GetResultFullPathNameW(i, fullPath, MAX_PATH);

			fs::path p(fullPath);
			callback(
				p.stem().wstring(), // 文件名（不含扩展）
				p.wstring(), // 完整路径
				p.parent_path().wstring(), // 父目录
				p.extension().wstring()); // 扩展名
		}
	}

	void TraverseFilesForEverythingSDK(
		const std::wstring& folderPath2,
		const TraverseOptions& options,
		std::function<void(const std::wstring& name,
							const std::wstring& fullPath,
							const std::wstring& parent,
							const std::wstring& ext)>&& callback) override {
		namespace fs = std::filesystem;
		const std::wstring folderPath = ExpandEnvironmentVariables(folderPath2, EXE_FOLDER_PATH);
		if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) return;

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
			// 如果是递归搜索，则使用原始的路径搜索
			search_query << L"\"" << folderPath << L"\" ";
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

		std::wcout << L"Executing optimized Everything search: " << search_query.str() << std::endl;

		// 使用 Everything SDK
		Everything_SetSearchW(search_query.str().c_str());
		Everything_QueryW(TRUE);

		DWORD numResults = Everything_GetNumResults();

		for (DWORD i = 0; i < numResults; i++) {
			wchar_t fullPath[MAX_PATH];
			Everything_GetResultFullPathNameW(i, fullPath, MAX_PATH);
			// 因为查询已经精确过滤，不再需要手动判断父目录了
			addFile(fs::path(fullPath));
		}
	}

	// 搜索可能的目标文件
	std::vector<FileInfo> SearchPossibleTargets(const std::wstring& fileName, uint64_t resultCount) override {
		std::vector<FileInfo> candidates;

		// 构建搜索查询：搜索以fileName开头的.exe文件
		std::wstring searchQuery = fileName;

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


	std::wstring GetTheProcessedMatchingText(const std::wstring& source) override {
		// 这里可以执行一些处理，比如拼音转换、文本清理等
		return PinyinHelper::GetPinyinWithVariants(MyToLower(sanitizeVisible(source))); // 假设使用了 PinyinHelper 处理
	}

	bool LoadPlugin(const std::wstring& dllPath) {
		std::wstring pluginName = GetPluginNameFromPathW(dllPath);

		PluginInfo info;
		info.filePath = dllPath;
		info.name = pluginName;

		if (LoadSinglePlugin(dllPath, info)) {
			info.pluginId = static_cast<uint16_t>(m_plugins.size());
			info.plugin->OnPluginIdChange(info.pluginId);
			m_plugins[info.pluginId] = std::move(info);
			return true;
		}

		ConsolePrintln(TAG, L"LoadSinglePlugin failed for: " + pluginName);
		return false;
	}

	const std::unordered_map<std::string, SettingItem>& GetSettingsMap() override {
		return g_settings_map;
	}

	const std::unordered_map<std::string, std::function<void()>>& GetAppLaunchActionCallbacks() override {
		return appLaunchActionCallBacks;
	}

	void RegisterAppLaunchActionCallback(const std::string& key, std::function<void()> callback) override {
		appLaunchActionCallBacks[key] = std::move(callback);
	}

	void UnregisterAppLaunchActionCallback(const std::string& key) override {
		appLaunchActionCallBacks.erase(key);
	}

	bool IsPluginEnabledByPackageName(const std::wstring& packageName) const override {
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

	bool UnloadPlugin(const uint16_t pluginId) {
		if (m_plugins.find(pluginId) != m_plugins.end()) {
			UnloadSinglePlugin(m_plugins[pluginId]);
			m_plugins.erase(pluginId);
			RebuildPluginIndices();
			NotifyActionsChanged();
			return true;
		}
		return false;
	}

	void OnMainWindowShowNotifi(bool isShow) {
		for (auto& pair : m_plugins) {
			pair.second.plugin->OnMainWindowShow(isShow);
		}
	}

	void LoadAllPlugins(const std::wstring& pluginDir) {
		ConsolePrintln(TAG, L"LoadAllPlugins start, dir: " + pluginDir);
		g_pluginCatalog.clear();

		if (!std::filesystem::exists(pluginDir)) {
			ConsolePrintln(TAG, L"Plugin directory does not exist: " + pluginDir);
			return;
		}
		if (!std::filesystem::is_directory(pluginDir)) {
			ConsolePrintln(TAG, L"Plugin path is not a directory: " + pluginDir);
			return;
		}

		const nlohmann::json userConfig = LoadUserPluginConfig();
		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(pluginDir)) {
			if (entry.is_regular_file() && entry.path().extension() == ".dll") {
				PluginCatalogInfo catalogInfo;
				if (!ProbePluginCatalogInfo(entry.path().wstring(), catalogInfo)) {
					catalogInfo.filePath = entry.path().wstring();
					catalogInfo.name = entry.path().stem().wstring();
				}
				catalogInfo.enabled = IsPluginEnabledByConfig(catalogInfo.pkgName, userConfig);
				g_pluginCatalog.push_back(catalogInfo);

				if (!catalogInfo.enabled) {
					ConsolePrintln(TAG, L"Skip disabled plugin: " + catalogInfo.pkgName + L", file: " + entry.path().filename().wstring());
					continue;
				}

				const auto pluginLoadStart = GetTickCount64();
				bool loadResult = LoadPlugin(entry.path().wstring());
				const uint64_t pluginLoadTime = GetTickCount64() - pluginLoadStart;
				if (loadResult) {
					if (const PluginInfo* loadedPlugin = FindLoadedPluginByFilePath(entry.path().wstring())) {
						m_plugins[loadedPlugin->pluginId].loadTimeMs = pluginLoadTime;
						if (g_pluginCatalog.back().pkgName.empty() || g_pluginCatalog.back().defaultSettingJson.empty()) {
							g_pluginCatalog.back().name = loadedPlugin->name;
							g_pluginCatalog.back().version = loadedPlugin->version;
							g_pluginCatalog.back().pkgName = loadedPlugin->pkgName;
							g_pluginCatalog.back().defaultSettingJson = loadedPlugin->plugin->DefaultSettingJson();
						}
					}
				}
				ConsolePrintln(TAG, "Load result for " + entry.path().filename().string() + ": " + (loadResult ? "SUCCESS" : "FAILED"));
			}
		}

		ConsolePrintln(TAG, "LoadAllPlugins complete, total plugins loaded: " + std::to_string(m_plugins.size()));

		NotifyActionsChanged();
	}


	void UnloadAllPlugins() {
		for (auto& pair : m_plugins) {
			UnloadSinglePlugin(pair.second);
		}
		m_plugins.clear();
		NotifyActionsChanged();
	}

	void RefreshAllActions() {
		ConsolePrintln(TAG, L"RefreshAllActions start, clearing existing actions...");
		m_suppressNotifications = true; // 暂时禁用通知

		for (auto& pair : m_plugins) {
			if (pair.second.loaded && pair.second.plugin) {
				ConsolePrintln(TAG, L"Loading actions from plugin: " + pair.second.name);
				const auto refreshStart = GetTickCount64();
				pair.second.plugin->RefreshAllActions();
				pair.second.refreshTimeMs = GetTickCount64() - refreshStart;
				// ConsolePrintln(TAG, L"After loading, total actions: " + std::to_wstring(m_allActions.size()));
			}
		}

		m_suppressNotifications = false; // 重新启用通知
		// ConsolePrintln(TAG, L"RefreshAllActions complete - plugin size:" + std::to_wstring(m_plugins.size()) + L", total actions: " + std::to_wstring(m_allActions.size()));
		NotifyActionsChanged();
	}

	void DispatchUserInput(const std::wstring& input) {
		m_suppressNotifications = true; // 防止在用户输入过程中触发递归刷新

		for (const auto& plugin : m_plugins) {
			if (plugin.second.loaded && plugin.second.plugin) {
				plugin.second.plugin->OnUserInput(input);
			}
		}

		m_suppressNotifications = false; // 重新启用通知
		//
		// // 如果动作数量发生变化，说明插件添加了新动作，需要通知主程序
		// if (m_allActions.size() != oldActionCount)
		// {
		// 	ConsolePrintln(TAG, L"DispatchUserInput detected action count change: " +
		// 				   std::to_wstring(oldActionCount) + L" -> " +
		// 				   std::to_wstring(m_allActions.size()));
		// 	NotifyActionsChanged(); // 只通知，不重新刷新
		// }
	}

	int DispatchSendHotKey(const std::shared_ptr<BaseAction>& action, const UINT vk, const UINT uint, const WPARAM wparam) {
		if (m_plugins.find(action->pluginId) == m_plugins.end()) {
			return 0;
		}
		return m_plugins[action->pluginId].plugin->OnSendHotKey(action, vk, uint, wparam);
	}

	bool DispatchItemRightClick(const std::shared_ptr<BaseAction>& action, HWND parentHwnd, POINT screenPt) {
		if (!action || m_plugins.find(action->pluginId) == m_plugins.end()) {
			ConsolePrintln(TAG, L"DispatchItemRightClick skipped: action/plugin not found");
			return false;
		}
		PluginInfo& info = m_plugins[action->pluginId];
		ConsolePrintln(TAG,
			L"DispatchItemRightClick pluginId=" + std::to_wstring(action->pluginId) +
			L", apiVersion=" + std::to_wstring(info.apiVersion) +
			L", loaded=" + std::to_wstring(info.loaded) +
			L", plugin=" + std::to_wstring(info.plugin != nullptr));
		if (!info.loaded || !info.plugin) {
			return false;
		}
		return info.plugin->OnItemRightClick(action, parentHwnd, screenPt);
	}

	bool DispatchItemShiftRightClick(const std::shared_ptr<BaseAction>& action, HWND parentHwnd, POINT screenPt) {
		if (!action || m_plugins.find(action->pluginId) == m_plugins.end()) {
			ConsolePrintln(TAG, L"DispatchItemShiftRightClick skipped: action/plugin not found");
			return false;
		}
		PluginInfo& info = m_plugins[action->pluginId];
		ConsolePrintln(TAG,
			L"DispatchItemShiftRightClick pluginId=" + std::to_wstring(action->pluginId) +
			L", apiVersion=" + std::to_wstring(info.apiVersion) +
			L", loaded=" + std::to_wstring(info.loaded) +
			L", plugin=" + std::to_wstring(info.plugin != nullptr));
		if (!info.loaded || !info.plugin) {
			return false;
		}
		return info.plugin->OnItemShiftRightClick(action, parentHwnd, screenPt);
	}

	static bool DispatchActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) {
		if (m_plugins.find(action->pluginId) == m_plugins.end()) {
			const auto action1 = std::dynamic_pointer_cast<BaseLaunchAction>(action);
			if (!action1) return false;
			action1->Invoke();
			return true;
		}

		return m_plugins[action->pluginId].plugin->OnActionExecute(action, arg);
		// for (auto& pair : m_plugins) {
		// 	if (pair.loaded && pair.plugin) {
		// 		pair.plugin->OnActionExecute(action,arg);
		// 	}
		// }
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

	void SyncPluginPrioritiesFromSettings() {
		for (auto& [pluginId, info] : m_plugins) {
			if (!info.pkgName.empty()) {
				const std::string priorityKey = wide_to_utf8(info.pkgName) + ".priority";
				const auto it = g_settings_map.find(priorityKey);
				info.priority = (it != g_settings_map.end()) ? static_cast<int32_t>(it->second.intValue) : 0;
			}
		}
	}

	void NotifyUserSettingsLoadDone() {
		SyncPluginPrioritiesFromSettings();
		for (auto& [pluginId, info] : m_plugins) {
			if (info.loaded && info.plugin) {
				info.plugin->OnUserSettingsLoadDone();
			}
		}
	}

	void SyncPluginsWithSettings() {
		for (auto& catalogInfo : g_pluginCatalog) {
			bool shouldEnable = catalogInfo.enabled;
			if (!catalogInfo.pkgName.empty()) {
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
					ConsolePrintln(TAG, L"SyncPluginsWithSettings load plugin: " + catalogInfo.filePath +
						L", result: " + (loadResult ? L"SUCCESS" : L"FAILED"));
					loadedPlugin = !catalogInfo.pkgName.empty()
						? FindLoadedPluginByPackageName(catalogInfo.pkgName)
						: FindLoadedPluginByFilePath(catalogInfo.filePath);
				}

				if (loadedPlugin) {
					catalogInfo.enabled = true;
					catalogInfo.name = loadedPlugin->name;
					catalogInfo.version = loadedPlugin->version;
					catalogInfo.pkgName = loadedPlugin->pkgName;
					if (catalogInfo.defaultSettingJson.empty()) {
						catalogInfo.defaultSettingJson = loadedPlugin->plugin->DefaultSettingJson();
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

	static std::vector<std::shared_ptr<BaseAction>> IsInterceptInputShowResultsDirectly(const std::wstring& input) {
		for (const auto& [fst, snd] : m_plugins) {
			if (snd.loaded && snd.plugin) {
				if (auto temp = snd.plugin->InterceptInputShowResultsDirectly(input); !temp.empty()) {
					return temp;
				}
			}
		}
		return {};
	}

	static void GetAllPluginActions(std::vector<std::shared_ptr<BaseAction>>& allActions) {
		std::vector<std::pair<int32_t, const PluginInfo*>> sortedPlugins;
		for (const auto& pair : m_plugins) {
			if (pair.second.loaded && pair.second.plugin) {
				sortedPlugins.emplace_back(pair.second.priority, &pair.second);
			}
		}
		std::stable_sort(sortedPlugins.begin(), sortedPlugins.end(),
			[](const auto& a, const auto& b) { return a.first > b.first; });

		for (const auto& [priority, pluginInfo] : sortedPlugins) {
			std::vector<std::shared_ptr<BaseAction>> temp = pluginInfo->plugin->GetTextMatchActions();
			for (auto& action : temp) {
				action->pluginPriority = priority;
			}
			allActions.insert(allActions.end(), temp.begin(), temp.end());
		}
	}

	void SetActionsChangedCallback(std::function<void()> callback) {
		m_onActionsChanged = callback;
	}

	std::vector<std::shared_ptr<BaseAction>> GetSuccessfullyMatchingTextActions(const std::wstring& input,
																				const std::vector<std::shared_ptr<BaseAction>>&
																				allActions) override {
		std::vector<std::shared_ptr<BaseAction>> filteredActions;
		if (allActions.empty()) {
			return filteredActions;
		}
		if (input.empty()) {
			return allActions;
		}
		TextMatch(input, allActions, filteredActions);
		return filteredActions;
	}

	// void AddAction(const BaseAction& item) override
	// {
	// auto it = std::find_if(m_allActions.begin(), m_allActions.end(),
	// 						[&](const BaseAction& action)
	// 						{
	// 							return action.id == item.id;
	// 						});

	// if (it != m_allActions.end())
	// {
	// 	*it = item;
	// }
	// else
	// {
	// m_allActions.push_back(item);
	// }
	// 	NotifyActionsChanged();
	// }

	// void RemoveAction(const std::wstring& itemId) override
	// {
	// 	m_allActions.erase(
	// 		std::remove_if(m_allActions.begin(), m_allActions.end(),
	// 						[&](const BaseAction& item)
	// 						{
	// 							return item.id == itemId;
	// 						}),
	// 		m_allActions.end());
	//
	// 	NotifyActionsChanged();
	// }


	// void ClearActions(const std::wstring& pluginName) override
	// {
	// 	m_allActions.erase(
	// 		std::remove_if(m_allActions.begin(), m_allActions.end(),
	// 						[&](const BaseAction& item)
	// 						{
	// 							return item.id.find(pluginName + L"_") == 0;
	// 						}),
	// 		m_allActions.end());
	//
	// 	NotifyActionsChanged();
	// }

	void ShowMessage(const std::wstring& title, const std::wstring& message) override {
		MessageBoxW(nullptr, message.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
	}

	HBITMAP LoadResIconAsBitmap(int nResID, int cx, int cy) override {
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

	void ShowSimpleToast(const std::wstring& title, const std::wstring& msg) override {
#if !defined(__MINGW32__)
		MyShowSimpleToast(title, msg);
#endif
	}
	
	void ChangeEditTextText(const std::wstring& basic_string) override {
		SetWindowTextW(g_editHwnd, basic_string.c_str());
		// 将光标移到文本末尾
		int textLength = GetWindowTextLengthW(g_editHwnd);
		SendMessageW(g_editHwnd, EM_SETSEL, (WPARAM)textLength, (LPARAM)textLength);
		SendMessageW(g_editHwnd, EM_SCROLLCARET, 0, 0);
	}

private:
	static void RebuildPluginIndices() {
		std::vector<std::pair<uint16_t, PluginInfo>> pluginList;
		pluginList.reserve(m_plugins.size());
		for (auto& pair : m_plugins) {
			pluginList.emplace_back(pair.first, std::move(pair.second));
		}

		std::sort(pluginList.begin(), pluginList.end(), [](const auto& lhs, const auto& rhs) {
			return lhs.first < rhs.first;
		});

		m_plugins.clear();
		for (size_t i = 0; i < pluginList.size(); ++i) {
			PluginInfo& info = pluginList[i].second;
			info.pluginId = static_cast<uint16_t>(i);
			if (info.plugin) {
				info.plugin->OnPluginIdChange(info.pluginId);
			}
			m_plugins[info.pluginId] = std::move(info);
		}
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
			Loge(TAG, L"LoadUserPluginConfig parse error", e.what());
			return {};
		}
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
			Loge(TAG, L"IsPluginEnabledByConfig parse value error", e.what());
			return true;
		}
	}

	static bool ProbePluginCatalogInfo(const std::wstring& dllPath, PluginCatalogInfo& catalogInfo) {
		catalogInfo.filePath = dllPath;
		catalogInfo.name = GetPluginNameFromPathW(dllPath);

		HMODULE handle = LoadLibraryW(dllPath.c_str());
		if (!handle) {
			ConsolePrintln(TAG, L"Probe LoadLibraryW failed for: " + dllPath + L", Error: " + std::to_wstring(GetLastError()));
			return false;
		}

		const auto createFunc = (CreatePluginFunc)GetProcAddress(handle, "CreatePlugin");
		const auto destroyFunc = (DestroyPluginFunc)GetProcAddress(handle, "DestroyPlugin");
		if (!createFunc || !destroyFunc) {
			ConsolePrintln(TAG, L"Probe GetProcAddress failed for: " + dllPath);
			FreeLibrary(handle);
			return false;
		}

		IPlugin* pluginPtr = createFunc();
		if (!pluginPtr) {
			ConsolePrintln(TAG, L"Probe CreatePlugin returned null for: " + dllPath);
			FreeLibrary(handle);
			return false;
		}

		catalogInfo.name = pluginPtr->GetPluginName();
		catalogInfo.version = pluginPtr->GetPluginVersion();
		catalogInfo.pkgName = pluginPtr->GetPluginPackageName();
		catalogInfo.defaultSettingJson = pluginPtr->DefaultSettingJson();

		destroyFunc(pluginPtr);
		FreeLibrary(handle);
		return true;
	}

	bool LoadSinglePlugin(const std::wstring& dllPath, PluginInfo& info) {
		ConsolePrintln(TAG, L"LoadSinglePlugin start: " + dllPath);

		info.handle = LoadLibraryW(dllPath.c_str());
		if (!info.handle) {
			ConsolePrintln(TAG, L"LoadLibraryW failed for: " + dllPath + L", Error: " + std::to_wstring(GetLastError()));
			return false;
		}

		info.createFunc = (CreatePluginFunc)GetProcAddress(info.handle, "CreatePlugin");
		info.destroyFunc = (DestroyPluginFunc)GetProcAddress(info.handle, "DestroyPlugin");
		info.getVersionFunc = (GetPluginApiVersionFunc)GetProcAddress(info.handle, "GetPluginApiVersion");

		if (!info.createFunc || !info.destroyFunc) {
			ConsolePrintln(TAG, "GetProcAddress failed - createFunc: " + std::to_string(info.createFunc != nullptr) +
							", destroyFunc: " + std::to_string(info.destroyFunc != nullptr));
			FreeLibrary(info.handle);
			info.handle = nullptr;
			return false;
		}

		if (info.getVersionFunc) {
			info.apiVersion = info.getVersionFunc();
		}
		ConsolePrintln(TAG, L"Plugin API version: " + std::to_wstring(info.apiVersion));

		IPlugin* pluginPtr = info.createFunc();
		if (!pluginPtr) {
			ConsolePrintln(TAG, "CreateSimplePlugin returned null");
			FreeLibrary(info.handle);
			info.handle = nullptr;
			return false;
		}

		info.plugin.reset(pluginPtr);

		if (!info.plugin->Initialize(this)) {
			ConsolePrintln(TAG, "Plugin Initialize failed");
			info.destroyFunc(pluginPtr);
			info.plugin.reset();
			FreeLibrary(info.handle);
			info.handle = nullptr;
			return false;
		}
		ConsolePrintln(TAG, "Plugin Initialize success");

		info.version = info.plugin->GetPluginVersion();
		info.pkgName = info.plugin->GetPluginPackageName();
		info.loaded = true;

		// info.plugin->LoadActions();

		return true;
	}

	static void UnloadSinglePlugin(PluginInfo& info) {
		if (info.plugin) {
			info.plugin->Shutdown();
			if (info.destroyFunc) {
				info.destroyFunc(info.plugin.release());
			}
		}

		if (info.handle) {
			FreeLibrary(info.handle);
			info.handle = nullptr;
		}

		info.loaded = false;
	}
	
	std::wstring& GetEditTextText() override {
		return editTextBuffer;
	}

	static std::wstring GetPluginNameFromPath(const std::wstring& dllPath) {
		std::filesystem::path path(dllPath);
		return path.stem().wstring();
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
};

inline std::unique_ptr<PluginManager> g_pluginManager;
