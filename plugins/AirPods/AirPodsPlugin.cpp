#include "../Plugin.hpp"
#include "../../util/BitmapUtil.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cwctype>
#include <memory>

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
		Deactivate();
		if (notifyHwnd) {
			DestroyWindow(notifyHwnd);
			notifyHwnd = nullptr;
		}
		g_airPodsHost = nullptr;
	}

	void OnMainWindowShow(const bool isShow) override {
		mainWindowVisible = isShow;
		if (!isShow) Deactivate();
	}

	// Called on the UI thread for every input change, so it also tells us when the
	// keyword is gone and Bluetooth should be released.
	std::vector<std::shared_ptr<BaseAction>> InterceptInputShowResultsDirectly(
		const std::wstring& input) override {
		if (TrimAndLower(input) != L"airpods") {
			Deactivate();
			return {};
		}
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
	static constexpr UINT WM_AIRPODS_STATE_CHANGED = WM_APP + 0x1A9;
	static constexpr UINT_PTR StaleRefreshTimerId = 1;
	static constexpr UINT StaleRefreshIntervalMs = 1000;

	std::vector<std::shared_ptr<BaseAction>> BuildResults() {
		const auto state = service.GetState();
		auto action = std::make_shared<AirPodsAction>();
		action->matchText = L"airpods";
		action->iconFilePath = BluetoothIconPath();

		if (!state.connected && !state.ready && state.error.empty()) {
			action->title = L"Looking for Apple headphones…";
			action->subTitle = L"Checking connected Bluetooth devices";
			action->iconIndex = icon_disconnect;
		} else if (!state.connected) {
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
		if (!host || !active || !mainWindowVisible) return;
		if (TrimAndLower(host->GetEditTextText()) != L"airpods") return;

		auto results = BuildResults();
		host->ShowResultsDerectly(results);
	}

	static LRESULT CALLBACK NotifyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		auto* self = reinterpret_cast<AirPodsPlugin*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (self && (msg == WM_AIRPODS_STATE_CHANGED ||
			(msg == WM_TIMER && wParam == StaleRefreshTimerId))) {
			self->updatePending = false;
			self->PushResultsIfActive();
			return 0;
		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	// Message-only window owned by the UI thread: Bluetooth callbacks post to it so the
	// result list is only ever touched on the UI thread.
	bool EnsureNotifyWindow() {
		if (notifyHwnd) return true;
		const HINSTANCE instance = GetModuleHandleW(nullptr);
		static constexpr wchar_t className[] = L"CandyLauncherAirPodsNotify";
		WNDCLASSEXW wc{sizeof(wc)};
		wc.lpfnWndProc = NotifyWndProc;
		wc.hInstance = instance;
		wc.lpszClassName = className;
		RegisterClassExW(&wc);
		notifyHwnd = CreateWindowExW(0, className, L"", 0, 0, 0, 0, 0,
									HWND_MESSAGE, nullptr, instance, nullptr);
		if (!notifyHwnd) return false;
		SetWindowLongPtrW(notifyHwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
		return true;
	}

	void Activate() {
		if (active || !mainWindowVisible) return;
		if (!EnsureNotifyWindow()) return;

		const HWND hwnd = notifyHwnd;
		service.SetOnChanged([this, hwnd] {
			// Coalesce bursts of Bluetooth events into a single repaint.
			if (!updatePending.exchange(true)) PostMessageW(hwnd, WM_AIRPODS_STATE_CHANGED, 0, 0);
		});
		service.Start();
		// Only needed so battery values disappear once they go stale.
		SetTimer(notifyHwnd, StaleRefreshTimerId, StaleRefreshIntervalMs, nullptr);
		active = true;
	}

	void Deactivate() {
		if (!active) return;
		active = false;
		if (notifyHwnd) KillTimer(notifyHwnd, StaleRefreshTimerId);
		service.Stop();
		updatePending = false;
	}

	AirPodsBatteryService service;
	HWND notifyHwnd = nullptr;
	std::atomic<bool> mainWindowVisible{false};
	std::atomic<bool> updatePending{false};
	// Only touched on the UI thread.
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
