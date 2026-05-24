#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include "WebSearchPluginData.hpp"
#include "WebSearchUtil.hpp"
#include "util/StringUtil.hpp"
#include "util/LogUtil.hpp"

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

struct WS_HotkeyEntry {
	int id;
	std::wstring engineKey;
	UINT modifiers;
	UINT vk;
};

static std::vector<WS_HotkeyEntry> g_wsHotkeyEntries;
static HANDLE g_wsHotkeyThread = nullptr;
static DWORD g_wsHotkeyThreadId = 0;

static bool WS_ParseHotkey(const std::wstring& hotkey, UINT& outMod, UINT& outVk) {
	outMod = 0; outVk = 0;
	if (hotkey.empty()) return false;

	// HotkeyEditView format: "Alt+G(4)(71)" — extract (modifiers)(vk) from suffix
	size_t p2End = hotkey.rfind(L')');
	if (p2End != std::wstring::npos) {
		size_t p2Start = hotkey.rfind(L'(', p2End);
		if (p2Start != std::wstring::npos && p2Start > 0) {
			size_t p1End = hotkey.rfind(L')', p2Start - 1);
			if (p1End != std::wstring::npos) {
				size_t p1Start = hotkey.rfind(L'(', p1End);
				if (p1Start != std::wstring::npos) {
					try {
						UINT mod = std::stoul(hotkey.substr(p1Start + 1, p1End - p1Start - 1));
						UINT vk  = std::stoul(hotkey.substr(p2Start + 1, p2End - p2Start - 1));
						if (vk != 0) { outMod = mod; outVk = vk; return true; }
					} catch (...) {}
				}
			}
		}
	}

	// Fallback: plain text format "Alt+G", "Ctrl+Shift+S"
	std::wstring lower = MyToLower(hotkey);
	std::vector<std::wstring> tokens;
	std::wstring tok;
	for (wchar_t c : lower) {
		if (c == L'+') { if (!tok.empty()) { tokens.push_back(tok); tok.clear(); } }
		else tok += c;
	}
	if (!tok.empty()) tokens.push_back(tok);
	if (tokens.empty()) return false;

	std::wstring keyStr = MyTrim(tokens.back());
	tokens.pop_back();

	for (auto& mod : tokens) {
		std::wstring m = MyTrim(mod);
		if (m == L"ctrl" || m == L"control") outMod |= MOD_CONTROL;
		else if (m == L"alt")                outMod |= MOD_ALT;
		else if (m == L"shift")              outMod |= MOD_SHIFT;
		else if (m == L"win")                outMod |= MOD_WIN;
	}

	if (keyStr.size() == 1) {
		wchar_t ch = static_cast<wchar_t>(std::towupper(keyStr[0]));
		if ((ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9')) {
			outVk = ch;
		} else {
			SHORT s = VkKeyScanW(ch);
			if (s != -1) outVk = LOBYTE(s);
		}
	} else if (keyStr.size() >= 2 && keyStr[0] == L'f') {
		try {
			int fn = std::stoi(keyStr.substr(1));
			if (fn >= 1 && fn <= 12) outVk = static_cast<UINT>(VK_F1 + fn - 1);
		} catch (...) {}
	}

	return outVk != 0;
}

// Grabs currently selected text via simulated Ctrl+C
static std::wstring WS_GetSelectedText() {
	if (OpenClipboard(nullptr)) { EmptyClipboard(); CloseClipboard(); }

	// Wait for hotkey modifier keys (Alt/Ctrl/Shift/Win) to be physically released.
	// Without this, SendInput injects Ctrl+C while (e.g.) Alt is still held, making
	// the target window receive Ctrl+Alt+C instead — which does nothing.
	const DWORD deadline = GetTickCount() + 1500;
	while (GetTickCount() < deadline) {
		bool anyDown =
			(GetAsyncKeyState(VK_CONTROL) & 0x8000) ||
			(GetAsyncKeyState(VK_MENU)    & 0x8000) ||
			(GetAsyncKeyState(VK_SHIFT)   & 0x8000) ||
			(GetAsyncKeyState(VK_LWIN)    & 0x8000) ||
			(GetAsyncKeyState(VK_RWIN)    & 0x8000);
		if (!anyDown) break;
		Sleep(10);
	}

	INPUT inputs[4] = {};
	inputs[0].type = INPUT_KEYBOARD; inputs[0].ki.wVk = VK_CONTROL;
	inputs[1].type = INPUT_KEYBOARD; inputs[1].ki.wVk = 'C';
	inputs[2].type = INPUT_KEYBOARD; inputs[2].ki.wVk = 'C'; inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
	inputs[3].type = INPUT_KEYBOARD; inputs[3].ki.wVk = VK_CONTROL; inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
	SendInput(4, inputs, sizeof(INPUT));
	Sleep(150);

	std::wstring result;
	if (OpenClipboard(nullptr)) {
		HANDLE hData = GetClipboardData(CF_UNICODETEXT);
		if (hData) {
			auto* p = static_cast<wchar_t*>(GlobalLock(hData));
			if (p) { result = p; GlobalUnlock(hData); }
		}
		CloseClipboard();
	}
	return MyTrim(result);
}

static DWORD WINAPI WS_HotkeyThreadProc(LPVOID) {
	for (auto& entry : g_wsHotkeyEntries) {
		if (!RegisterHotKey(nullptr, entry.id, entry.modifiers, entry.vk)) {
			PluginConsolePrintln(L"WebSearch", L"RegisterHotKey failed for: " + entry.engineKey);
		}
	}

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		if (msg.message == WM_HOTKEY) {
			int hkId = static_cast<int>(msg.wParam);
			for (auto& entry : g_wsHotkeyEntries) {
				if (entry.id != hkId) continue;
				for (auto& engine : g_searchEngines) {
					if (engine.key != entry.engineKey) continue;
					std::wstring query = WS_GetSelectedText();
					if (!query.empty()) {
						std::wstring url = BuildSearchUrl(engine.url, query);
						if (g_browser.empty()) {
							ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
						} else {
							ShellExecuteW(nullptr, L"open", g_browser.c_str(), url.c_str(), nullptr, SW_SHOWNORMAL);
						}
					}
					break;
				}
				break;
			}
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	for (auto& entry : g_wsHotkeyEntries) {
		UnregisterHotKey(nullptr, entry.id);
	}
	return 0;
}

static void WS_StopHotkeyThread() {
	if (!g_wsHotkeyThread) return;
	PostThreadMessageW(g_wsHotkeyThreadId, WM_QUIT, 0, 0);
	WaitForSingleObject(g_wsHotkeyThread, 2000);
	CloseHandle(g_wsHotkeyThread);
	g_wsHotkeyThread = nullptr;
	g_wsHotkeyThreadId = 0;
	g_wsHotkeyEntries.clear();
}

static void WS_StartHotkeyThread() {
	WS_StopHotkeyThread();

	g_wsHotkeyEntries.clear();
	int idBase = 0x2800;
	for (auto& engine : g_searchEngines) {
		if (engine.hotkey.empty()) continue;
		UINT mod, vk;
		if (WS_ParseHotkey(engine.hotkey, mod, vk)) {
			WS_HotkeyEntry e;
			e.id = idBase++;
			e.engineKey = engine.key;
			e.modifiers = mod | MOD_NOREPEAT;
			e.vk = vk;
			g_wsHotkeyEntries.push_back(e);
		}
	}

	if (g_wsHotkeyEntries.empty()) return;
	g_wsHotkeyThread = CreateThread(nullptr, 0, WS_HotkeyThreadProc, nullptr, 0, &g_wsHotkeyThreadId);
	PluginConsolePrintln(L"WebSearch", L"Hotkey thread started with " + std::to_wstring(g_wsHotkeyEntries.size()) + L" hotkeys");
}



