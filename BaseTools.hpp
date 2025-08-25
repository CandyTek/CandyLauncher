#pragma once

#include <algorithm>
#include <chrono>
#include <windows.h>
#include <string>
#include <sstream>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <tlhelp32.h>
#include <ShlGuid.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <filesystem>
#include "TraverseOptions.h"

// 标准打印输出方法
static void Println(const std::wstring &msg) {
	std::wstringstream ss;
	ss << L"" << msg << L"\n";
	OutputDebugStringW(ss.str().c_str());
}

static void Print(const std::wstring &msg) {
	std::wstringstream ss;
	ss << L"" << msg;
	OutputDebugStringW(ss.str().c_str());
}

// 将 std::string 转换为 std::wstring
static std::wstring StringToWString(const std::string &str) {
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
	std::wstring wstr(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
	// Remove null terminator at the end
	wstr.pop_back();
	return wstr;
}

// It's good practice to have a helper function for correct Unicode conversion.
// This converts a wstring (from WinAPI controls) to a UTF-8 encoded string (for JSON).
static std::string wide_to_utf8(const std::wstring &wstr) {
	if (wstr.empty()) return {};
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int) wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int) wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

static void ShowErrorMsgBox(std::wstring msg) {
	const DWORD err = GetLastError();
	wchar_t buf[256];
	msg += L"，错误代码：%lu";
	wsprintfW(buf, msg.data(), err);
	MessageBoxW(nullptr, buf, L"错误", MB_OK | MB_ICONERROR);
}

static void ShowErrorMsgBox(const std::string &msg) {
	const DWORD err = GetLastError();
	wchar_t buf[256];

	std::wstring wmsg = StringToWString(msg);
	wmsg += L"，错误代码：%lu";

	wsprintfW(buf, wmsg.c_str(), err);
	MessageBoxW(nullptr, buf, L"错误", MB_OK | MB_ICONERROR);
}

static std::wstring utf8_to_wide(const std::string &str) {
	if (str.empty()) return {};
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &wstrTo[0], size_needed);
	return wstrTo;
}

static std::wstring MyToLower(const std::wstring &str) {
	std::wstring lower = str;
	for (auto &ch: lower) ch = towlower(ch);
	return lower;
}

static std::wstring MyTrim(const std::wstring &str) {
	const size_t first = str.find_first_not_of(L" \t\n\r");
	if (first == std::wstring::npos) return L"";
	const size_t last = str.find_last_not_of(L" \t\n\r");
	return str.substr(first, last - first + 1);
}

template<typename T>
T MyMax(const T &a, const T &b) {
	return (a > b) ? a : b;
}

template<typename T>
T MyMin(const T &a, const T &b) {
	return (a < b) ? a : b;
}

static std::wstring GetExecutableFolder() {
	wchar_t path[MAX_PATH];
	const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
	if (length == 0 || length == MAX_PATH) {
		// 错误处理（可选）
		return L"";
	}

	// 找到最后一个反斜杠（目录分隔符）
	const std::wstring fullPath(path);
	const size_t pos = fullPath.find_last_of(L"\\/");
	if (pos != std::wstring::npos) {
		return fullPath.substr(0, pos);
	}

	return L"";
}

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

inline bool CopyTextToClipboard(HWND hWnd, const std::wstring &text) {
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

	void *p = GlobalLock(hMem);
	memcpy(p, text.c_str(), bytes);
	GlobalUnlock(hMem);
	SetClipboardData(CF_UNICODETEXT, hMem);
	CloseClipboard(); // hMem 的释放权交给剪贴板
	return true;
}

// 辅助函数：将 #RRGGBB 格式的字符串转为 COLORREF
static COLORREF HexToCOLORREF(const std::string &hex) {
	if (hex.length() < 7 || hex[0] != '#') {
		return RGB(0, 0, 0); // 格式错误返回黑色
	}
	long r = std::stol(hex.substr(1, 2), nullptr, 16);
	long g = std::stol(hex.substr(3, 2), nullptr, 16);
	long b = std::stol(hex.substr(5, 2), nullptr, 16);
	return RGB(r, g, b);
}

// 辅助函数：将 #AARRGGBB 或 #RRGGBB 格式的字符串转为 Gdiplus::Color
static Gdiplus::Color HexToGdiplusColor(const std::string &hex) {
	if (hex.empty() || hex[0] != '#') {
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
	} else if (hex.length() == 9) // #AARRGGBB
	{
		int a = std::stoi(hex.substr(1, 2), nullptr, 16);
		int r = std::stoi(hex.substr(3, 2), nullptr, 16);
		int g = std::stoi(hex.substr(5, 2), nullptr, 16);
		int b = std::stoi(hex.substr(7, 2), nullptr, 16);
		return Gdiplus::Color(a, r, g, b); // 包含 Alpha
	} else {
		return Gdiplus::Color(255, 0, 0, 0); // 格式错误返回不透明黑色
	}
}

static bool isValidHexColor(const std::string &color) {
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

static std::string validateHexColor(const std::string &color, const std::string &defaultColor) {
	if (color.empty() || !isValidHexColor(color)) {
		return defaultColor;
	}
	return color;
}

static std::string validateHexColor(const std::string &color) {
	return validateHexColor(color, "#FFFFFF");
}

static std::chrono::time_point<std::chrono::steady_clock> methodTimerStartTimestamp;

class path;

inline void MethodTimerStart() {
	methodTimerStartTimestamp = std::chrono::high_resolution_clock::now();
}

inline void MethodTimerEnd(const std::wstring &label = L"Method") {
	OutputDebugStringW(
			(label + L": " + std::to_wstring(
					std::chrono::duration_cast<std::chrono::milliseconds>(
							std::chrono::high_resolution_clock::now() - methodTimerStartTimestamp).
							count()) + L" ms\n").c_str());
}

// 大小写不敏感的 wstring 后缀判断
inline bool MyEndsWith(const std::wstring &str, const std::wstring &suffix) {
	if (str.size() < suffix.size())
		return false;

	auto itStr = str.end() - static_cast<int>(suffix.size());
	auto itSuf = suffix.begin();

	while (itSuf != suffix.end()) {
		if (towlower(*itStr) != towlower(*itSuf))
			return false;
		++itStr;
		++itSuf;
	}
	return true;
}

// 多个后缀匹配
inline bool MyEndsWith(const std::wstring &str,
					   std::initializer_list<std::wstring> suffixes) {
	for (const auto &suffix: suffixes) {
		if (MyEndsWith(str, suffix))
			return true;
	}
	return false;
}

inline std::wstring GetShortcutTarget(const std::wstring &lnkPath) {
	CoInitialize(nullptr);

	std::wstring target;

	IShellLink *psl = nullptr;
	if (SUCCEEDED(
			CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink,
							 reinterpret_cast<void **>(&psl)
			))) {
		IPersistFile *ppf = nullptr;
		if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&ppf)))) {
			if (SUCCEEDED(ppf->Load(lnkPath.c_str(), STGM_READ))) {
				WCHAR szPath[MAX_PATH];
				WIN32_FIND_DATA wfd = {0};
				if (SUCCEEDED(psl->GetPath(szPath, MAX_PATH, &wfd, SLGP_UNCPRIORITY))) {
					target = szPath;
				}
			}
			ppf->Release();
		}
		psl->Release();
	}

	CoUninitialize();
	return target;
}

inline std::wstring SaveGetShortcutTarget(const std::wstring &lnkPath) {
	DWORD attr = GetFileAttributesW(lnkPath.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES) {
	} else {
		if (MyEndsWith(lnkPath, {L".lnk"})) {
			std::wstring actualPath = GetShortcutTarget(lnkPath);
			return actualPath;
		}
	}
	return L"";
}

inline std::wstring SaveGetShortcutTargetAndReturn(const std::wstring &lnkPath) {
	DWORD attr = GetFileAttributesW(lnkPath.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES) {
	} else {
		if (MyEndsWith(lnkPath, {L".lnk"})) {
			std::wstring actualPath = GetShortcutTarget(lnkPath);
			return actualPath;
		}
	}
	return lnkPath;
}

inline std::wstring GetCurrentWorkingDirectoryW() {
	wchar_t buffer[MAX_PATH];
	DWORD result = ::GetCurrentDirectoryW(MAX_PATH, buffer);
	if (result > 0 && result <= MAX_PATH) {
		return std::wstring(buffer);
	}
	return L".";
}

inline bool IsChineseChar2(const std::wstring& str)
{
	return str[0] >= 0x4E00 && str[0] <= 0x9FFF;
}

inline bool IsChineseChar(const wchar_t ch)
{
	return ch >= 0x4E00 && ch <= 0x9FFF;
}

// 从文件读取 JSON，支持 UTF-8 (带/不带 BOM)
inline std::string ReadUtf8File(const std::wstring& configPath)
{
	// 打开文件
	std::ifstream in(configPath);
	if (!in)
	{
		std::wcerr << L"配置文件不存在：" << configPath << std::endl;
		std::string utf8json2;
		return utf8json2;
	}
	
	// 先打印完整路径
	namespace fs = std::filesystem;
	fs::path p(configPath);
	try
	{
		auto absPath = fs::absolute(p);
		std::wcout << L"配置文件路径: " << absPath.wstring() << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cerr << "路径解析失败: " << e.what() << std::endl;
	}


	// 读取整个文件
	std::ostringstream buffer;
	buffer << in.rdbuf();
	std::string utf8json = buffer.str();

	// 去掉 UTF-8 BOM
	if (utf8json.size() >= 3 &&
		(unsigned char)utf8json[0] == 0xEF &&
		(unsigned char)utf8json[1] == 0xBB &&
		(unsigned char)utf8json[2] == 0xBF)
	{
		utf8json.erase(0, 3);
	}
	return utf8json;
}

// 从文件读取 JSON，支持 UTF-8 (带/不带 BOM)
inline std::string ReadUtf8File(const std::string& configPath)
{
	return ReadUtf8File(utf8_to_wide(configPath));
}

inline std::string ReadUtf8FileBinary(std::wstring& path) {
	FILE *fp = nullptr;
	if (_wfopen_s(&fp, path.c_str(), L"rb") != 0 || !fp) {
		std::wcerr << L"设置基础文件不存在：" << path << std::endl;
		return "";
	}

	std::string data;
	constexpr size_t kBuf = 4096;
	std::vector<char> buf(kBuf);
	size_t n;
	while ((n = fread(buf.data(), 1, buf.size(), fp)) > 0) {
		data.append(buf.data(), n);
	}
	fclose(fp);
	// 如果文件是 UTF-8 BOM，去掉 BOM
	if (data.size() >= 3 &&
		static_cast<unsigned char>(data[0]) == 0xEF &&
		static_cast<unsigned char>(data[1]) == 0xBB &&
		static_cast<unsigned char>(data[2]) == 0xBF) {
		data.erase(0, 3);
	}
	return data;
}

inline std::string ReadUtf8FileBinary(std::string& path) {
	return ReadUtf8FileBinary(utf8_to_wide(path));
}

// 辅助函数：执行命令行并获取其标准输出
static std::string ExecuteCommandAndGetOutput(const std::wstring& command)
{
	HANDLE hChildStd_OUT_Rd = NULL;
	HANDLE hChildStd_OUT_Wr = NULL;
	SECURITY_ATTRIBUTES sa;

	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	// 创建一个管道用于子进程的 STDOUT
	if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &sa, 0))
	{
		throw std::runtime_error("StdoutRd CreatePipe failed");
	}
	if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
	{
		throw std::runtime_error("Stdout SetHandleInformation failed");
	}

	PROCESS_INFORMATION piProcInfo;
	STARTUPINFOW siStartInfo;
	BOOL bSuccess = FALSE;

	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFOW));
	siStartInfo.cb = sizeof(STARTUPINFOW);
	siStartInfo.hStdError = hChildStd_OUT_Wr;
	siStartInfo.hStdOutput = hChildStd_OUT_Wr;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	std::wstring cmd = command;
	std::vector<wchar_t> cmdline(cmd.begin(), cmd.end());
	cmdline.push_back(L'\0');

	bSuccess = CreateProcessW(NULL,
							  cmdline.data(),     // command line 
							  NULL,          // process security attributes 
							  NULL,          // primary thread security attributes 
							  TRUE,          // handles are inherited 
							  CREATE_NO_WINDOW, // creation flags: 不显示命令行窗口
							  NULL,          // use parent's environment 
							  NULL,          // use parent's starting directory 
							  &siStartInfo,  // STARTUPINFO pointer 
							  &piProcInfo);  // receives PROCESS_INFORMATION 

	if (!bSuccess)
	{
		CloseHandle(hChildStd_OUT_Rd);
		CloseHandle(hChildStd_OUT_Wr);
		throw std::runtime_error("CreateProcess failed");
	}

	// 必须关闭写管道句柄，否则 ReadFile 会一直阻塞等待
	CloseHandle(hChildStd_OUT_Wr);

	std::string result;
	DWORD dwRead;
	std::array<char, 4096> buffer;

	// 从管道读取子进程的输出
	for (;;)
	{
		bSuccess = ReadFile(hChildStd_OUT_Rd, buffer.data(), static_cast<DWORD>(buffer.size()), &dwRead, NULL);
		if (!bSuccess || dwRead == 0) break;
		result.append(buffer.data(), dwRead);
	}

	// 清理
	CloseHandle(hChildStd_OUT_Rd);
	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread);

	return result;
}

// 辅助函数：将使用指定代码页的多字节字符串转换为宽字符串(wstring)
static std::wstring MultiByteToWide(const std::string& mb_str, UINT codePage)
{
	if (mb_str.empty()) {
		return std::wstring();
	}
	int wide_len = MultiByteToWideChar(codePage, 0, mb_str.c_str(), -1, NULL, 0);
	if (wide_len == 0) {
		// 可以根据需要进行错误处理
		return std::wstring();
	}
	std::wstring wide_str(wide_len, 0);
	MultiByteToWideChar(codePage, 0, mb_str.c_str(), -1, &wide_str[0], wide_len);
	// MultiByteToWideChar转换后会包含null终止符，我们可能需要去掉
	if (!wide_str.empty() && wide_str.back() == L'\0') {
		wide_str.pop_back();
	}
	return wide_str;
}

static std::wstring Utf8ToWString(const std::string& str)
{
	if (str.empty()) return {};

	const int wideLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
	if (wideLen == 0) throw std::runtime_error("MultiByteToWideChar failed");

	std::wstring wstr(wideLen - 1, 0); // -1 去掉 null terminator
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], wideLen);

	return wstr;
}

static std::string WStringToUtf8(const std::wstring& wstr)
{
	if (wstr.empty()) return "";

	const int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (size_needed == 0) throw std::runtime_error("WideCharToMultiByte failed");

	std::string str(size_needed - 1, 0); // -1 去除 null terminator
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size_needed, nullptr, nullptr);

	return str;
}

static std::wstring ExpandEnvironmentVariables(const std::wstring& path,const std::wstring& exeFolderPath)
{
	if (path.empty()) return path;

	std::wstring processedPath = path;

	// Handle paths starting with backslash by prepending EXE_FOLDER_PATH
	if (path.size() > 0 && path[0] == L'\\')
	{
		processedPath = exeFolderPath + path;
	}

	DWORD size = ExpandEnvironmentStringsW(processedPath.c_str(), nullptr, 0);
	if (size == 0) return processedPath;

	std::wstring expanded(size - 1, 0);
	DWORD result = ExpandEnvironmentStringsW(processedPath.c_str(), &expanded[0], size);
	if (result == 0) return processedPath;

	return expanded;
}

static bool FolderExists(const std::wstring& folderPath)
{
	return std::filesystem::exists(folderPath) && std::filesystem::is_directory(folderPath);
}

// 将字符串向量转换为换行分隔的字符串
static std::string VectorToString(const std::vector<std::string> &vec) {
	std::string result;
	for (size_t i = 0; i < vec.size(); ++i) {
		result += vec[i];
		if (i < vec.size() - 1) {
			result += "\r\n";
		}
	}
	return result;
}

// 将字符串向量转换为换行分隔的字符串
static std::wstring VectorToString(const std::vector<std::wstring> &vec) {
	std::wstring result;
	for (size_t i = 0; i < vec.size(); ++i) {
		result += vec[i];
		if (i < vec.size() - 1) {
			result += L"\r\n";
		}
	}
	return result;
}

// 将换行分隔的字符串转换为字符串向量
static std::vector<std::string> StringToVector(const std::string &str) {
	std::vector<std::string> result;
	std::stringstream ss(str);
	std::string line;
	while (std::getline(ss, line)) {
		if (line.back() == '\r') {
			line.pop_back();
		}
		if (!line.empty()) {
			result.push_back(line);
		}
	}
	return result;
}

// 将换行分隔的字符串转换为字符串向量
static std::vector<std::wstring> StringToVector(const std::wstring &str) {
	std::vector<std::wstring> result;
	std::wstringstream ss(str);
	std::wstring line;
	while (std::getline(ss, line)) {
		if (line.back() == '\r') {
			line.pop_back();
		}
		if (!line.empty()) {
			result.push_back(line);
		}
	}
	return result;
}

