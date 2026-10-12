#pragma once
#include "FolderPluginData.hpp"

// 主列表里的 "创建或编辑自动化动作组" 入口
class AutomationEditorAction final : public candy::ActionBase {
public:
	AutomationEditorAction() {
		title = L"创建或编辑自动化动作组";
		subtitle = L"打开自动化动作组 JSON 编辑器";
		// 图标由主程序按 iconPath 延迟解析
		iconPath = EXE_FOLDER_PATH2 + L"\\CandyLauncher.exe";
		stableKey = L"FolderPlugin_automation_editor";
	}
};
