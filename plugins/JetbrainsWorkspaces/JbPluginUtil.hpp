#pragma once

#include "JbAction.hpp"
#include "JbPluginData.hpp"
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <regex>
#include <Windows.h>
#include <ShlObj.h>
#include "3rdparty/pugixml/src/pugixml.hpp"
#include "util/LogUtil.hpp"
#include "util/StringUtil.hpp"

// Structure to hold JetBrains IDE information
struct JetBrainsIDE {
	std::wstring name; // e.g., "IntelliJ IDEA", "PyCharm", "Android Studio"
	// std::wstring productCode; // e.g., "AI", "PY", "IU"
	std::wstring exePath; // Full path to the IDE executable
	std::wstring recentProjectsXmlPath;
};

// Parse a single recentProjects.xml file
inline std::vector<std::shared_ptr<JbAction>> ParseRecentProjectsXml(
	const std::wstring& xmlPath,
	const JetBrainsIDE& ide) {
	std::vector<std::shared_ptr<JbAction>> result;

	// Load XML document using pugixml (char-based interface)
	pugi::xml_document doc;
	std::string xmlPathUtf8 = wide_to_utf8(xmlPath);
	pugi::xml_parse_result parseResult = doc.load_file(xmlPathUtf8.c_str());

	if (!parseResult) {
		return result;
	}
	pugi::xpath_node_set entries = doc.select_nodes(
		"/application/component[@name='RecentProjectsManager']"
		"/option[@name='additionalInfo']/map/entry"
	);

	for (auto& entryNode : entries) {
		pugi::xml_node entry = entryNode.node();
		const char* keyAttr = entry.attribute("key").as_string();
		if (!keyAttr || !*keyAttr) continue;
		std::wstring projectPath = utf8_to_wide(keyAttr);

		// Get frame title from nested option element
		pugi::xml_node metaInfo = entry.select_node(".//RecentProjectMetaInfo").node();
		if (!metaInfo) {
			continue;
		}

		const char* titleAttr = metaInfo.attribute("frameTitle").as_string();
		if (!titleAttr || !*titleAttr) continue;
		std::wstring frameTitle = utf8_to_wide(titleAttr);

		// Extract just the project name (before the first dash or em dash)
		std::wstring projectName = frameTitle;
		size_t dashPos = frameTitle.rfind(L" - ");
		if (dashPos != std::wstring::npos) {
			projectName = frameTitle.substr(0, dashPos);
		} else {
			// Try em dash
			dashPos = frameTitle.find(L"\u2013");
			if (dashPos != std::wstring::npos) {
				// Remove leading/trailing spaces
				if (dashPos > 0 && frameTitle[dashPos - 1] == L' ') dashPos--;
				projectName = frameTitle.substr(0, dashPos);
			}
		}

		// Create action
		auto action = std::make_shared<JbAction>();
		action->title = ide.name + L" - " + projectName;
		action->subTitle = projectPath;
		action->iconFilePath = ide.exePath;
		action->iconFilePathIndex = 0;

		// Store additional data
		action->projectPath = projectPath;
		action->ideName = ide.name;

		// Match text includes IDE name, project name, and path for better searching
		std::wstring matchText = ide.name + L" " + projectName + L" " + projectPath;
		action->matchText = m_host->GetTheProcessedMatchingText(matchText);

		result.push_back(action);
	}

	return result;
}

// Get the AppData\Roaming path
inline std::wstring GetRoamingAppDataPath() {
	wchar_t* path = nullptr;
	if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path) == S_OK) {
		std::wstring result(path);
		CoTaskMemFree(path);
		return result;
	}
	return L"";
}

// Map product code to IDE name
// inline std::wstring GetIDEName(const std::wstring& productCode) {
// 	static std::map<std::wstring, std::wstring> ideNames = {
// 		{L"AI", L"Android Studio"},
// 		{L"IU", L"IntelliJ IDEA Ultimate"},
// 		{L"IC", L"IntelliJ IDEA Community"},
// 		{L"PY", L"PyCharm Professional"},
// 		{L"PC", L"PyCharm Community"},
// 		{L"WS", L"WebStorm"},
// 		{L"PS", L"PhpStorm"},
// 		{L"RM", L"RubyMine"},
// 		{L"CL", L"CLion"},
// 		{L"GO", L"GoLand"},
// 		{L"RD", L"Rider"},
// 		{L"DB", L"DataGrip"}
// 	};
//
// 	auto it = ideNames.find(productCode);
// 	if (it != ideNames.end()) {
// 		return it->second;
// 	}
// 	return L"JetBrains IDE";
// }

// Find all JetBrains IDEs and their recentProjects.xml files
inline std::vector<JetBrainsIDE> FindJetBrainsIDEs() {
	std::vector<JetBrainsIDE> ides;

	const std::wstring roamingPath = GetRoamingAppDataPath();
	if (roamingPath.empty() || !m_host) {
		return ides;
	}

	// Search for recentProjects.xml files in AppData\Roaming\JetBrains\*
	std::wstring searchPath = roamingPath;

	// Map to store unique IDE installations by product code
	std::map<std::wstring, JetBrainsIDE> ideMap;

	m_host->TraverseFilesSimpleForEverythingSDK(
		searchPath, true, {L".xml"}, LR"(\options\recentProjects.xml)",
		[&](const std::wstring& name, const std::wstring& fullPath,
			const std::wstring& parent, const std::wstring& ext) {
			if (name == L"recentProjects") {
				// ConsolePrintln(fullPath);

				// Try to extract product code from path
				// Path structure: AppData\Roaming\JetBrains\<ProductCode><Version>\options\recentProjects.xml
				// Example: AppData\Roaming\JetBrains\AndroidStudio2021.1\options\recentProjects.xml

				// Get the parent directory name (should contain product code)
				// size_t lastSlash = parent.find_last_of(L"\\/");
				// if (lastSlash != std::wstring::npos) {
				std::wregex re(LR"(\\Roaming\\([^\\]+)\\([^\\]+)\\options)");
				std::wsmatch match;
				// std::wstring productCode = L"JB";
				JetBrainsIDE ide;
				ide.name = L"Jbrains";

				if (std::regex_search(parent, match, re)) {
					// std::cout << "Word1: " << match[1] << std::endl;
					// std::cout << "Word2: " << match[2] << std::endl;
					// productCode
					std::wstring word2 = match[2]; // CLion2025.1

					// 去掉 word2 末尾的数字和点
					std::wregex trim_re(LR"([\d\.]+$)");
					word2 = std::regex_replace(word2, trim_re, L"");

					ide.name = word2;
				}
				// std::wstring parentDir = parent.substr(lastSlash + 1);

				// Extract product code (usually first 2-3 characters)
				// std::wregex productRegex(L"^([A-Z]{2,3})");
				// std::wsmatch match;
				// std::wstring productCode = L"JB";

				// if (std::regex_search(parentDir, match, productRegex)) {
				// 	productCode = match[1].str();
				// }

				// Only add if we haven't seen this product code yet, or if this is a newer version
				// if (ideMap.find(productCode) == ideMap.end()) {
				// ide.name = GetIDEName(productCode);
				// ide.productCode = productCode;
				ide.recentProjectsXmlPath = fullPath;

				// Try to find IDE executable (simplified - could be enhanced)
				ide.exePath = L""; // Will be left empty, icon will be default

				ideMap[ide.name] = ide;
				// }
				// }
			}
		}
	);

	// Convert map to vector
	for (const auto& pair : ideMap) {
		ides.push_back(pair.second);
	}

	return ides;
}

// Get all JetBrains workspace actions
inline std::vector<std::shared_ptr<JbAction>> GetAllJetBrainsWorkspaces() {
	std::vector<std::shared_ptr<JbAction>> result;

	if (!m_host) {
		return result;
	}

	// Find all JetBrains IDEs
	std::vector<JetBrainsIDE> ides = FindJetBrainsIDEs();

	// Parse each IDE's recentProjects.xml
	for (const auto& ide : ides) {
		auto projects = ParseRecentProjectsXml(ide.recentProjectsXmlPath, ide);
		result.insert(result.end(),
					std::make_move_iterator(projects.begin()),
					std::make_move_iterator(projects.end()));
	}

	return result;
}
