#pragma once

#include "util/json.hpp"
#include "util/FileUtil.hpp"
#include "util/LogUtil.hpp"
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <thread>

struct AutomationStep {
	std::wstring path;
	std::wstring params;
	std::wstring workingDir;
	std::wstring command;
	unsigned delayMs = 0;
};

struct AutomationDocument {
	std::wstring name;
	std::vector<AutomationStep> steps;
};

// bool LoadAutomationDocument(const std::wstring& path, AutomationDocument& document, std::wstring& error);
// bool SaveAutomationDocument(const std::wstring& path, const AutomationDocument& document, std::wstring& error);
// void RunAutomationDocument(const std::wstring& path);


inline std::wstring Wide(const std::string& value) {
	if (value.empty()) return {};
	int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
	if (!length) return {};
	std::wstring result(length, L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length);
	return result;
}

inline std::string Utf8(const std::wstring& value) {
	if (value.empty()) return {};
	int length = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
	std::string result(length, '\0');
	WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
	return result;
}

inline std::wstring StringField(const nlohmann::json& item, const char* key) {
	return item.contains(key) && item[key].is_string() ? Wide(item[key].get<std::string>()) : L"";
}


inline bool LoadAutomationDocument(const std::wstring& path, AutomationDocument& document, std::wstring& error) {
	try {
		const std::string text = ReadUtf8File(path);
		if (text.empty()) { error = L"文件为空或无法读取。"; return false; }
		const auto root = nlohmann::json::parse(text);
		if (!root.is_object()) { error = L"JSON 顶层必须是对象。"; return false; }
		const auto* items = root.contains("item") ? &root["item"] : (root.contains("steps") ? &root["steps"] : nullptr);
		if (!items || !items->is_array()) { error = L"缺少 item 步骤数组。"; return false; }
		AutomationDocument loaded;
		loaded.name = StringField(root, "name");
		if (loaded.name.empty()) loaded.name = std::filesystem::path(path).stem().wstring();
		for (const auto& item : *items) {
			if (!item.is_object()) { error = L"步骤必须是对象。"; return false; }
			AutomationStep step;
			step.path = StringField(item, "path");
			step.params = StringField(item, "params");
			if (step.params.empty()) step.params = StringField(item, "target"); // AHK 旧列表使用 target
			step.workingDir = StringField(item, "workingdir");
			step.command = StringField(item, "command");
			if (item.contains("delay_ms")) {
				if (!item["delay_ms"].is_number_unsigned() && !item["delay_ms"].is_number_integer()) {
					error = L"delay_ms 必须是整数。"; return false;
				}
				const auto delay = item["delay_ms"].get<long long>();
				if (delay < 0 || delay > 600000) { error = L"delay_ms 超出 0 到 600000 的范围。"; return false; }
				step.delayMs = static_cast<unsigned>(delay);
			}
			if (step.path.empty() && step.command.empty()) { error = L"步骤缺少 path 或 command。"; return false; }
			loaded.steps.push_back(std::move(step));
		}
		document = std::move(loaded);
		return true;
	} catch (const std::exception&) {
		error = L"JSON 解析失败，请检查文件格式。";
		return false;
	}
}

inline bool SaveAutomationDocument(const std::wstring& path, const AutomationDocument& document, std::wstring& error) {
	try {
		nlohmann::json root = {{"version", 1}, {"name", Utf8(document.name)}, {"item", nlohmann::json::array()}};
		for (const auto& step : document.steps) {
			root["item"].push_back({
				{"path", Utf8(step.path)}, {"params", Utf8(step.params)},
				{"workingdir", Utf8(step.workingDir)}, {"command", Utf8(step.command)},
				{"delay_ms", step.delayMs}
			});
		}
		const std::wstring temporary = path + L".tmp";
		std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
		if (!output) { error = L"无法创建临时文件。"; return false; }
		const std::string bytes = root.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
		output.write("\xEF\xBB\xBF", 3);
		output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		output.close();
		if (!output) { DeleteFileW(temporary.c_str()); error = L"文件写入失败。"; return false; }
		if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
			DeleteFileW(temporary.c_str()); error = L"无法替换 JSON 文件。"; return false;
		}
		return true;
	} catch (const std::exception&) {
		error = L"无法序列化 JSON。";
		return false;
	}
}

inline void RunAutomationDocument(const std::wstring& path) {
	std::thread([path]() {
		AutomationDocument document;
		std::wstring error;
		if (!LoadAutomationDocument(path, document, error)) {
			Loge(L"FolderPlugin", L"Automation load failed: ", path, L" ", error);
			MessageBoxW(nullptr, error.c_str(), L"自动化动作组", MB_OK | MB_ICONERROR);
			return;
		}
		for (const auto& step : document.steps) {
			if (step.delayMs) Sleep(step.delayMs);
			const bool custom = !step.command.empty();
			const std::wstring executable = custom ? L"cmd.exe" : step.path;
			const std::wstring arguments = custom ? L"/d /c " + step.command : step.params;
			HINSTANCE result = ShellExecuteW(nullptr, L"open", executable.c_str(),
				arguments.empty() ? nullptr : arguments.c_str(),
				step.workingDir.empty() ? nullptr : step.workingDir.c_str(), SW_SHOWNORMAL);
			if (reinterpret_cast<INT_PTR>(result) <= 32) {
				Loge(L"FolderPlugin", L"Automation step failed: ", executable, L" code=", reinterpret_cast<INT_PTR>(result));
			}
		}
	}).detach();
}
