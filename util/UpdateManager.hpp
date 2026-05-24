#pragma once

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <commctrl.h>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

#include "../3rdparty/github-releases-autoupdater/src/cautoupdatergithub.h"
#include "../common/GlobalState.hpp"
#include "FileUtil.hpp"
#include "LogUtil.hpp"
#include "MyToastUtil.hpp"
#include "StringUtil.hpp"

constexpr UINT WM_APP_UPDATE_AVAILABLE = WM_APP + 201;
constexpr UINT WM_APP_UPDATE_ERROR = WM_APP + 202;
constexpr UINT WM_APP_UPDATE_DOWNLOAD_FINISHED = WM_APP + 203;

namespace AppUpdate {

constexpr const char* kGithubRepoName = "CandyTek/CandyLauncher";
constexpr const char* kIgnoredUpdateVersionKey = "pref_ignored_update_version";
constexpr int kUpdateDialogButtonUpdate = 1001;
constexpr int kUpdateDialogButtonIgnoreVersion = 1002;
constexpr int kUpdateDialogButtonDisableAutoPrompt = 1003;
constexpr int kUpdateDialogButtonLater = 1004;

inline std::atomic_bool g_isCheckingForUpdate = false;
inline std::atomic_bool g_isDownloadingUpdate = false;
inline std::atomic_bool g_hasScheduledStartupCheck = false;

inline nlohmann::json LoadUserSettingsJson() {
	const std::string text = ReadUtf8File(USER_SETTINGS_PATH);
	if (text.empty()) {
		return nlohmann::json::object();
	}

	try {
		nlohmann::json config = nlohmann::json::parse(text);
		if (config.is_object()) {
			return config;
		}
	} catch (const std::exception& e) {
		Loge(L"Update", L"Failed to parse user_settings.json while saving update preference", e.what());
	}

	return nlohmann::json::object();
}

inline void SaveUserSettingsJson(const nlohmann::json& config) {
	namespace fs = std::filesystem;
	fs::path path(USER_SETTINGS_PATH);
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if (!output) {
		Loge(L"Update", L"Failed to open user_settings.json for writing update preference");
		return;
	}

	output << config.dump(4) << std::endl;
}

inline std::string GetIgnoredUpdateVersion() {
	const nlohmann::json config = LoadUserSettingsJson();
	if (!config.contains(kIgnoredUpdateVersionKey) || !config[kIgnoredUpdateVersionKey].is_string()) {
		return "";
	}
	return config[kIgnoredUpdateVersionKey].get<std::string>();
}

inline void SaveIgnoredUpdateVersion(const std::string& versionString) {
	nlohmann::json config = LoadUserSettingsJson();
	config[kIgnoredUpdateVersionKey] = versionString;
	SaveUserSettingsJson(config);
	ConsolePrintln(L"Update", L"Ignored update version: v" + utf8_to_wide(versionString));
}

inline void SetSettingBoolInList(std::vector<SettingItem>& settings, const std::string& key, const bool value) {
	for (SettingItem& setting : settings) {
		if (setting.key == key) {
			setting.boolValue = value;
		}
		if (!setting.children.empty()) {
			SetSettingBoolInList(setting.children, key, value);
		}
	}
}

inline void DisableAutomaticUpdatePrompts() {
	nlohmann::json config = LoadUserSettingsJson();
	config["pref_check_app_version"] = false;
	SaveUserSettingsJson(config);

	auto it = g_settings_map.find("pref_check_app_version");
	if (it != g_settings_map.end()) {
		it->second.boolValue = false;
	}
	SetSettingBoolInList(g_settings_ui_last_save, "pref_check_app_version", false);
	SetSettingBoolInList(g_settings_ui, "pref_check_app_version", false);
	ConsolePrintln(L"Update", L"Automatic update prompt disabled");
}

struct UpdateAvailablePayload {
	bool manual = false;
	CAutoUpdaterGithub::ChangeLog changelog;
};

struct UpdateErrorPayload {
	bool manual = false;
	std::string errorMessage;
};

struct UpdateFinishedPayload {
	bool manual = false;
	std::string versionString;
};

inline std::string ExtractCurrentVersionString() {
	for (const auto& item : g_settings_ui_last_save) {
		if (item.key != "pref_version_name") continue;

		const std::string title = item.title;
		size_t versionPos = title.find('V');
		if (versionPos == std::string::npos) {
			versionPos = title.find('v');
		}
		if (versionPos == std::string::npos) continue;

		size_t endPos = title.find('(', versionPos);
		if (endPos == std::string::npos) {
			endPos = title.size();
		}

		std::string version = MyTrim(title.substr(versionPos + 1, endPos - versionPos - 1));
		if (!version.empty()) {
			return version;
		}
	}

	return "0.0.0";
}

inline std::wstring BuildUpdateSummaryText(const CAutoUpdaterGithub::ChangeLog& changelog) {
	if (changelog.empty()) {
		return L"";
	}

	const auto& latest = changelog.front();
	std::wstring text = L"New version found: v" + utf8_to_wide(latest.versionString);
	if (!latest.date.empty()) {
		text += L"\r\nPublished: " + utf8_to_wide(latest.date);
	}
	text += L"\r\n\r\nCandyLauncher will download and launch the installer.";

	if (changelog.size() > 1) {
		text += L"\r\n\r\nDetected versions:";
		const size_t count = std::min<size_t>(changelog.size(), 3);
		for (size_t i = 0; i < count; ++i) {
			text += L"\r\n- v" + utf8_to_wide(changelog[i].versionString);
			if (!changelog[i].date.empty()) {
				text += L" (" + utf8_to_wide(changelog[i].date) + L")";
			}
		}
	}

	text += L"\r\n\r\nStart update now?";
	return text;
}

inline int ShowAutomaticUpdateDialog(const CAutoUpdaterGithub::ChangeLog& changelog) {
	const std::wstring content = BuildUpdateSummaryText(changelog);
	const TASKDIALOG_BUTTON buttons[] = {
		{kUpdateDialogButtonUpdate, LR"(立即更新)"},
		{kUpdateDialogButtonIgnoreVersion, LR"(忽略当前版本)"},
		{kUpdateDialogButtonDisableAutoPrompt, LR"(不再提示更新)"},
		{kUpdateDialogButtonLater, LR"(稍后提醒)"}
	};

	TASKDIALOGCONFIG config{};
	config.cbSize = sizeof(config);
	config.hwndParent = g_mainHwnd;
	config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
	config.dwCommonButtons = 0;
	config.pszWindowTitle = L"Update";
	config.pszMainIcon = TD_INFORMATION_ICON;
	config.pszMainInstruction = L"\u53d1\u73b0\u65b0\u7248\u672c";
	config.pszContent = content.c_str();
	config.cButtons = static_cast<UINT>(std::size(buttons));
	config.pButtons = buttons;
	config.nDefaultButton = kUpdateDialogButtonUpdate;

	int selectedButton = kUpdateDialogButtonLater;
	const HRESULT hr = TaskDialogIndirect(&config, &selectedButton, nullptr, nullptr);
	if (SUCCEEDED(hr)) {
		return selectedButton;
	}

	const int fallbackResult = MessageBoxW(
		g_mainHwnd,
		content.c_str(),
		L"Update",
		MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
	return fallbackResult == IDYES ? kUpdateDialogButtonUpdate : kUpdateDialogButtonLater;
}

class UpdateStatusListener final : public CAutoUpdaterGithub::UpdateStatusListener {
public:
	explicit UpdateStatusListener(const bool isManual) : _manual(isManual) {
	}

	void onUpdateAvailable(const CAutoUpdaterGithub::ChangeLog& changelog) override {
		auto* payload = new UpdateAvailablePayload{_manual, changelog};
		if (!PostMessageW(g_mainHwnd, WM_APP_UPDATE_AVAILABLE, 0, reinterpret_cast<LPARAM>(payload))) {
			delete payload;
		}
	}

	void onUpdateDownloadProgress(float percentageDownloaded) override {
		ConsolePrintln(L"Update", L"Download progress: " + std::to_wstring(static_cast<int>(percentageDownloaded)) + L"%");
	}

	void onUpdateDownloadFinished() override {
		auto* payload = new UpdateFinishedPayload{_manual, _targetVersion};
		if (!PostMessageW(g_mainHwnd, WM_APP_UPDATE_DOWNLOAD_FINISHED, 0, reinterpret_cast<LPARAM>(payload))) {
			delete payload;
		}
	}

	void onUpdateError(const std::string& errorMessage) override {
		auto* payload = new UpdateErrorPayload{_manual, errorMessage};
		if (!PostMessageW(g_mainHwnd, WM_APP_UPDATE_ERROR, 0, reinterpret_cast<LPARAM>(payload))) {
			delete payload;
		}
	}

	void setTargetVersion(const std::string& version) {
		_targetVersion = version;
	}

private:
	bool _manual = false;
	std::string _targetVersion;
};

inline void StartDownloadUpdate(const std::string& updateUrl, const std::string& versionString, const bool manual) {
	if (updateUrl.empty()) {
		MessageBoxW(g_mainHwnd, L"No supported update package was found in the GitHub release.", L"Update", MB_OK | MB_ICONWARNING | MB_TOPMOST);
		return;
	}

	bool expected = false;
	if (!g_isDownloadingUpdate.compare_exchange_strong(expected, true)) {
		if (manual) {
			MessageBoxW(g_mainHwnd, L"An update installer is already downloading.", L"Update", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
		}
		return;
	}

	MyShowSimpleToast(L"CandyLauncher", L"Downloading update package");
	std::thread([updateUrl, versionString, manual]() {
		UpdateStatusListener listener(manual);
		listener.setTargetVersion(versionString);

		CAutoUpdaterGithub updater(kGithubRepoName, ExtractCurrentVersionString());
		updater.setUpdateStatusListener(&listener);
		updater.downloadAndInstallUpdate(updateUrl);
		g_isDownloadingUpdate = false;
	}).detach();
}

inline void StartCheckForUpdates(const bool manual) {
	if (g_isDownloadingUpdate.load()) {
		if (manual) {
			MessageBoxW(g_mainHwnd, L"An update installer is already downloading.", L"Update", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
		}
		return;
	}

	bool expected = false;
	if (!g_isCheckingForUpdate.compare_exchange_strong(expected, true)) {
		if (manual) {
			MessageBoxW(g_mainHwnd, L"Update check is already running.", L"Update", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
		}
		return;
	}

	ConsolePrintln(L"Update", L"Starting update check");
	std::thread([manual]() {
		UpdateStatusListener listener(manual);
		CAutoUpdaterGithub updater(kGithubRepoName, ExtractCurrentVersionString());
		updater.setUpdateStatusListener(&listener);
		updater.checkForUpdates();
		g_isCheckingForUpdate = false;
	}).detach();
}

inline void ScheduleStartupUpdateCheck() {
	if (!g_settings_map["pref_check_app_version"].boolValue) {
		return;
	}

	bool expected = false;
	if (!g_hasScheduledStartupCheck.compare_exchange_strong(expected, true)) {
		return;
	}

	std::thread([]() {
		std::this_thread::sleep_for(std::chrono::seconds(3));
		StartCheckForUpdates(false);
	}).detach();
}

inline LRESULT HandleUpdateAvailableMessage(LPARAM lParam) {
	std::unique_ptr<UpdateAvailablePayload> payload(reinterpret_cast<UpdateAvailablePayload*>(lParam));
	if (!payload) return 0;

	if (payload->changelog.empty()) {
		if (payload->manual) {
			MessageBoxW(g_mainHwnd, L"You are already using the latest version.", L"Update", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
		}
		return 0;
	}

	const auto& latest = payload->changelog.front();
	if (!payload->manual && latest.versionString == GetIgnoredUpdateVersion()) {
		ConsolePrintln(L"Update", L"Skipped ignored update version: v" + utf8_to_wide(latest.versionString));
		return 0;
	}

	if (latest.versionUpdateUrl.empty()) {
		MessageBoxW(g_mainHwnd, L"A newer version was found, but the release does not contain a supported update package.", L"Update", MB_OK | MB_ICONWARNING | MB_TOPMOST);
		return 0;
	}

	const int result = payload->manual
		? MessageBoxW(
			g_mainHwnd,
			BuildUpdateSummaryText(payload->changelog).c_str(),
			L"Update",
			MB_YESNO | MB_ICONQUESTION | MB_TOPMOST)
		: ShowAutomaticUpdateDialog(payload->changelog);

	if (payload->manual && result == IDYES) {
		StartDownloadUpdate(latest.versionUpdateUrl, latest.versionString, payload->manual);
	} else if (!payload->manual && result == kUpdateDialogButtonUpdate) {
		StartDownloadUpdate(latest.versionUpdateUrl, latest.versionString, payload->manual);
	} else if (!payload->manual && result == kUpdateDialogButtonIgnoreVersion) {
		SaveIgnoredUpdateVersion(latest.versionString);
	} else if (!payload->manual && result == kUpdateDialogButtonDisableAutoPrompt) {
		DisableAutomaticUpdatePrompts();
	}
	return 0;
}

inline LRESULT HandleUpdateErrorMessage(LPARAM lParam) {
	std::unique_ptr<UpdateErrorPayload> payload(reinterpret_cast<UpdateErrorPayload*>(lParam));
	if (!payload) return 0;

	Loge(L"Update", utf8_to_wide(payload->errorMessage));
	if (payload->manual) {
		MessageBoxW(
			g_mainHwnd,
			(L"Update check failed:\r\n" + utf8_to_wide(payload->errorMessage)).c_str(),
			L"Update",
			MB_OK | MB_ICONERROR | MB_TOPMOST);
	} else {
		MyShowSimpleToast(L"CandyLauncher", L"Automatic update check failed");
	}
	return 0;
}

inline LRESULT HandleUpdateDownloadFinishedMessage(LPARAM lParam) {
	std::unique_ptr<UpdateFinishedPayload> payload(reinterpret_cast<UpdateFinishedPayload*>(lParam));
	if (!payload) return 0;

	std::wstring msg = L"Update package started";
	if (!payload->versionString.empty()) {
		msg += L": v" + utf8_to_wide(payload->versionString);
	}
	MyShowSimpleToast(L"CandyLauncher", msg);
	if (payload->manual) {
		MessageBoxW(g_mainHwnd, msg.c_str(), L"Update", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
	}
	if (g_mainHwnd != nullptr) {
		DestroyWindow(g_mainHwnd);
	}
	return 0;
}

} // namespace AppUpdate
