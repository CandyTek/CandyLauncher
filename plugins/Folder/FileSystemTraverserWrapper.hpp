#pragma once


#include <windows.h>
#include <iostream>
#include <sstream>
#include <algorithm>

#include <shellapi.h>
#include <filesystem>
#include "../../common/Constants.hpp"
#include "../../model/TraverseOptions.hpp"
#include "util/FileSystemTraverser.hpp"
#include "util/RegistryAppsTraverser.hpp"
#include <psapi.h>

#include "FileAction.hpp"
#pragma comment(lib, "Psapi.lib")


static void TraverseFiles(
	const std::wstring& folderPath,
	const TraverseOptions& options,
	std::vector<std::shared_ptr<FileAction>>& outActions
) {
	TraverseFiles(folderPath, options, EXE_FOLDER_PATH2, [&](const std::wstring& name, const std::wstring& fullPath,
															const std::wstring& parent, const std::wstring& ext) {
		const auto action = std::make_shared<FileAction>(
			name, fullPath, false, parent, L""
		);
		outActions.push_back(action);
	});
}

/*static void TraverseFilesForEverything(
		const std::wstring &folderPath,
		const TraverseOptions &options,
		std::vector<std::shared_ptr<RunCommandAction>> &outActions) {
	if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) return;

	// addFile lambda 保持不变，可以完美复用
	auto addFile = [&](const fs::path &path) {
		std::wstring filename = path.filename().wstring(); // without extension

		if (shouldExclude(options, filename)) return;

		// 重命名映射
		if (const auto it = options.renameMap.find(filename); it != options.renameMap.end()) {
			filename = it->second;
		}

		const auto action = std::make_shared<RunCommandAction>(
				filename, path.wstring(), false, true, path.parent_path().wstring()
		);

		outActions.push_back(action);
	};

	// 1. 构建 Everything 查询命令
	std::wstringstream command;
	// 假设 es.exe 在 PATH 中，或者提供完整路径
	command << L"D:\\Download3\\ES-1.1.0.30.x64\\es.exe -utf8-bom ";

	// -p 指定搜索路径
	command << L"-p \"" << folderPath << L"\" ";

	// 从扩展名列表动态生成正则表达式
	if (!options.extensions.empty()) {
		std::wstringstream regex_stream;
		regex_stream << L"\"("; // 正则表达式部分用引号括起来

		for (size_t i = 0; i < options.extensions.size(); ++i) {
			std::wstring ext = options.extensions[i];

			// 为正则表达式转义特殊字符，尤其是 '.'
			std::wstring escaped_ext;
			for (wchar_t c: ext) {
				if (c == L'.' || c == L'\\' || c == L'?' || c == L'*' || c == L'+' || c == L'(' || c == L')' ||
					c == L'[' || c == L']' || c == L'{' || c == L'}' || c == L'^' || c == L'$') {
					escaped_ext += L'\\';
				}
				escaped_ext += c;
			}

			regex_stream << escaped_ext << (i < options.extensions.size() - 1 ? L"|" : L"");
		}
		regex_stream << L")$\""; // 以 $ 结尾，确保是文件扩展名

		// -r 指定正则表达式
		command << L"-r " << regex_stream.str();
	}

	try {
		// 2. 执行命令并获取纯文本输出
		std::string commandOutput = ExecuteCommandAndGetOutput(command.str());
		//        std::cout << commandOutput << std::endl;

		if (commandOutput.empty()) return;

		// 3. 逐行解析输出
		std::stringstream ss(commandOutput);
		std::string line;
		fs::path searchFolderPath(folderPath); // 预先创建path对象用于比较

		while (std::getline(ss, line)) {
			if (line.empty()) continue;
			line.erase(line.find_last_not_of("\r\n") + 1);
			// es.exe 的输出是UTF-8编码，使用u8path可以正确处理包含非英文字符的路径
			//            fs::path filePath = fs::u8path(line);
			std::wstring wide_path_str = MultiByteToWide(line, CP_ACP); // CP_ACP 表示系统的当前活动代码页
			// 直接用 wstring 构造 path 对象，这是在Windows上最可靠的方式
			//            std::cout << WStringToUtf8(wide_path_str) << std::endl;

			fs::path filePath(wide_path_str);
			//            std::cout << WStringToUtf8(filePath.wstring()) << std::endl;

			// 4. 如果是非递归搜索，需要额外判断父目录是否匹配
			if (!options.recursive) {
				if (filePath.parent_path() != searchFolderPath) {
					continue; // 如果父目录不匹配，则跳过此文件
				}
			}

			// 复用 addFile 逻辑
			//            std::cout << filePath << std::endl;
			addFile(filePath);
		}
	}
	catch (const std::exception &e) {
		// 错误处理，可以考虑回退到原始的文件系统遍历方法
		// e.g., LogError("Failed to search with Everything: " + std::string(e.what()));
		std::wcout << (L"Failed to search with Everything: " + utf8_to_wide(std::string(e.what()))) << std::endl;

	}
}
*/


static void TraverseRegistryApps(
	std::vector<std::shared_ptr<FileAction>>& outActions, const TraverseOptions& options) {
	TraverseRegistryApps([&](const std::wstring& name,
							const std::wstring& fullPath,
							const std::wstring& workingdir) {
		const auto action = std::make_shared<FileAction>(
			name, fullPath, false, workingdir
		);
		outActions.push_back(action);
	}, options);
}


// 索引PATH环境变量中的所有可执行文件
static void TraversePATHExecutables2(std::vector<std::shared_ptr<FileAction>>& outActions, TraverseOptions& options) {
	TraversePATHExecutables2([&](const std::wstring& name,
								const std::wstring& fullPath,
								const std::wstring& parent,
								const std::wstring& ext) {
		const auto action = std::make_shared<FileAction>(
			name, fullPath, false, parent
		);
		outActions.push_back(action);
	}, options, EXE_FOLDER_PATH2);
}

// 重载版本：返回RunCommandAction向量
static void TraversePATHExecutables(std::vector<std::shared_ptr<FileAction>>& outActions, TraverseOptions&
									options) {
	TraversePATHExecutables([&](const std::wstring& name, const std::wstring& fullPath,
								const std::wstring& parent, const std::wstring& ext) {
		const auto action = std::make_shared<FileAction>(
			name, fullPath, false, parent
		);
		outActions.push_back(action);
	}, options);
}
