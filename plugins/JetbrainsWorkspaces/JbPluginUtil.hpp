#pragma once

#include "JbAction.hpp"
#include "JbPluginData.hpp"
#include <vector>
#include <memory>
#include <string>
#include <map>
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

// 遍历 dir 下的直接子目录，fn 参数为子目录完整路径
template <class Fn>
void ForEachSubDirectory(const std::wstring& dir, Fn&& fn) {
	WIN32_FIND_DATAW findData;
	const std::wstring pattern = dir + L"\\*";
	HANDLE hFind = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &findData,
									FindExSearchLimitToDirectories, nullptr, FIND_FIRST_EX_LARGE_FETCH);
	if (hFind == INVALID_HANDLE_VALUE) return;
	do {
		if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
		if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) continue;
		fn(dir + L"\\" + findData.cFileName);
	} while (FindNextFileW(hFind, &findData));
	FindClose(hFind);
}

// Find all JetBrains IDEs and their recentProjects.xml files
inline std::vector<JetBrainsIDE> FindJetBrainsIDEs() {
	std::vector<JetBrainsIDE> ides;

	const std::wstring roamingPath = GetRoamingAppDataPath();
	if (roamingPath.empty() || !m_host) {
		return ides;
	}

	// Path structure: AppData\Roaming\<Vendor>\<ProductCode><Version>\options\recentProjects.xml
	// Example: AppData\Roaming\Google\AndroidStudio2021.1\options\recentProjects.xml
	// 直接遍历两层目录，比通过 Everything SDK 搜索整个 Roaming 快得多，且不依赖 Everything 运行
	struct Candidate {
		JetBrainsIDE ide;
		std::vector<unsigned long> version;
	};
	// Map to store unique IDE installations by product name
	std::map<std::wstring, Candidate> ideMap;

	ForEachSubDirectory(roamingPath, [&](const std::wstring& vendorDir) {
		ForEachSubDirectory(vendorDir, [&](const std::wstring& productDir) {
			const std::wstring xmlPath = productDir + LR"(\options\recentProjects.xml)";
			const DWORD attrs = GetFileAttributesW(xmlPath.c_str());
			if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
				return;
			}

			// 去掉目录名末尾的数字和点，CLion2025.1 -> CLion
			const std::wstring dirName = productDir.substr(productDir.find_last_of(L'\\') + 1);
			const size_t end = dirName.find_last_not_of(L"0123456789.");
			const std::wstring name = dirName.substr(0, end == std::wstring::npos ? 0 : end + 1);
			if (name.empty()) return;

			// 解析版本号 2025.1 -> {2025, 1}
			std::vector<unsigned long> version;
			for (size_t pos = name.size(); pos < dirName.size();) {
				size_t dot = dirName.find(L'.', pos);
				if (dot == std::wstring::npos) dot = dirName.size();
				if (dot > pos) version.push_back(std::wcstoul(dirName.c_str() + pos, nullptr, 10));
				pos = dot + 1;
			}

			// 同一 IDE 存在多个版本时，取版本号最高的那个（旧版本的 xml 格式可能不同）
			auto it = ideMap.find(name);
			if (it != ideMap.end() && it->second.version >= version) return;

			Candidate& candidate = ideMap[name];
			candidate.ide.name = name;
			candidate.ide.recentProjectsXmlPath = xmlPath;
			// Try to find IDE executable (simplified - could be enhanced)
			candidate.ide.exePath = L""; // Will be left empty, icon will be default
			candidate.version = std::move(version);
		});
	});

	// Convert map to vector
	ides.reserve(ideMap.size());
	for (auto& pair : ideMap) {
		ides.push_back(std::move(pair.second.ide));
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
