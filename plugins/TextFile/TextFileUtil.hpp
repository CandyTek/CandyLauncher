#pragma once

#include "TextFileAction.hpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <regex>

#include "model/TraverseOptions.hpp"
#include "util/BitmapUtil.hpp"
#include "util/StringUtil.hpp"
#include "util/LogUtil.hpp"

// 展开环境变量
inline std::wstring ExpandEnvironmentVariables(const std::wstring& path) {
	wchar_t expanded[MAX_PATH * 2];
	ExpandEnvironmentStringsW(path.c_str(), expanded, MAX_PATH * 2);
	return std::wstring(expanded);
}

// 解析文件夹路径配置，格式：路径<.ext1|.ext2>
inline bool ParseFolderConfig(const std::wstring& config, std::wstring& folderPath, std::vector<std::wstring>& extensions) {
	size_t pos = config.find(L'<');
	size_t endPos = config.find(L'>', pos);
	bool hasExts = false;
	if (endPos != std::wstring::npos&&pos == std::wstring::npos) {
		return false;
	}
	if (endPos == std::wstring::npos&&pos != std::wstring::npos) {
		return false;
	}
	hasExts=endPos != std::wstring::npos;

	if (hasExts) {
		folderPath = MyTrim(config.substr(0, pos));
	}else {
		folderPath = config;
	}
	folderPath = ExpandEnvironmentVariables(folderPath);
	std::wstring extStr;
	if (hasExts) {
		extStr = config.substr(pos + 1, endPos - pos - 1);
	}

	// 分割扩展名
	size_t start = 0;
	while (start < extStr.length()) {
		size_t pipePos = extStr.find(L'|', start);
		if (pipePos == std::wstring::npos) {
			std::wstring ext = MyTrim(extStr.substr(start));
			if (!ext.empty()) {
				extensions.push_back(ext);
			}
			break;
		}
		std::wstring ext = MyTrim(extStr.substr(start, pipePos - start));
		if (!ext.empty()) {
			extensions.push_back(ext);
		}
		start = pipePos + 1;
	}

	return true;
}

// 读取文本文件内容
inline std::wstring ReadTextFileContent(const std::wstring& filePath, size_t maxSize = 2 * 1024 * 1024) {
	try {
		std::ifstream file(filePath, std::ios::binary | std::ios::ate);
		if (!file.is_open()) {
			return L"";
		}

		std::streamsize size = file.tellg();
		if (size > static_cast<std::streamsize>(maxSize)) {
			return L""; // 文件过大，跳过
		}

		file.seekg(0, std::ios::beg);

		std::vector<char> buffer(static_cast<size_t>(size));
		if (!file.read(buffer.data(), size)) {
			return L"";
		}

		// 尝试作为 UTF-8 读取
		std::string content(buffer.begin(), buffer.end());

		// 检测是否有 BOM
		if (content.size() >= 3 &&
			static_cast<unsigned char>(content[0]) == 0xEF &&
			static_cast<unsigned char>(content[1]) == 0xBB &&
			static_cast<unsigned char>(content[2]) == 0xBF) {
			content = content.substr(3);
		}

		return utf8_to_wide(content);
	} catch (...) {
		return L"";
	}
}

// 获取文件名（不含路径）
inline std::wstring GetFileName(const std::wstring& filePath) {
	size_t pos = filePath.find_last_of(L"\\/");
	if (pos != std::wstring::npos) {
		return filePath.substr(pos + 1);
	}
	return filePath;
}

// 从文本中提取上下文（找到匹配位置的前后文本）
inline std::wstring ExtractContext(const std::wstring& content, const std::wstring& searchText, size_t contextLength = 50) {
	std::wstring lowerContent = MyToLower(content);
	std::wstring lowerSearch = MyToLower(searchText);

	size_t pos = lowerContent.find(lowerSearch);
	if (pos == std::wstring::npos) {
		return L"";
	}

	size_t start = (pos > contextLength) ? (pos - contextLength) : 0;
	size_t end = std::min(pos + searchText.length() + contextLength, content.length());

	std::wstring context = content.substr(start, end - start);

	// 替换换行符为空格
	for (auto& ch : context) {
		if (ch == L'\n' || ch == L'\r' || ch == L'\t') {
			ch = L' ';
		}
	}

	if (start > 0) {
		context = L"..." + context;
	}
	if (end < content.length()) {
		context = context + L"...";
	}

	return context;
}

// 获取所有文本文件
inline std::vector<std::shared_ptr<BaseAction>> GetAllTextFile() {
	std::vector<std::shared_ptr<BaseAction>> actions;

	if (!m_host) {
		return actions;
	}

	try {
		// 获取配置
		auto& settingsMap = m_host->GetSettingsMap();
		auto it = settingsMap.find("com.candytek.textfileplugin.folder_list");
		if (it == settingsMap.end()) {
			return actions;
		}

		const auto& folderListValue = it->second;
		if (folderListValue.stringArr.empty()) {
			return actions;
		}

		const auto& folderList = folderListValue.stringArr;

		// 遍历每个配置项
		for (const auto& configStr : folderList) {
			std::wstring config = utf8_to_wide(configStr);
			std::wstring folderPath;
			std::vector<std::wstring> extensions;

			if (!ParseFolderConfig(config, folderPath, extensions)) {
				ConsolePrintln(L"TextFilePlugin", L"Failed to parse config: " + config);
				continue;
			}

			// 检查文件夹是否存在
			if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath)) {
				ConsolePrintln(L"TextFilePlugin", L"Folder not found: " + folderPath);
				continue;
			}

			ConsolePrintln(L"TextFilePlugin", L"Scanning folder: " + folderPath);

			// 使用 Everything SDK 遍历文件
			TraverseOptions options;
			options.folder = folderPath;
			options.extensions = extensions;
			options.recursive = true;
			options.type = L"path";

			m_host->TraverseFilesForEverythingSDK(folderPath, options, [&](const std::wstring& name,
																			const std::wstring& fullPath,
																			const std::wstring& parent,
																			const std::wstring& ext) {
				try {
					// 读取文件内容
					std::wstring content = ReadTextFileContent(fullPath);
					if (content.empty()) {
						return;
					}

					// 创建 Action
					auto action = std::make_shared<TextFileAction>();
					action->title = name;
					action->iconFilePath = fullPath;
					action->iconFilePathIndex = GetSysImageIndex(fullPath);
					action->matchText = m_host->GetTheProcessedMatchingText(action->title)+MyToLower(sanitizeVisible(content)); // 将全文内容用于匹配

					actions.push_back(action);
				} catch (const std::exception& e) {
					Loge(L"TextFilePlugin", L"Error processing file: " + fullPath, e.what());
				} catch (...) {
					Loge(L"TextFilePlugin", L"Unknown error processing file: " + fullPath, "");
				}
			});
		}
	} catch (const std::exception& e) {
		Loge(L"TextFilePlugin", L"Error in GetAllTextFile", e.what());
	} catch (...) {
		Loge(L"TextFilePlugin", L"Unknown error in GetAllTextFile", "");
	}

	return actions;
}

