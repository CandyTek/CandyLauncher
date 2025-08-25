#pragma once

#include "RunCommandAction.hpp"
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <functional>

#include "DataKeeper.hpp"
#include "actions/ActionNormal.hpp"
#include "TraverseOptions.h"


class ListedRunnerPlugin
{
public:
	// ListedRunnerPlugin(CallbackFunction callbackFunction1);
	ListedRunnerPlugin();
	// ListedRunnerPlugin(const std::unordered_map<std::string, CallbackFunction>& callbackFunction1);
	ListedRunnerPlugin(const std::unordered_map<std::string, std::function<void()>>& callbackFunction1);
	[[nodiscard]] const std::vector<std::shared_ptr<RunCommandAction>>& GetActions() const;

	void LoadConfiguration();
	static HICON GetFileIcon(const std::wstring& filePath, bool largeIcon = false);
	
	static void TraverseUwpApps(const TraverseOptions& options, std::vector<std::shared_ptr<RunCommandAction>>& outActions);

	[[nodiscard]] std::vector<std::shared_ptr<RunCommandAction>> Search(const std::wstring& query) const;
	static HBITMAP GetIconFromPath(const std::wstring& path);

private:
	std::wstring configPath;
	std::vector<std::shared_ptr<RunCommandAction>> actions;

	static std::vector<std::wstring> SplitWords(const std::wstring& input);
	static void WriteDefaultConfig(const std::wstring& path);

};

