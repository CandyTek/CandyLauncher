#pragma once
#include <algorithm>
#include <windows.h>
#include <string>
#include <sstream>
#include <gdiplus.h>


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

template <typename T>
T MyMax(const T& a, const T& b)
{
	return (a > b) ? a : b;
}

template <typename T>
T MyMin(const T& a, const T& b)
{
	return (a < b) ? a : b;
}

static std::wstring GetExecutableFolder()
{
	wchar_t path[MAX_PATH];
	const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
	if (length == 0 || length == MAX_PATH)
	{
		// 错误处理（可选）
		return L"";
	}

	// 找到最后一个反斜杠（目录分隔符）
	const std::wstring fullPath(path);
	const size_t pos = fullPath.find_last_of(L"\\/");
	if (pos != std::wstring::npos)
	{
		return fullPath.substr(0, pos);
	}

	return L"";
}

static std::wstring GetClipboardText()
{
	// 尝试打开剪贴板
	if (!OpenClipboard(nullptr))
	{
		return nullptr;
	}

	// 获取 Unicode 文本格式的剪贴板数据
	HANDLE hData = GetClipboardData(CF_UNICODETEXT);
	if (hData == nullptr)
	{
		CloseClipboard();
		return nullptr;
	}

	// 锁定内存句柄以获取实际数据指针
	LPCWSTR pszText = static_cast<LPCWSTR>(GlobalLock(hData));
	if (pszText == nullptr)
	{
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

// 从类似 "Ctrl+Alt+A(3)(65)" 字符串中提取并注册全局热键
static bool RegisterHotkeyFromString(HWND hWnd, const std::string& hotkeyStr, int hotkeyId)
{
	UINT modifiers = 0;
	UINT vk = 0;

	// 查找括号中的 VK 值
	size_t posStart = hotkeyStr.rfind('(');
	size_t posEnd = hotkeyStr.rfind(')');

	if (posStart == std::wstring::npos || posEnd == std::wstring::npos || posEnd <= posStart + 1)
	{
		return false;
	}

	std::string vkStr = hotkeyStr.substr(posStart + 1, posEnd - posStart - 1);
	try
	{
		vk = std::stoi(vkStr);
	}
	catch (...)
	{
		return false;
	}

	// 查找 modifiers 值
	posEnd = posStart - 1;
	posStart = hotkeyStr.rfind('(', posEnd);
	if (posStart == std::wstring::npos || posEnd <= posStart + 1)
	{
		return false;
	}

	std::string modStr = hotkeyStr.substr(posStart + 1, posEnd - posStart - 1);
	try
	{
		modifiers = std::stoi(modStr);
	}
	catch (...)
	{
		return false;
	}

	// 取消旧的热键（可选）
	UnregisterHotKey(hWnd, hotkeyId);

	// 注册新的热键
	return RegisterHotKey(hWnd, hotkeyId, modifiers, vk);
}

// 辅助函数：将 #RRGGBB 格式的字符串转为 COLORREF
static COLORREF HexToCOLORREF(const std::string& hex)
{
	if (hex.length() < 7 || hex[0] != '#')
	{
		return RGB(0, 0, 0); // 格式错误返回黑色
	}
	long r = std::stol(hex.substr(1, 2), nullptr, 16);
	long g = std::stol(hex.substr(3, 2), nullptr, 16);
	long b = std::stol(hex.substr(5, 2), nullptr, 16);
	return RGB(r, g, b);
}

// 辅助函数：将 #AARRGGBB 或 #RRGGBB 格式的字符串转为 Gdiplus::Color
static Gdiplus::Color HexToGdiplusColor(const std::string& hex)
{
	if (hex.empty() || hex[0] != '#')
	{
		return Gdiplus::Color(255, 0, 0, 0); // 默认不透明黑色
	}

	// 检查格式：
	// - #RRGGBB（6位） → 默认 Alpha=255（不透明）
	// - #AARRGGBB（8位） → 包含 Alpha 通道
	if (hex.length() == 7) // #RRGGBB
	{
		int r = std::stoi(hex.substr(1, 2), nullptr, 16);
		int g = std::stoi(hex.substr(3, 2), nullptr, 16);
		int b = std::stoi(hex.substr(5, 2), nullptr, 16);
		return Gdiplus::Color(255, r, g, b); // 默认不透明
	}
	else if (hex.length() == 9) // #AARRGGBB
	{
		int a = std::stoi(hex.substr(1, 2), nullptr, 16);
		int r = std::stoi(hex.substr(3, 2), nullptr, 16);
		int g = std::stoi(hex.substr(5, 2), nullptr, 16);
		int b = std::stoi(hex.substr(7, 2), nullptr, 16);
		return Gdiplus::Color(a, r, g, b); // 包含 Alpha
	}
	else
	{
		return Gdiplus::Color(255, 0, 0, 0); // 格式错误返回不透明黑色
	}
}


static bool isValidHexColor(const std::string& color) {
	// 检查长度：6位（#RRGGBB）或8位（#RRGGBBAA）
	if (color.length() != 7 && color.length() != 9) {
		return false;
	}
    
	// 检查是否以#开头
	if (color[0] != '#') {
		return false;
	}
    
	// 检查其余字符是否为十六进制数字
	return std::all_of(color.begin() + 1, color.end(), [](char c) {
		return std::isxdigit(static_cast<unsigned char>(c));
	});
}

static std::string validateHexColor(const std::string& color,const std::string& defaultColor) {
	if (color.empty() || !isValidHexColor(color)) {
		return defaultColor;
	}
	return color;
}

static std::string validateHexColor(const std::string& color) {
	return validateHexColor(color,"#FFFFFF");
}
