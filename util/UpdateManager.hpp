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
#include "common/AppVersion.h"
#include "../common/GlobalState.hpp"
#include "../common/I18n.hpp"
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

inline std::wstring UpdateDialogTitle() {
	return LoadI18nString(IDS_I18N_UPDATE_TITLE, L"Update");
}
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
	Logi(L"Update", L"Ignored update version: v", versionString);
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
	Logi(L"Update", L"Automatic update prompt disabled");
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

// 当前程序版本，唯一来源是 CMakeLists.txt 的 project(VERSION)，经 AppVersion.h 生成
// 与 GitHub Release tag 比较时，tag 的 "v" 前缀会被 updater 去掉，因此 tag 应命名为 v1.2.0 这种形式
inline std::string ExtractCurrentVersionString() {
	return APP_VERSION_STRING;
}

inline std::wstring BuildUpdateSummaryText(const CAutoUpdaterGithub::ChangeLog& changelog) {
	if (changelog.empty()) {
		return L"";
	}

	const auto& latest = changelog.front();
	std::wstring text = LoadI18nString(IDS_I18N_UPDATE_NEW_VERSION_PREFIX, L"New version found: v") + utf8_to_wide(latest.versionString);
	if (!latest.date.empty()) {
		text += L"\r\n" + LoadI18nString(IDS_I18N_UPDATE_PUBLISHED_PREFIX, L"Published: ") + utf8_to_wide(latest.date);
	}
	text += L"\r\n\r\n" + LoadI18nString(IDS_I18N_UPDATE_WILL_DOWNLOAD, L"CandyLauncher will download and install the update package.");

	if (changelog.size() > 1) {
		text += L"\r\n\r\n" + LoadI18nString(IDS_I18N_UPDATE_DETECTED_VERSIONS, L"Detected versions:");
		const size_t count = std::min<size_t>(changelog.size(), 3);
		for (size_t i = 0; i < count; ++i) {
			text += L"\r\n- v" + utf8_to_wide(changelog[i].versionString);
			if (!changelog[i].date.empty()) {
				text += L" (" + utf8_to_wide(changelog[i].date) + L")";
			}
		}
	}

	text += L"\r\n\r\n" + LoadI18nString(IDS_I18N_UPDATE_START_NOW, L"Start update now?");
	return text;
}

inline int ShowAutomaticUpdateDialog(const CAutoUpdaterGithub::ChangeLog& changelog) {
	const std::wstring content = BuildUpdateSummaryText(changelog);
	const std::wstring title = UpdateDialogTitle();
	const std::wstring mainInstruction = LoadI18nString(IDS_I18N_UPDATE_MAIN_INSTRUCTION, L"New version available");
	const std::wstring btnUpdate = LoadI18nString(IDS_I18N_UPDATE_BTN_UPDATE_NOW, L"Update now");
	const std::wstring btnIgnore = LoadI18nString(IDS_I18N_UPDATE_BTN_IGNORE_VERSION, L"Ignore this version");
	const std::wstring btnDisable = LoadI18nString(IDS_I18N_UPDATE_BTN_DISABLE_PROMPT, L"Don't remind me again");
	const std::wstring btnLater = LoadI18nString(IDS_I18N_UPDATE_BTN_LATER, L"Remind me later");
	const TASKDIALOG_BUTTON buttons[] = {
		{kUpdateDialogButtonUpdate, btnUpdate.c_str()},
		{kUpdateDialogButtonIgnoreVersion, btnIgnore.c_str()},
		{kUpdateDialogButtonDisableAutoPrompt, btnDisable.c_str()},
		{kUpdateDialogButtonLater, btnLater.c_str()}
	};

	TASKDIALOGCONFIG config{};
	config.cbSize = sizeof(config);
	config.hwndParent = g_mainHwnd;
	config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
	config.dwCommonButtons = 0;
	config.pszWindowTitle = title.c_str();
	config.pszMainIcon = TD_INFORMATION_ICON;
	config.pszMainInstruction = mainInstruction.c_str();
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
		title.c_str(),
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
		Logi(L"Update", L"Download progress: ", static_cast<int>(percentageDownloaded), L"%");
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

inline void StartDownloadUpdate(const CAutoUpdaterGithub::VersionEntry& entry, const bool manual) {
	if (entry.versionUpdateUrl.empty()) {
		MessageBoxW(g_mainHwnd, LoadI18nString(IDS_I18N_UPDATE_NO_PACKAGE, L"No supported update package was found in the GitHub release.").c_str(), UpdateDialogTitle().c_str(), MB_OK | MB_ICONWARNING | MB_TOPMOST);
		return;
	}
	// 下载后由 updater 库校验 SHA-256（来自 GitHub API 的 digest 字段），缺失则拒绝安装
	if (entry.versionUpdateSha256.empty()) {
		MessageBoxW(g_mainHwnd, LoadI18nString(IDS_I18N_UPDATE_NO_DIGEST, L"The GitHub release asset has no SHA-256 digest, so the update cannot be verified and will not be installed.").c_str(), UpdateDialogTitle().c_str(), MB_OK | MB_ICONWARNING | MB_TOPMOST);
		return;
	}

	bool expected = false;
	if (!g_isDownloadingUpdate.compare_exchange_strong(expected, true)) {
		if (manual) {
			MessageBoxW(g_mainHwnd, LoadI18nString(IDS_I18N_UPDATE_ALREADY_DOWNLOADING, L"An update package is already downloading.").c_str(), UpdateDialogTitle().c_str(), MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
		}
		return;
	}

	MyShowSimpleToast(L"CandyLauncher", LoadI18nString(IDS_I18N_UPDATE_DOWNLOADING_TOAST, L"Downloading update package"));
	Logi(L"Update", L"Downloading v", utf8_to_wide(entry.versionString), L" expected sha256=", utf8_to_wide(entry.versionUpdateSha256), L" size=", entry.versionUpdateSize);
	std::thread([entry, manual]() {
		UpdateStatusListener listener(manual);
		listener.setTargetVersion(entry.versionString);

		CAutoUpdaterGithub updater(kGithubRepoName, ExtractCurrentVersionString());
		updater.setUpdateStatusListener(&listener);
		updater.downloadAndInstallUpdate(entry);
		g_isDownloadingUpdate = false;
	}).detach();
}

inline void StartCheckForUpdates(const bool manual) {
	if (g_isDownloadingUpdate.load()) {
		if (manual) {
			MessageBoxW(g_mainHwnd, LoadI18nString(IDS_I18N_UPDATE_ALREADY_DOWNLOADING, L"An update package is already downloading.").c_str(), UpdateDialogTitle().c_str(), MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
		}
		return;
	}

	bool expected = false;
	if (!g_isCheckingForUpdate.compare_exchange_strong(expected, true)) {
		if (manual) {
			MessageBoxW(g_mainHwnd, LoadI18nString(IDS_I18N_UPDATE_CHECK_RUNNING, L"Update check is already running.").c_str(), UpdateDialogTitle().c_str(), MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
		}
		return;
	}

	Logi(L"Update", L"Starting update check");
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
			MessageBoxW(g_mainHwnd, LoadI18nString(IDS_I18N_UPDATE_LATEST, L"You are already using the latest version.").c_str(), UpdateDialogTitle().c_str(), MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
		}
		return 0;
	}

	const auto& latest = payload->changelog.front();
	if (!payload->manual && latest.versionString == GetIgnoredUpdateVersion()) {
		Logi(L"Update", L"Skipped ignored update version: v", latest.versionString);
		return 0;
	}

	if (latest.versionUpdateUrl.empty()) {
		MessageBoxW(g_mainHwnd, LoadI18nString(IDS_I18N_UPDATE_NEWER_NO_PACKAGE, L"A newer version was found, but the release does not contain a supported update package.").c_str(), UpdateDialogTitle().c_str(), MB_OK | MB_ICONWARNING | MB_TOPMOST);
		return 0;
	}

	const int result = payload->manual
		? MessageBoxW(
			g_mainHwnd,
			BuildUpdateSummaryText(payload->changelog).c_str(),
			UpdateDialogTitle().c_str(),
			MB_YESNO | MB_ICONQUESTION | MB_TOPMOST)
		: ShowAutomaticUpdateDialog(payload->changelog);

	if (payload->manual && result == IDYES) {
		StartDownloadUpdate(latest, payload->manual);
	} else if (!payload->manual && result == kUpdateDialogButtonUpdate) {
		StartDownloadUpdate(latest, payload->manual);
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

	Loge(L"Update", payload->errorMessage);
	if (payload->manual) {
		MessageBoxW(
			g_mainHwnd,
			(LoadI18nString(IDS_I18N_UPDATE_CHECK_FAILED_PREFIX, L"Update check failed:") + L"\r\n" + utf8_to_wide(payload->errorMessage)).c_str(),
			UpdateDialogTitle().c_str(),
			MB_OK | MB_ICONERROR | MB_TOPMOST);
	} else {
		MyShowSimpleToast(L"CandyLauncher", LoadI18nString(IDS_I18N_UPDATE_AUTO_CHECK_FAILED, L"Automatic update check failed"));
	}
	return 0;
}

inline LRESULT HandleUpdateDownloadFinishedMessage(LPARAM lParam) {
	std::unique_ptr<UpdateFinishedPayload> payload(reinterpret_cast<UpdateFinishedPayload*>(lParam));
	if (!payload) return 0;

	std::wstring msg = LoadI18nString(IDS_I18N_UPDATE_PACKAGE_STARTED, L"Update package started");
	if (!payload->versionString.empty()) {
		msg += L": v" + utf8_to_wide(payload->versionString);
	}
	MyShowSimpleToast(L"CandyLauncher", msg);
	if (payload->manual) {
		MessageBoxW(g_mainHwnd, msg.c_str(), UpdateDialogTitle().c_str(), MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
	}
	if (g_mainHwnd != nullptr) {
		DestroyWindow(g_mainHwnd);
	}
	return 0;
}

} // namespace AppUpdate
