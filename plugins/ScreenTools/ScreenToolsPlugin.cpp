#include "util/LogUtil.hpp"
#include "plugins/sdk/CandyPluginSdk.hpp"
#include "QrDecoder.h"
#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

#include "stb_image_write.h"
#include "plugins/PluginHotkeyManager.hpp"
#include "util/BitmapUtil.hpp"
#include "util/DpiUtil.hpp"
#include "util/MyJsonUtil.hpp"
#include "util/ParallelUtil.hpp"
#include "util/UrlUtil.hpp"

namespace
{
    struct Image
    {
        int width = 0, height = 0;
        int left = 0, top = 0;
        std::vector<uint8_t> pixels;
    };

    Image Capture(RECT rect);

    // 截图 / 识别在独立线程上运行。线程全部登记在这里，Shutdown 时关掉它们的窗口并等待结束，
    // 否则插件实例销毁、DLL 卸载后线程还会继续执行已释放的代码
    // 进程本身 DPI 不感知，新线程默认也不感知：GetSystemMetrics 的虚拟屏幕、GetCursorPos、选区窗口位置和
    // 屏幕 DC 的 BitBlt 都会按主显示器缩放比例虚拟化，多块不同 DPI 的显示器上选区与实际截图错位。
    // 工作线程进入 Per-Monitor V2 作用域后全部以物理像素工作；Win7 等不支持的系统上作用域不生效，行为同以前
    std::atomic<bool> g_stopping{false};
    std::mutex g_workersMutex;

    struct Worker
    {
        std::thread thread;
        std::shared_ptr<std::atomic<bool>> finished;
    };

    std::vector<Worker> g_workers;

    bool StartWorker(std::function<void()> fn)
    {
        std::lock_guard<std::mutex> lock(g_workersMutex);
        if (g_stopping) return false;
        for (auto it = g_workers.begin(); it != g_workers.end();)
        {
            if (*it->finished)
            {
                it->thread.join();
                it = g_workers.erase(it);
            }
            else ++it;
        }
        auto finished = std::make_shared<std::atomic<bool>>(false);
        std::thread thread([fn = std::move(fn), finished]
        {
            {
                Dpi::ThreadContextScope dpiScope(Dpi::PerMonitorV2Context());
                fn();
            }
            *finished = true;
        });
        g_workers.push_back({std::move(thread), std::move(finished)});
        return true;
    }

    BOOL CALLBACK CloseThreadWindowProc(const HWND hwnd, LPARAM)
    {
        // 选区窗口走 DefWindowProc 销毁并结束消息循环，MessageBox 也会随之关闭
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return TRUE;
    }

    void StopWorkers()
    {
        std::vector<Worker> workers;
        {
            std::lock_guard<std::mutex> lock(g_workersMutex);
            g_stopping = true;
            workers.swap(g_workers);
        }
        for (auto& worker : workers)
        {
            const HANDLE handle = worker.thread.native_handle();
            const DWORD threadId = GetThreadId(handle);
            // 工作线程可能正通过 SetWindowText 同步发消息给 UI 线程，等待期间必须处理 sent message，
            // 否则在 UI 线程上 join 会死锁；PM_NOREMOVE 只派发 sent message，不消费队列里的其它消息
            while (!*worker.finished)
            {
                EnumThreadWindows(threadId, CloseThreadWindowProc, 0);
                const DWORD wait = MsgWaitForMultipleObjects(1, &handle, FALSE, 50, QS_SENDMESSAGE);
                if (wait == WAIT_OBJECT_0 + 1)
                {
                    MSG msg;
                    PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE);
                }
                else if (wait == WAIT_OBJECT_0 || wait == WAIT_FAILED) break;
            }
            worker.thread.join();
        }
    }

    struct Selection
    {
        RECT rect{};
        POINT start{};
        POINT end{};
        POINT origin{};
        SIZE size{};
        HDC memory = nullptr;
        HBITMAP bitmap = nullptr;
        HGDIOBJ oldBitmap = nullptr;
        uint32_t* pixels = nullptr;
        const Image* frame = nullptr;
        std::vector<uint32_t> background;
        // 上一帧选区在窗口坐标下覆盖的像素区域（已裁剪到窗口内）。多屏大分辨率时虚拟桌面可达上亿字节，
        // 每次鼠标移动只能还原并上传新旧选区的并集，否则整帧 memcpy + UpdateLayeredWindow 会明显卡顿
        RECT drawn{};
        bool uploaded = false;
        bool dragging = false;
        bool done = false;
        bool cancelled = true;
    };

    RECT Normalize(POINT a, POINT b)
    {
        return {std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)};
    }

    RECT ClampToWindow(const Selection& state, RECT rect)
    {
        rect.left = std::clamp(rect.left, 0L, static_cast<LONG>(state.size.cx));
        rect.right = std::clamp(rect.right, 0L, static_cast<LONG>(state.size.cx));
        rect.top = std::clamp(rect.top, 0L, static_cast<LONG>(state.size.cy));
        rect.bottom = std::clamp(rect.bottom, 0L, static_cast<LONG>(state.size.cy));
        if (rect.right <= rect.left || rect.bottom <= rect.top) return {};
        return rect;
    }

    bool IsEmptyRect(const RECT& rect)
    {
        return rect.right <= rect.left || rect.bottom <= rect.top;
    }

    // 把 background 中 rect 覆盖的行段拷回绘制缓冲，只触碰需要重绘的像素
    void RestoreBackground(Selection& state, const RECT& rect)
    {
        if (IsEmptyRect(rect)) return;
        const size_t bytes = static_cast<size_t>(rect.right - rect.left) * sizeof(uint32_t);
        for (LONG y = rect.top; y < rect.bottom; ++y)
        {
            const size_t offset = static_cast<size_t>(y) * state.size.cx + rect.left;
            memcpy(state.pixels + offset, state.background.data() + offset, bytes);
        }
    }

    uint32_t PremultipliedColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
    {
        return (uint32_t(alpha) << 24) | (uint32_t(red * alpha / 255) << 16) |
            (uint32_t(green * alpha / 255) << 8) | uint32_t(blue * alpha / 255);
    }

    void FillSelectionRect(Selection& state, RECT rect, uint32_t color)
    {
        rect.left = std::clamp(rect.left, 0L, static_cast<LONG>(state.size.cx));
        rect.right = std::clamp(rect.right, 0L, static_cast<LONG>(state.size.cx));
        rect.top = std::clamp(rect.top, 0L, static_cast<LONG>(state.size.cy));
        rect.bottom = std::clamp(rect.bottom, 0L, static_cast<LONG>(state.size.cy));
        for (LONG y = rect.top; y < rect.bottom; ++y)
            std::fill(state.pixels + static_cast<size_t>(y) * state.size.cx + rect.left,
                      state.pixels + static_cast<size_t>(y) * state.size.cx + rect.right, color);
    }

    uint32_t TintPixel(const uint8_t* pixel, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
    {
        const auto blend = [alpha](uint8_t original, uint8_t tint)
        {
            return (uint32_t(original) * (255 - alpha) + uint32_t(tint) * alpha) / 255;
        };
        return 0xff000000 | (blend(pixel[2], red) << 16) |
            (blend(pixel[1], green) << 8) | blend(pixel[0], blue);
    }

    void TintSelectionRect(Selection& state, RECT rect)
    {
        rect.left = std::clamp(rect.left, 0L, static_cast<LONG>(state.size.cx));
        rect.right = std::clamp(rect.right, 0L, static_cast<LONG>(state.size.cx));
        rect.top = std::clamp(rect.top, 0L, static_cast<LONG>(state.size.cy));
        rect.bottom = std::clamp(rect.bottom, 0L, static_cast<LONG>(state.size.cy));
        if (rect.right <= rect.left || rect.bottom <= rect.top) return;
        // 选区拖到整块 4K 屏时有近千万像素，按行分到多个线程；小选区仍在当前线程完成
        const size_t rows = static_cast<size_t>(rect.bottom - rect.top);
        const size_t width = static_cast<size_t>(rect.right - rect.left);
        const size_t minRowsPerThread = std::max<size_t>(1, (size_t{1} << 18) / std::max<size_t>(1, width));
        const uint8_t* src = state.frame->pixels.data();
        uint32_t* dst = state.pixels;
        const size_t stride = static_cast<size_t>(state.size.cx);
        ParallelForRange(rows, minRowsPerThread, [&](size_t begin, size_t end)
        {
            for (size_t row = begin; row < end; ++row)
            {
                const size_t index = (static_cast<size_t>(rect.top) + row) * stride + rect.left;
                for (size_t x = 0; x < width; ++x)
                    dst[index + x] = TintPixel(src + (index + x) * 4, 40, 130, 245, 64);
            }
        });
    }

    bool RenderSelection(HWND hwnd, Selection& state)
    {
        const uint32_t blueFill = PremultipliedColor(40, 130, 245, 64);
        const uint32_t blueBorder = state.frame ? 0xff2d91ff : PremultipliedColor(45, 145, 255, 245);
        RECT rect{};
        if (state.dragging)
        {
            rect = Normalize(state.start, state.end);
            OffsetRect(&rect, -state.origin.x, -state.origin.y);
            rect = ClampToWindow(state, rect);
        }
        // 脏区 = 旧选区 ∪ 新选区；首帧需要完整上传一次整张位图
        RECT dirty{};
        if (!state.uploaded)
        {
            memcpy(state.pixels, state.background.data(), state.background.size() * sizeof(uint32_t));
            dirty = {0, 0, state.size.cx, state.size.cy};
        }
        else
        {
            UnionRect(&dirty, &state.drawn, &rect);
            RestoreBackground(state, dirty);
        }
        if (state.dragging && !IsEmptyRect(rect))
        {
            if (state.frame) TintSelectionRect(state, rect);
            else FillSelectionRect(state, rect, blueFill);
            // The corners make the selection easier to see over a bright screenshot.
            constexpr LONG border = 2, corner = 12, cornerWidth = 4;
            FillSelectionRect(state, {rect.left, rect.top, rect.right, rect.top + border}, blueBorder);
            FillSelectionRect(state, {rect.left, rect.bottom - border, rect.right, rect.bottom}, blueBorder);
            FillSelectionRect(state, {rect.left, rect.top, rect.left + border, rect.bottom}, blueBorder);
            FillSelectionRect(state, {rect.right - border, rect.top, rect.right, rect.bottom}, blueBorder);
            if (rect.right - rect.left >= corner * 2 && rect.bottom - rect.top >= corner * 2)
            {
                FillSelectionRect(state, {rect.left, rect.top, rect.left + corner, rect.top + cornerWidth}, blueBorder);
                FillSelectionRect(state, {rect.left, rect.top, rect.left + cornerWidth, rect.top + corner}, blueBorder);
                FillSelectionRect(state, {rect.right - corner, rect.top, rect.right, rect.top + cornerWidth},
                                  blueBorder);
                FillSelectionRect(state, {rect.right - cornerWidth, rect.top, rect.right, rect.top + corner},
                                  blueBorder);
                FillSelectionRect(state, {rect.left, rect.bottom - cornerWidth, rect.left + corner, rect.bottom},
                                  blueBorder);
                FillSelectionRect(state, {rect.left, rect.bottom - corner, rect.left + cornerWidth, rect.bottom},
                                  blueBorder);
                FillSelectionRect(state, {rect.right - corner, rect.bottom - cornerWidth, rect.right, rect.bottom},
                                  blueBorder);
                FillSelectionRect(state, {rect.right - cornerWidth, rect.bottom - corner, rect.right, rect.bottom},
                                  blueBorder);
            }
        }
        state.drawn = rect;
        if (IsEmptyRect(dirty)) return true;
        POINT source{};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        UPDATELAYEREDWINDOWINFO info{};
        info.cbSize = sizeof(info);
        info.pptDst = &state.origin;
        info.psize = &state.size;
        info.hdcSrc = state.memory;
        info.pptSrc = &source;
        info.pblend = &blend;
        info.dwFlags = ULW_ALPHA;
        // prcDirty 让 DWM 只重新读取脏区，而不是整张虚拟桌面大小的位图
        info.prcDirty = state.uploaded ? &dirty : nullptr;
        if (UpdateLayeredWindowIndirect(hwnd, &info) == FALSE)
        {
            // 个别驱动/远程桌面下带脏区的更新可能失败，退回整帧更新
            if (UpdateLayeredWindow(hwnd, nullptr, &state.origin, &state.size, state.memory,
                                    &source, 0, &blend, ULW_ALPHA) == FALSE)
                return false;
        }
        state.uploaded = true;
        return true;
    }

    LRESULT CALLBACK SelectProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    {
        auto* state = reinterpret_cast<Selection*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        switch (message)
        {
        case WM_NCCREATE:
            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams));
            return TRUE;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE && state)
            {
                state->done = true;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_LBUTTONDOWN:
            if (state)
            {
                GetCursorPos(&state->start);
                state->end = state->start;
                state->dragging = true;
                SetCapture(hwnd);
                RenderSelection(hwnd, *state);
            }
            return 0;
        case WM_MOUSEMOVE:
            if (state && state->dragging)
            {
                GetCursorPos(&state->end);
                RenderSelection(hwnd, *state);
            }
            return 0;
        case WM_LBUTTONUP:
            if (state && state->dragging)
            {
                GetCursorPos(&state->end);
                state->rect = Normalize(state->start, state->end);
                state->cancelled = state->rect.right - state->rect.left < 16 || state->rect.bottom - state->rect.top <
                    16;
                state->done = true;
                ReleaseCapture();
                DestroyWindow(hwnd);
            }
            return 0;
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                BeginPaint(hwnd, &ps);
                EndPaint(hwnd, &ps);
                return 0;
            }
        case WM_DESTROY:
            if (state) state->done = true;
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    bool SelectArea(RECT& result, Image* frozen = nullptr)
    {
        // The action is invoked while the launcher is foreground; hide it before capturing.
        const HWND foreground = GetForegroundWindow();
        wchar_t windowClass[64]{};
        if (foreground && GetClassNameW(foreground, windowClass, 64) && wcscmp(windowClass, L"CandyLauncherClass") == 0)
            ShowWindow(foreground, SW_HIDE);
        const wchar_t* className = L"CandyScreenToolsSelection";
        WNDCLASSW wc{};
        wc.lpfnWndProc = SelectProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = className;
        wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
        if (!RegisterClassW(&wc)) return false;
        Selection state;
        state.origin = {GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN)};
        state.size = {GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN)};
        if (frozen)
        {
            Sleep(120);
            *frozen = Capture({
                state.origin.x, state.origin.y,
                state.origin.x + state.size.cx, state.origin.y + state.size.cy
            });
            if (frozen->pixels.empty())
            {
                UnregisterClassW(className, wc.hInstance);
                return false;
            }
            state.frame = frozen;
        }
        state.background.resize(static_cast<size_t>(state.size.cx) * state.size.cy);
        if (frozen)
        {
            // 多屏 4K 下有数千万像素，按行分给多个线程压暗
            const uint8_t* src = frozen->pixels.data();
            uint32_t* dst = state.background.data();
            ParallelForRange(state.background.size(), size_t{1} << 18, [src, dst](size_t begin, size_t end)
            {
                for (size_t i = begin; i < end; ++i)
                    dst[i] = TintPixel(src + i * 4, 10, 18, 32, 90);
            });
        }
        else
        {
            std::fill(state.background.begin(), state.background.end(), PremultipliedColor(10, 18, 32, 90));
        }
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = state.size.cx;
        bmi.bmiHeader.biHeight = -state.size.cy;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        state.memory = CreateCompatibleDC(nullptr);
        if (state.memory)
            state.bitmap = CreateDIBSection(state.memory, &bmi, DIB_RGB_COLORS,
                                            reinterpret_cast<void**>(&state.pixels), nullptr, 0);
        if (!state.memory || !state.bitmap)
        {
            if (state.bitmap) DeleteObject(state.bitmap);
            if (state.memory) DeleteDC(state.memory);
            UnregisterClassW(className, wc.hInstance);
            return false;
        }
        state.oldBitmap = SelectObject(state.memory, state.bitmap);
        HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
                                    className, L"选择区域（Esc 取消）", WS_POPUP,
                                    state.origin.x, state.origin.y, state.size.cx, state.size.cy,
                                    nullptr, nullptr, GetModuleHandleW(nullptr), &state);
        if (!hwnd || !RenderSelection(hwnd, state))
        {
            if (hwnd) DestroyWindow(hwnd);
            SelectObject(state.memory, state.oldBitmap);
            DeleteObject(state.bitmap);
            DeleteDC(state.memory);
            UnregisterClassW(className, wc.hInstance);
            return false;
        }
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
        MSG msg;
        int messageResult = 1;
        while (!state.done && (messageResult = GetMessageW(&msg, nullptr, 0, 0)) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (messageResult == 0) PostQuitMessage(static_cast<int>(msg.wParam));
        if (!state.done) DestroyWindow(hwnd);
        SelectObject(state.memory, state.oldBitmap);
        DeleteObject(state.bitmap);
        DeleteDC(state.memory);
        UnregisterClassW(className, wc.hInstance);
        result = state.rect;
        return !state.cancelled;
    }

    Image Capture(RECT rect)
    {
        Image out;
        out.width = rect.right - rect.left;
        out.height = rect.bottom - rect.top;
        out.left = rect.left;
        out.top = rect.top;
        if (out.width <= 0 || out.height <= 0) return {};
        out.pixels.resize(static_cast<size_t>(out.width) * out.height * 4);
        HDC screen = GetDC(nullptr);
        HDC memory = CreateCompatibleDC(screen);
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = out.width;
        bmi.bmiHeader.biHeight = -out.height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP bitmap = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bitmap)
        {
            DeleteDC(memory);
            ReleaseDC(nullptr, screen);
            return {};
        }
        HGDIOBJ old = SelectObject(memory, bitmap);
        bool ok = BitBlt(memory, 0, 0, out.width, out.height, screen, rect.left, rect.top, SRCCOPY | CAPTUREBLT) != 0;
        if (ok) memcpy(out.pixels.data(), bits, out.pixels.size());
        SelectObject(memory, old);
        DeleteObject(bitmap);
        DeleteDC(memory);
        ReleaseDC(nullptr, screen);
        return ok ? out : Image{};
    }

    Image CropFrozenImage(const Image& frozen, RECT rect)
    {
        const int left = std::clamp<LONG>(rect.left - frozen.left, 0, frozen.width);
        const int top = std::clamp<LONG>(rect.top - frozen.top, 0, frozen.height);
        const int right = std::clamp<LONG>(rect.right - frozen.left, 0, frozen.width);
        const int bottom = std::clamp<LONG>(rect.bottom - frozen.top, 0, frozen.height);
        if (right <= left || bottom <= top) return {};
        Image result;
        result.width = right - left;
        result.height = bottom - top;
        result.left = frozen.left + left;
        result.top = frozen.top + top;
        result.pixels.resize(static_cast<size_t>(result.width) * result.height * 4);
        for (int y = 0; y < result.height; ++y)
            memcpy(result.pixels.data() + static_cast<size_t>(y) * result.width * 4,
                   frozen.pixels.data() + (static_cast<size_t>(top + y) * frozen.width + left) * 4,
                   static_cast<size_t>(result.width) * 4);
        return result;
    }

    bool CopyUtf8(const std::string& text)
    {
        if (!OpenClipboard(nullptr)) return false;
        EmptyClipboard();
        int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (static_cast<size_t>(count) + 1) * sizeof(wchar_t));
        bool ok = false;
        if (memory)
        {
            auto* data = static_cast<wchar_t*>(GlobalLock(memory));
            if (data)
            {
                MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), data, count);
                data[count] = 0;
                GlobalUnlock(memory);
                ok = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
            }
            if (!ok) GlobalFree(memory);
        }
        CloseClipboard();
        return ok;
    }

    std::wstring Wide(const std::string& text)
    {
        int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        std::wstring result(n, L'\0');
        if (n) MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), n);
        return result;
    }

    // 在工作线程上运行；set_edit_text / show_main_window 内部是 SetWindowText / ShowWindow，可跨线程调用
    void ReadQrCode(const candy::Host* m_host)
    {
        RECT area{};
        Image frozen;
        if (!SelectArea(area, &frozen) || g_stopping)
        {
            // if (frozen.pixels.empty()) MessageBoxW(nullptr, L"截图失败", L"识别二维码", MB_ICONERROR);
            return;
        }
        Image image = CropFrozenImage(frozen, area);
        if (image.pixels.empty())
        {
            // MessageBoxW(nullptr, L"截图失败", L"识别二维码", MB_ICONERROR);
            return;
        }
        auto codes = DecodeQrCodes(image.pixels.data(), image.width, image.height);
        if (codes.empty())
        {
            // MessageBoxW(nullptr, L"所选区域未识别到二维码", L"识别二维码", MB_ICONINFORMATION);
            return;
        }
        std::string result;
        for (const auto& code : codes)
        {
            if (!result.empty()) result += "\r\n";
            result += code;
        }
        if (!m_host || g_stopping) return;
        m_host->SetEditText(
            utf8_to_wide(makeCompactJsonWithoutBraces(result)) + (isValidUrl(result) ? L"urlarg " : L"textarg "));
        m_host->ShowMainWindow(SW_SHOW, true);
    }

    // Compare a sparse sample of the previous frame's bottom with the new frame's top.
    // The best overlap is used to discard pixels already present in the output.
    int FindOverlap(const Image& previous, const Image& next)
    {
        const int h = previous.height, w = previous.width;
        if (h != next.height || w != next.width) return 0;
        const int minOverlap = std::max(24, h / 8);
        const int maxOverlap = h - std::max(16, h / 12);
        if (minOverlap >= maxOverlap) return 0;
        int best = 0;
        double bestError = 1e9;
        for (int overlap = minOverlap; overlap <= maxOverlap; ++overlap)
        {
            uint64_t error = 0, samples = 0;
            for (int y = 8; y < overlap - 8; y += 9)
            {
                const int oldY = h - overlap + y;
                for (int x = 8; x < w - 8; x += 11)
                {
                    size_t a = (static_cast<size_t>(oldY) * w + x) * 4;
                    size_t b = (static_cast<size_t>(y) * w + x) * 4;
                    for (int c = 0; c < 3; ++c)
                        error +=
                            std::abs(int(previous.pixels[a + c]) - int(next.pixels[b + c]));
                    samples += 3;
                }
            }
            if (samples && double(error) / samples < bestError)
            {
                bestError = double(error) / samples;
                best = overlap;
            }
        }
        return bestError < 10.0 ? best : 0;
    }

    void ScrollAt(POINT point)
    {
        SetCursorPos(point.x, point.y);
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        input.mi.mouseData = static_cast<DWORD>(-WHEEL_DELTA * 3);
        SendInput(1, &input, sizeof(input));
    }

    void CaptureLongImage()
    {
        RECT area{};
        if (!SelectArea(area) || g_stopping) return;
        POINT center{(area.left + area.right) / 2, (area.top + area.bottom) / 2};
        HWND target = WindowFromPoint(center);
        if (target) SetForegroundWindow(GetAncestor(target, GA_ROOT));
        Sleep(200);
        Image first = Capture(area);
        if (first.pixels.empty())
        {
            if (!g_stopping) MessageBoxW(nullptr, L"截图失败", L"截长图", MB_ICONERROR);
            return;
        }
        Image previous = first;
        std::vector<uint8_t> output = first.pixels;
        int outputHeight = first.height;
        const int maxHeight = std::min(
            30000, static_cast<int>((256ull * 1024 * 1024) / (static_cast<size_t>(first.width) * 4)));
        for (int frame = 1; frame < 30 && outputHeight < maxHeight; ++frame)
        {
            if (g_stopping) return;
            ScrollAt(center);
            Sleep(500);
            Image next = Capture(area);
            if (next.pixels.empty()) break;
            if (next.pixels == previous.pixels) break;
            int overlap = FindOverlap(previous, next);
            if (!overlap) break;
            int added = next.height - overlap;
            if (added < 16 || outputHeight + added > maxHeight) break;
            output.insert(output.end(), next.pixels.begin() + static_cast<size_t>(overlap) * next.width * 4,
                          next.pixels.end());
            outputHeight += added;
            previous = std::move(next);
        }
        if (g_stopping) return;
        PWSTR pictures = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, nullptr, &pictures)))
        {
            MessageBoxW(nullptr, L"无法取得图片目录", L"截长图", MB_ICONERROR);
            return;
        }
        std::filesystem::path folder(pictures);
        CoTaskMemFree(pictures);
        SYSTEMTIME now{};
        GetLocalTime(&now);
        wchar_t filename[80];
        swprintf_s(filename, L"CandyLongShot_%04d%02d%02d_%02d%02d%02d_%03d.png", now.wYear, now.wMonth, now.wDay,
                   now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
        auto path = folder / filename;
        // Write beside the destination so the rename also works when Pictures is on another drive.
        auto temp = path;
        temp += L".tmp";
        // stb's file API uses the active code page on Windows, so write by callback to a wide-path FILE.
        for (size_t i = 0; i < output.size(); i += 4)
        {
            std::swap(output[i], output[i + 2]);
            output[i + 3] = 255;
        }
        FILE* file = nullptr;
        if (_wfopen_s(&file, temp.c_str(), L"wb") != 0 || !file)
        {
            MessageBoxW(nullptr, L"无法创建图片文件", L"截长图", MB_ICONERROR);
            return;
        }
        struct Writer
        {
            FILE* file;
            bool ok = true;
        } writer{file};
        int ok = stbi_write_png_to_func([](void* context, void* bytes, int count)
                                        {
                                            auto* writer = static_cast<Writer*>(context);
                                            if (fwrite(bytes, 1, count, writer->file) != static_cast<size_t>(count))
                                                writer->ok = false;
                                        }, &writer,
                                        first.width, outputHeight, 4, output.data(), first.width * 4);
        if (fclose(file) != 0 || !writer.ok) ok = 0;
        if (!ok || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
        {
            DeleteFileW(temp.c_str());
            if (!g_stopping) MessageBoxW(nullptr, L"保存 PNG 失败", L"截长图", MB_ICONERROR);
            return;
        }
        if (g_stopping) return;
        std::wstring message = L"已保存到：\n" + path.wstring();
        if (outputHeight == first.height) message += L"\n\n未检测到可拼接的滚动内容，仅保存了首屏。";
        MessageBoxW(nullptr, message.c_str(), L"截长图", MB_ICONINFORMATION);
    }

    class ScreenAction final : public candy::ActionBase
    {
    public:
        std::wstring id;
    };
}

inline CommonUtil::PluginHotkeyManager g_hotkeyMgr;

class ScreenToolsPlugin final : public candy::Plugin
{
    const candy::Host* host = nullptr;
    candy::ActionSet<ScreenAction> actions{0};

public:
    bool Initialize() override
    {
        host = &GetHost();
        g_stopping = false;
        return true;
    }

    // host 指针在 Shutdown 等待所有工作线程结束之后才置空，所以线程里用它是安全的
    void StartQrScan() const
    {
        const candy::Host* h = host;
        StartWorker([h] { ReadQrCode(h); });
    }

    static void StartLongCapture()
    {
        StartWorker([] { CaptureLongImage(); });
    }

    void InitScreenToolsHotkeys() const
    {
        g_hotkeyMgr.Clear();

        const std::wstring qrcodeHotkey = host->GetString(L"com.candytek.screentools.hotkey_qrcode_scan");
        if (!g_hotkeyMgr.AddHotkey(0x2800, qrcodeHotkey, [this](int) { StartQrScan(); }))
        {
            Loge(L"ScreenTools", L"Invalid hotkey for qrcode scan: (", qrcodeHotkey, L")");
        }

        const std::wstring longHotkey = host->GetString(L"com.candytek.screentools.hotkey_long_ss");
        if (!g_hotkeyMgr.AddHotkey(0x2801, longHotkey, [](int) { StartLongCapture(); }))
        {
            Loge(L"ScreenTools", L"Invalid hotkey for long screenshot: (", longHotkey, L")");
        }

        g_hotkeyMgr.Start();
    }

    // 只发布两个固定条目，可在工作线程上运行；matchText 留空由主程序处理
    void RefreshActions() override
    {
        std::vector<std::shared_ptr<ScreenAction>> items;
        for (auto entry : {
                 std::tuple{L"long", L"截长图", L"框选滚动区域，自动滚动并保存 PNG"},
                 std::tuple{L"qr", L"框选识别二维码1", L"框选屏幕区域，识别并复制二维码内容"}
             })
        {
            auto action = std::make_shared<ScreenAction>();
            action->id = std::get<0>(entry);
            action->title = std::get<1>(entry);
            action->subtitle = std::get<2>(entry);
            action->stableKey = L"ScreenTools_" + action->id;
            // 列表图标：截长图用 imageres.dll 的相机，二维码识别用 shell32.dll 的放大镜
            action->iconBitmap = action->id == L"long"
                                     ? LoadDllIconAsBitmap(L"imageres.dll", 52)
                                     : LoadDllIconAsBitmap(L"shell32.dll", 22);
            items.push_back(action);
        }
        actions.Publish(GetHost(), std::move(items));
    }

    void OnUserSettingsLoaded() override
    {
        InitScreenToolsHotkeys();
    }

    void OnSettingChanged(const std::wstring& key) override
    {
        Logi(L"ScreenTools", L"setting changed: ", key);
    }

    void Shutdown() override
    {
        // 先拒绝新操作并等正在进行的截图 / 识别结束，再停热键线程，最后才能让 host 失效
        StopWorkers();
        g_hotkeyMgr.Clear();
        actions.ClearLocal();
        host = nullptr;
    }

    bool OnActionExecute(const uint64_t userData, const std::wstring&) override
    {
        const auto selected = actions.Find(userData);
        if (!selected) return false;
        if (selected->id == L"long") StartLongCapture();
        else if (selected->id == L"qr") StartQrScan();
        else return false;
        return true;
    }
};

CANDY_PLUGIN_EXPORT_EX(ScreenToolsPlugin, L"Screen Tools", L"Scrolling screenshots and QR code recognition", L"1.0.0",
                       L"com.candytek.screentools", nullptr, CL_PLUGIN_REFRESH_ANY_THREAD)
