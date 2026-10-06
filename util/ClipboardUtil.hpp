#pragma once

#include <windows.h>
#include <string>
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

inline bool CopyTextToClipboard(HWND hWnd, const std::string& text) {
	return CopyTextToClipboard(hWnd, utf8_to_wide(text));
}

// 从剪贴板获取文本内容并转换为UTF-8
inline std::string GetClipboardTextAsUTF8() {
	if (!OpenClipboard(NULL)) {
		return "";
	}

	std::string result;
	HANDLE hData = GetClipboardData(CF_UNICODETEXT);
	if (hData) {
		wchar_t* pwszText = static_cast<wchar_t*>(GlobalLock(hData));
		if (pwszText) {
			// 转换 Unicode 到 UTF-8
			int size = WideCharToMultiByte(CP_UTF8, 0, pwszText, -1, NULL, 0, NULL, NULL);
			if (size > 0) {
				std::vector<char> buffer(size);
				WideCharToMultiByte(CP_UTF8, 0, pwszText, -1, buffer.data(), size, NULL, NULL);
				result = buffer.data();
			}
			GlobalUnlock(hData);
		}
	}
	CloseClipboard();
	return result;
}

// 将文本写入剪贴板
inline bool SetClipboardText(std::wstring_view text) {
	if (!OpenClipboard(nullptr)) {
		return false;
	}

	EmptyClipboard();

	// 计算字节数（包含末尾的 '\0'）
	const size_t charCount = text.size() + 1;
	const size_t byteSize = charCount * sizeof(wchar_t);

	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, byteSize);
	if (!hMem) {
		CloseClipboard();
		return false;
	}

	auto* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
	if (!pMem) {
		GlobalFree(hMem);
		CloseClipboard();
		return false;
	}

	// 直接内存拷贝，避免多次 API 扫描
	memcpy(pMem, text.data(), text.size() * sizeof(wchar_t));
	pMem[text.size()] = L'\0';

	GlobalUnlock(hMem);

	// 成功调用 SetClipboardData 后，内存块归系统所有，不能手动 GlobalFree
	if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
		GlobalFree(hMem);
		CloseClipboard();
		return false;
	}

	CloseClipboard();
	return true;
}

// 辅助重载：仅负责 UTF-8 到 UTF-16 的转换
inline bool SetClipboardText(std::string_view textUtf8) {
	if (textUtf8.empty()) {
		return SetClipboardText(std::wstring_view{});
	}

	int size = MultiByteToWideChar(CP_UTF8, 0, textUtf8.data(), static_cast<int>(textUtf8.size()), nullptr, 0);
	if (size <= 0) {
		return false;
	}

	std::wstring wideStr(size, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, textUtf8.data(), static_cast<int>(textUtf8.size()), wideStr.data(), size);

	return SetClipboardText(wideStr);
}

// Grabs currently selected text via simulated Ctrl+C
static std::wstring GetSelectedText()
{
    if (OpenClipboard(nullptr))
    {
        EmptyClipboard();
        CloseClipboard();
    }

    // Wait for hotkey modifier keys (Alt/Ctrl/Shift/Win) to be physically released.
    // Without this, SendInput injects Ctrl+C while (e.g.) Alt is still held, making
    // the target window receive Ctrl+Alt+C instead — which does nothing.
    const DWORD deadline = GetTickCount() + 1500;
    while (GetTickCount() < deadline)
    {
        bool anyDown =
            (GetAsyncKeyState(VK_CONTROL) & 0x8000) ||
            (GetAsyncKeyState(VK_MENU) & 0x8000) ||
            (GetAsyncKeyState(VK_SHIFT) & 0x8000) ||
            (GetAsyncKeyState(VK_LWIN) & 0x8000) ||
            (GetAsyncKeyState(VK_RWIN) & 0x8000);
        if (!anyDown) break;
        Sleep(10);
    }

    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'C';
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'C';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, inputs, sizeof(INPUT));
    Sleep(150);

    std::wstring result;
    if (OpenClipboard(nullptr))
    {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData)
        {
            auto* p = static_cast<wchar_t*>(GlobalLock(hData));
            if (p)
            {
                result = p;
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
    return MyTrim(result);
}
