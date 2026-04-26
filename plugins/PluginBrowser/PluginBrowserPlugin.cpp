#include "../Plugin.hpp"
#include <memory>

#include "PluginBrowserAction.hpp"
#include "PluginBrowserPluginData.hpp"

namespace {
constexpr const wchar_t* PLUGIN_BROWSER_PACKAGE_NAME = L"com.candytek.pluginbrowserplugin";
}

class PluginBrowserPlugin : public IPlugin {
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;

	void ReloadActions() {
		allPluginActions.clear();
		if (!g_pluginBrowserHost) {
			return;
		}

		const auto catalog = g_pluginBrowserHost->GetPluginCatalogEntries(g_pluginBrowserId);
		for (const auto& item : catalog) {
			auto action = std::make_shared<PluginBrowserAction>();
			action->title = item.name.empty() ? item.pkgName : item.name;
			action->packageName = item.pkgName;
			action->enabled = item.enabled;
			action->canToggle = item.pkgName != PLUGIN_BROWSER_PACKAGE_NAME;
			action->subTitle = (item.enabled ? L"[已启用] " : L"[已禁用] ") + item.pkgName;
			if (!action->canToggle) {
				action->subTitle += L" | 当前管理插件不允许关闭";
			} else {
				action->subTitle += L" | 回车切换开关";
			}
			action->matchText = g_pluginBrowserHost->GetTheProcessedMatchingText(action->title + L" " + item.pkgName);
			action->iconBitmap = LoadShell32IconAsBitmap(item.enabled ? 144 : 131);
			allPluginActions.push_back(action);
		}
	}

public:
	std::wstring GetPluginName() const override {
		return L"插件浏览器";
	}

	std::wstring GetPluginDescription() const override {
		return L"浏览现有插件并用回车切换启用状态";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginPackageName() const override {
		return PLUGIN_BROWSER_PACKAGE_NAME;
	}

	void OnPluginIdChange(uint16_t pluginId) override {
		g_pluginBrowserId = pluginId;
	}

	bool Initialize(IPluginHost* host) override {
		g_pluginBrowserHost = host;
		return g_pluginBrowserHost != nullptr;
	}

	void Shutdown() override {
		allPluginActions.clear();
		g_pluginBrowserHost = nullptr;
	}

	void RefreshAllActions() override {
		ReloadActions();
	}
	
	void OnUserSettingsLoadDone() override {
		ReloadActions();
	}

	std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() override {
		return allPluginActions;
	}

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!g_pluginBrowserHost) {
			return false;
		}

		const auto browserAction = std::dynamic_pointer_cast<PluginBrowserAction>(action);
		if (!browserAction) {
			return false;
		}

		if (!browserAction->canToggle) {
			// g_pluginBrowserHost->ShowSimpleToast(L"插件浏览器", L"当前管理插件不允许关闭");
			return false;
		}

		const bool nextEnabled = !browserAction->enabled;
		const bool ok = g_pluginBrowserHost->SetPluginEnabled(g_pluginBrowserId, browserAction->packageName, nextEnabled);
		if (!ok) {
			g_pluginBrowserHost->ShowSimpleToast(L"插件浏览器", L"切换插件状态失败");
			return false;
		}

		// g_pluginBrowserHost->ShowSimpleToast(
		// 	L"插件浏览器",
		// 	(nextEnabled ? L"已启用: " : L"已禁用: ") + browserAction->title);
		return true;
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new PluginBrowserPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
