#include "../Plugin.hpp"
#include <memory>
#include <fstream>

#include "VisualStudioAction.hpp"
#include "VisualStudioPluginData.hpp"
#include "VisualStudioUtil.hpp"
#include "../../util/StringUtil.hpp"

class VisualStudioPlugin : public IPlugin {
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;
	std::wstring startStr = L"vs ";

public:
	VisualStudioPlugin() = default;
	~VisualStudioPlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"Visual Studio 项目";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.visualstudioplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"索引 Visual Studio 历史打开的项目和解决方案";
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
			Loge(L"VisualStudio Plugin", L"RefreshAllActions: m_host is null");
			return;
		}

		ConsolePrintln(L"VisualStudio Plugin", L"RefreshAllActions start");
		allPluginActions.clear();

		// 获取所有 Visual Studio 项目
		allPluginActions = GetAllVisualStudioProjects();

		ConsolePrintln(L"VisualStudio Plugin", L"Loaded " + std::to_wstring(allPluginActions.size()) + L" projects");
	}

	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.visualstudioplugin.start_str",
			"title": "直接激活命令（留空表示不需要前缀）",
			"type": "string",
			"subPage": "plugin",
			"defValue": "vs "
		},
		{
			"key": "com.candytek.visualstudioplugin.show_prerelease",
			"type": "bool",
			"title": "显示预发布版本的项目",
			"subPage": "plugin",
			"defValue": true
		},
		{
			"key": "com.candytek.visualstudioplugin.max_results",
			"type": "long",
			"title": "每个 VS 实例最大项目数",
			"subPage": "plugin",
			"defValue": 1000
		}
	]
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		startStr = utf8_to_wide(m_host->GetSettingsMap().at("com.candytek.visualstudioplugin.start_str").stringValue);
		showPrerelease = m_host->GetSettingsMap().at("com.candytek.visualstudioplugin.show_prerelease").boolValue;
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
		if (!startStr.empty() && StartsWith(input, startStr)) {
			// 单独显示本插件actions
			return m_host->GetSuccessfullyMatchingTextActions(input.substr(startStr.size()), allPluginActions);
		}
		// 由插件系统管理
		return {};
	}

	int OnSendHotKey(const std::shared_ptr<BaseAction> action, const UINT vk, const UINT currentModifiers, const WPARAM wparam) override {
		if (!m_host) return 0;
		if (currentModifiers == MOD_ALT && vk == 'A') {
			auto action1 = std::dynamic_pointer_cast<VisualStudioAction>(action);
			if (!action1) return 0;
			json j;
			j["arg"] = wide_to_utf8(action1->projectPath);
			m_host->ChangeEditTextText(utf8_to_wide(j.dump()) + m_host->GetEditTextText());
			return 1;
		}
		return 0;
	}


	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		auto action1 = std::dynamic_pointer_cast<VisualStudioAction>(action);
		if (!action1) return false;

		try {
			// 使用 Visual Studio 打开项目/解决方案
			ShellExecuteW(NULL, L"open", action1->visualStudioPath.c_str(),
						(L"\"" + action1->projectPath + L"\"").c_str(), NULL, SW_SHOWNORMAL);
		} catch (...) {
			return false;
		}

		return true;
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new VisualStudioPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
