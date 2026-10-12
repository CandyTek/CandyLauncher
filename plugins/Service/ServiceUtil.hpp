#pragma once

#include "ServiceAction.hpp"
#include "ServicePluginData.hpp"
#include <Windows.h>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>

#include "util/StringUtil.hpp"
#include "util/LogUtil.hpp"

// 服务图标（使用 services.msc 所在的 mmc.exe），所有条目共享，由主程序延迟解析
static const wchar_t* SERVICE_ICON_PATH = LR"(C:\Windows\System32\mmc.exe)";

// 将服务状态转换为可读字符串
static std::wstring GetServiceStatusString(DWORD status) {
	switch (status) {
	case SERVICE_STOPPED: return ServiceTr(L"status.stopped", L"Stopped");
	case SERVICE_START_PENDING: return ServiceTr(L"status.start_pending", L"Starting");
	case SERVICE_STOP_PENDING: return ServiceTr(L"status.stop_pending", L"Stopping");
	case SERVICE_RUNNING: return ServiceTr(L"status.running", L"Running");
	case SERVICE_CONTINUE_PENDING: return ServiceTr(L"status.continue_pending", L"Continuing");
	case SERVICE_PAUSE_PENDING: return ServiceTr(L"status.pause_pending", L"Pausing");
	case SERVICE_PAUSED: return ServiceTr(L"status.paused", L"Paused");
	default: return ServiceTr(L"status.unknown", L"Unknown");
	}
}

// 将服务启动类型转换为可读字符串
static std::wstring GetServiceStartModeString(DWORD startType, bool delayedAutoStart) {
	if (startType == SERVICE_AUTO_START && delayedAutoStart) {
		return ServiceTr(L"start.auto_delayed", L"Automatic (Delayed Start)");
	}

	switch (startType) {
	case SERVICE_BOOT_START: return ServiceTr(L"start.boot", L"Boot");
	case SERVICE_SYSTEM_START: return ServiceTr(L"start.system", L"System");
	case SERVICE_AUTO_START: return ServiceTr(L"start.auto", L"Automatic");
	case SERVICE_DEMAND_START: return ServiceTr(L"start.manual", L"Manual");
	case SERVICE_DISABLED: return ServiceTr(L"start.disabled", L"Disabled");
	default: return ServiceTr(L"start.unknown", L"Unknown");
	}
}

// 通过 SCM 查询服务启动类型（较慢，每个服务都需要多次 RPC 调用）
static std::wstring QueryServiceStartModeFromScm(SC_HANDLE hSCManager, const std::wstring& serviceName) {
	SC_HANDLE hService = OpenServiceW(hSCManager, serviceName.c_str(), SERVICE_QUERY_CONFIG);
	std::wstring startMode = ServiceTr(L"start.unknown", L"Unknown");
	if (!hService) return startMode;

	DWORD bytesNeeded = 0;
	QueryServiceConfigW(hService, nullptr, 0, &bytesNeeded);
	if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
		std::vector<BYTE> buffer(bytesNeeded);
		auto config = reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(buffer.data());
		if (QueryServiceConfigW(hService, config, bytesNeeded, &bytesNeeded)) {
			bool delayed = false;
			if (config->dwStartType == SERVICE_AUTO_START) {
				SERVICE_DELAYED_AUTO_START_INFO info = {};
				DWORD infoBytes = 0;
				if (QueryServiceConfig2W(hService, SERVICE_CONFIG_DELAYED_AUTO_START_INFO,
										reinterpret_cast<LPBYTE>(&info), sizeof(info), &infoBytes)) {
					delayed = info.fDelayedAutostart != FALSE;
				}
			}
			startMode = GetServiceStartModeString(config->dwStartType, delayed);
		}
	}
	CloseServiceHandle(hService);
	return startMode;
}

// 直接从注册表读取服务启动类型，比逐个调用 QueryServiceConfigW 快得多
static bool ReadServiceStartModeFromRegistry(HKEY hServicesKey, const std::wstring& serviceName, std::wstring& startMode) {
	HKEY hKey;
	if (RegOpenKeyExW(hServicesKey, serviceName.c_str(), 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) {
		return false;
	}

	DWORD startType = 0;
	DWORD dataSize = sizeof(DWORD);
	const bool ok = RegGetValueW(hKey, nullptr, L"Start", RRF_RT_REG_DWORD, nullptr, &startType, &dataSize) == ERROR_SUCCESS;
	if (ok) {
		DWORD delayedAutoStart = 0;
		if (startType == SERVICE_AUTO_START) {
			dataSize = sizeof(DWORD);
			RegGetValueW(hKey, nullptr, L"DelayedAutostart", RRF_RT_REG_DWORD, nullptr, &delayedAutoStart, &dataSize);
		}
		startMode = GetServiceStartModeString(startType, delayedAutoStart == 1);
	}
	RegCloseKey(hKey);
	return ok;
}

// 获取所有 Windows 服务
static std::vector<std::shared_ptr<ServiceAction>> GetAllWindowsServices() {
	std::vector<std::shared_ptr<ServiceAction>> result;

	try {
		// 打开服务管理器
		SC_HANDLE hSCManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
		if (!hSCManager) {
			Loge(L"Service", L"Failed to open service manager: ", GetLastError());
			return result;
		}

		// 预分配足够大的缓冲区，通常一次调用即可拿到全部服务，省去先查询所需大小的那次调用
		DWORD bytesNeeded = 0;
		DWORD servicesReturned = 0;
		std::vector<BYTE> buffer(256 * 1024);
		BOOL enumOk = FALSE;
		for (int attempt = 0; attempt < 3; ++attempt) {
			DWORD resumeHandle = 0;
			enumOk = EnumServicesStatusExW(
				hSCManager,
				SC_ENUM_PROCESS_INFO,
				SERVICE_WIN32,
				SERVICE_STATE_ALL,
				buffer.data(),
				static_cast<DWORD>(buffer.size()),
				&bytesNeeded,
				&servicesReturned,
				&resumeHandle,
				nullptr
			);
			if (enumOk || GetLastError() != ERROR_MORE_DATA) break;
			// 缓冲区不足：bytesNeeded 只是剩余部分的大小，扩容后从头重新获取
			buffer.resize(buffer.size() + bytesNeeded);
		}

		if (!enumOk) {
			CloseServiceHandle(hSCManager);
			Loge(L"Service", L"Failed to enumerate services: ", GetLastError());
			return result;
		}
		auto services = reinterpret_cast<LPENUM_SERVICE_STATUS_PROCESSW>(buffer.data());

		HKEY hServicesKey = nullptr;
		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services", 0,
						KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &hServicesKey) != ERROR_SUCCESS) {
			hServicesKey = nullptr;
		}

		result.reserve(servicesReturned);

		// 遍历所有服务
		for (DWORD i = 0; i < servicesReturned; i++) {
			auto action = std::make_shared<ServiceAction>();

			// 获取服务名称和显示名称
			action->serviceName = services[i].lpServiceName;
			action->displayName = services[i].lpDisplayName;

			// 获取服务状态
			DWORD status = services[i].ServiceStatusProcess.dwCurrentState;
			action->status = GetServiceStatusString(status);
			action->isRunning = (status == SERVICE_RUNNING);

			// 获取启动类型，注册表读取失败时回退到 SCM 查询
			if (!hServicesKey || !ReadServiceStartModeFromRegistry(hServicesKey, action->serviceName, action->startMode)) {
				action->startMode = QueryServiceStartModeFromScm(hSCManager, action->serviceName);
			}

			// 设置标题和副标题
			action->title = action->displayName;
			action->UpdateSubtitle();

			// 设置图标和稳定标识
			action->iconPath = SERVICE_ICON_PATH;
			action->stableKey = action->serviceName;

			// 在插件内处理匹配文本，主程序原样使用
			action->matchTextVerbatim = true;
			try {
				if (isMatchTextServiceName) {
					action->matchText = m_host->ProcessMatchText(action->displayName) +
						action->serviceName;
				} else {
					action->matchText = m_host->ProcessMatchText(action->displayName);
				}
			} catch (...) {
				action->matchText = action->displayName;
			}

			result.push_back(action);
		}

		if (hServicesKey) RegCloseKey(hServicesKey);
		CloseServiceHandle(hSCManager);

		// 按显示名称排序
		std::sort(result.begin(), result.end(),
				[](const std::shared_ptr<ServiceAction>& a, const std::shared_ptr<ServiceAction>& b) {
					return a->displayName < b->displayName;
				});
	} catch (const std::exception& e) {
		Loge(L"Service", L"Exception in GetAllWindowsServices: ", e.what());
	}

	return result;
}

// 启动服务
static bool StartWindowsService(const std::wstring& serviceName) {
	SC_HANDLE hSCManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
	if (!hSCManager) {
		return false;
	}

	SC_HANDLE hService = OpenServiceW(hSCManager, serviceName.c_str(), SERVICE_START);
	if (!hService) {
		CloseServiceHandle(hSCManager);
		return false;
	}

	BOOL result = StartServiceW(hService, 0, nullptr);

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCManager);

	return result != FALSE;
}

// 停止服务
static bool StopWindowsService(const std::wstring& serviceName) {
	SC_HANDLE hSCManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
	if (!hSCManager) {
		return false;
	}

	SC_HANDLE hService = OpenServiceW(hSCManager, serviceName.c_str(), SERVICE_STOP);
	if (!hService) {
		CloseServiceHandle(hSCManager);
		return false;
	}

	SERVICE_STATUS status;
	BOOL result = ControlService(hService, SERVICE_CONTROL_STOP, &status);

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCManager);

	return result != FALSE;
}

// 重启服务
static bool RestartWindowsService(const std::wstring& serviceName) {
	if (StopWindowsService(serviceName)) {
		// 等待服务完全停止
		Sleep(1000);
		return StartWindowsService(serviceName);
	}
	return false;
}

// 更新单个服务的状态信息（效率更高，无需重新枚举所有服务）
static bool UpdateSingleServiceStatus(std::shared_ptr<ServiceAction> action) {
	if (!action) return false;

	try {
		// 确保线程安全上下文正确
		ImpersonateSelf(SecurityImpersonation);

		SC_HANDLE hSCManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
		if (!hSCManager) {
			return false;
		}

		SC_HANDLE hService = OpenServiceW(hSCManager, action->serviceName.c_str(), SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);

		if (!hService) {
			CloseServiceHandle(hSCManager);
			return false;
		}

		// 获取服务状态
		SERVICE_STATUS_PROCESS ssp;
		DWORD bytesNeeded;
		if (QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO,
								reinterpret_cast<LPBYTE>(&ssp), sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
			// 更新状态
			DWORD status = ssp.dwCurrentState;
			action->status = GetServiceStatusString(status);
			action->isRunning = (status == SERVICE_RUNNING);

			// 更新副标题
			action->UpdateSubtitle();
		}

		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);

		return true;
	} catch (...) {
		return false;
	}
}
