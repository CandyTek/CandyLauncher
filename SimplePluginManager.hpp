#pragma once
#include "plugins/SimplePlugin.h"
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <filesystem>
#include <algorithm>
#include <windows.h>

#include "BaseTools.hpp"

class SimplePluginManager : public ISimplePluginHost
{
private:
	struct PluginInfo
	{
		HMODULE handle = nullptr;
		std::unique_ptr<ISimplePlugin> plugin;
		std::wstring name;
		std::string version;
		std::wstring filePath;
		bool loaded = false;

		CreateSimplePluginFunc createFunc = nullptr;
		DestroySimplePluginFunc destroyFunc = nullptr;
		GetSimplePluginApiVersionFunc getVersionFunc = nullptr;
	};

	std::map<std::wstring, PluginInfo> m_plugins;
	std::vector<SimpleActionItem> m_allActions;
	std::function<void()> m_onActionsChanged;
	bool m_suppressNotifications = false;

public:
	SimplePluginManager() = default;

	~SimplePluginManager()
	{
		UnloadAllPlugins();
	}

	bool LoadPlugin(const std::wstring& dllPath)
	{
		std::wstring pluginName = GetPluginNameFromPathW(dllPath);
		ConsolePrintln(L"LoadPlugin start: " + pluginName + L" from " + dllPath);

		if (m_plugins.find(pluginName) != m_plugins.end())
		{
			ConsolePrintln(L"Plugin already loaded: " + pluginName);
			return false;
		}

		PluginInfo info;
		info.filePath = dllPath;
		info.name = pluginName;

		if (LoadSinglePlugin(dllPath, info))
		{
			m_plugins[pluginName] = std::move(info);
			ConsolePrintln(L"Plugin successfully added to map: " + pluginName + L", total plugins: " + std::to_wstring(m_plugins.size()));
			return true;
		}

		ConsolePrintln(L"LoadSinglePlugin failed for: " + pluginName);
		return false;
	}

	bool UnloadPlugin(const std::wstring& pluginName)
	{
		auto it = m_plugins.find(pluginName);
		if (it == m_plugins.end())
		{
			return false;
		}

		ClearActions(pluginName);
		UnloadSinglePlugin(it->second);
		m_plugins.erase(it);

		NotifyActionsChanged();
		return true;
	}

	void LoadAllPlugins(const std::string& pluginDir)
	{
		ConsolePrintln("LoadAllPlugins start, dir: " + pluginDir);

		if (!std::filesystem::exists(pluginDir))
		{
			ConsolePrintln("Plugin directory does not exist: " + pluginDir);
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

		// 手动刷新所有插件的actions，确保新加载的actions被正确添加
		RefreshAllActions();

		NotifyActionsChanged();
	}

	void UnloadAllPlugins()
	{
		for (auto& pair : m_plugins)
		{
			UnloadSinglePlugin(pair.second);
		}
		m_plugins.clear();
		m_allActions.clear();
		NotifyActionsChanged();
	}

	void RefreshAllActions()
	{
		ConsolePrintln(L"RefreshAllActions start, clearing existing actions...");
		m_allActions.clear();
		m_suppressNotifications = true; // 暂时禁用通知

		for (auto& pair : m_plugins)
		{
			if (pair.second.loaded && pair.second.plugin)
			{
				ConsolePrintln(L"Loading actions from plugin: " + pair.first);
				pair.second.plugin->LoadActions();
				ConsolePrintln(L"After loading, total actions: " + std::to_wstring(m_allActions.size()));
			}
		}

		m_suppressNotifications = false; // 重新启用通知
		ConsolePrintln(L"RefreshAllActions complete - plugin size:" + std::to_wstring(m_plugins.size()) + L", total actions: " + std::to_wstring(m_allActions.size()));
		NotifyActionsChanged();
	}

	void DispatchUserInput(const std::wstring& input)
	{
		size_t oldActionCount = m_allActions.size();
		m_suppressNotifications = true; // 防止在用户输入过程中触发递归刷新

		for (auto& pair : m_plugins)
		{
			if (pair.second.loaded && pair.second.plugin)
			{
				pair.second.plugin->OnUserInput(input);
			}
		}

		m_suppressNotifications = false; // 重新启用通知

		// 如果动作数量发生变化，说明插件添加了新动作，需要通知主程序
		if (m_allActions.size() != oldActionCount)
		{
			ConsolePrintln(L"DispatchUserInput detected action count change: " +
						   std::to_wstring(oldActionCount) + L" -> " +
						   std::to_wstring(m_allActions.size()));
			NotifyActionsChanged(); // 只通知，不重新刷新
		}
	}

	void DispatchActionExecute(const std::string& actionId)
	{
		for (auto& pair : m_plugins)
		{
			if (pair.second.loaded && pair.second.plugin)
			{
				pair.second.plugin->OnActionExecute(actionId);
			}
		}
	}

	std::vector<std::wstring> GetLoadedPlugins() const
	{
		std::vector<std::wstring> result;
		for (const auto& pair : m_plugins)
		{
			if (pair.second.loaded)
			{
				result.push_back(pair.first);
			}
		}
		return result;
	}

	std::vector<SimpleActionItem> GetAllActions() const
	{
		return m_allActions;
	}

	std::vector<SimpleActionItem> GetPluginActions(const std::wstring& pluginName) const
	{
		std::vector<SimpleActionItem> result;
		for (const auto& action : m_allActions)
		{
			if (action.id.find(pluginName + L"_") == 0)
			{
				result.push_back(action);
			}
		}
		return result;
	}

	void SetActionsChangedCallback(std::function<void()> callback)
	{
		m_onActionsChanged = callback;
	}

	void AddAction(const SimpleActionItem& item) override
	{
		auto it = std::find_if(m_allActions.begin(), m_allActions.end(),
								[&](const SimpleActionItem& action)
								{
									return action.id == item.id;
								});

		if (it != m_allActions.end())
		{
			*it = item;
		}
		else
		{
			m_allActions.push_back(item);
		}

		NotifyActionsChanged();
	}

	void RemoveAction(const std::wstring& itemId) override
	{
		m_allActions.erase(
			std::remove_if(m_allActions.begin(), m_allActions.end(),
							[&](const SimpleActionItem& item)
							{
								return item.id == itemId;
							}),
			m_allActions.end());

		NotifyActionsChanged();
	}

	void UpdateAction(const SimpleActionItem& item) override
	{
		AddAction(item);
	}

	void ClearActions(const std::wstring& pluginName) override
	{
		m_allActions.erase(
			std::remove_if(m_allActions.begin(), m_allActions.end(),
							[&](const SimpleActionItem& item)
							{
								return item.id.find(pluginName + L"_") == 0;
							}),
			m_allActions.end());

		NotifyActionsChanged();
	}

	void ShowMessage(const std::string& title, const std::string& message) override
	{
		MessageBoxA(nullptr, message.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
	}

private:
	bool LoadSinglePlugin(const std::wstring& dllPath, PluginInfo& info)
	{
		ConsolePrintln(L"LoadSinglePlugin start: " + dllPath);

		info.handle = LoadLibraryW(dllPath.c_str());
		if (!info.handle)
		{
			ConsolePrintln(L"LoadLibraryA failed for: " + dllPath + L", Error: " + std::to_wstring(GetLastError()));
			return false;
		}
		ConsolePrintln(L"LoadLibraryA success for: " + dllPath);

		info.createFunc = (CreateSimplePluginFunc)GetProcAddress(info.handle, "CreateSimplePlugin");
		info.destroyFunc = (DestroySimplePluginFunc)GetProcAddress(info.handle, "DestroySimplePlugin");
		info.getVersionFunc = (GetSimplePluginApiVersionFunc)GetProcAddress(info.handle, "GetSimplePluginApiVersion");

		if (!info.createFunc || !info.destroyFunc)
		{
			ConsolePrintln("GetProcAddress failed - createFunc: " + std::to_string(info.createFunc != nullptr) +
						   ", destroyFunc: " + std::to_string(info.destroyFunc != nullptr));
			FreeLibrary(info.handle);
			info.handle = nullptr;
			return false;
		}
		ConsolePrintln("GetProcAddress success");

		ISimplePlugin* pluginPtr = info.createFunc();
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

		info.version = info.plugin->GetVersion();
		info.loaded = true;

		info.plugin->LoadActions();
		ConsolePrintln(L"LoadSinglePlugin complete: " + dllPath);

		return true;
	}

	void UnloadSinglePlugin(PluginInfo& info)
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

	std::string GetPluginNameFromPath(const std::string& dllPath)
	{
		std::filesystem::path path(dllPath);
		return path.stem().string();
	}

	std::wstring GetPluginNameFromPathW(const std::wstring& dllPath)
	{
		std::filesystem::path path(dllPath);
		return path.stem().wstring();
	}

	void NotifyActionsChanged()
	{
		if (m_suppressNotifications) {
			return; // 如果通知被抑制，直接返回
		}
		if (m_onActionsChanged)
		{
			m_onActionsChanged();
		}
	}
};
