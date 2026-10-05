#pragma once
#include "plugins/BaseAction.hpp"

class AutomationEditorAction : public BaseAction {
    public:
        AutomationEditorAction() {
            matchText = g_host->GetTheProcessedMatchingText(title);
            pluginId = m_pluginId;
            iconPath = EXE_FOLDER_PATH2 + L"\\CandyLauncher.exe";
            iconIndex = GetSysImageIndex(iconPath);
        }
        std::wstring& getTitle() override { return title; }
        std::wstring& getSubTitle() override { return subtitle; }
        std::wstring& getIconFilePath() override { return iconPath; }
        int getIconFilePathIndex() override {
            return iconIndex;
        }
        HBITMAP getIconBitmap() override { return nullptr; }
    private:
        std::wstring title = L"创建或编辑自动化动作组";
        std::wstring subtitle = L"打开自动化动作组 JSON 编辑器";
        std::wstring iconPath;
        int iconIndex = -1;
};
