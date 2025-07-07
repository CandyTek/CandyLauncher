#pragma once
#include "ActionBase.hpp"
#include "MainTools.hpp"
#include "PinyinHelper.h"
#include <ShlObj.h>
#include <wrl/client.h>
#include <shlwapi.h>
#include <shobjidl.h>
// 不能少
#include <shellapi.h>

#include "BaseTools.hpp"
#include "DataKeeper.hpp"

using namespace Microsoft::WRL;

// 启动的错误处理
static auto ReportShellExecuteError = [](INT_PTR code, const std::wstring& path) {
	std::wstringstream ss;
	ss << L"Failed to execute: " << path << L"\n";

	switch (code)
	{
	case 0:  ss << L"The operating system is out of memory or resources."; break;
	case ERROR_FILE_NOT_FOUND: ss << L"The specified file was not found."; break;
	case ERROR_PATH_NOT_FOUND: ss << L"The specified path was not found."; break;
	case ERROR_BAD_FORMAT:     ss << L"The .exe file is invalid (non-Win32 .exe or error in image file)."; break;
	case SE_ERR_ACCESSDENIED:  ss << L"Access denied. The user may have refused the elevation."; break;
	case SE_ERR_ASSOCINCOMPLETE:
	case SE_ERR_DDEBUSY:
	case SE_ERR_NOASSOC:
		ss << L"File association error or DDE error."; break;
	default:
		ss << L"Unknown error. Code: " << code;
		break;
	}

	ss << L"\nLastError: " << GetLastError();
	ShowErrorMsgBox(ss.str());
};


class RunCommandAction : public ActionBase
{
public:
	RunCommandAction(
		const std::wstring& justName,
		const std::wstring& targetFilePath,
		const bool isUwpItem = false,
		const bool admin = false,
		const std::wstring& workingDir = L"",
		std::wstring args = L"")
		: IsUwpItem(isUwpItem),
		RunCommand(PinyinHelper::GetPinyinLongStr(MyToLower(justName))),
		targetFilePath(targetFilePath),
		defaultAsAdmin(admin), // 用拼音转换工具时替换
		workingDirectory(workingDir.empty() ? GetDirectory(targetFilePath) : workingDir),
		arguments(std::move(args))
	{
		SetTitle(justName);
		SetExecutable(true);
		if (!targetFilePath.empty())
			SetSubtitle(targetFilePath + L" " + arguments);
		else
			SetSubtitle(justName + L" " + arguments);
	}

	RunCommandAction(): IsUwpItem(false), defaultAsAdmin(false)
	{
	}

	// 运行命令
	void InvokeWithTarget(const wchar_t* target) 
	{
		if (IsUwpItem)
		{
			HINSTANCE hInst = ShellExecuteW(nullptr, pref_run_item_as_admin ? L"runas" : L"open",
											targetFilePath.c_str(), target, nullptr, SW_SHOWNORMAL);
			if (reinterpret_cast<INT_PTR>(hInst) <= 32)
			{
				// 错误，可能权限不够、路径不存在等
				ReportShellExecuteError(reinterpret_cast<INT_PTR>(hInst), targetFilePath);
			}
		}
		else
		{
			HINSTANCE hInst = ShellExecuteW(nullptr, pref_run_item_as_admin ? L"runas" : L"open",
											targetFilePath.c_str(), target,
											workingDirectory.c_str(), SW_SHOWNORMAL);
			//char const* temp=WStringToConstChar(targetFilePath);
			//	 system(temp);
			if (reinterpret_cast<INT_PTR>(hInst) <= 32)
			{
				// 错误，可能权限不够、路径不存在等
				ReportShellExecuteError(reinterpret_cast<INT_PTR>(hInst), targetFilePath);
			}
		}
	}

	// 运行命令
	void Invoke() override
	{
		InvokeWithTarget(nullptr); 
	}

	// 运行附带剪贴板参数
	void InvokeWithTargetClipBoard()
	{
		InvokeWithTarget(GetClipboardText().c_str());
	}
	
	// 打开所在文件夹
	void InvokeOpenFolder() const
	{
		const std::wstring folderPath = GetDirectory(targetFilePath);
		if (!folderPath.empty())
		{
			const std::wstring param = L"/select, " + targetFilePath;
			ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
		}
	}

	// 打开目标（快捷方式）指向文件夹
	void InvokeOpenGoalFolder() const
	{
		if (GetExtension(targetFilePath) == L".lnk")
		{
			const std::wstring resolvedPath = GetShortcutTarget(targetFilePath);
			if (!resolvedPath.empty())
			{
				const std::wstring param = L"/select, " + resolvedPath;
				ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
				return;
			}
		}
		InvokeOpenFolder();
	}


	[[nodiscard]] std::wstring GetTargetPath() const
	{
		return targetFilePath;
	}

	int iImageIndex = -1;

	bool IsUwpItem;
	std::wstring RunCommand;

protected:
	std::wstring targetFilePath;

private:
	bool defaultAsAdmin;
	std::wstring workingDirectory;
	std::wstring arguments;

	static std::wstring GetDirectory(const std::wstring& path)
	{
		const size_t pos = path.find_last_of(L"\\/");
		return (pos != std::wstring::npos) ? path.substr(0, pos) : L"";
	}

	static std::wstring GetExtension(const std::wstring& path)
	{
		const size_t dotPos = path.find_last_of('.');
		return (dotPos != std::wstring::npos) ? path.substr(dotPos) : L"";
	}

	static std::wstring GetShortcutTarget(const std::wstring& shortcutPath)
	{
		if (!PathFileExistsW(shortcutPath.c_str()))
			return L"";

		CoInitialize(nullptr);
		ComPtr<IShellLinkW> shellLink;
		if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink))))
		{
			CoUninitialize();
			return L"";
		}

		ComPtr<IPersistFile> persistFile;
		if (FAILED(shellLink.As(&persistFile)) ||
			FAILED(persistFile->Load(shortcutPath.c_str(), STGM_READ)))
		{
			CoUninitialize();
			return L"";
		}

		wchar_t targetPath[MAX_PATH];
		WIN32_FIND_DATAW data;
		if (SUCCEEDED(shellLink->GetPath(targetPath, MAX_PATH, &data, SLGP_UNCPRIORITY)))
		{
			CoUninitialize();
			return targetPath;
		}

		CoUninitialize();
		return L"";
	}
};
