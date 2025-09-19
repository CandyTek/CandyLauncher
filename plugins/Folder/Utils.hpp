//
// Created by Administrator on 2025/10/1.
//
#pragma once

#ifndef UTILS_HPP
#define UTILS_HPP
#include "FolderDataKeeper.hpp"
#include "common/DataKeeper.hpp"
#include "model/TraverseOptions.h"
#include "util/json.hpp"
#include "util/MainTools.hpp"

#endif //UTILS_HPP

#include <windows.h>
#include <string>
#include <debugapi.h>
#include <sstream>
#include <ShlObj.h>
#include <thread>
#include <commctrl.h>
#include <vector>
#include <memory>
#include <shobjidl.h>
#include <shellapi.h>
#include <comdef.h>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <array>
#include <dwmapi.h>
#include <io.h>
#include <fcntl.h>



// 解析 runner.json 文件
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
	//	MessageBoxA(nullptr, ("Failed to parse runner.json: " + std::string(e.what())).c_str(),
	//				"Error", MB_OK | MB_ICONERROR);
	//}

	return configs;
}


// 保存配置到文件，当前先测试，不执行保存
static void SaveConfigToFile(std::vector<TraverseOptions>& runnerConfigs) {
	if(true){
		return;
	}
	try {
		nlohmann::json j;
		for (const auto &config: runnerConfigs) {
			nlohmann::json item;
			item["command"] = config.command;
			item["folder"] = config.folder;
			item["exclude_words"] = config.excludeWords;
			item["excludes"] = config.excludeNames;
			item["rename_sources"] = config.renameSources;
			item["rename_targets"] = config.renameTargets;
			j.push_back(item);
		}

		std::ofstream file(RUNNER_CONFIG_PATH2);
		file << j.dump(1, '\t');
		file.close();
	}
	catch (const std::exception &e) {
		MessageBoxA(nullptr, ("Failed to save runner.json: " + std::string(e.what())).c_str(),
					"Error", MB_OK | MB_ICONERROR);
	}
}
