#pragma once

#include "ServiceAction.hpp"
#include <Windows.h>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>

#include "util/StringUtil.hpp"
#include "util/LogUtil.hpp"
#include "util/BitmapUtil.hpp"

// 将服务状态转换为可读字符串
static std::wstring GetServiceStatusString(DWORD status) {
	switch (status) {
	case SERVICE_STOPPED: return L"已停止";
	case SERVICE_START_PENDING: return L"正在启动";
	case SERVICE_STOP_PENDING: return L"正在停止";
	case SERVICE_RUNNING: return L"运行中";
	case SERVICE_CONTINUE_PENDING: return L"正在继续";
	case SERVICE_PAUSE_PENDING: return L"正在暂停";
	case SERVICE_PAUSED: return L"已暂停";
	default: return L"未知";
	}
}

// 将服务启动类型转换为可读字符串
static std::wstring GetServiceStartModeString(DWORD startType, const std::wstring& serviceName) {
	std::wstring result;

	switch (startType) {
	case SERVICE_BOOT_START: result = L"引导";
		break;
	case SERVICE_SYSTEM_START: result = L"系统";
		break;
	case SERVICE_AUTO_START: result = L"自动";
		break;
	case SERVICE_DEMAND_START: result = L"手动";
		break;
	case SERVICE_DISABLED: result = L"已禁用";
		break;
	default: result = L"未知";
		break;
	}

	// 检查是否为延迟启动
	if (startType == SERVICE_AUTO_START) {
		std::wstring regPath = L"SYSTEM\\CurrentControlSet\\Services\\" + serviceName;
		HKEY hKey;

		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
			DWORD delayedAutoStart = 0;
			DWORD dataSize = sizeof(DWORD);

			if (RegQueryValueExW(hKey, L"DelayedAutostart", nullptr, nullptr,
								(LPBYTE)&delayedAutoStart, &dataSize) == ERROR_SUCCESS) {
				if (delayedAutoStart == 1) {
					result = L"自动（延迟启动）";
				}
			}

			RegCloseKey(hKey);
		}
	}

	return result;
}

// 获取所有 Windows 服务
static std::vector<std::shared_ptr<BaseAction>> GetAllWindowsServices() {
	std::vector<std::shared_ptr<BaseAction>> result;

	try {
		// 打开服务管理器
		SC_HANDLE hSCManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
		if (!hSCManager) {
			Loge(L"Service", L"Failed to open service manager", std::to_string(GetLastError()).c_str());
			return result;
		}

		// 第一次调用获取所需缓冲区大小
		DWORD bytesNeeded = 0;
		DWORD servicesReturned = 0;
		DWORD resumeHandle = 0;

		EnumServicesStatusExW(
			hSCManager,
			SC_ENUM_PROCESS_INFO,
			SERVICE_WIN32,
			SERVICE_STATE_ALL,
			nullptr,
			0,
			&bytesNeeded,
			&servicesReturned,
			&resumeHandle,
			nullptr
		);

		if (GetLastError() != ERROR_MORE_DATA) {
			CloseServiceHandle(hSCManager);
			Loge(L"Service", L"Failed to enumerate services", std::to_string(GetLastError()).c_str());
			return result;
		}

		// 分配缓冲区并获取服务列表
		std::vector<BYTE> buffer(bytesNeeded);
		auto services = reinterpret_cast<LPENUM_SERVICE_STATUS_PROCESSW>(buffer.data());

		if (!EnumServicesStatusExW(
			hSCManager,
			SC_ENUM_PROCESS_INFO,
			SERVICE_WIN32,
			SERVICE_STATE_ALL,
			buffer.data(),
			bytesNeeded,
			&bytesNeeded,
			&servicesReturned,
			&resumeHandle,
			nullptr
		)) {
			CloseServiceHandle(hSCManager);
			Loge(L"Service", L"Failed to enumerate services", std::to_string(GetLastError()).c_str());
			return result;
		}

		// 获取服务图标路径（使用 services.msc）
		std::wstring serviceIconPath = L"C:\\Windows\\System32\\mmc.exe";
		int iconFilePathIndex = GetSysImageIndex(serviceIconPath);

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

			// 打开服务以获取启动类型
			SC_HANDLE hService = OpenServiceW(hSCManager, action->serviceName.c_str(), SERVICE_QUERY_CONFIG);
			if (hService) {
				DWORD configBytesNeeded = 0;
				QueryServiceConfigW(hService, nullptr, 0, &configBytesNeeded);

				if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
					std::vector<BYTE> configBuffer(configBytesNeeded);
					auto serviceConfig = reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(configBuffer.data());

					if (QueryServiceConfigW(hService, serviceConfig, configBytesNeeded, &configBytesNeeded)) {
						action->startMode = GetServiceStartModeString(serviceConfig->dwStartType, action->serviceName);
					}
				}

				CloseServiceHandle(hService);
			} else {
				action->startMode = L"未知";
			}

			// 设置标题和副标题
			action->title = action->displayName;
			action->subTitle = L"状态: " + action->status +
				L" - 启动: " + action->startMode +
				L" - 名称: " + action->serviceName;

			// 设置图标
			action->iconFilePath = serviceIconPath;
			action->iconFilePathIndex = iconFilePathIndex;

			// 设置匹配文本
			try {
				if (isMatchTextServiceName) {
					action->matchText = m_host->GetTheProcessedMatchingText(action->displayName) +
						action->serviceName;
				} else {
					action->matchText = m_host->GetTheProcessedMatchingText(action->displayName);
				}
			} catch (...) {
				action->matchText = action->displayName;
			}

			result.push_back(action);
		}

		CloseServiceHandle(hSCManager);

		// 按显示名称排序
		std::sort(result.begin(), result.end(),
				[](const std::shared_ptr<BaseAction>& a, const std::shared_ptr<BaseAction>& b) {
					auto actionA = std::dynamic_pointer_cast<ServiceAction>(a);
					auto actionB = std::dynamic_pointer_cast<ServiceAction>(b);
					if (!actionA || !actionB) return false;
					return actionA->displayName < actionB->displayName;
				});

		ConsolePrintln(L"Service Plugin", L"Loaded " + std::to_wstring(result.size()) + L" services");
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
			action->subTitle = L"状态: " + action->status +
				L" - 启动: " + action->startMode +
				L" - 名称: " + action->serviceName;
		}

		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);

		return true;
	} catch (...) {
		return false;
	}
}
