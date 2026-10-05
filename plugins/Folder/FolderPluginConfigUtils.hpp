//
// Created by Administrator on 2025/10/1.
//
#pragma once

#include "FolderPluginData.hpp"
#include "model/TraverseOptions.hpp"
#include "util/json.hpp"


#include <windows.h>
#include <string>
#include <ShlObj.h>
#include <vector>
#include <memory>
#include <fstream>

// Existing user configurations predate the automation folder. Add only this
// source, preserving every other user-defined index entry.
static bool EnsureAutomationActionIndex() {
	const std::string original = ReadUtf8File(RUNNER_CONFIG_PATH2);
	if (original.empty()) return false;
	try {
		auto config = nlohmann::json::parse(original);
		if (!config.is_array()) return false;
		bool found = false;
		for (auto& item : config) {
			if (item.is_object() && item.value("folder", std::string()) == "\\plugins\\AutomationActions") {
				if (item.value("exts", nlohmann::json::array()) == nlohmann::json::array({".json", ".cmd"}) &&
					item.value("index_files_only", false) && !item.value("is_contain_subfolder", true)) return true;
				item["exts"] = {".json", ".cmd"};
				item["index_files_only"] = true;
				item["is_contain_subfolder"] = false;
				found = true;
				break;
			}
		}
		if (!found) config.push_back({
			{"exclude_words", nlohmann::json::array()}, {"excludes", nlohmann::json::array()},
			{"exts", {".json", ".cmd"}}, {"folder", "\\plugins\\AutomationActions"},
			{"index_files_only", true}, {"is_contain_subfolder", false},
			{"name", "自动化动作组"}, {"rename_sources", nlohmann::json::array()},
			{"rename_targets", nlohmann::json::array()}, {"type", "folder"}
		});
		const std::wstring temporary = RUNNER_CONFIG_PATH2 + L".automation.tmp";
		std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
		if (!output) return false;
		output.write("\xEF\xBB\xBF", 3);
		output << config.dump(1, '\t');
		output.close();
		if (!output) {
			DeleteFileW(temporary.c_str());
			return false;
		}
		if (!MoveFileExW(temporary.c_str(), RUNNER_CONFIG_PATH2.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
			DeleteFileW(temporary.c_str());
			return false;
		}
		return true;
	} catch (const std::exception&) {
		return false;
	}
}

static std::vector<std::string> WideVectorToUtf8Vector(const std::vector<std::wstring>& values) {
	std::vector<std::string> result;
	result.reserve(values.size());
	for (const auto& value : values) {
		result.push_back(wide_to_utf8(value));
	}
	return result;
}

// 解析 config_folder_plugin.json 文件
static std::vector<TraverseOptions> ParseRunnerConfig() {
	std::vector<TraverseOptions> configs;
	std::string jsonText = ReadUtf8File(RUNNER_CONFIG_PATH2);
	nlohmann::json runnerConfig;

	try {
		runnerConfig = nlohmann::json::parse(jsonText);
	}
	catch (const nlohmann::json::parse_error &e) {
		std::string error_msg = "Config file load error in '" + wide_to_utf8(RUNNER_CONFIG_PATH2) +
								"'. Using default settings.\n\nError: " + e.what();
	}

	//try {
	for (const nlohmann::basic_json<> &item: runnerConfig) {
		TraverseOptions config = getTraverseOptions(item);
		configs.push_back(config);
	}
	//}
	//catch (const std::exception &e) {
	//	MessageBoxA(nullptr, ("Failed to parse config_folder_plugin.json: " + std::string(e.what())).c_str(),
	//				"Error", MB_OK | MB_ICONERROR);
	//}

	return configs;
}


// 保存配置到文件，当前先测试，不执行保存
static void SaveConfigToFile(std::vector<TraverseOptions>& runnerConfigs) {
	if(true){
		//return;
	}
	try {
		nlohmann::json j;
		for (const auto &config: runnerConfigs) {
			nlohmann::json item;
			item["name"] = wide_to_utf8(config.name);
			item["folder"] = wide_to_utf8(config.folder);
			item["type"] = wide_to_utf8(config.type);
			item["is_contain_subfolder"] = config.recursive;
			item["index_files_only"] = config.indexFilesOnly;
			item["exclude_words"] = WideVectorToUtf8Vector(config.excludeWords);
			item["excludes"] = WideVectorToUtf8Vector(config.excludeNames);
			item["rename_sources"] = WideVectorToUtf8Vector(config.renameSources);
			item["rename_targets"] = WideVectorToUtf8Vector(config.renameTargets);
			item["exts"] = WideVectorToUtf8Vector(config.extensions);
			j.push_back(item);
		}

		std::ofstream file(RUNNER_CONFIG_PATH2, std::ios::binary);
		const char utf8Bom[] = "\xEF\xBB\xBF";
		file.write(utf8Bom, sizeof(utf8Bom) - 1);
		file << j.dump(1, '\t');
		file.close();
	}
	catch (const std::exception &e) {
		MessageBoxA(nullptr, ("Failed to save config_folder_plugin.json: " + std::string(e.what())).c_str(),
					"Error", MB_OK | MB_ICONERROR);
	}
}
