#pragma once
#include "Plugin.h"
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <filesystem>
#include <algorithm>
#include <windows.h>

#include "../util/BaseTools.hpp"

struct PluginInfo
{
	HMODULE handle = nullptr;
	std::unique_ptr<IPlugin> plugin;
	std::wstring name;
	std::wstring version;
	std::wstring pkgName;
	std::wstring filePath;
	int16_t pluginId;
	bool loaded = false;

	CreatePluginFunc createFunc = nullptr;
	DestroyPluginFunc destroyFunc = nullptr;
	GetPluginApiVersionFunc getVersionFunc = nullptr;
};

inline std::vector<PluginInfo> m_plugins;
inline int16_t pluginCount = 0;

class PluginManager : public IPluginHost
{
private:
	std::function<void()> m_onActionsChanged;
	bool m_suppressNotifications = false;

public:
	PluginManager() = default;

	~PluginManager()
	{
		UnloadAllPlugins();
	}

	
  void TraverseFilesForEverythingSDK(
	const std::wstring &folderPath,
	const TraverseOptions &options,
	std::function<void(const std::wstring &name,
							 const std::wstring &fullPath,
							 const std::wstring &parent,
							 const std::wstring &ext)> &&callback) override {
		if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) return;

		auto addFile = [&](const fs::path &path) {
			std::wstring filename = path.stem().wstring();
			if (shouldExclude(options, filename)) return;

			if (const auto it = options.renameMap.find(filename); it != options.renameMap.end())
				filename = it->second;

			// 这里不限定 callback 的参数，可以传多种信息
			callback(
					filename,                     // 逻辑名（被 rename 过）
					path.wstring(),               // 完整路径
					path.parent_path().wstring(), // 父目录
					path.extension().wstring()    // 扩展名
			);
		};


		// 构造更高效的搜索查询
		// 例如: parent:"C:\ProgramData\Microsoft\Windows\Start Menu\Programs" ext:exe;lnk
		std::wstringstream search_query;
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

	std::wstring GetTheProcessedMatchingText(const std::wstring& source) override
	{
		// 这里可以执行一些处理，比如拼音转换、文本清理等
		return PinyinHelper::GetPinyinLongStr(MyToLower(source)); // 假设使用了 PinyinHelper 处理
	}

	bool LoadPlugin(const std::wstring& dllPath)
	{
		std::wstring pluginName = GetPluginNameFromPathW(dllPath);
		ConsolePrintln(L"LoadPlugin start: " + pluginName + L" from " + dllPath);

		PluginInfo info;
		info.filePath = dllPath;
		info.name = pluginName;

		if (LoadSinglePlugin(dllPath, info))
		{
			info.pluginId = static_cast<int16_t>(m_plugins.size());
			m_plugins.push_back(std::move(info));
			ConsolePrintln(
				L"Plugin successfully added to map: " + pluginName + L", total plugins: " + std::to_wstring(
					m_plugins.size()));
			return true;
		}

		ConsolePrintln(L"LoadSinglePlugin failed for: " + pluginName);
		return false;
	}
	
	const std::unordered_map<std::string, SettingItem>& GetSettingsMap() override {
		return g_settings_map;
	}

	bool UnloadPlugin(const int16_t pluginId)
	{
		if (m_plugins.size() > static_cast<size_t>(pluginId))
		{
			UnloadSinglePlugin(m_plugins[pluginId]);
			m_plugins.erase(m_plugins.begin() + pluginId);
			NotifyActionsChanged();
			return true;
		}
		return false;
	}

	void OnMainWindowShowNotifi(bool isShow)
	{
		for (auto& pair : m_plugins)
		{
			pair.plugin->OnMainWindowShow(isShow);
		}
	}

	void LoadAllPlugins(const std::wstring& pluginDir)
	{
		ConsolePrintln(L"LoadAllPlugins start, dir: " + pluginDir);

		if (!std::filesystem::exists(pluginDir))
		{
			ConsolePrintln(L"Plugin directory does not exist: " + pluginDir);
			return;
		}

		ConsolePrintln("Plugin directory exists, scanning for DLLs...");

		for (const auto& entry : std::filesystem::directory_iterator(pluginDir))
		{
			ConsolePrintln("Found file: " + entry.path().string());
			if (entry.is_regular_file() && entry.path().extension() == ".dll")
			{
				ConsolePrintln("Found DLL: " + entry.path().string());
				bool loadResult = LoadPlugin(entry.path().wstring());
				ConsolePrintln("Load result for " + entry.path().string() + ": " + (loadResult ? "SUCCESS" : "FAILED"));
			}
		}

		ConsolePrintln("LoadAllPlugins complete, total plugins loaded: " + std::to_string(m_plugins.size()));

		NotifyActionsChanged();
	}
	

	void UnloadAllPlugins()
	{
		for (auto& pair : m_plugins)
		{
			UnloadSinglePlugin(pair);
		}
		m_plugins.clear();
		NotifyActionsChanged();
	}

	void RefreshAllActions()
	{
		ConsolePrintln(L"RefreshAllActions start, clearing existing actions...");
		m_suppressNotifications = true; // 暂时禁用通知

		for (auto& pair : m_plugins)
		{
			if (pair.loaded && pair.plugin)
			{
				ConsolePrintln(L"Loading actions from plugin: " + pair.name);
				pair.plugin->RefreshAllActions();
				// ConsolePrintln(L"After loading, total actions: " + std::to_wstring(m_allActions.size()));
			}
		}

		m_suppressNotifications = false; // 重新启用通知
		// ConsolePrintln(L"RefreshAllActions complete - plugin size:" + std::to_wstring(m_plugins.size()) + L", total actions: " + std::to_wstring(m_allActions.size()));
		NotifyActionsChanged();
	}

	void DispatchUserInput(const std::wstring& input)
	{
		m_suppressNotifications = true; // 防止在用户输入过程中触发递归刷新

		for (const auto& plugin : m_plugins)
		{
			if (plugin.loaded && plugin.plugin)
			{
				plugin.plugin->OnUserInput(input);
			}
		}

		m_suppressNotifications = false; // 重新启用通知
		//
		// // 如果动作数量发生变化，说明插件添加了新动作，需要通知主程序
		// if (m_allActions.size() != oldActionCount)
		// {
		// 	ConsolePrintln(L"DispatchUserInput detected action count change: " +
		// 				   std::to_wstring(oldActionCount) + L" -> " +
		// 				   std::to_wstring(m_allActions.size()));
		// 	NotifyActionsChanged(); // 只通知，不重新刷新
		// }
	}

	static void DispatchActionExecute(std::shared_ptr<ActionBase>& action)
	{
		for (auto& pair : m_plugins)
		{
			if (pair.loaded && pair.plugin)
			{
				pair.plugin->OnActionExecute(action);
			}
		}
	}

	std::vector<std::wstring> GetLoadedPlugins() const
	{
		std::vector<std::wstring> result;
		for (const auto& pair : m_plugins)
		{
			if (pair.loaded)
			{
				result.push_back(pair.name);
			}
		}
		return result;
	}

	static void GetPluginActions(const int16_t pluginId, std::vector<std::shared_ptr<ActionBase>>& actions)
	{
		actions = m_plugins[pluginId].plugin->GetAllActions();
	}

	static int16_t IsInterceptInputShowResultsDirectly(const std::wstring& input)
	{
		for (const auto& pair : m_plugins)
		{
			if (pair.loaded && pair.plugin)
			{
				bool temp = pair.plugin->InterceptInputShowResultsDirectly(input);
				if (temp)
				{
					return pair.pluginId;
				}
			}
		}
		return -1;
	}

	static void GetAllPluginActions(std::vector<std::shared_ptr<ActionBase>>& allActions)
	{
		for (const auto& pair : m_plugins)
		{
			if (pair.loaded && pair.plugin)
			{
				const std::vector<std::shared_ptr<ActionBase>> temp = pair.plugin->GetAllActions();
				allActions.insert(allActions.end(), temp.begin(), temp.end());
			}
		}
	}

	void SetActionsChangedCallback(std::function<void()> callback)
	{
		m_onActionsChanged = callback;
	}

	// void AddAction(const ActionBase& item) override
	// {
	// auto it = std::find_if(m_allActions.begin(), m_allActions.end(),
	// 						[&](const ActionBase& action)
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
	// 						[&](const ActionBase& item)
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
	// 						[&](const ActionBase& item)
	// 						{
	// 							return item.id.find(pluginName + L"_") == 0;
	// 						}),
	// 		m_allActions.end());
	//
	// 	NotifyActionsChanged();
	// }

	void ShowMessage(const std::wstring& title, const std::wstring& message) override
	{
		MessageBoxW(nullptr, message.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
	}

private:
	bool LoadSinglePlugin(const std::wstring& dllPath, PluginInfo& info)
	{
		ConsolePrintln(L"LoadSinglePlugin start: " + dllPath);

		info.handle = LoadLibraryW(dllPath.c_str());
		if (!info.handle)
		{
			ConsolePrintln(L"LoadLibraryW failed for: " + dllPath + L", Error: " + std::to_wstring(GetLastError()));
			return false;
		}
		ConsolePrintln(L"LoadLibraryW success for: " + dllPath);

		info.createFunc = (CreatePluginFunc)GetProcAddress(info.handle, "CreatePlugin");
		info.destroyFunc = (DestroyPluginFunc)GetProcAddress(info.handle, "DestroyPlugin");
		info.getVersionFunc = (GetPluginApiVersionFunc)GetProcAddress(info.handle, "GetPluginApiVersion");

		if (!info.createFunc || !info.destroyFunc)
		{
			ConsolePrintln("GetProcAddress failed - createFunc: " + std::to_string(info.createFunc != nullptr) +
				", destroyFunc: " + std::to_string(info.destroyFunc != nullptr));
			FreeLibrary(info.handle);
			info.handle = nullptr;
			return false;
		}
		ConsolePrintln("GetProcAddress success");

		IPlugin* pluginPtr = info.createFunc();
		if (!pluginPtr)
		{
			ConsolePrintln("CreateSimplePlugin returned null");
			FreeLibrary(info.handle);
			info.handle = nullptr;
			return false;
		}
		ConsolePrintln("CreateSimplePlugin success");

		info.plugin.reset(pluginPtr);

		if (!info.plugin->Initialize(this))
		{
			ConsolePrintln("Plugin Initialize failed");
			info.destroyFunc(pluginPtr);
			info.plugin.reset();
			FreeLibrary(info.handle);
			info.handle = nullptr;
			return false;
		}
		ConsolePrintln("Plugin Initialize success");

		info.version = info.plugin->GetPluginVersion();
		info.pkgName = info.plugin->GetPluginPackageName();
		info.loaded = true;

		// info.plugin->LoadActions();
		ConsolePrintln(L"LoadSinglePlugin complete: " + dllPath);

		return true;
	}

	static void UnloadSinglePlugin(PluginInfo& info)
	{
		if (info.plugin)
		{
			info.plugin->Shutdown();
			if (info.destroyFunc)
			{
				info.destroyFunc(info.plugin.release());
			}
		}

		if (info.handle)
		{
			FreeLibrary(info.handle);
			info.handle = nullptr;
		}

		info.loaded = false;
	}

	static std::wstring GetPluginNameFromPath(const std::wstring& dllPath)
	{
		std::filesystem::path path(dllPath);
		return path.stem().wstring();
	}

	static std::wstring GetPluginNameFromPathW(const std::wstring& dllPath)
	{
		std::filesystem::path path(dllPath);
		return path.stem().wstring();
	}

	void NotifyActionsChanged()
	{
		if (m_suppressNotifications)
		{
			return; // 如果通知被抑制，直接返回
		}
		if (m_onActionsChanged)
		{
			m_onActionsChanged();
		}
	}
};

inline std::unique_ptr<PluginManager> g_pluginManager;
