#pragma once

#include "RunCommandAction.cpp"
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>

#include "DataKeeper.hpp"


class ListedRunnerPlugin {

public:
    ListedRunnerPlugin(CallbackFunction callbackFunction1);
    [[nodiscard]] const std::vector<std::shared_ptr<RunCommandAction>>& GetActions() const;

    void LoadConfiguration();
    static HICON GetFileIcon(const std::wstring& filePath, bool largeIcon = false);

    struct TraverseOptions
    {
        std::vector<std::wstring> extensions; // e.g., {L".exe", L".lnk"}
        std::unordered_set<std::wstring> excludeNames; // 完整排除
        std::unordered_set<std::wstring> excludeWords; // 包含关键词排除
        std::unordered_map<std::wstring, std::wstring> renameMap; // name -> displayName
        bool recursive = false;
    };

    static void TraverseFiles(const std::wstring& folderPath,const TraverseOptions& options,std::vector<std::shared_ptr<RunCommandAction>>& outActions);

    [[nodiscard]] std::vector<std::shared_ptr<RunCommandAction>> Search(const std::wstring& query) const;
    static HBITMAP GetIconFromPath(const std::wstring& path);
    static std::wstring GetShortcutTarget(const std::wstring& lnkPath);

private:
    std::wstring configPath;
    std::vector<std::shared_ptr<RunCommandAction>> actions;

    static std::vector<std::wstring> SplitWords(const std::wstring& input);
    static void WriteDefaultConfig(const std::wstring& path);
};
