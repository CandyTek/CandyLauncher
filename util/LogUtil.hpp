#pragma once

#include <chrono>
#include <windows.h>
#include <string>
#include <sstream>
#include <filesystem>

#include "StringUtil.hpp"


// 标准打印输出方法
static void Println(const std::wstring& msg) {
	std::wstringstream ss;
	ss << L"" << msg << L"\n";
	OutputDebugStringW(ss.str().c_str());
}

static void Print(const std::wstring& msg) {
	std::wstringstream ss;
	ss << L"" << msg;
	OutputDebugStringW(ss.str().c_str());
}


static void ConsolePrintln(const std::wstring& msg) {
	// 将宽字符转换为UTF-8然后输出，避免wcout中文显示问题
	// const std::string utf8_msg = wide_to_utf8(msg);
	// ConsolePrintln(utf8_msg);
	// std::wcout << msg << std::endl;
	std::cout << wide_to_utf8(msg) << std::endl;


	// wprintf( L"%s\n", msg.c_str() );
	// printf( "%s\n", (wide_to_utf8(msg)).c_str() );
	// std::wcerr << msg << std::endl;
}

static void ConsolePrintln(const std::string& msg) {
	// printf("%s\n", msg.c_str());
	// std::wcout << utf8_to_wide(msg) << std::endl;
	ConsolePrintln(utf8_to_wide(msg));
}

static void ConsolePrintln(const std::wstring& tag, const std::wstring& msg) {
	ConsolePrintln(L"[" + tag + L"]" + msg);
}

static void ConsolePrintln(const std::string& tag, const std::string& msg) {
	ConsolePrintln("[" + tag + "]" + msg);
}

static void ConsolePrintln(const std::wstring& tag, const std::string& msg) {
	ConsolePrintln("[" + wide_to_utf8(tag) + "]" + msg);
}

static void Loge(const std::string& tag, const std::string& msg, char const* errorWhat) {
	std::wcerr << L"[" + utf8_to_wide(tag) + L"]Error: " + utf8_to_wide(msg) << errorWhat << std::endl;
}

static void Loge(const std::string& tag, const std::wstring& msg, char const* errorWhat) {
	std::wcerr << L"[" + utf8_to_wide(tag) + L"]Error: " + msg << errorWhat << std::endl;
}

static void Loge(const std::wstring& tag, const std::string& msg, char const* errorWhat) {
	std::wcerr << L"[" + tag + L"]Error: " << utf8_to_wide(msg) << errorWhat << std::endl;
}

static void Loge(const std::wstring& tag, const std::wstring& msg, char const* errorWhat) {
	std::wcerr << L"[" + tag + L"]Error: " + msg << errorWhat << std::endl;
}

static void Logi(const std::wstring& tag, const std::string& msg, char const* errorWhat) {
	std::wclog << L"[" + tag + L"]Error: " << utf8_to_wide(msg) << errorWhat << std::endl;
}

static void Logi(const std::wstring& tag, const std::wstring& msg, char const* errorWhat) {
	std::wclog << L"[" + tag + L"]Error: " + msg << errorWhat << std::endl;
}

static void Loge(const std::string& tag, const std::string& msg) {
	std::wcerr << L"[" + utf8_to_wide(tag) + L"]Error: " + utf8_to_wide(msg) << std::endl;
}

static void Loge(const std::string& tag, const std::wstring& msg) {
	std::wcerr << L"[" + utf8_to_wide(tag) + L"]Error: " + msg << std::endl;
}

static void Loge(const std::wstring& tag, const std::string& msg) {
	std::wcerr << L"[" + tag + L"]Error: " + utf8_to_wide(msg) << std::endl;
}

static void Loge(const std::wstring& tag, const std::wstring& msg) {
	std::wcerr << L"[" + tag + L"]Error: " + msg << std::endl;
}

static void PluginConsolePrintln(const std::wstring& tag, const std::wstring& msg) {
	std::wcout << L"[" + tag + L"]" + msg << std::endl;
}

static void PluginConsolePrintln(const std::string& tag, const std::string& msg) {
	PluginConsolePrintln(utf8_to_wide(tag), utf8_to_wide(msg));
}

static void ConsolePrint(const std::wstring& msg) {
	std::wstringstream ss;
	ss << L"" << msg;
	OutputDebugStringW(ss.str().c_str());
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


inline std::chrono::steady_clock::time_point methodTimerStartTimestamp;

inline void MethodTimerStart() {
	methodTimerStartTimestamp = std::chrono::steady_clock::now();
}
inline void MethodTimerEnd(const std::wstring& label = L"Method") {
	ConsolePrintln(
		label + L": " + std::to_wstring(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - methodTimerStartTimestamp).
			count()) + L" ms\n");
}
