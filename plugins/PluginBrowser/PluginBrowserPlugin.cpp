#include <memory>

#include "PluginBrowserAction.hpp"
#include "PluginBrowserPluginData.hpp"

namespace {
constexpr const wchar_t* PLUGIN_BROWSER_PACKAGE_NAME = L"com.candytek.pluginbrowserplugin";
}

class PluginBrowserPlugin : public candy::Plugin {
private:
	candy::ActionSet<PluginBrowserAction> actions{0};

	void ReloadActions() {
		const candy::Host& host = GetHost();
		std::vector<std::shared_ptr<PluginBrowserAction>> items;
		// 受限接口：只有本插件能枚举目录
		for (const auto& item : host.GetPluginCatalog()) {
			auto action = std::make_shared<PluginBrowserAction>();
			action->title = item.name.empty() ? item.packageName : item.name;
			action->packageName = item.packageName;
			action->stableKey = item.packageName;
			action->enabled = item.enabled;
			action->canToggle = item.packageName != PLUGIN_BROWSER_PACKAGE_NAME;
			// 2.3：目录里的名称 / 描述已按界面语言本地化
			action->subtitle = (item.enabled ? Tr(L"status.enabled", L"[Enabled] ") : Tr(L"status.disabled", L"[Disabled] "))
				+ (item.description.empty() ? item.packageName : item.description);
			action->subtitle += L" | " + (action->canToggle ? Tr(L"hint.toggle", L"Enter toggles the plugin")
														: Tr(L"hint.locked", L"The plugin browser cannot be disabled"));
			action->matchText = action->title + L" " + item.packageName;
			// 两种状态各共用一张图标
			action->iconKey = item.enabled ? L"enabled" : L"disabled";
			action->iconBitmap = LoadShell32IconAsBitmap(item.enabled ? 144 : 131);
			items.push_back(std::move(action));
		}
		actions.Publish(host, std::move(items));
	}

public:
	bool Initialize() override {
		g_pluginBrowserHost = &GetHost();
		return true;
	}

	void Shutdown() override {
		actions.ClearLocal();
		g_pluginBrowserHost = nullptr;
	}

	void RefreshActions() override {
		ReloadActions();
	}

	void OnUserSettingsLoaded() override {
		ReloadActions();
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto browserAction = actions.Find(userData);
		if (!browserAction || !browserAction->canToggle) {
			return false;
		}

		const bool nextEnabled = !browserAction->enabled;
		if (!GetHost().SetPluginEnabled(browserAction->packageName, nextEnabled)) {
			GetHost().ShowToast(Tr(L"plugin.name", L"Plugin Browser"), Tr(L"msg.toggle_failed", L"Failed to toggle the plugin"));
			return false;
		}
		return true;
	}
};

CANDY_PLUGIN_EXPORT(PluginBrowserPlugin, L"Plugin Browser", L"Browse installed plugins and toggle them with Enter", L"1.1.0",
					PLUGIN_BROWSER_PACKAGE_NAME, nullptr)
