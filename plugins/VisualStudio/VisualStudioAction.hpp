#pragma once
#include <string>
#include "VisualStudioPluginData.hpp"

class VisualStudioAction final : public candy::ActionBase {
public:
	std::wstring projectPath;       // 项目/解决方案完整路径
	std::wstring visualStudioPath;  // Visual Studio 可执行文件路径
	std::wstring displayName;       // Visual Studio 版本显示名称
	bool isPrerelease = false;      // 是否为预发布版本
	bool isFavorite = false;        // 是否为收藏项目
};
