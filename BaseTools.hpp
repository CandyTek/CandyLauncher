#pragma once
#include <windows.h>
#include <string>
#include <sstream>


// 标准打印输出方法
static void Println(const std::wstring& msg)
{
	std::wstringstream ss;
	ss << L"" << msg << L"\n";
	OutputDebugStringW(ss.str().c_str());
}

static void Print(const std::wstring& msg)
{
	std::wstringstream ss;
	ss << L"" << msg;
	OutputDebugStringW(ss.str().c_str());
}

// 将 std::string 转换为 std::wstring
static std::wstring StringToWString(const std::string& str)
{
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
	std::wstring wstr(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
	// Remove null terminator at the end
	wstr.pop_back();
	return wstr;
}

// It's good practice to have a helper function for correct Unicode conversion.
// This converts a wstring (from WinAPI controls) to a UTF-8 encoded string (for JSON).
static std::string wide_to_utf8(const std::wstring& wstr)
{
	if (wstr.empty()) return {};
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}


static void ShowErrorMsgBox(std::wstring msg)
{
	const DWORD err = GetLastError();
	wchar_t buf[256];
	msg += L"，错误代码：%lu";
	wsprintfW(buf, msg.data(), err);
	MessageBoxW(nullptr, buf, L"错误", MB_OK | MB_ICONERROR);
}

static void ShowErrorMsgBox(const std::string& msg)
{
	const DWORD err = GetLastError();
	wchar_t buf[256];

	std::wstring wmsg = StringToWString(msg);
	wmsg += L"，错误代码：%lu";

	wsprintfW(buf, wmsg.c_str(), err);
	MessageBoxW(nullptr, buf, L"错误", MB_OK | MB_ICONERROR);
}


static std::wstring utf8_to_wide(const std::string& str)
{
	if (str.empty()) return {};
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &wstrTo[0], size_needed);
	return wstrTo;
}

static std::wstring MyToLower(const std::wstring& str)
{
	std::wstring lower = str;
	for (auto& ch : lower) ch = towlower(ch);
	return lower;
}

static std::wstring MyTrim(const std::wstring& str)
{
	const size_t first = str.find_first_not_of(L" \t\n\r");
	if (first == std::wstring::npos) return L"";
	const size_t last = str.find_last_not_of(L" \t\n\r");
	return str.substr(first, last - first + 1);
}

template<typename T>
T MyMax(const T& a, const T& b) {
	return (a > b) ? a : b;
}

template<typename T>
T MyMin(const T& a, const T& b) {
	return (a < b) ? a : b;
}
