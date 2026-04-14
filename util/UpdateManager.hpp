#pragma once

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "../3rdparty/github-releases-autoupdater/src/cautoupdatergithub.h"
#include "../common/GlobalState.hpp"
#include "LogUtil.hpp"
#include "MyToastUtil.hpp"
#include "StringUtil.hpp"

constexpr UINT WM_APP_UPDATE_AVAILABLE = WM_APP + 201;
constexpr UINT WM_APP_UPDATE_ERROR = WM_APP + 202;
constexpr UINT WM_APP_UPDATE_DOWNLOAD_FINISHED = WM_APP + 203;

namespace AppUpdate {

constexpr const char* kGithubRepoName = "CandyTek/CandyLauncher";

inline std::atomic_bool g_isCheckingForUpdate = false;
inline std::atomic_bool g_isDownloadingUpdate = false;
inline std::atomic_bool g_hasScheduledStartupCheck = false;

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
	if (latest.versionUpdateUrl.empty()) {
		MessageBoxW(g_mainHwnd, L"A newer version was found, but the release does not contain a supported update package.", L"Update", MB_OK | MB_ICONWARNING | MB_TOPMOST);
		return 0;
	}

	const int result = MessageBoxW(
		g_mainHwnd,
		BuildUpdateSummaryText(payload->changelog).c_str(),
		L"Update",
		MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);

	if (result == IDYES) {
		StartDownloadUpdate(latest.versionUpdateUrl, latest.versionString, payload->manual);
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
