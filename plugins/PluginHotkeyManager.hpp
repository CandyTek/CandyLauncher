#pragma once
#include <mutex>
#include <functional>
#include <windows.h>
#include <vector>
#include <string>

#include "util/ClipboardUtil.hpp"
#include "util/HotkeyUtils.hpp"
#include "util/StringUtil.hpp"
#include "util/LogUtil.hpp"

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

namespace CommonUtil
{
    class PluginHotkeyManager{
public:
    using HotkeyCallback = std::function<void(int id)>;

    PluginHotkeyManager() = default;
    ~PluginHotkeyManager() { Stop(); }

    PluginHotkeyManager(const PluginHotkeyManager&) = delete;
    PluginHotkeyManager& operator=(const PluginHotkeyManager&) = delete;

    // 解析热键字符串，支持两种格式：
    // 1. "Alt+G(4)(71)" 带尾随 (modifiers)(vk)
    // 2. "Ctrl+Alt+F1", "Shift+Win+A" 纯文本
    static bool ParseHotkey(const std::wstring& hotkeyStr, UINT& outMod, UINT& outVk) {
        outMod = 0; outVk = 0;
        if (hotkeyStr.empty()) return false;

        // 格式 1: "Alt+G(4)(71)"
        size_t p2End = hotkeyStr.rfind(L')');
        if (p2End != std::wstring::npos) {
            size_t p2Start = hotkeyStr.rfind(L'(', p2End);
            if (p2Start != std::wstring::npos && p2Start > 0) {
                size_t p1End = hotkeyStr.rfind(L')', p2Start - 1);
                if (p1End != std::wstring::npos) {
                    size_t p1Start = hotkeyStr.rfind(L'(', p1End);
                    if (p1Start != std::wstring::npos) {
                        try {
                            outMod = std::stoul(hotkeyStr.substr(p1Start + 1, p1End - p1Start - 1));
                            outVk  = std::stoul(hotkeyStr.substr(p2Start + 1, p2End - p2Start - 1));
                            return outVk != 0;
                        } catch (...) {}
                    }
                }
            }
        }

        // 格式 2: 纯文本按键解析
        std::vector<std::wstring> tokens;
        std::wstring cur;
        for (wchar_t ch : hotkeyStr) {
            if (ch == L'+') {
                if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
            } else if (ch != L' ' && ch != L'\t') {
                cur += static_cast<wchar_t>(std::towlower(ch));
            }
        }
        if (!cur.empty()) tokens.push_back(cur);
        if (tokens.empty()) return false;

        std::wstring keyToken = tokens.back();
        tokens.pop_back();

        for (const auto& mod : tokens) {
            if (mod == L"ctrl" || mod == L"control") outMod |= MOD_CONTROL;
            else if (mod == L"alt")                 outMod |= MOD_ALT;
            else if (mod == L"shift")               outMod |= MOD_SHIFT;
            else if (mod == L"win")                 outMod |= MOD_WIN;
        }

        if (keyToken.size() == 1) {
            wchar_t ch = static_cast<wchar_t>(std::towupper(keyToken[0]));
            if ((ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9')) {
                outVk = ch;
            } else {
                SHORT s = VkKeyScanW(ch);
                if (s != -1) outVk = LOBYTE(s);
            }
        } else if (keyToken.size() >= 2 && keyToken[0] == L'f') {
            try {
                int fn = std::stoi(keyToken.substr(1));
                if (fn >= 1 && fn <= 24) outVk = static_cast<UINT>(VK_F1 + fn - 1);
            } catch (...) {}
        }

        return outVk != 0;
    }

    // 注册热键（支持配置是否自动加 MOD_NOREPEAT）
    bool AddHotkey(int id, UINT modifiers, UINT vk, HotkeyCallback cb, bool noRepeat = true) {
        if (vk == 0 || !cb) return false;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_items.push_back({ id, modifiers | (noRepeat ? MOD_NOREPEAT : 0), vk, std::move(cb) });
        return true;
    }

    bool AddHotkey(int id, const std::wstring& hotkeyStr, HotkeyCallback cb, bool noRepeat = true) {
        UINT mod = 0, vk = 0;
        if (!ParseHotkey(hotkeyStr, mod, vk)) return false;
        return AddHotkey(id, mod, vk, std::move(cb), noRepeat);
    }

    // 启动热键监听线程
    bool Start() {
        Stop();
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_items.empty()) return false;

        m_readyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!m_readyEvent) return false;
        m_hThread = CreateThread(nullptr, 0, ThreadProcThunk, this, 0, &m_threadId);
        if (!m_hThread) {
            CloseHandle(m_readyEvent);
            m_readyEvent = nullptr;
            return false;
        }
        WaitForSingleObject(m_readyEvent, INFINITE);
        CloseHandle(m_readyEvent);
        m_readyEvent = nullptr;
        return true;
    }

    // 停止并清理热键监听
    void Stop() {
        if (!m_hThread) return;
        PostThreadMessageW(m_threadId, WM_QUIT, 0, 0);
        WaitForSingleObject(m_hThread, INFINITE);
        CloseHandle(m_hThread);
        m_hThread = nullptr;
        m_threadId = 0;
    }

    void Clear() {
        Stop();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_items.clear();
    }

private:
    struct Item {
        int id;
        UINT mod;
        UINT vk;
        HotkeyCallback callback;
    };

    static DWORD WINAPI ThreadProcThunk(LPVOID param) {
        return static_cast<PluginHotkeyManager*>(param)->ThreadProc();
    }

    DWORD ThreadProc() {
        // Create the message queue before Start returns, so Stop can always post WM_QUIT.
        MSG msg;
        PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

        // 在本线程注册所有热键
        std::vector<int> registeredIds;
        for (const auto& item : m_items) {
            if (RegisterHotKey(nullptr, item.id, item.mod, item.vk)) {
                registeredIds.push_back(item.id);
            } else {
                Loge(L"PluginHotkeyManager", L"RegisterHotKey failed, id=", item.id,
                     L", error=", GetLastError());
            }
        }
        SetEvent(m_readyEvent);

        while (GetMessageW(&msg, nullptr, 0, 0)) {
            if (msg.message == WM_HOTKEY) {
                int id = static_cast<int>(msg.wParam);
                HotkeyCallback targetCb = nullptr;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    for (const auto& it : m_items) {
                        if (it.id == id) {
                            targetCb = it.callback;
                            break;
                        }
                    }
                }
                if (targetCb) {
                    targetCb(id);
                }
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // 线程退出前反注册所有热键
        for (int id : registeredIds) {
            UnregisterHotKey(nullptr, id);
        }
        return 0;
    }

    std::vector<Item> m_items;
    std::mutex m_mutex;
    HANDLE m_hThread = nullptr;
    HANDLE m_readyEvent = nullptr;`
    DWORD m_threadId = 0;
};
}
