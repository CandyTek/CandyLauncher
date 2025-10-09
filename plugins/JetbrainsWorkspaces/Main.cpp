#include "../Plugin.hpp"
#include <memory>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "PluginAction.hpp"
#include "PluginData.hpp"
#include "PluginUtil.hpp"
#include "../../util/StringUtil.hpp"
#include "util/MyJsonUtil.hpp"

inline std::vector<std::shared_ptr<BaseAction>> allPluginActions;

class JetbrainsWorkspacesPlugin : public IPlugin {
private:
	std::wstring startStr = L"jb ";

public:
	JetbrainsWorkspacesPlugin() = default;
	~JetbrainsWorkspacesPlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"Jetbrains Workspaces";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.jetbrainsworkspacesplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"Jetbrains 家族历史项目搜索插件";
	}

	bool Initialize(IPluginHost* host) override {
		m_host = host;
		return m_host != nullptr;
	}

	void OnPluginIdChange(const uint16_t pluginId) override {
		m_pluginId = pluginId;
	}

	void RefreshAllActions() override {
		if (!m_host) {
			return;
		}

		allPluginActions.clear();

		// Get all JetBrains workspace projects
		auto workspaces = GetAllJetBrainsWorkspaces();

		// Add to plugin actions
		allPluginActions.reserve(workspaces.size());
		for (auto& workspace : workspaces) {
			allPluginActions.emplace_back(std::move(workspace));
		}
	}


	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.jetbrainsworkspacesplugin.start_str",
			"title": "直接激活命令（留空表示不需要前缀）",
			"type": "string",
			"subPage": "plugin",
			"defValue": "jb "
		},
		{
			"key": "com.candytek.jetbrainsworkspacesplugin.edit_config_ide_paths",
			"title": "编辑 IDE 路径配置",
			"type": "button",
			"subPage": "plugin",
			"defValue": ""
		}
	]
}

   )";
	}

	void OnSettingItemExecute(const SettingItem* setting, HWND parentHwnd) override {
		if (setting->key == "com.candytek.jetbrainsworkspacesplugin.edit_config_ide_paths") {
			wchar_t path[MAX_PATH];
			wcsncpy_s(path, CONFIG_IDE_PATHS_PATH.c_str(), MAX_PATH - 1);
			ShellExecute(nullptr, L"open", L"notepad.exe", path, nullptr, SW_SHOW);
		}
	}

	void OnUserSettingsLoadDone() override {
		startStr = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.jetbrainsworkspacesplugin.start_str").stringValue);
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
		if (!startStr.empty() && MyStartsWith2(input, startStr)) {
			// 单独显示本插件actions
			return m_host->GetSuccessfullyMatchingTextActions(input.substr(startStr.size()),allPluginActions);
		}
		// 由插件系统管理
		return {};
	}
	
	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		auto workspaceAction = std::dynamic_pointer_cast<PluginAction>(action);
		if (!workspaceAction) return false;

		// 执行时用指定软件打开项目位置
		// The subTitle contains the project path

		if (const std::wstring projectPath = workspaceAction->subTitle; !projectPath.empty()) {
			m_host->ChangeEditTextText(utf8_to_wide(makeCompactJsonWithoutBraces(workspaceAction->subTitle))+L" "+workspaceAction->ideName);
		}
		return false;
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new JetbrainsWorkspacesPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
