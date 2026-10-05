#include "plugins/Folder/AutomationActionModel.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
	const auto folder = std::filesystem::temp_directory_path() /
		(L"CandyAutomationTest-" + std::to_wstring(GetCurrentProcessId()));
	std::filesystem::create_directories(folder);
	const auto path = folder / L"中文动作组.json";
	const auto legacy = folder / L"legacy.json";
	const auto invalid = folder / L"invalid.json";
	std::wstring error;
	AutomationDocument source;
	source.name = L"中文动作组";
	source.steps.push_back({L"C:\\工具\\程序.exe", L"--name 中文", L"C:\\工作目录", L"", 250});
	source.steps.push_back({L"", L"", L"", L"echo 中文", 0});
	if (!SaveAutomationDocument(path.wstring(), source, error)) return 1;
	AutomationDocument loaded;
	if (!LoadAutomationDocument(path.wstring(), loaded, error)) return 2;
	if (loaded.name != source.name || loaded.steps.size() != 2 ||
		loaded.steps[0].path != source.steps[0].path || loaded.steps[0].params != source.steps[0].params ||
		loaded.steps[0].workingDir != source.steps[0].workingDir || loaded.steps[0].delayMs != 250 ||
		loaded.steps[1].command != source.steps[1].command) return 3;
	{
		std::ofstream output(legacy, std::ios::binary);
		output << R"({"item":[{"path":"C:\\legacy.exe","target":"--old","workingdir":"C:\\old"}]})";
	}
	if (!LoadAutomationDocument(legacy.wstring(), loaded, error) || loaded.steps.size() != 1 ||
		loaded.steps[0].params != L"--old" || loaded.steps[0].workingDir != L"C:\\old") return 4;
	{
		std::ofstream output(invalid, std::ios::binary);
		output << R"({"item":[{"path":"app.exe","delay_ms":600001}]})";
	}
	if (LoadAutomationDocument(invalid.wstring(), loaded, error)) return 5;
	std::filesystem::remove_all(folder);
	std::cout << "AutomationActionModelTest passed\n";
	return 0;
}
