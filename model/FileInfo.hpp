#pragma once
#include <filesystem>
#include <string>


struct FileInfo {
	// 文件名称（不包含扩展名）
	std::wstring label;
	std::filesystem::path file_path;
};
