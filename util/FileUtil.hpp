#pragma once

#include <windows.h>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <filesystem>

#include "LogUtil.hpp"
#include "StringUtil.hpp"

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


inline std::wstring GetCurrentWorkingDirectoryW() {
	wchar_t buffer[MAX_PATH];
	DWORD result = ::GetCurrentDirectoryW(MAX_PATH, buffer);
	if (result > 0 && result <= MAX_PATH) {
		return std::wstring(buffer);
	}
	return L".";
}


// 从文件读取 JSON，支持 UTF-8 (带/不带 BOM)
inline std::string ReadUtf8File(const std::wstring& configPath) {
	namespace fs = std::filesystem;
	// 打开文件
	fs::path p(configPath);
	std::ifstream in(p, std::ios::binary);
	if (!in) {
		std::wcerr << L"配置文件不存在：" << configPath << std::endl;
		std::string utf8json2;
		return utf8json2;
	}

	// 先打印完整路径
	try {
		auto absPath = fs::absolute(p);
		ConsolePrintln(L"ReadUtf8File", L"Configuration file path: " + absPath.wstring());
	} catch (const std::exception& e) {
		std::wcerr << L"路径解析失败: " << e.what() << std::endl;
	}


	// 读取整个文件
	std::ostringstream buffer;
	buffer << in.rdbuf();
	std::string utf8json = buffer.str();

	// 去掉 UTF-8 BOM
	if (utf8json.size() >= 3 &&
		(unsigned char)utf8json[0] == 0xEF &&
		(unsigned char)utf8json[1] == 0xBB &&
		(unsigned char)utf8json[2] == 0xBF) {
		utf8json.erase(0, 3);
	}
	return utf8json;
}

// 从文件读取 JSON，支持 UTF-8 (带/不带 BOM)
inline std::string ReadUtf8File(const std::string& configPath) {
	return ReadUtf8File(utf8_to_wide(configPath));
}

inline std::string ReadUtf8FileBinary(std::wstring& path) {
	FILE* fp = nullptr;
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
	std::wstring temp = utf8_to_wide(path);
	return ReadUtf8FileBinary(temp);
}


inline bool FolderExists(const std::wstring& folderPath) {
	return std::filesystem::exists(folderPath) && std::filesystem::is_directory(folderPath);
}


inline std::wstring GetFileNameFromPath(const std::wstring& fullPath) {
	// 找最后一个 '\' 或 '/'
	size_t pos = fullPath.find_last_of(L"\\/");
	if (pos == std::wstring::npos) {
		return fullPath; // 没有分隔符，说明整个就是文件名
	}
	return fullPath.substr(pos + 1);
}
