#include "AirPodsPluginData.hpp"
#include "../../util/BitmapUtil.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cwctype>
#include <memory>

#include "AirPodsAction.hpp"
#include "AirPodsBatteryService.hpp"
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

	// 图标文件缺失时 SHGetFileInfo 会返回系统图标列表的 0 号（空白图标），所以显式返回 -1，
	// 让条目回退到 iconPath（蓝牙图标）
	int IconIndexForFile(const std::wstring& path) {
		if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
			Loge(L"AirPodsPlugin", L"Icon file not found: ", path);
			return -1;
		}
		return GetSysImageIndex(path);
	}

	std::wstring DisplayName(const AirPodsBatteryState& state) {
		if (!state.modelName.empty() && state.modelName != L"Apple headphones") {
			return state.modelName;
		}
		if (!state.deviceName.empty()) return state.deviceName;
		return L"Apple headphones";
	}
}

class AirPodsPlugin final : public candy::Plugin {
public:
	bool Initialize() override {
		m_host = &GetHost();
		s_instance = this;

		const std::wstring path = GetExecutableFolder() + LR"(\plugins\icon\)";

		icon_normal = IconIndexForFile(path + LR"(ic_airpods_normal.ico)");
		icon_charge = IconIndexForFile(path + LR"(ic_airpods_charge.ico)");
		icon_charge_case = IconIndexForFile(path + LR"(ic_airpods_charge_case.ico)");
		icon_disconnect = IconIndexForFile(path + LR"(ic_airpods_disconnected.ico)");
		return true;
	}

	void RefreshActions() override {
		// Bluetooth work is activated lazily when the keyword is used.
	}

	void Shutdown() override {
		mainWindowVisible = false;
		Deactivate();
		results.ClearLocal();
		s_instance = nullptr;
		m_host = nullptr;
	}

	void OnMainWindowShow(const bool isShow) override {
		mainWindowVisible = isShow;
		if (!isShow) Deactivate();
	}

	// Called on the UI thread for every input change (no trigger keywords on purpose), so it
	// also tells us when the keyword is gone and Bluetooth should be released.
	void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override {
		if (TrimAndLower(input) != L"airpods") {
			Deactivate();
			return;
		}
		Activate();
		results.Publish(GetHost(), BuildResults());
		sink.AddAll(results.Id());
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring&) override {
		const auto airPodsAction = results.Find(userData);
		if (!airPodsAction) return false;
		GetHost().ShowMessage(L"AirPods Battery", airPodsAction->details);
		return true;
	}

	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override {
		const auto airPodsAction = results.Find(userData);
		if (!airPodsAction || airPodsAction->details.empty()) return false;
		text = airPodsAction->details;
		return true;
	}

private:
	static constexpr UINT StaleRefreshIntervalMs = 1000;

	std::vector<std::shared_ptr<AirPodsAction>> BuildResults() {
		const auto state = service.GetState();
		auto action = std::make_shared<AirPodsAction>();
		action->matchText = L"airpods";
		action->matchTextVerbatim = true;
		action->stableKey = L"airpods";
		action->iconPath = BluetoothIconPath();

		if (!state.connected && !state.ready && state.error.empty()) {
			action->title = L"Looking for Apple headphones…";
			action->subtitle = L"Checking connected Bluetooth devices";
			action->iconIndex = icon_disconnect;
		} else if (!state.connected) {
			action->title = L"No connected Apple headphones";
			action->subtitle = state.error.empty()
				? L"Connect your AirPods or supported Beats headphones via Bluetooth"
				: L"Bluetooth error: " + state.error;
			action->iconIndex = icon_disconnect;
		} else if (!state.left && !state.right && !state.caseBox) {
			action->title = DisplayName(state);
			action->subtitle = L"Connected · waiting for a battery advertisement";
			action->iconIndex = icon_normal;
		} else {
			action->title = DisplayName(state) + L" · L " +
				BatteryText(state.left, state.leftCharging) + L" · R " +
				BatteryText(state.right, state.rightCharging);
			action->subtitle = L"Connected";
			if (!state.deviceName.empty() && state.deviceName != DisplayName(state)) {
				action->subtitle += L" · " + state.deviceName;
			}
			if (state.leftCharging || state.rightCharging) {
				action->iconIndex = icon_charge;
			}else {
				action->iconIndex = icon_normal;
			}
		}

		action->details = action->title + L"\n" + action->subtitle;
		std::vector<std::shared_ptr<AirPodsAction>> items{action};

		if (state.connected && (state.caseBox || state.caseCharging)) {
			auto caseAction = std::make_shared<AirPodsAction>();
			caseAction->matchText = L"airpods case";
			caseAction->matchTextVerbatim = true;
			caseAction->stableKey = L"airpods case";
			caseAction->iconPath = action->iconPath;
			caseAction->title = L"Charging Case · " +
				BatteryText(state.caseBox, state.caseCharging);
			caseAction->subtitle = DisplayName(state);
			caseAction->details = caseAction->title + L"\n" + caseAction->subtitle;
			caseAction->iconIndex = icon_charge_case;
			items.push_back(caseAction);
		}

		return items;
	}

	// Runs on Bluetooth threads and from the stale timer: Publish is any-thread, and the host
	// rebuilds the visible list from the current input (which goes through OnInterceptInput again,
	// so results only show while the keyword is still typed). Bursts are coalesced by the host.
	void PublishIfActive() {
		if (!m_host || !active || !mainWindowVisible) return;
		results.Publish(GetHost(), BuildResults());
		GetHost().NotifyActionsChanged();
	}

	// Window-less thread timer on the UI thread, dispatched by the host message loop.
	static void CALLBACK StaleRefreshTimerProc(HWND, UINT, const UINT_PTR timerId, DWORD) {
		if (s_instance && s_instance->staleTimerId == timerId) s_instance->PublishIfActive();
	}

	void Activate() {
		if (active || !mainWindowVisible) return;

		service.SetOnChanged([this] {
			PublishIfActive();
		});
		service.Start();
		// Only needed so battery values disappear once they go stale.
		staleTimerId = SetTimer(nullptr, 0, StaleRefreshIntervalMs, StaleRefreshTimerProc);
		active = true;
	}

	void Deactivate() {
		if (!active) return;
		active = false;
		if (staleTimerId) {
			KillTimer(nullptr, staleTimerId);
			staleTimerId = 0;
		}
		service.Stop();
	}

	// 0 号集合不参与全局搜索，只在输入 airpods 时作为结果列表显示
	candy::ActionSet<AirPodsAction> results{0, 0u};
	AirPodsBatteryService service;
	// Only touched on the UI thread.
	UINT_PTR staleTimerId = 0;
	std::atomic<bool> mainWindowVisible{false};
	std::atomic<bool> active{false};
	inline static AirPodsPlugin* s_instance = nullptr;
};

CANDY_PLUGIN_EXPORT(AirPodsPlugin, L"AirPods Battery",
					L"Type airpods to show the battery level of connected Apple headphones", L"1.1.0",
					L"com.candytek.airpodsbatteryplugin", nullptr)
