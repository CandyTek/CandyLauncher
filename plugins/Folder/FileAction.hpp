#pragma once

#include <ShlObj.h>
#include <wrl/client.h>
#include <shlwapi.h>
#include <shobjidl.h>
// 不能少
#include <shellapi.h>

#include "FolderPluginData.hpp"
#include "util/BaseTools.hpp"
#include "util/BitmapUtil.hpp"
#include "util/ClipboardUtil.hpp"

using namespace Microsoft::WRL;


// 启动的错误处理
static auto ReportShellExecuteError = [](INT_PTR code, const std::wstring& path) {
	std::wstringstream ss;
	ss << L"Failed to execute: " << path << L"\n";

	switch (code) {
	case 0: ss << L"The operating system is out of memory or resources.";
		break;
	case ERROR_FILE_NOT_FOUND: ss << L"The specified file was not found.";
		break;
	case ERROR_PATH_NOT_FOUND: ss << L"The specified path was not found.";
		break;
	case ERROR_BAD_FORMAT: ss << L"The .exe file is invalid (non-Win32 .exe or error in image file).";
		break;
	case SE_ERR_ACCESSDENIED: ss << L"Access denied. The user may have refused the elevation.";
		break;
	case SE_ERR_ASSOCINCOMPLETE:
	case SE_ERR_DDEBUSY:
	case SE_ERR_NOASSOC: ss << L"File association error or DDE error.";
		break;
	default: ss << L"Unknown error. Code: " << code;
		break;
	}

	ss << L"\nLastError: " << GetLastError();
	ShowErrorMsgBox(ss.str());
};


class FileAction final : public candy::ActionBase {
public:
	// 被委托的构造函数（原构造函数）会完全执行完毕，才会执行第二个
	FileAction(
		const std::wstring& justName,
		const std::wstring& targetFilePath,
		const bool isRunAsAdmin = false,
		const std::wstring& workingDir = L"",
		const std::wstring args = L"") :
		targetFilePath(targetFilePath),
		defaultAsAdmin(isRunAsAdmin),
		workingDirectory(workingDir.empty() &&
			(targetFilePath.rfind(L"ms-settings:", 0) == 0 || targetFilePath.rfind(L"::{", 0) == 0)
			? L"" : (workingDir.empty() ? GetDirectory(targetFilePath) : workingDir)),
		arguments(std::move(args)) {
		// matchText 留空，由主程序根据 title 生成（小写 + 拼音）
		title = (justName);
		// 系统图标由主程序按 iconPath 延迟解析；刷新后同一路径视为同一条目
		iconPath = targetFilePath;
		stableKey = targetFilePath;

		if (!targetFilePath.empty()) subtitle = (targetFilePath + L" " + arguments);
		else subtitle = (justName + L" " + arguments);
	}

	// RunCommandAction(
	// 	const std::wstring& justName,
	// 	const std::wstring& targetFilePath2,
	// 	const std::wstring& otherSource2,
	// 	HBITMAP _hIcon = nullptr
	// ) : RunCommandAction(justName, targetFilePath2) // 委托构造
	// {
	// 	iconBitmap = _hIcon;
	// 	otherSource = otherSource2;
	// }

	// 运行命令
	void InvokeWithTarget(const wchar_t* target, bool isForceAdmin) {
		const std::wstring arg = m_host ? m_host->GetActionArg() : L"";
		std::wstring safeTarget = target == nullptr ? arg : std::wstring(target) + L" " + arg;
		// 设置项在主线程读取；条目可能在刷新后被释放，线程里只用拷贝的字符串。
		// 线程由 g_folderWork 跟踪，插件卸载前会等待它结束（ShellExecute 可能弹 UAC 等待很久）
		const bool runAsAdmin = (m_host && m_host->GetBool(L"pref_run_item_as_admin", false)) || isForceAdmin;
		const bool started = g_folderWork.Run([targetFilePath = targetFilePath, workingDirectory = workingDirectory,
											target = std::move(safeTarget), runAsAdmin]() {
			// open 使用 nullptr 走默认动词：部分类型（如 .cpl 只有 cplopen/runas）没有 "open" 动词，会返回 1155
			HINSTANCE hInst = ShellExecuteW(
				nullptr, runAsAdmin ? L"runas" : nullptr,
				targetFilePath.c_str(), target.c_str(),
				workingDirectory.empty() ? nullptr : workingDirectory.c_str(), SW_SHOWNORMAL);
			//char const* temp=WStringToConstChar(targetFilePath);
			//	 system(temp);
			INT_PTR result = reinterpret_cast<INT_PTR>(hInst);
			if (result <= 32) {
				// 插件正在卸载时不再弹错误框，让 Shutdown 尽快 join 到本线程
				if (g_folderWork.IsCancelled()) return;
				// 当使用管理员模式打开失败后，回退到正常模式打开
				if (runAsAdmin) {
					if (result == SE_ERR_NOASSOC) // 1155
					{
						// 再尝试用 open 打开
						HINSTANCE hInst2 = ShellExecuteW(nullptr, nullptr,
														targetFilePath.c_str(), target.c_str(),
										workingDirectory.empty() ? nullptr : workingDirectory.c_str(), SW_SHOWNORMAL);
						result = reinterpret_cast<INT_PTR>(hInst2);
						if (result <= 32) {
							ReportShellExecuteError(reinterpret_cast<INT_PTR>(hInst2), targetFilePath);
							return;
						}
					}
				} else {
					// 错误，可能权限不够、路径不存在等
					ReportShellExecuteError(reinterpret_cast<INT_PTR>(hInst), targetFilePath);
				}
			}
		});
		if (!started) Logw(L"FolderPlugin", L"Launch ignored, plugin is shutting down: ", targetFilePath);
	}

	// 运行命令
	void Invoke() {
		InvokeWithTarget(nullptr, false);
	}

	// 运行附带剪贴板参数
	void InvokeWithTargetClipBoard() {
		InvokeWithTarget(GetClipboardText().c_str(), false);
	}

	// 打开所在文件夹
	void InvokeOpenFolder() const {
		const std::wstring folderPath = GetDirectory(targetFilePath);
		if (!folderPath.empty()) {
			const std::wstring param = L"/select, " + targetFilePath;
			ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
		}
	}

	// 打开目标（快捷方式）指向文件夹
	void InvokeOpenGoalFolder() const {
		if (GetExtension(targetFilePath) == L".lnk") {
			const std::wstring resolvedPath = GetShortcutTarget(targetFilePath);
			if (!resolvedPath.empty()) {
				const std::wstring param = L"/select, " + resolvedPath;
				ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
				return;
			}
		}
		InvokeOpenFolder();
	}


	[[nodiscard]] std::wstring GetTargetPath() const {
		return targetFilePath;
	}

	// 索引重命名后同步更新显示名；匹配文本由主程序在重新发布时根据新标题生成
	void SetTitle(const std::wstring& newTitle) {
		title = newTitle;
		if (targetFilePath.empty()) subtitle = newTitle + L" " + arguments;
	}

	std::wstring uwpSource;

protected:
	std::wstring targetFilePath;

private:
	bool defaultAsAdmin;
	std::wstring workingDirectory;
	std::wstring arguments;

	static std::wstring GetDirectory(const std::wstring& path) {
		const size_t pos = path.find_last_of(L"\\/");
		return (pos != std::wstring::npos) ? path.substr(0, pos) : L"";
	}

	static std::wstring GetExtension(const std::wstring& path) {
		const size_t dotPos = path.find_last_of('.');
		return (dotPos != std::wstring::npos) ? path.substr(dotPos) : L"";
	}

	static std::wstring GetShortcutTarget(const std::wstring& shortcutPath) {
		if (!PathFileExistsW(shortcutPath.c_str())) return L"";

		CoInitialize(nullptr);
		ComPtr<IShellLinkW> shellLink;
		if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink)))) {
			CoUninitialize();
			return L"";
		}

		ComPtr<IPersistFile> persistFile;
		if (FAILED(shellLink.As(&persistFile)) ||
			FAILED(persistFile->Load(shortcutPath.c_str(), STGM_READ))) {
			CoUninitialize();
			return L"";
		}

		wchar_t targetPath[MAX_PATH];
		WIN32_FIND_DATAW data = {0};
		if (SUCCEEDED(shellLink->GetPath(targetPath, MAX_PATH, &data, SLGP_UNCPRIORITY))) {
			CoUninitialize();
			return targetPath;
		}

		CoUninitialize();
		return L"";
	}
};
