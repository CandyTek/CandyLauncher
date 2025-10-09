#pragma once

#include "VisualStudioAction.hpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <array>
#include <pugixml.hpp>

#include "util/StringUtil.hpp"
#include "util/LogUtil.hpp"
#include "util/BitmapUtil.hpp"
#include "util/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

// 存储 Visual Studio 实例信息
struct VSInstance {
	std::wstring instanceId;
	std::wstring displayName;
	std::wstring productPath;
	std::wstring productLineVersion;
	bool isPrerelease = false;
	std::wstring applicationPrivateSettingsPath;
};

// 执行命令并获取输出
static std::string ExecuteCommand(const std::wstring& command) {
	std::string result;

	// 转换为窄字符串
	std::string cmdA = wide_to_utf8(command);

	std::array<char, 128> buffer;
	FILE* pipe = _popen(cmdA.c_str(), "r");

	if (!pipe) {
		Loge(L"VisualStudio", L"Failed to execute command: " + command);
		return result;
	}

	try {
		while (fgets(buffer.data(), (int)buffer.size(), pipe) != nullptr) {
			result += buffer.data();
		}
	} catch (...) {
		_pclose(pipe);
		throw;
	}

	_pclose(pipe);
	return result;
}

// 使用 vswhere.exe 获取 Visual Studio 实例列表
static std::vector<VSInstance> GetVisualStudioInstances() {
	std::vector<VSInstance> instances;

	try {
		// 尝试从两个位置查找 vswhere.exe
		std::vector<std::wstring> vsWherePaths = {
			LR"(C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe)",
			L"vswhere.exe" // 从 PATH 查找
		};

		std::wstring vsWherePath;
		for (const auto& path : vsWherePaths) {
			if (path == L"vswhere.exe" || fs::exists(path)) {
				vsWherePath = path;
				break;
			}
		}

		if (vsWherePath.empty()) {
			Loge(L"VisualStudio", L"vswhere.exe not found");
			return instances;
		}

		// 执行 vswhere.exe 并获取 JSON 输出
		std::string output = ExecuteCommand(L"\"" + vsWherePath + L"\" -all -prerelease -format json -utf8");
		// ConsolePrintln(L"VisualStudio", output);


		if (output.empty()) {
			ConsolePrintln(L"VisualStudio", L"vswhere.exe returned empty output");
			return instances;
		}

		// 解析 JSON 输出
		json vswhereJson = json::parse(output);

		if (!vswhereJson.is_array()) {
			Loge(L"VisualStudio", L"vswhere.exe output is not an array");
			return instances;
		}

		// 获取 LOCALAPPDATA 环境变量
		wchar_t localAppData[MAX_PATH];
		ExpandEnvironmentStringsW(L"%LOCALAPPDATA%", localAppData, MAX_PATH);
		fs::path vsDataDir = fs::path(localAppData) / L"Microsoft" / L"VisualStudio";

		for (const auto& item : vswhereJson) {
			try {
				VSInstance instance;

				// 读取实例 ID
				if (item.contains("instanceId") && item["instanceId"].is_string()) {
					instance.instanceId = utf8_to_wide(item["instanceId"].get<std::string>());
				} else {
					continue;
				}

				// 读取显示名称
				if (item.contains("displayName") && item["displayName"].is_string()) {
					instance.displayName = utf8_to_wide(item["displayName"].get<std::string>());
				}

				// 读取产品路径
				if (item.contains("productPath") && item["productPath"].is_string()) {
					instance.productPath = utf8_to_wide(item["productPath"].get<std::string>());
				}

				// 读取产品线版本
				if (item.contains("catalog") && item["catalog"].is_object()) {
					const auto& catalog = item["catalog"];
					if (catalog.contains("productLineVersion") && catalog["productLineVersion"].is_string()) {
						instance.productLineVersion = utf8_to_wide(catalog["productLineVersion"].get<std::string>());
					}
				}

				// 读取是否为预发布版本
				if (item.contains("isPrerelease") && item["isPrerelease"].is_boolean()) {
					instance.isPrerelease = item["isPrerelease"].get<bool>();
				}

				// 查找 ApplicationPrivateSettings.xml
				if (!fs::exists(vsDataDir)) {
					continue;
				}

				for (const auto& entry : fs::directory_iterator(vsDataDir)) {
					if (entry.is_directory()) {
						std::wstring dirName = entry.path().filename().wstring();

						// 检查目录名是否包含实例 ID，且不是备份目录
						if (dirName.find(instance.instanceId) != std::wstring::npos &&
							dirName.find(L"SettingsBackup_") == std::wstring::npos) {
							fs::path settingsPath = entry.path() / L"ApplicationPrivateSettings.xml";
							if (fs::exists(settingsPath)) {
								instance.applicationPrivateSettingsPath = settingsPath.wstring();
								break;
							}
						}
					}
				}

				// 只添加找到设置文件的实例
				if (!instance.applicationPrivateSettingsPath.empty()) {
					instances.push_back(instance);
					ConsolePrintln(L"VisualStudio", L"Found VS instance: " + instance.displayName);
				}
			} catch (const std::exception& e) {
				Loge(L"VisualStudio", L"Error parsing VS instance: ", e.what());
				continue;
			}
		}
	} catch (const std::exception& e) {
		Loge(L"VisualStudio", L"Error getting VS instances: ", e.what());
	}

	return instances;
}

// 从 ApplicationPrivateSettings.xml 中提取 CodeContainers JSON 字符串
static std::string ExtractCodeContainersJson(const std::wstring& xmlPath) {
	try {
		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_file(xmlPath.c_str());
		if (!result) {
			std::wcerr << L"Failed to load XML: " << xmlPath << std::endl;
			return "";
		}

		// 查找 collection name="CodeContainers.Offline"
		pugi::xpath_node collectionNode = doc.select_node("/content/indexed/collection[@name='CodeContainers.Offline']");
		if (!collectionNode) {
			return "";
		}

		// 找到第一个 <value name="value"> 节点
		pugi::xpath_node valueNode = collectionNode.node().select_node("value[@name='value']");
		if (!valueNode) {
			return "";
		}

		std::string jsonStr = valueNode.node().child_value();

		// XML 解码（pugixml 不会自动解码 &quot; 之类的）
		size_t pos = 0;
		while ((pos = jsonStr.find("&quot;", pos)) != std::string::npos) {
			jsonStr.replace(pos, 6, "\"");
		}
		while ((pos = jsonStr.find("&amp;", pos)) != std::string::npos) {
			jsonStr.replace(pos, 5, "&");
		}
		while ((pos = jsonStr.find("&lt;", pos)) != std::string::npos) {
			jsonStr.replace(pos, 4, "<");
		}
		while ((pos = jsonStr.find("&gt;", pos)) != std::string::npos) {
			jsonStr.replace(pos, 4, ">");
		}

		return jsonStr;
	} catch (const std::exception& e) {
		Loge(L"VisualStudio", L"Error extracting CodeContainers: ", e.what());
		return "";
	}
}

// 从 CodeContainers JSON 中解析项目列表
static std::vector<std::shared_ptr<BaseAction>> ParseCodeContainers(
	const std::string& jsonStr,
	const VSInstance& instance,
	int maxResults = 1000) {
	std::vector<std::shared_ptr<BaseAction>> result;

	try {
		if (jsonStr.empty()) {
			return result;
		}

		json containersJson = json::parse(jsonStr);

		if (!containersJson.is_array()) {
			return result;
		}

		int iconFilePathIndex = GetSysImageIndex(instance.productPath);
		int count = 0;

		for (const auto& container : containersJson) {
			if (maxResults > 0 && count >= maxResults) {
				break;
			}

			try {
				// 解析 Value.LocalProperties.FullPath
				if (!container.contains("Value") || !container["Value"].is_object()) {
					continue;
				}

				const auto& value = container["Value"];

				if (!value.contains("LocalProperties") || !value["LocalProperties"].is_object()) {
					continue;
				}

				const auto& localProps = value["LocalProperties"];

				if (!localProps.contains("FullPath") || !localProps["FullPath"].is_string()) {
					continue;
				}

				std::wstring fullPath = utf8_to_wide(localProps["FullPath"].get<std::string>());

				// 检查路径是否存在
				if (!fs::exists(fullPath)) {
					continue;
				}

				auto action = std::make_shared<VisualStudioAction>();

				action->projectPath = fullPath;
				action->visualStudioPath = instance.productPath;
				action->displayName = instance.displayName;
				action->isPrerelease = instance.isPrerelease;

				// 读取 IsFavorite
				if (value.contains("IsFavorite") && value["IsFavorite"].is_boolean()) {
					action->isFavorite = value["IsFavorite"].get<bool>();
				}

				// 提取文件名作为标题
				fs::path p(fullPath);
				action->title = p.filename().wstring();
				// ConsolePrintln(L"VisualStudio",action->title);

				// 副标题显示完整路径
				action->subTitle = fullPath;

				// 设置图标为 Visual Studio 可执行文件
				action->iconFilePath = instance.productPath;
				action->iconFilePathIndex = iconFilePathIndex;

				// 设置匹配文本
				try {
					action->matchText = m_host->GetTheProcessedMatchingText(action->title) + fullPath;
				} catch (...) {
					action->matchText = action->title;
				}

				result.push_back(action);
				count++;
			} catch (const std::exception& e) {
				Loge(L"VisualStudio", L"Error parsing code container: ", e.what());
				continue;
			}
		}

		ConsolePrintln(L"VisualStudio",
						L"Loaded " + std::to_wstring(result.size()) +
						L" projects from " + instance.displayName);
	} catch (const std::exception& e) {
		Loge(L"VisualStudio", L"Error parsing CodeContainers JSON: ", e.what());
	}

	return result;
}

// 获取所有 Visual Studio 项目
static std::vector<std::shared_ptr<BaseAction>> GetAllVisualStudioProjects() {
	std::vector<std::shared_ptr<BaseAction>> result;

	if (!m_host) {
		Loge(L"VisualStudio", L"m_host is null");
		return result;
	}

	try {
		// 从配置读取最大结果数
		int maxResults = static_cast<int>(
			m_host->GetSettingsMap().at("com.candytek.visualstudioplugin.max_results").intValue);

		// 获取所有 Visual Studio 实例
		std::vector<VSInstance> instances = GetVisualStudioInstances();

		ConsolePrintln(L"VisualStudio", L"Found " + std::to_wstring(instances.size()) + L" VS instances");

		for (const auto& instance : instances) {
			// 如果不显示预发布版本，则跳过
			if (instance.isPrerelease && !showPrerelease) {
				continue;
			}

			// 提取 CodeContainers JSON
			std::string jsonStr = ExtractCodeContainersJson(instance.applicationPrivateSettingsPath);

			// 解析项目列表
			auto projects = ParseCodeContainers(jsonStr, instance, maxResults);

			result.insert(result.end(),
						std::make_move_iterator(projects.begin()),
						std::make_move_iterator(projects.end()));
		}

		ConsolePrintln(L"VisualStudio", L"Total loaded " + std::to_wstring(result.size()) + L" projects");
	} catch (const std::exception& e) {
		Loge(L"VisualStudio", L"Error getting all VS projects: ", e.what());
	}

	return result;
}
