#include "../Plugin.hpp"
#include "../../util/BitmapUtil.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cwctype>
#include <memory>
#include <mutex>
#include <thread>

#include "AirPodsAction.hpp"
#include "AirPodsBatteryService.hpp"
#include "AirPodsPluginData.hpp"
#include "util/FileUtil.hpp"

inline int icon_normal = 0;
inline int icon_charge = 0;
inline int icon_charge_case = 0;
inline int icon_disconnect = 0;

namespace {
	std::wstring BatteryText(const std::optional<int>& value, const bool charging) {
		if (!value) return L"--";
		return std::to_wstring(*value) + L"%" + (charging ? L" ⚡" : L"");
	}

	std::wstring TrimAndLower(std::wstring value) {
		const auto first = value.find_first_not_of(L" \t\r\n");
		if (first == std::wstring::npos) return {};
		const auto last = value.find_last_not_of(L" \t\r\n");
		value = value.substr(first, last - first + 1);
		std::transform(value.begin(), value.end(), value.begin(), towlower);
		return value;
	}

	std::wstring BluetoothIconPath() {
		wchar_t systemDirectory[MAX_PATH]{};
		if (GetSystemDirectoryW(systemDirectory, MAX_PATH) == 0) return {};
		return std::wstring(systemDirectory) + L"\\bthprops.cpl";
	}

	std::wstring DisplayName(const AirPodsBatteryState& state) {
		if (!state.modelName.empty() && state.modelName != L"Apple headphones") {
			return state.modelName;
		}
		if (!state.deviceName.empty()) return state.deviceName;
		return L"Apple headphones";
	}
}

class AirPodsPlugin final : public IPlugin {
public:
	std::wstring GetPluginName() const override {
		return L"AirPods Battery";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.airpodsbatteryplugin";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.1.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"Type airpods to show the battery level of connected Apple headphones";
	}

	bool Initialize(IPluginHost* host) override {
		g_airPodsHost = host;

		const std::wstring path = GetExecutableFolder() + LR"(\plugins\icon\)";
		
		icon_normal = GetSysImageIndex(path + LR"(ic_airpods_normal.ico)");
		icon_charge = GetSysImageIndex(path + LR"(ic_airpods_charge.ico)");
		icon_charge_case = GetSysImageIndex(path + LR"(ic_airpods_charge_case.ico)");
		icon_disconnect = GetSysImageIndex(path + LR"(ic_airpods_disconnected.ico)");
		return g_airPodsHost != nullptr;
	}

	void OnPluginIdChange(const uint16_t pluginId) override {
		g_airPodsPluginId = pluginId;
	}

	void RefreshAllActions() override {
		// Bluetooth work is activated lazily when the keyword is used.
	}

	void Shutdown() override {
		mainWindowVisible = false;
		keywordActive = false;
		Deactivate();
		g_airPodsHost = nullptr;
	}

	void OnMainWindowShow(const bool isShow) override {
		mainWindowVisible = isShow;
		if (isShow) {
			auto* host = g_airPodsHost;
			const bool hasKeyword =
				host && TrimAndLower(host->GetEditTextText()) == L"airpods";
			keywordActive = hasKeyword;
			if (hasKeyword) {
				Activate();
				PushResultsIfActive();
			}
		} else {
			keywordActive = false;
			Deactivate();
		}
	}

	void OnUserInput(const std::wstring& input) override {
		const bool hasKeyword =
			mainWindowVisible && TrimAndLower(input) == L"airpods";
		keywordActive = hasKeyword;
		if (hasKeyword) {
			Activate();
		} else {
			Deactivate();
		}
	}

	std::vector<std::shared_ptr<BaseAction>> InterceptInputShowResultsDirectly(
		const std::wstring& input) override {
		if (TrimAndLower(input) != L"airpods") return {};
		keywordActive = true;
		Activate();
		return BuildResults();
	}

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring&) override {
		if (!g_airPodsHost) return false;
		const auto airPodsAction = std::dynamic_pointer_cast<AirPodsAction>(action);
		if (!airPodsAction) return false;
		g_airPodsHost->ShowMessage(L"AirPods Battery", airPodsAction->details);
		return true;
	}

private:
	std::vector<std::shared_ptr<BaseAction>> BuildResults() {
		const auto state = service.GetState();
		auto action = std::make_shared<AirPodsAction>();
		action->matchText = L"airpods";
		action->iconFilePath = BluetoothIconPath();

		if (!state.connected) {
			action->title = L"No connected Apple headphones";
			action->subTitle = state.error.empty()
				? L"Connect your AirPods or supported Beats headphones via Bluetooth"
				: L"Bluetooth error: " + state.error;
			action->iconIndex = icon_disconnect;
		} else if (!state.left && !state.right && !state.caseBox) {
			action->title = DisplayName(state);
			action->subTitle = L"Connected · waiting for a battery advertisement";
			action->iconIndex = icon_normal;
		} else {
			action->title = DisplayName(state) + L" · L " +
				BatteryText(state.left, state.leftCharging) + L" · R " +
				BatteryText(state.right, state.rightCharging);
			action->subTitle = L"Connected";
			if (!state.deviceName.empty() && state.deviceName != DisplayName(state)) {
				action->subTitle += L" · " + state.deviceName;
			}
			if (state.leftCharging || state.rightCharging) {
				action->iconIndex = icon_charge;
			}else {
				action->iconIndex = icon_normal;
			}
		}

		action->details = action->title + L"\n" + action->subTitle;
		std::vector<std::shared_ptr<BaseAction>> results{action};

		if (state.connected && (state.caseBox || state.caseCharging)) {
			auto caseAction = std::make_shared<AirPodsAction>();
			caseAction->matchText = L"airpods case";
			caseAction->iconFilePath = action->iconFilePath;
			caseAction->title = L"Charging Case · " +
				BatteryText(state.caseBox, state.caseCharging);
			caseAction->subTitle = DisplayName(state);
			caseAction->details = caseAction->title + L"\n" + caseAction->subTitle;
			caseAction->iconIndex = icon_charge_case;
			results.push_back(caseAction);
		}

		return results;
	}

	void PushResultsIfActive() {
		auto* host = g_airPodsHost;
		if (!host || !mainWindowVisible || !keywordActive) return;
		if (TrimAndLower(host->GetEditTextText()) != L"airpods") return;

		auto results = BuildResults();
		host->ShowResultsDerectly(results);
	}

	void Activate() {
		std::lock_guard<std::mutex> lock(activationMutex);
		if (active || !mainWindowVisible || !keywordActive) return;

		service.Start();
		stopRefresh = false;
		refreshThread = std::thread([this] {
			while (!stopRefresh) {
				if (mainWindowVisible && keywordActive) {
					PushResultsIfActive();
				}
				for (int i = 0; i < 5 && !stopRefresh; ++i) {
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			}
		});
		active = true;
	}

	void Deactivate() {
		std::lock_guard<std::mutex> lock(activationMutex);
		if (!active) return;

		stopRefresh = true;
		if (refreshThread.joinable()) {
			refreshThread.join();
		}
		service.Stop();
		active = false;
	}

	AirPodsBatteryService service;
	std::thread refreshThread;
	std::mutex activationMutex;
	std::atomic<bool> mainWindowVisible{false};
	std::atomic<bool> keywordActive{false};
	std::atomic<bool> stopRefresh{false};
	bool active = false;
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new AirPodsPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
