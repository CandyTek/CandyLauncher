#pragma once

#include <algorithm>
#include <chrono>
#include <windows.h>
#include <string>
#include <gdiplus.h>
#include <shlwapi.h>

#include <shlobj.h>

#include <filesystem>

#include "StringUtil.hpp"


static std::wstring GetClipboardText() {
	// 尝试打开剪贴板
	if (!OpenClipboard(nullptr)) {
		return nullptr;
	}

	// 获取 Unicode 文本格式的剪贴板数据
	HANDLE hData = GetClipboardData(CF_UNICODETEXT);
	if (hData == nullptr) {
		CloseClipboard();
		return nullptr;
	}

	// 锁定内存句柄以获取实际数据指针
	LPCWSTR pszText = static_cast<LPCWSTR>(GlobalLock(hData));
	if (pszText == nullptr) {
		CloseClipboard();
		return nullptr;
	}

	// 复制数据到 std::wstring
	std::wstring text(pszText);

	// 解锁全局内存
	GlobalUnlock(hData);

	// 关闭剪贴板
	CloseClipboard();

	return text;
}

inline bool CopyTextToClipboard(HWND hWnd, const std::wstring& text) {
	if (!OpenClipboard(hWnd)) return false;
	if (!EmptyClipboard()) {
		CloseClipboard();
		return false;
	}

	size_t bytes = (text.size() + 1) * sizeof(wchar_t);
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
	if (!hMem) {
		CloseClipboard();
		return false;
	}

	void* p = GlobalLock(hMem);
	memcpy(p, text.c_str(), bytes);
	GlobalUnlock(hMem);
	SetClipboardData(CF_UNICODETEXT, hMem);
	CloseClipboard(); // hMem 的释放权交给剪贴板
	return true;
}
