#pragma once

#include <windows.h>
#include <string>
#include <sstream>
#include <filesystem>
#include <cwctype>

// It's good practice to have a helper function for correct Unicode conversion.
// This converts a wstring (from WinAPI controls) to a UTF-8 encoded string (for JSON).
static std::string wide_to_utf8(const std::wstring& wstr) {
	if (wstr.empty()) return {};
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

static std::wstring utf8_to_wide(const std::string& str) {
	if (str.empty()) return {};
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &wstrTo[0], size_needed);
	return wstrTo;
}

// 将 std::string 转换为 std::wstring
static std::wstring StringToWString(const std::string& str) {
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
	std::wstring wstr(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
	// Remove null terminator at the end
	wstr.pop_back();
	return wstr;
}

static std::wstring MyToLower(const std::wstring& str) {
	std::wstring lower = str;
	for (auto& ch : lower) ch = towlower(ch);
	return lower;
}

static std::wstring MyToUpper(const std::wstring& str) {
	std::wstring upper = str;
	for (auto& ch : upper) ch = towupper(ch);
	return upper;
}

static std::string MyToLower(const std::string& str) {
	std::string lower = str;
	for (auto& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
	return lower;
}

static std::string MyToUpper(const std::string& str) {
	std::string upper = str;
	for (auto& ch : upper) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
	return upper;
}

static std::wstring MyTrim(const std::wstring& str) {
	const size_t first = str.find_first_not_of(L" \t\n\r");
	if (first == std::wstring::npos) return L"";
	const size_t last = str.find_last_not_of(L" \t\n\r");
	return str.substr(first, last - first + 1);
}

static std::string MyTrim(const std::string& str) {
	const size_t first = str.find_first_not_of(" \t\n\r");
	if (first == std::string::npos) return "";
	const size_t last = str.find_last_not_of(" \t\n\r");
	return str.substr(first, last - first + 1);
}

// 大小写不敏感的 wstring 后缀判断
inline bool MyEndsWith(const std::wstring& str, const std::wstring& suffix) {
	if (str.size() < suffix.size()) return false;

	auto itStr = str.end() - static_cast<int>(suffix.size());
	auto itSuf = suffix.begin();

	while (itSuf != suffix.end()) {
		if (towlower(*itStr) != towlower(*itSuf)) return false;
		++itStr;
		++itSuf;
	}
	return true;
}

// 多个后缀匹配
inline bool MyEndsWith(const std::wstring& str,
						std::initializer_list<std::wstring> suffixes) {
	for (const auto& suffix : suffixes) {
		if (MyEndsWith(str, suffix)) return true;
	}
	return false;
}

// 大小写敏感的 wstring 前缀判断
inline bool MyStartsWith2(const std::wstring& str, const std::wstring& prefix) {
	if (prefix.size() > str.size()) return false;
	return str.compare(0, prefix.size(), prefix) == 0;
}

// 大小写敏感的 wstring 前缀判断
inline bool MyEndsWith2(const std::wstring& str, const std::wstring& prefix) {
	if (prefix.size() > str.size()) return false;
	return str.compare(str.size() - prefix.size(), prefix.size(), prefix) == 0;
}
// 大小写敏感的 wstring 前缀判断
inline bool MyEndsWith2(const std::string& str, const std::string& prefix) {
	if (prefix.size() > str.size()) return false;
	return str.compare(str.size() - prefix.size(), prefix.size(), prefix) == 0;
}

inline bool MyStartsWith3(const std::wstring& str, const std::wstring& prefix) {
	if (str.size() < prefix.size()) return false;

	auto itStr = str.begin();
	auto itPrefix = prefix.begin();

	while (itPrefix != prefix.end()) {
		if (*itStr != *itPrefix) return false;
		++itStr;
		++itPrefix;
	}
	return true;
}

// 大小写不敏感的 wstring 前缀判断
inline bool MyStartsWith(const std::wstring& str, const std::wstring& prefix) {
	if (str.size() < prefix.size()) return false;

	auto itStr = str.begin();
	auto itPrefix = prefix.begin();

	while (itPrefix != prefix.end()) {
		if (towlower(*itStr) != towlower(*itPrefix)) return false;
		++itStr;
		++itPrefix;
	}
	return true;
}

// 多个后缀匹配
inline bool MyStartsWith(const std::wstring& str,
						std::initializer_list<std::wstring> suffixes) {
	for (const auto& suffix : suffixes) {
		if (MyEndsWith(str, suffix)) return true;
	}
	return false;
}

inline bool IsChineseChar2(const std::wstring& str) {
	return str[0] >= 0x4E00 && str[0] <= 0x9FFF;
}

inline bool IsChineseChar(const wchar_t ch) {
	return ch >= 0x4E00 && ch <= 0x9FFF;
}

inline bool IsChineseChar(const char* str) {
	// UTF-8: 汉字通常占3个字节，范围为 E4~E9 开头
	return (str[0] >= 0xE4 && str[0] <= 0xE9) &&
		(str[1] >= 0x80 && str[1] <= 0xBF) &&
		(str[2] >= 0x80 && str[2] <= 0xBF);
}

// 辅助函数：将使用指定代码页的多字节字符串转换为宽字符串(wstring)
static std::wstring MultiByteToWide(const std::string& mb_str, UINT codePage) {
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

static std::wstring Utf8ToWString(const std::string& str) {
	if (str.empty()) return {};

	const int wideLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
	if (wideLen == 0) throw std::runtime_error("MultiByteToWideChar failed");

	std::wstring wstr(wideLen - 1, 0); // -1 去掉 null terminator
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], wideLen);

	return wstr;
}

static std::string WStringToUtf8(const std::wstring& wstr) {
	if (wstr.empty()) return "";

	const int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (size_needed == 0) throw std::runtime_error("WideCharToMultiByte failed");

	std::string str(size_needed - 1, 0); // -1 去除 null terminator
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size_needed, nullptr, nullptr);

	return str;
}

// 将字符串向量转换为换行分隔的字符串
static std::string VectorToString(const std::vector<std::string>& vec) {
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
static std::wstring VectorToString(const std::vector<std::wstring>& vec) {
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
static std::vector<std::string> StringToVector(const std::string& str) {
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
static std::vector<std::wstring> StringToVector(const std::wstring& str) {
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

// 将换行分隔的字符串转换为字符串向量
static std::vector<std::wstring> StringToVectorAndLower(const std::wstring& str) {
	std::vector<std::wstring> result;
	std::wstringstream ss(str);
	std::wstring line;
	while (std::getline(ss, line)) {
		if (line.back() == '\r') {
			line.pop_back();
		}
		if (!line.empty()) {
			result.push_back(MyToLower(line));
		}
	}
	return result;
}

inline std::vector<std::wstring> SplitWords(const std::wstring& input) {
	std::vector<std::wstring> result;
	size_t start = 0, end = 0;
	while ((end = input.find(L' ', start)) != std::wstring::npos) {
		if (end > start) result.push_back(input.substr(start, end - start));
		start = end + 1;
	}
	if (start < input.size()) result.push_back(input.substr(start));
	return result;
}


// 查找完整 JSON 结束位置（考虑嵌套与字符串引号）
inline size_t find_json_end(const std::string& s) {
	int depth = 0;
	bool in_string = false;
	for (size_t i = 0; i < s.size(); ++i) {
		char c = s[i];
		if (c == '"' && (i == 0 || s[i - 1] != '\\')) in_string = !in_string;
		else if (!in_string) {
			if (c == '{') depth++;
			else if (c == '}') {
				depth--;
				if (depth == 0) return i; // 找到完整 JSON 结束
			}
		}
	}
	return std::string::npos;
}

// 查找完整 JSON 结束位置（考虑嵌套与字符串引号）
inline size_t find_json_end(const std::wstring& s) {
	int depth = 0;
	bool in_string = false;
	for (size_t i = 0; i < s.size(); ++i) {
		wchar_t c = s[i];
		if (c == L'"' && (i == 0 || s[i - 1] != L'\\')) in_string = !in_string;
		else if (!in_string) {
			if (c == L'{') depth++;
			else if (c == L'}') {
				depth--;
				if (depth == 0) return i; // 找到完整 JSON 结束
			}
		}
	}
	return std::wstring::npos;
}

inline std::wstring sanitizeVisible(const std::wstring& input) {
	std::wstring result;
	for (wchar_t ch : input) {
		// iswprint 判断是否为可打印字符（支持 Unicode）
		if (std::iswprint(ch)) {
			result += ch;
		}
	}

	// 去掉首尾空格
	auto start = result.find_first_not_of(L" \t\n\r");
	auto end = result.find_last_not_of(L" \t\n\r");
	if (start == std::wstring::npos) return L"";
	return result.substr(start, end - start + 1);
}

