//
// Created by Administrator on 2025/10/1.
//
#pragma once

#include "FolderPluginData.hpp"
#include "model/TraverseOptions.hpp"
#include "util/json.hpp"
#include "util/MainTools.hpp"



#include <windows.h>
#include <string>
#include <ShlObj.h>
#include <vector>
#include <memory>
#include <fstream>

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
			item["command"] = wide_to_utf8(config.command);
			item["folder"] = wide_to_utf8(config.folder);
			item["type"] = wide_to_utf8(config.type);
			item["exclude_words"] = WideVectorToUtf8Vector(config.excludeWords);
			item["excludes"] = WideVectorToUtf8Vector(config.excludeNames);
			item["rename_sources"] = WideVectorToUtf8Vector(config.renameSources);
			item["rename_targets"] = WideVectorToUtf8Vector(config.renameTargets);
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
