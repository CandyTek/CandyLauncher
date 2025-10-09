#include "../Plugin.hpp"
#include <memory>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "PluginAction.hpp"
#include "PluginData.hpp"
#include "PluginUtil.hpp"
#include "../../util/StringUtil.hpp"

inline std::vector<std::shared_ptr<BaseAction>> allPluginActions;

class VSCodeWorkspacesPlugin : public IPlugin {
private:
	std::wstring startStr = L"vsc ";

public:
	VSCodeWorkspacesPlugin() = default;
	~VSCodeWorkspacesPlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"Visual Studio Code 工作区";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.vscodeworkspacesplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"Visual Studio Code 历史项目搜索插件";
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
		// Add to plugin actions
		allPluginActions = GetAllVSCodeWorkspaces();
	}


	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.vscodeworkspacesplugin.start_str",
			"title": "直接激活命令（留空表示不需要前缀）",
			"type": "string",
			"subPage": "plugin",
			"defValue": "vsc "
		},
		{
			"key": "com.candytek.vscodeworkspacesplugin.vscode_path",
			"title": "VSCode 软件路径",
			"type": "string",
			"subPage": "plugin",
			"defValue": ""
		}
	]
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		startStr = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.vscodeworkspacesplugin.start_str").stringValue);
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
			return m_host->GetSuccessfullyMatchingTextActions(input.substr(startStr.size()), allPluginActions);
		}
		// 由插件系统管理
		return {};
	}

	static std::wstring GetActionFileIconPath(const std::shared_ptr<PluginAction>& workspaceAction) {
		if (!workspaceAction->iconFilePath.empty() &&
			GetFileAttributesW(workspaceAction->iconFilePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
			return workspaceAction->iconFilePath;
		}
		return L"";
	}

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		const std::shared_ptr<PluginAction> workspaceAction = std::dynamic_pointer_cast<PluginAction>(action);
		if (!workspaceAction) return false;

		// Determine VSCode executable path
		std::wstring vscodePath;
		const std::wstring configuredPath = utf8_to_wide(
			m_host->GetSettingsMap().at("com.candytek.vscodeworkspacesplugin.vscode_path").stringValue);

		if (configuredPath.empty()) {
			vscodePath = GetActionFileIconPath(workspaceAction);
		} else {
			// 优先使用用户配置的路径
			wchar_t expandedPath[MAX_PATH];
			ExpandEnvironmentStringsW(configuredPath.c_str(), expandedPath, MAX_PATH);

			// 当用户配置的路径有误
			if (GetFileAttributesW(expandedPath) == INVALID_FILE_ATTRIBUTES) {
				vscodePath = GetActionFileIconPath(workspaceAction);
			} else {
				vscodePath = expandedPath;
			}
		}

		if (vscodePath.empty()) {
			m_host->ShowMessage(L"Error", L"找不到 vscode 程序路径，请前往设置进行配置");
			return false;
		}


		// Build command line arguments
		const std::wstring arguments = L"--folder-uri '" + workspaceAction->originalUri + L"'";
		const std::wstring arguments2 = L"-Command \".'" + vscodePath + L"' " + arguments + L"\"";

		PluginConsolePrintln(L"run", vscodePath + L" arg:" + arguments2);

		// vscode 毛病比较多，使用普通的ShellExecuteW 和 CreateProcessW 都会有很大的概率打不开
		HINSTANCE hInstance = ShellExecuteW(
			nullptr,
			L"open",
			L"powershell.exe",
			arguments2.c_str(),
			nullptr,
			SW_HIDE
		);

		if ((INT_PTR)hInstance > 32) {
			return true;
		} else {
			m_host->ShowMessage(L"Error", L"Failed to execute PowerShell command");
			return false;
		}
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new VSCodeWorkspacesPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
