// 崩溃捕获：记录最近的程序日志，程序崩溃时弹出窗口显示日志，点击 OK 后彻底退出进程。
// 本文件不依赖项目内其他头文件，崩溃路径中不要调用程序自身的 UI / 皮肤 / GDI+ 代码。

#pragma once

#include <windows.h>
#include <dbghelp.h>
#include <ehdata.h> // ThrowInfo / CatchableType / TypeDescriptor，用于解析未捕获的 C++ 异常对象
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <cstdio>

#include "common/AppVersion.h"

#pragma comment(lib, "dbghelp.lib")

namespace CrashHandler {

// ---------------------------------------------------------------------------
// 日志环形缓冲：挂在 std::cout / std::cerr / std::clog 上，复制每一行到内存
// ---------------------------------------------------------------------------
constexpr size_t MAX_LOG_LINES = 3000;
constexpr size_t MAX_LOG_BYTES = 2 * 1024 * 1024;
constexpr size_t MAX_REPORT_CHARS = 2560 * 1024;
constexpr DWORD MSVC_CPP_EXCEPTION_CODE = 0xE06D7363;
constexpr ULONG_PTR MSVC_CPP_EXCEPTION_MAGIC = 0x19930520;

class LogRing {
public:
	void Append(const char* s, size_t n) {
		std::lock_guard<std::mutex> lock(m_mutex);
		for (size_t i = 0; i < n; ++i) {
			const char c = s[i];
			if (c == '\n') {
				PushLine();
			} else if (c != '\r') {
				m_pending.push_back(c);
			}
		}
	}

	// 崩溃路径调用：尽量不阻塞，拿不到锁也照常读取（接受可能的撕裂数据）
	template <typename Fn>
	void ForEachLine(Fn&& fn) {
		const bool locked = m_mutex.try_lock();
		for (const auto& line : m_lines) fn(line);
		if (!m_pending.empty()) fn(m_pending);
		if (locked) m_mutex.unlock();
	}

private:
	void PushLine() {
		m_totalBytes += m_pending.size();
		m_lines.push_back(std::move(m_pending));
		m_pending.clear();
		while (!m_lines.empty() && (m_lines.size() > MAX_LOG_LINES || m_totalBytes > MAX_LOG_BYTES)) {
			m_totalBytes -= m_lines.front().size();
			m_lines.pop_front();
		}
	}

	std::mutex m_mutex;
	std::deque<std::string> m_lines;
	std::string m_pending;
	size_t m_totalBytes = 0;
};

inline LogRing g_logRing;

// 把写入复制一份到 LogRing，再转发给原始的 streambuf。
// 无控制台时原始 stdout 写入会失败，这里始终返回成功，避免 std::cout 进入 badbit 后丢弃后续所有日志。
class TeeStreamBuf final : public std::streambuf {
public:
	explicit TeeStreamBuf(std::streambuf* original) : m_original(original) {}

#ifdef _MSVC_STL_VERSION
	// MSVC 的 basic_ostream::sentry 在每次 operator<< 期间调用 rdbuf()->_Lock()/_Unlock()，
	// basic_filebuf 用它锁住底层 FILE，内部的 fputc/fwrite 依赖这把锁。
	// 替换 rdbuf 后必须把锁转发给原始 filebuf，否则多线程同时写日志会损坏 FILE 的 _cnt 字段，
	// Debug CRT 在 fwrite 中报 "Inconsistent Stream Count. Flush between consecutive read and write"。
	void _Lock() override {
		if (m_original) m_original->_Lock();
	}

	void _Unlock() override {
		if (m_original) m_original->_Unlock();
	}
#endif

protected:
	int_type overflow(const int_type ch) override {
		if (ch == traits_type::eof()) return traits_type::not_eof(ch);
		const char c = static_cast<char>(ch);
		g_logRing.Append(&c, 1);
		if (m_original) m_original->sputc(c);
		return ch;
	}

	std::streamsize xsputn(const char* s, const std::streamsize n) override {
		g_logRing.Append(s, static_cast<size_t>(n));
		if (m_original) m_original->sputn(s, n);
		return n;
	}

	int sync() override {
		if (m_original) m_original->pubsync();
		return 0;
	}

private:
	std::streambuf* m_original;
};

inline TeeStreamBuf* g_teeCout = nullptr;
inline TeeStreamBuf* g_teeCerr = nullptr;
inline TeeStreamBuf* g_teeClog = nullptr;

// ---------------------------------------------------------------------------
// 崩溃状态
// ---------------------------------------------------------------------------
inline std::atomic<bool> g_crashing{false};
inline LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;
// 崩溃前的清理回调（隐藏主窗口、删除托盘图标等），只允许做最简单的事
inline void (*g_cleanupCallback)() = nullptr;
// 是否在崩溃时生成 crash_dump.dmp
inline bool g_miniDumpEnabled = true;
// 启动时预留容量，崩溃路径中追加不会触发重新分配
// 崩溃报告缓冲。Install 时只预留地址空间（VirtualAlloc MEM_RESERVE，不计入提交内存；以前用 std::wstring::reserve
// 预留 5 MB 堆内存，整个运行期都白占着），崩溃时 Commit 一次性提交后再写入，崩溃路径上仍然不碰堆。
// 只提供 Append* 用到的那部分 std::wstring 接口，写入永不超过容量（Append* 先检查），末尾始终保留 NUL 供 c_str 使用。
class ReportBuffer {
public:
	void Reserve(const size_t chars) {
		if (m_base) return;
		m_reservedChars = chars + 1;
		m_base = static_cast<wchar_t*>(VirtualAlloc(nullptr, m_reservedChars * sizeof(wchar_t), MEM_RESERVE, PAGE_READWRITE));
	}

	// 崩溃路径调用：提交全部预留页。失败时容量保持 0，所有写入成为空操作（报告为空，dump 仍会写）
	void Commit() {
		if (!m_base || m_capacity) return;
		if (VirtualAlloc(m_base, m_reservedChars * sizeof(wchar_t), MEM_COMMIT, PAGE_READWRITE)) {
			m_capacity = m_reservedChars - 1;
			m_base[0] = 0;
		}
	}

	size_t size() const { return m_size; }
	size_t capacity() const { return m_capacity; }
	bool empty() const { return m_size == 0; }
	const wchar_t* data() const { return m_base; }
	const wchar_t* c_str() const { return m_capacity ? m_base : L""; }
	wchar_t& operator[](const size_t i) { return m_base[i]; }
	const wchar_t& operator[](const size_t i) const { return m_base[i]; }

	void clear() {
		m_size = 0;
		if (m_capacity) m_base[0] = 0;
	}

	// 只在容量内改变长度（AppendUtf8 先 resize 再直接写入）
	void resize(const size_t n) {
		if (n > m_capacity) return;
		m_size = n;
		m_base[m_size] = 0;
	}

	void append(const wchar_t* s, const size_t n) {
		if (m_size + n > m_capacity) return;
		memcpy(m_base + m_size, s, n * sizeof(wchar_t));
		m_size += n;
		m_base[m_size] = 0;
	}

private:
	wchar_t* m_base = nullptr;
	size_t m_reservedChars = 0;
	size_t m_capacity = 0;
	size_t m_size = 0;
};

inline ReportBuffer g_report;
inline wchar_t g_exeDir[MAX_PATH] = {};

struct CrashContext {
	const wchar_t* reason = nullptr;
	const wchar_t* detail = nullptr;
	EXCEPTION_POINTERS* exceptionPointers = nullptr;
	CONTEXT capturedContext{};
	bool hasCapturedContext = false;
	DWORD threadId = 0;
};

// ---------------------------------------------------------------------------
// 文本工具（只操作已预留容量的字符串）
// ---------------------------------------------------------------------------
inline bool IsChineseUi() {
	return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE;
}

inline const wchar_t* Tr(const wchar_t* zh, const wchar_t* en) {
	return IsChineseUi() ? zh : en;
}

inline void AppendW(ReportBuffer& out, const wchar_t* s) {
	if (!s) return;
	const size_t n = wcslen(s);
	if (out.size() + n > out.capacity()) return;
	out.append(s, n);
}

inline void AppendUtf8(ReportBuffer& out, const char* s, const size_t n) {
	if (n == 0) return;
	const int needed = MultiByteToWideChar(CP_UTF8, 0, s, static_cast<int>(n), nullptr, 0);
	if (needed <= 0) return;
	const size_t oldSize = out.size();
	if (oldSize + static_cast<size_t>(needed) > out.capacity()) return;
	out.resize(oldSize + static_cast<size_t>(needed));
	MultiByteToWideChar(CP_UTF8, 0, s, static_cast<int>(n), &out[oldSize], needed);
}

inline void AppendHex(ReportBuffer& out, const ULONG_PTR value) {
	wchar_t buf[32];
	swprintf_s(buf, L"0x%llX", static_cast<unsigned long long>(value));
	AppendW(out, buf);
}

inline void AppendUInt(ReportBuffer& out, const unsigned long long value) {
	wchar_t buf[32];
	swprintf_s(buf, L"%llu", value);
	AppendW(out, buf);
}

inline const wchar_t* ExceptionCodeName(const DWORD code) {
	switch (code) {
	case EXCEPTION_ACCESS_VIOLATION: return L"EXCEPTION_ACCESS_VIOLATION";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return L"EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
	case EXCEPTION_DATATYPE_MISALIGNMENT: return L"EXCEPTION_DATATYPE_MISALIGNMENT";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO: return L"EXCEPTION_FLT_DIVIDE_BY_ZERO";
	case EXCEPTION_FLT_INVALID_OPERATION: return L"EXCEPTION_FLT_INVALID_OPERATION";
	case EXCEPTION_ILLEGAL_INSTRUCTION: return L"EXCEPTION_ILLEGAL_INSTRUCTION";
	case EXCEPTION_IN_PAGE_ERROR: return L"EXCEPTION_IN_PAGE_ERROR";
	case EXCEPTION_INT_DIVIDE_BY_ZERO: return L"EXCEPTION_INT_DIVIDE_BY_ZERO";
	case EXCEPTION_INT_OVERFLOW: return L"EXCEPTION_INT_OVERFLOW";
	case EXCEPTION_PRIV_INSTRUCTION: return L"EXCEPTION_PRIV_INSTRUCTION";
	case EXCEPTION_STACK_OVERFLOW: return L"EXCEPTION_STACK_OVERFLOW";
	case EXCEPTION_BREAKPOINT: return L"EXCEPTION_BREAKPOINT";
	case EXCEPTION_INVALID_HANDLE: return L"EXCEPTION_INVALID_HANDLE";
	case MSVC_CPP_EXCEPTION_CODE: return L"C++ exception (not caught)";
	case 0xC0000374: return L"STATUS_HEAP_CORRUPTION";
	default: return L"";
	}
}

// 地址所属模块名及偏移
inline void AppendModuleOffset(ReportBuffer& out, const ULONG_PTR address) {
	HMODULE module = nullptr;
	if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
							reinterpret_cast<LPCWSTR>(address), &module) && module) {
		wchar_t path[MAX_PATH] = {};
		GetModuleFileNameW(module, path, MAX_PATH);
		const wchar_t* name = wcsrchr(path, L'\\');
		AppendW(out, name ? name + 1 : path);
		AppendW(out, L"+");
		AppendHex(out, address - reinterpret_cast<ULONG_PTR>(module));
	} else {
		AppendW(out, L"<unknown module>");
	}
}

// ---------------------------------------------------------------------------
// 调用栈（StackWalk64，有 PDB 时显示符号，没有时显示 模块+偏移）
// ---------------------------------------------------------------------------
inline void AppendStackTrace(ReportBuffer& out, const CONTEXT& context, HANDLE thread) {
	const HANDLE process = GetCurrentProcess();
	SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
	const bool symInit = SymInitializeW(process, nullptr, TRUE) != FALSE;

	CONTEXT ctx = context;
	STACKFRAME64 frame{};
	DWORD machine;
#if defined(_M_X64)
	machine = IMAGE_FILE_MACHINE_AMD64;
	frame.AddrPC.Offset = ctx.Rip;
	frame.AddrFrame.Offset = ctx.Rbp;
	frame.AddrStack.Offset = ctx.Rsp;
#elif defined(_M_IX86)
	machine = IMAGE_FILE_MACHINE_I386;
	frame.AddrPC.Offset = ctx.Eip;
	frame.AddrFrame.Offset = ctx.Ebp;
	frame.AddrStack.Offset = ctx.Esp;
#elif defined(_M_ARM64)
	machine = IMAGE_FILE_MACHINE_ARM64;
	frame.AddrPC.Offset = ctx.Pc;
	frame.AddrFrame.Offset = ctx.Fp;
	frame.AddrStack.Offset = ctx.Sp;
#else
	AppendW(out, L"(stack trace not supported on this architecture)\r\n");
	return;
#endif
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Mode = AddrModeFlat;

	alignas(SYMBOL_INFOW) char symbolBuffer[sizeof(SYMBOL_INFOW) + 256 * sizeof(wchar_t)];
	for (int i = 0; i < 48; ++i) {
		if (!StackWalk64(machine, process, thread, &frame, &ctx, nullptr,
						SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
			break;
		}
		if (frame.AddrPC.Offset == 0) break;

		wchar_t index[16];
		swprintf_s(index, L"  #%02d ", i);
		AppendW(out, index);
		AppendHex(out, static_cast<ULONG_PTR>(frame.AddrPC.Offset));
		AppendW(out, L"  ");
		AppendModuleOffset(out, static_cast<ULONG_PTR>(frame.AddrPC.Offset));

		if (symInit) {
			auto* symbol = reinterpret_cast<SYMBOL_INFOW*>(symbolBuffer);
			memset(symbolBuffer, 0, sizeof(symbolBuffer));
			symbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
			symbol->MaxNameLen = 255;
			DWORD64 displacement = 0;
			if (SymFromAddrW(process, frame.AddrPC.Offset, &displacement, symbol)) {
				AppendW(out, L"  ");
				AppendW(out, symbol->Name);
				AppendW(out, L"+");
				AppendHex(out, static_cast<ULONG_PTR>(displacement));
				IMAGEHLP_LINEW64 line{};
				line.SizeOfStruct = sizeof(line);
				DWORD lineDisplacement = 0;
				if (SymGetLineFromAddrW64(process, frame.AddrPC.Offset, &lineDisplacement, &line)) {
					AppendW(out, L"  (");
					AppendW(out, line.FileName);
					AppendW(out, L":");
					AppendUInt(out, line.LineNumber);
					AppendW(out, L")");
				}
			}
		}
		AppendW(out, L"\r\n");
	}
	if (symInit) SymCleanup(process);
}

// ---------------------------------------------------------------------------
// 落盘：crash_log.txt（UTF-8）与 crash_dump.dmp
// ---------------------------------------------------------------------------
// 分块转换为 UTF-8 写入，不额外申请堆内存
inline bool WriteReportFile(const wchar_t* path) {
	const HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;

	DWORD written = 0;
	bool ok = WriteFile(file, "\xEF\xBB\xBF", 3, &written, nullptr) != FALSE;
	constexpr size_t CHUNK_CHARS = 16 * 1024;
	static char utf8Buffer[CHUNK_CHARS * 3];
	size_t pos = 0;
	while (ok && pos < g_report.size()) {
		size_t count = g_report.size() - pos;
		if (count > CHUNK_CHARS) count = CHUNK_CHARS;
		// 不要在代理对中间切断
		if (pos + count < g_report.size() && IS_HIGH_SURROGATE(g_report[pos + count - 1])) --count;
		const int bytes = WideCharToMultiByte(CP_UTF8, 0, g_report.data() + pos, static_cast<int>(count),
												utf8Buffer, static_cast<int>(sizeof(utf8Buffer)), nullptr, nullptr);
		if (bytes > 0) ok = WriteFile(file, utf8Buffer, static_cast<DWORD>(bytes), &written, nullptr) != FALSE;
		pos += count;
	}
	CloseHandle(file);
	return ok;
}

inline bool WriteMiniDump(const wchar_t* path, const CrashContext& ctx) {
	const HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;

	MINIDUMP_EXCEPTION_INFORMATION info{};
	info.ThreadId = ctx.threadId;
	info.ExceptionPointers = ctx.exceptionPointers;
	info.ClientPointers = FALSE;

	const auto type = static_cast<MINIDUMP_TYPE>(
		MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
	const BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type,
										ctx.exceptionPointers ? &info : nullptr, nullptr, nullptr);
	CloseHandle(file);
	return ok != FALSE;
}

// ---------------------------------------------------------------------------
// 生成报告文本
// ---------------------------------------------------------------------------
inline void BuildReport(const CrashContext& ctx, HANDLE crashedThread, const wchar_t* logPath, const wchar_t* dumpPath) {
	g_report.clear();
	AppendW(g_report, L"CandyLauncher ");
	AppendUtf8(g_report, APP_VERSION_STRING, strlen(APP_VERSION_STRING));
	AppendW(g_report, L" (");
	AppendUtf8(g_report, APP_ARCH_STRING, strlen(APP_ARCH_STRING));
	AppendW(g_report, Tr(L") 已崩溃\r\n", L") has crashed\r\n"));

	SYSTEMTIME st;
	GetLocalTime(&st);
	wchar_t timeBuf[64];
	swprintf_s(timeBuf, L"%04u-%02u-%02u %02u:%02u:%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	AppendW(g_report, Tr(L"时间: ", L"Time: "));
	AppendW(g_report, timeBuf);
	AppendW(g_report, L"\r\n");

	AppendW(g_report, Tr(L"原因: ", L"Reason: "));
	AppendW(g_report, ctx.reason);
	if (ctx.detail && ctx.detail[0]) {
		AppendW(g_report, L" - ");
		AppendW(g_report, ctx.detail);
	}
	AppendW(g_report, L"\r\n");

	if (ctx.exceptionPointers && ctx.exceptionPointers->ExceptionRecord) {
		const EXCEPTION_RECORD* rec = ctx.exceptionPointers->ExceptionRecord;
		AppendW(g_report, Tr(L"异常代码: ", L"Exception code: "));
		AppendHex(g_report, rec->ExceptionCode);
		const wchar_t* name = ExceptionCodeName(rec->ExceptionCode);
		if (name[0]) {
			AppendW(g_report, L" ");
			AppendW(g_report, name);
		}
		if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2) {
			AppendW(g_report, rec->ExceptionInformation[0] == 0 ? Tr(L"（读取 ", L" (read ")
								: rec->ExceptionInformation[0] == 1 ? Tr(L"（写入 ", L" (write ")
								: Tr(L"（执行 ", L" (execute "));
			AppendHex(g_report, rec->ExceptionInformation[1]);
			AppendW(g_report, L")");
		}
		AppendW(g_report, L"\r\n");
		AppendW(g_report, Tr(L"异常地址: ", L"Exception address: "));
		AppendHex(g_report, reinterpret_cast<ULONG_PTR>(rec->ExceptionAddress));
		AppendW(g_report, L"  ");
		AppendModuleOffset(g_report, reinterpret_cast<ULONG_PTR>(rec->ExceptionAddress));
		AppendW(g_report, L"\r\n");
	}

	AppendW(g_report, Tr(L"线程 ID: ", L"Thread id: "));
	AppendUInt(g_report, ctx.threadId);
	AppendW(g_report, L"\r\n");
	AppendW(g_report, Tr(L"日志文件: ", L"Crash log: "));
	AppendW(g_report, logPath);
	AppendW(g_report, L"\r\n");
	if (dumpPath) {
		AppendW(g_report, Tr(L"转储文件: ", L"Minidump: "));
		AppendW(g_report, dumpPath);
		AppendW(g_report, L"\r\n");
	}

	const CONTEXT* context = nullptr;
	if (ctx.exceptionPointers && ctx.exceptionPointers->ContextRecord) context = ctx.exceptionPointers->ContextRecord;
	else if (ctx.hasCapturedContext) context = &ctx.capturedContext;
	if (context && crashedThread) {
		AppendW(g_report, Tr(L"\r\n---- 调用栈 ----\r\n", L"\r\n---- Call stack ----\r\n"));
		AppendStackTrace(g_report, *context, crashedThread);
	}

	AppendW(g_report, Tr(L"\r\n---- 程序日志 ----\r\n", L"\r\n---- Program log ----\r\n"));
	g_logRing.ForEachLine([](const std::string& line) {
		AppendUtf8(g_report, line.data(), line.size());
		AppendW(g_report, L"\r\n");
	});
}

// ---------------------------------------------------------------------------
// 崩溃对话框：独立窗口类 + 只读多行编辑框 + OK 按钮，运行在独立线程上
// ---------------------------------------------------------------------------
constexpr int CRASH_DLG_ID_EDIT = 101;
constexpr int CRASH_DLG_ID_OK = IDOK;
constexpr int CRASH_DLG_MARGIN = 12;
constexpr int CRASH_DLG_BUTTON_W = 110;
constexpr int CRASH_DLG_BUTTON_H = 30;

inline HFONT g_dlgFont = nullptr;
inline HFONT g_dlgMonoFont = nullptr;

inline void TerminateSelf(const UINT exitCode) {
	TerminateProcess(GetCurrentProcess(), exitCode);
}

inline void LayoutCrashDialog(HWND hwnd) {
	RECT rc;
	GetClientRect(hwnd, &rc);
	const int width = rc.right - rc.left;
	const int height = rc.bottom - rc.top;
	const HWND edit = GetDlgItem(hwnd, CRASH_DLG_ID_EDIT);
	const HWND ok = GetDlgItem(hwnd, CRASH_DLG_ID_OK);
	const int editHeight = height - CRASH_DLG_MARGIN * 3 - CRASH_DLG_BUTTON_H;
	if (edit) {
		MoveWindow(edit, CRASH_DLG_MARGIN, CRASH_DLG_MARGIN,
					width - CRASH_DLG_MARGIN * 2, editHeight > 0 ? editHeight : 0, TRUE);
	}
	if (ok) {
		MoveWindow(ok, width - CRASH_DLG_MARGIN - CRASH_DLG_BUTTON_W,
					height - CRASH_DLG_MARGIN - CRASH_DLG_BUTTON_H,
					CRASH_DLG_BUTTON_W, CRASH_DLG_BUTTON_H, TRUE);
	}
}

inline LRESULT CALLBACK CrashDialogProc(HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
	switch (msg) {
	case WM_CREATE:
		{
			const HINSTANCE inst = reinterpret_cast<LPCREATESTRUCTW>(lParam)->hInstance;
			const HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
												WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY
												| ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
												0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(CRASH_DLG_ID_EDIT)), inst, nullptr);
			const HWND ok = CreateWindowExW(0, L"BUTTON", L"OK",
											WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
											0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(CRASH_DLG_ID_OK)), inst, nullptr);
			g_dlgMonoFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
										OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
										FIXED_PITCH | FF_MODERN, L"Consolas");
			g_dlgFont = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
									OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
									DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
			if (edit) {
				if (g_dlgMonoFont) SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(g_dlgMonoFont), TRUE);
				SendMessageW(edit, EM_SETLIMITTEXT, 0, 0);
				SetWindowTextW(edit, g_report.c_str());
			}
			if (ok && g_dlgFont) SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(g_dlgFont), TRUE);
			LayoutCrashDialog(hwnd);
			if (ok) SetFocus(ok);
			return 0;
		}
	case WM_SIZE:
		LayoutCrashDialog(hwnd);
		return 0;
	case WM_GETMINMAXINFO:
		{
			auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
			mmi->ptMinTrackSize.x = 420;
			mmi->ptMinTrackSize.y = 240;
			return 0;
		}
	case WM_COMMAND:
		if (LOWORD(wParam) == CRASH_DLG_ID_OK || LOWORD(wParam) == IDCANCEL) {
			DestroyWindow(hwnd);
			return 0;
		}
		break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default: break;
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

inline void ShowCrashDialog() {
	const HINSTANCE inst = GetModuleHandleW(nullptr);
	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = CrashDialogProc;
	wc.hInstance = inst;
	wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
	wc.lpszClassName = L"CandyLauncherCrashDialog";
	wc.hIcon = LoadIconW(nullptr, IDI_ERROR);
	wc.hIconSm = wc.hIcon;
	if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;

	const int screenW = GetSystemMetrics(SM_CXSCREEN);
	const int screenH = GetSystemMetrics(SM_CYSCREEN);
	const int width = 820;
	const int height = 560;
	const HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_APPWINDOW, wc.lpszClassName,
										Tr(L"CandyLauncher 已崩溃", L"CandyLauncher has crashed"),
										WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
										(screenW - width) / 2, (screenH - height) / 2, width, height,
										nullptr, nullptr, inst, nullptr);
	if (!hwnd) return;
	ShowWindow(hwnd, SW_SHOWNORMAL);
	SetForegroundWindow(hwnd);
	MessageBeep(MB_ICONERROR);

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0)) {
		if (IsDialogMessageW(hwnd, &msg)) continue;
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}

// 不含任何需要析构的 C++ 对象，供 __try 包裹清理回调
inline void SafeCallCleanup() {
	__try {
		if (g_cleanupCallback) g_cleanupCallback();
	} __except (EXCEPTION_EXECUTE_HANDLER) {
	}
}

// ---------------------------------------------------------------------------
// 崩溃主流程（运行在新线程上，崩溃线程阻塞等待）
// ---------------------------------------------------------------------------
inline DWORD WINAPI CrashWorkerThread(LPVOID param) {
	auto* ctx = static_cast<CrashContext*>(param);

	wchar_t logPath[MAX_PATH];
	wchar_t dumpPath[MAX_PATH];
	swprintf_s(logPath, L"%s\\crash_log.txt", g_exeDir);
	swprintf_s(dumpPath, L"%s\\crash_dump.dmp", g_exeDir);

	const HANDLE crashedThread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION | THREAD_SUSPEND_RESUME,
											FALSE, ctx->threadId);

	SafeCallCleanup();
	const bool dumpWritten = g_miniDumpEnabled && WriteMiniDump(dumpPath, *ctx);
	// 报告缓冲在这里才真正提交内存（Install 只预留了地址空间）
	g_report.Commit();
	BuildReport(*ctx, crashedThread, logPath, dumpWritten ? dumpPath : nullptr);
	WriteReportFile(logPath);
	if (crashedThread) CloseHandle(crashedThread);

	ShowCrashDialog();
	TerminateSelf(1);
	return 0;
}

inline void HandleCrash(CrashContext& ctx) {
	// 崩溃处理期间再次崩溃：直接结束进程，避免递归
	if (g_crashing.exchange(true)) {
		TerminateSelf(3);
	}
	// 调试器附加时（如 CLion）先在此断下，便于查看 abort / terminate / 无效参数的调用点；继续运行则走正常的报告流程
	if (IsDebuggerPresent()) {
		__debugbreak();
	}
	ctx.threadId = GetCurrentThreadId();
	if (!ctx.exceptionPointers && !ctx.hasCapturedContext) {
		RtlCaptureContext(&ctx.capturedContext);
		ctx.hasCapturedContext = true;
	}

	// 栈溢出时当前线程已无可用栈空间，所有工作都放到新线程执行
	const HANDLE worker = CreateThread(nullptr, 1024 * 1024, CrashWorkerThread, &ctx, 0, nullptr);
	if (worker) {
		WaitForSingleObject(worker, INFINITE);
		CloseHandle(worker);
	} else {
		CrashWorkerThread(&ctx);
	}
	TerminateSelf(1);
}

// ---------------------------------------------------------------------------
// 各类入口
// ---------------------------------------------------------------------------
// RTTI 修饰名转为可读名：".?AVruntime_error@std@@" -> "std::runtime_error"
inline void FormatDecoratedTypeName(const char* decorated, char* out, const size_t outLen) {
	out[0] = 0;
	if (!decorated) return;
	const char* p = decorated;
	if (strncmp(p, ".?AV", 4) == 0 || strncmp(p, ".?AU", 4) == 0 || strncmp(p, ".?AW", 4) == 0) p += 4;
	else {
		strncpy_s(out, outLen, decorated, _TRUNCATE);
		return;
	}
	// 以 '@' 分隔，顺序为内层到外层，需要反转
	const char* parts[16];
	size_t lengths[16];
	int count = 0;
	while (*p && *p != '@' && count < 16) {
		const char* start = p;
		while (*p && *p != '@') ++p;
		parts[count] = start;
		lengths[count] = static_cast<size_t>(p - start);
		++count;
		if (*p == '@') ++p;
	}
	size_t used = 0;
	for (int i = count - 1; i >= 0; --i) {
		if (used + lengths[i] + 3 >= outLen) break;
		memcpy(out + used, parts[i], lengths[i]);
		used += lengths[i];
		if (i > 0) {
			memcpy(out + used, "::", 2);
			used += 2;
		}
	}
	out[used] = 0;
}

// 未捕获的 C++ 异常在 MSVC 中先到达顶层过滤器（而不是 std::terminate）。
// 这里从异常记录里找出被抛出的对象，若派生自 std::exception 则取出 what()。
// 只用裸指针和栈缓冲，便于用 __try 包裹。
inline void ExtractCppExceptionDetail(const EXCEPTION_RECORD* rec, wchar_t* out, const size_t outLen) {
	out[0] = 0;
	__try {
		if (rec->NumberParameters < 3 || rec->ExceptionInformation[0] != MSVC_CPP_EXCEPTION_MAGIC) return;
		const char* object = reinterpret_cast<const char*>(rec->ExceptionInformation[1]);
		const ThrowInfo* throwInfo = reinterpret_cast<const ThrowInfo*>(rec->ExceptionInformation[2]);
		if (!object || !throwInfo) return;
#if _EH_RELATIVE_TYPEINFO
		if (rec->NumberParameters < 4) return;
		const ULONG_PTR imageBase = rec->ExceptionInformation[3];
		const CatchableTypeArray* types = reinterpret_cast<const CatchableTypeArray*>(imageBase + throwInfo->pCatchableTypeArray);
#else
		const CatchableTypeArray* types = throwInfo->pCatchableTypeArray;
#endif
		if (!types || types->nCatchableTypes <= 0) return;

		char typeName[512] = {};
		const char* whatText = nullptr;
		for (int i = 0; i < types->nCatchableTypes; ++i) {
#if _EH_RELATIVE_TYPEINFO
			const CatchableType* ct = reinterpret_cast<const CatchableType*>(imageBase + types->arrayOfCatchableTypes[i]);
			const TypeDescriptor* td = reinterpret_cast<const TypeDescriptor*>(imageBase + ct->pType);
#else
			const CatchableType* ct = types->arrayOfCatchableTypes[i];
			const TypeDescriptor* td = ct->pType;
#endif
			if (!ct || !td) continue;
			// 第一个即实际抛出的类型
			if (i == 0) FormatDecoratedTypeName(td->name, typeName, sizeof(typeName));
			if (strcmp(td->name, ".?AVexception@std@@") == 0 && ct->thisDisplacement.pdisp < 0) {
				const auto* e = reinterpret_cast<const std::exception*>(object + ct->thisDisplacement.mdisp);
				whatText = e->what();
			}
		}

		char utf8[1024];
		if (whatText) sprintf_s(utf8, "%s: %s", typeName, whatText);
		else strcpy_s(utf8, typeName);
		MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, static_cast<int>(outLen));
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		out[0] = 0;
	}
}

inline LONG WINAPI UnhandledExceptionFilterImpl(EXCEPTION_POINTERS* ep) {
	if (IsDebuggerPresent()) return EXCEPTION_CONTINUE_SEARCH;
	static wchar_t detail[1024] = {};
	CrashContext ctx;
	ctx.reason = Tr(L"未处理的异常", L"Unhandled exception");
	ctx.exceptionPointers = ep;
	if (ep && ep->ExceptionRecord && ep->ExceptionRecord->ExceptionCode == MSVC_CPP_EXCEPTION_CODE) {
		ExtractCppExceptionDetail(ep->ExceptionRecord, detail, 1024);
		ctx.detail = detail;
	}
	HandleCrash(ctx);
	return EXCEPTION_EXECUTE_HANDLER;
}

inline void TerminateHandlerImpl() {
	static wchar_t detail[512] = {};
	detail[0] = 0;
	try {
		if (const std::exception_ptr current = std::current_exception()) std::rethrow_exception(current);
	} catch (const std::exception& e) {
		MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, detail, 511);
	} catch (...) {
		wcscpy_s(detail, L"non-std exception");
	}
	CrashContext ctx;
	ctx.reason = Tr(L"std::terminate（未捕获的 C++ 异常）", L"std::terminate (uncaught C++ exception)");
	ctx.detail = detail;
	HandleCrash(ctx);
}

inline void InvalidParameterHandlerImpl(const wchar_t* expression, const wchar_t* function,
										const wchar_t* file, const unsigned line, uintptr_t) {
	static wchar_t detail[512] = {};
	if (expression || function || file) {
		swprintf_s(detail, L"%s in %s (%s:%u)", expression ? expression : L"", function ? function : L"",
					file ? file : L"", line);
	} else {
		detail[0] = 0;
	}
	CrashContext ctx;
	ctx.reason = Tr(L"CRT 无效参数", L"Invalid CRT parameter");
	ctx.detail = detail;
	HandleCrash(ctx);
}

inline void PureCallHandlerImpl() {
	CrashContext ctx;
	ctx.reason = Tr(L"纯虚函数调用", L"Pure virtual function call");
	HandleCrash(ctx);
}

inline void AbortSignalHandlerImpl(int) {
	CrashContext ctx;
	ctx.reason = L"abort()";
	HandleCrash(ctx);
}

// 插件 DLL 或第三方库可能覆盖顶层异常过滤器，插件加载后需要重新安装
inline void ReinstallExceptionFilter() {
	SetUnhandledExceptionFilter(UnhandledExceptionFilterImpl);
}

// 在 wWinMain 最开始调用一次
inline void Install() {
	g_report.Reserve(MAX_REPORT_CHARS);

	GetModuleFileNameW(nullptr, g_exeDir, MAX_PATH);
	if (wchar_t* slash = wcsrchr(g_exeDir, L'\\')) *slash = 0;

	if (!g_teeCout) {
		g_teeCout = new TeeStreamBuf(std::cout.rdbuf());
		g_teeCerr = new TeeStreamBuf(std::cerr.rdbuf());
		g_teeClog = new TeeStreamBuf(std::clog.rdbuf());
		std::cout.rdbuf(g_teeCout);
		std::cerr.rdbuf(g_teeCerr);
		std::clog.rdbuf(g_teeClog);
	}

	g_previousFilter = SetUnhandledExceptionFilter(UnhandledExceptionFilterImpl);
	std::set_terminate(TerminateHandlerImpl);
	_set_invalid_parameter_handler(InvalidParameterHandlerImpl);
	_set_purecall_handler(PureCallHandlerImpl);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	signal(SIGABRT, AbortSignalHandlerImpl);
	// 不弹出系统的错误报告框，由本模块接管
	SetErrorMode(SetErrorMode(0) | SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
}

inline void SetCleanupCallback(void (*callback)()) {
	g_cleanupCallback = callback;
}

// 关闭后崩溃时只生成 crash_log.txt，不生成 crash_dump.dmp
inline void SetMiniDumpEnabled(const bool enabled) {
	g_miniDumpEnabled = enabled;
}

// 测试用：--crash-test[=access|terminate|abort]
#pragma optimize("", off)
inline void TriggerTestCrash(const wchar_t* kind) {
	if (kind && wcsncmp(kind, L"terminate", 9) == 0) {
		throw std::runtime_error("crash test: uncaught exception");
	}
	if (kind && wcsncmp(kind, L"abort", 5) == 0) {
		abort();
	}
	volatile int* p = nullptr;
	*p = 42;
}
#pragma optimize("", on)

} // namespace CrashHandler
