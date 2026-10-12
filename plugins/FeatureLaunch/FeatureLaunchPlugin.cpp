#include <memory>

#include "FeatureLaunchAction.hpp"
#include "FeatureLaunchPluginData.hpp"

constexpr const wchar_t* OPEN_FOLDER_INDEXED_MANAGER_CALLBACK_KEY = L"openFolderIndexedManager";
constexpr const wchar_t* FOLDER_PLUGIN_PACKAGE_NAME = L"com.candytek.folderplugin";

class FeatureLaunchPlugin : public candy::Plugin {
private:
	candy::ActionSet<FeatureLaunchAction> actions{0};

	static std::shared_ptr<FeatureLaunchAction> Make(const wchar_t* title, const wchar_t* subTitle, const int icon,
													const wchar_t* command) {
		return std::make_shared<FeatureLaunchAction>(title, subTitle, icon, command);
	}

public:
	bool Initialize() override {
		m_host = &GetHost();
		return true;
	}

	void RefreshActions() override {
		const candy::Host& host = GetHost();
		std::vector<std::shared_ptr<FeatureLaunchAction>> items;
		items.push_back(Make(L"退出软件", L"退出本软件", 131, L"quit"));
		items.push_back(Make(L"重启软件", L"重启本软件", 238, L"restart"));
		items.push_back(Make(L"查看用户软件配置 JSON 文件", L"打开user_settings.json文件", 264, L"editUserConfig"));
		items.push_back(Make(L"查看所有软件配置 JSON 文件", L"打开settings.json文件", 69, L"viewAllSettings"));
		// 文件夹插件加载后会注册该命令
		if (host.IsPluginEnabled(FOLDER_PLUGIN_PACKAGE_NAME) && host.HasAppCommand(OPEN_FOLDER_INDEXED_MANAGER_CALLBACK_KEY)) {
			items.push_back(Make(L"文件夹索引管理器", L"打开文件夹插件的索引管理器", 4, OPEN_FOLDER_INDEXED_MANAGER_CALLBACK_KEY));
		}
		items.push_back(Make(L"软件帮助文档", L"前往浏览器查看帮助文档", 154, L"openHelp"));

		if (host.GetBool(L"com.candytek.featurelaunchplugin.moreitem", false)) {
			items.push_back(Make(L"检查更新", L"前往浏览器检查更新", 249, L"checkUpdate"));
			items.push_back(Make(L"反馈建议", L"前往浏览器反馈建议", 220, L"feedback"));
			items.push_back(Make(L"打开Github项目主页", L"前往浏览器浏览项目主页", 220, L"openGithub"));
		}
		actions.Publish(host, std::move(items));
	}

	void OnSettingChanged(const std::wstring& key) override {
		if (key == L"com.candytek.featurelaunchplugin.moreitem") RefreshActions();
	}

	void Shutdown() override {
		actions.ClearLocal();
		m_host = nullptr;
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto action = actions.Find(userData);
		if (!action) return false;
		return GetHost().InvokeAppCommand(action->commandKey.c_str());
	}
};

CANDY_PLUGIN_EXPORT(FeatureLaunchPlugin, L"CandyLauncher Shortcuts", L"Quick access to CandyLauncher settings and features", L"1.1.0",
					L"com.candytek.featurelaunchplugin", nullptr)
