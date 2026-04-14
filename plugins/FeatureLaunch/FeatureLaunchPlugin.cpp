#include "../Plugin.hpp"
#include <memory>

#include "FeatureLaunchAction.hpp"
#include "FeatureLaunchPluginData.hpp"

class FeatureLaunchPlugin : public IPlugin {
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;

public:
	FeatureLaunchPlugin() = default;
	~FeatureLaunchPlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"CandyLauncher 常用跳转";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.featurelaunchplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"CandyLauncher 的常用跳转项";
	}

	bool Initialize(IPluginHost* host) override {
		m_host = host;
		return m_host != nullptr;
	}

	void OnPluginIdChange(const uint16_t pluginId) override {
		m_pluginId = pluginId;
	}

	void RefreshAllActions() override {
		if (!m_host) return;

		allPluginActions.clear();

		allPluginActions.push_back(std::make_shared<FeatureLaunchAction>(L"退出软件", L"退出本软件", BaseLaunchAction::LOADTYPE_SHELL32, 131,
																m_host->GetAppLaunchActionCallbacks().at("quit")));

		allPluginActions.push_back(std::make_shared<FeatureLaunchAction>(L"重启软件", L"重启本软件", BaseLaunchAction::LOADTYPE_SHELL32, 238,
																m_host->GetAppLaunchActionCallbacks().at("restart")));
		allPluginActions.push_back(
			std::make_shared<FeatureLaunchAction>(L"查看用户软件配置 JSON 文件", L"打开user_settings.json文件", BaseLaunchAction::LOADTYPE_SHELL32, 264,
												m_host->GetAppLaunchActionCallbacks().at("editUserConfig")));
		allPluginActions.push_back(
			std::make_shared<FeatureLaunchAction>(L"查看所有软件配置 JSON 文件", L"打开settings.json文件", BaseLaunchAction::LOADTYPE_SHELL32, 69,
												m_host->GetAppLaunchActionCallbacks().at("viewAllSettings")));
		// allExampleActions.push_back(
		// 	std::make_shared<NormalLaunchAction>(L"查看用户索引配置 JSON 文件", L"打开config_folder_plugin.json文件", IDI_CLOSE,
		// 									m_host->GetAppLaunchActionCallbacks().at("viewIndexConfig")));
		allPluginActions.push_back(std::make_shared<FeatureLaunchAction>(L"软件帮助文档", L"前往浏览器查看帮助文档", BaseLaunchAction::LOADTYPE_SHELL32, 154,
																m_host->GetAppLaunchActionCallbacks().at("openHelp")));

		if (m_host->GetSettingsMap().at("com.candytek.featurelaunchplugin.moreitem").boolValue) {
			allPluginActions.push_back(std::make_shared<FeatureLaunchAction>(L"检查更新", L"前往浏览器检查更新", BaseLaunchAction::LOADTYPE_SHELL32, 249,
																	m_host->GetAppLaunchActionCallbacks().at("checkUpdate")));
			allPluginActions.push_back(std::make_shared<FeatureLaunchAction>(L"反馈建议", L"前往浏览器反馈建议", BaseLaunchAction::LOADTYPE_SHELL32, 220,
																	m_host->GetAppLaunchActionCallbacks().at("feedback")));
			allPluginActions.push_back(std::make_shared<FeatureLaunchAction>(L"打开Github项目主页", L"前往浏览器浏览项目主页", BaseLaunchAction::LOADTYPE_SHELL32,
																	220,
																	m_host->GetAppLaunchActionCallbacks().at("openGithub")));
		}
	}

	std::wstring DefaultSettingJson() override {
		return LR"(
{
	"version": 1,
	"prefList": [
		{
			"key": "com.candytek.featurelaunchplugin.moreitem",
			"type": "bool",
			"title": "索引更多跳转项",
			"subPage": "plugin",
			"defValue": false
		}
	]
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		// auto it = settings_map.find("pref_pinyin_mode");
		// if (it != settings_map.end()) {
		// 	pinyinType = it->second.stringValue;
		// }
	}

	void Shutdown() override {
		if (m_host) {
			allPluginActions.clear();
		}
		m_host = nullptr;
	}

	std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() override {
		if (!m_host) return {};
		return allPluginActions;
	}

	void OnMainWindowShow(bool isShow) override {
	}


	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		auto action1 = std::dynamic_pointer_cast<FeatureLaunchAction>(action);
		if (!action1) return false;
		action1->Invoke();
		return true;
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new FeatureLaunchPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
