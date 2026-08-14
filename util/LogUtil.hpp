#pragma once

#include <chrono>
#include <windows.h>
#include <string>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <unordered_map>

#include "StringUtil.hpp"

constexpr bool USE_WIDE_LOG = false; 

template <typename T>
auto format_arg(const T& arg) {
	using RawT = std::decay_t<T>;
	if constexpr (USE_WIDE_LOG) {
		// 开启宽字符日志，统一转为 std::wstring 输出
		if constexpr (std::is_same_v<RawT, std::string>) {
			return utf8_to_wide(arg);
		} else if constexpr (std::is_same_v<RawT, const char*> || std::is_same_v<RawT, char*>) {
			return arg ? utf8_to_wide(arg) : L"";
		} else {
			return arg; 
		}
	} else {
		// 开启窄字符日志，统一转为 std::string 输出
		if constexpr (std::is_same_v<RawT, std::wstring>) {
			return wide_to_utf8(arg);
		} else if constexpr (std::is_same_v<RawT, const wchar_t*> || std::is_same_v<RawT, wchar_t*>) {
			return arg ? wide_to_utf8(arg) : "";
		} else {
			return arg; 
		}
	}
}

// 2. 核心流式写入工具 (避免字符串 + 拼接，直接流式写入，零额外内存分配)
template <typename Stream, typename... Args>
void LogWrite(Stream& stream, const Args&... args) {
	// 展开所有参数并按顺序丢进流里
	((stream << format_arg(args)), ...);
	stream << std::endl;
}

// 普通日志 Logi
template <typename Tag, typename... ExtraArgs>
void Logi(const Tag& tag, const ExtraArgs&... extra) {
	if constexpr (sizeof...(ExtraArgs) == 0) {
		// 只有 1 个参数：直接输出，不加 Tag 前缀
		if constexpr (USE_WIDE_LOG) {
			LogWrite(std::wcout, tag);
		} else {
			LogWrite(std::cout, tag);
		}
	} else {
		// 多于 1 个参数：第一个参数作为 Tag 输出 [Tag]，后面紧跟 extra...
		if constexpr (USE_WIDE_LOG) {
			LogWrite(std::wcout, L"[", tag, L"] ", extra...);
		} else {
			LogWrite(std::cout, "[", tag, "] ", extra...);
		}
	}
}

// Log Error 级
template <typename Tag, typename... ExtraArgs>
void Loge(const Tag& tag, const ExtraArgs&... extra) {
	if constexpr (sizeof...(ExtraArgs) == 0) {
		// 只有 1 个参数：直接输出，不加 Tag
		if constexpr (USE_WIDE_LOG) {
			LogWrite(std::wcerr, L"Error: ", tag);
		} else {
			LogWrite(std::cerr, "Error: ", tag);
		}
	} else {
		// 多于 1 个参数：带 Tag 输出
		if constexpr (USE_WIDE_LOG) {
			LogWrite(std::wcerr, L"[", tag, L"] Error: ", extra...);
		} else {
			LogWrite(std::cerr, "[", tag, "] Error: ", extra...);
		}
	}
}

// Log Error 有缓冲级
template <typename Tag, typename... ExtraArgs>
void Logw(const Tag& tag, const ExtraArgs&... extra) {
	if constexpr (sizeof...(ExtraArgs) == 0) {
		// 只有 1 个参数：直接输出，不加 Tag
		if constexpr (USE_WIDE_LOG) {
			LogWrite(std::wclog, L"Info: ", tag);
		} else {
			LogWrite(std::clog, "Info: ", tag);
		}
	} else {
		// 多于 1 个参数：带 Tag 输出
		if constexpr (USE_WIDE_LOG) {
			LogWrite(std::wclog, L"[", tag, L"] Info: ", extra...);
		} else {
			LogWrite(std::clog, "[", tag, "] Info: ", extra...);
		}
	}
}

static void ShowErrorMsgBox(std::wstring msg) {
	const DWORD err = GetLastError();
	wchar_t buf[256];
	msg += L"，错误代码：%lu";
	wsprintfW(buf, msg.data(), err);
	MessageBoxW(nullptr, buf, L"错误", MB_OK | MB_ICONERROR | MB_TOPMOST);
}

static void ShowErrorMsgBox(const std::string& msg) {
	const DWORD err = GetLastError();
	wchar_t buf[256];

	std::wstring wmsg = StringToWString(msg);
	wmsg += L"，错误代码：%lu";

	wsprintfW(buf, wmsg.c_str(), err);
	MessageBoxW(nullptr, buf, L"错误", MB_OK | MB_ICONERROR | MB_TOPMOST);
}

[[deprecated("Use Logi() instead")]]
static void MyPrintln(const std::wstring& msg) {
	std::wstringstream ss;
	ss << L"" << msg << L"\n";
	OutputDebugStringW(ss.str().c_str());
}

[[deprecated("Use Logi() instead")]]
static void MyPrint(const std::wstring& msg) {
	std::wstringstream ss;
	ss << L"" << msg;
	OutputDebugStringW(ss.str().c_str());
}

inline std::unordered_map<std::wstring, std::chrono::steady_clock::time_point> methodTimerStartTimestamp;

inline void MethodTimerStart(const std::wstring& label = L"Method") {
	methodTimerStartTimestamp[label] = std::chrono::steady_clock::now();
}

inline void MethodTimerEnd(const std::wstring& label = L"Method") {
	auto it = methodTimerStartTimestamp.find(label);

	if (it != methodTimerStartTimestamp.end()) {
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - it->second
		).count();
		Logi(L"MethodTimer", label, L": ", duration, L" ms");
		// methodTimerStartTimestamp.erase(it); // 可选：用完删除
	} else {
		Logi(L"MethodTimer", label, L": timer not found");
	}
}
