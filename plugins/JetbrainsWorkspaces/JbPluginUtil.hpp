#pragma once

#include "JbAction.hpp"
#include "JbPluginData.hpp"
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <fstream>
#include <algorithm>
#include <Windows.h>
#include <ShlObj.h>
#include "3rdparty/pugixml/src/pugixml.hpp"
#include "util/LogUtil.hpp"
#include "util/StringUtil.hpp"
#include "util/BitmapUtil.hpp"

// Structure to hold JetBrains IDE information
struct JetBrainsIDE {
	std::wstring name; // e.g., "IntelliJ IDEA", "PyCharm", "Android Studio"
	// std::wstring productCode; // e.g., "AI", "PY", "IU"
	std::wstring exePath; // Full path to the IDE executable
	int iconIndex = -1; // exePath 在系统图像列表中的索引
	std::wstring recentProjectsXmlPath;
};

// 展开 recentProjects.xml 中的路径宏，并转为 Windows 路径分隔符
// $USER_HOME$/CLionProjects/demo -> C:\Users\xxx\CLionProjects\demo
inline std::wstring ExpandIdePathMacros(std::wstring path, const std::wstring& configDir) {
	static const std::wstring userHome = [] {
		wchar_t buffer[MAX_PATH]{};
		const DWORD len = GetEnvironmentVariableW(L"USERPROFILE", buffer, MAX_PATH);
		return len > 0 && len < MAX_PATH ? std::wstring(buffer, len) : std::wstring();
	}();
	const std::pair<std::wstring, const std::wstring*> macros[] = {
		{L"$USER_HOME$", &userHome},
		{L"$APPLICATION_CONFIG_DIR$", &configDir},
	};
	for (const auto& [macro, value] : macros) {
		if (value->empty()) continue;
		for (size_t pos = path.find(macro); pos != std::wstring::npos; pos = path.find(macro, pos + value->size())) {
			path.replace(pos, macro.size(), *value);
		}
	}
	std::replace(path.begin(), path.end(), L'/', L'\\');
	return path;
}

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
	// xmlPath: <configDir>\options\recentProjects.xml
	std::wstring configDir = xmlPath.substr(0, xmlPath.find_last_of(L'\\'));
	configDir = configDir.substr(0, configDir.find_last_of(L'\\'));

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
		action->iconFilePathIndex = ide.iconIndex;

		// Store additional data
		action->projectPath = ExpandIdePathMacros(projectPath, configDir);
		action->ideName = ide.name;

		// Match text includes IDE name, project name, and path for better searching
		std::wstring matchText = ide.name + L" " + projectName + L" " + projectPath;
		action->matchText = m_host->GetTheProcessedMatchingText(matchText);

		result.push_back(action);
	}

	return result;
}

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

inline std::wstring GetKnownFolder(REFKNOWNFOLDERID folderId) {
	wchar_t* path = nullptr;
	if (SHGetKnownFolderPath(folderId, 0, nullptr, &path) == S_OK) {
		std::wstring result(path);
		CoTaskMemFree(path);
		return result;
	}
	return L"";
}

// Get the AppData\Roaming path
inline std::wstring GetRoamingAppDataPath() {
	return GetKnownFolder(FOLDERID_RoamingAppData);
}

inline bool IsExistingFile(const std::wstring& path) {
	const DWORD attrs = GetFileAttributesW(path.c_str());
	return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

// 拆分配置目录名，CLion2025.1 -> name: CLion, version: {2025, 1}
inline void ParseIdeDirName(const std::wstring& dirName, std::wstring& name, std::vector<unsigned long>& version) {
	const size_t end = dirName.find_last_not_of(L"0123456789.");
	name = dirName.substr(0, end == std::wstring::npos ? 0 : end + 1);
	version.clear();
	for (size_t pos = name.size(); pos < dirName.size();) {
		size_t dot = dirName.find(L'.', pos);
		if (dot == std::wstring::npos) dot = dirName.size();
		if (dot > pos) version.push_back(std::wcstoul(dirName.c_str() + pos, nullptr, 10));
		pos = dot + 1;
	}
}

// 在 IDE 安装目录中找到启动程序
// 优先读取 product-info.json 中的 launcherPath，否则取 bin 下的 *64.exe
inline std::wstring FindIdeExecutable(const std::wstring& homeDir) {
	std::ifstream productInfo(homeDir + L"\\product-info.json", std::ios::binary);
	if (productInfo) {
		const std::string content((std::istreambuf_iterator<char>(productInfo)), std::istreambuf_iterator<char>());
		const std::string key = "\"launcherPath\"";
		for (size_t pos = content.find(key); pos != std::string::npos; pos = content.find(key, pos + key.size())) {
			const size_t colon = content.find(':', pos + key.size());
			if (colon == std::string::npos) break;
			const size_t start = content.find('"', colon);
			if (start == std::string::npos) break;
			const size_t end = content.find('"', start + 1);
			if (end == std::string::npos) break;
			std::wstring launcher = utf8_to_wide(content.substr(start + 1, end - start - 1));
			if (!EndsWithIgnoreCase(launcher, L".exe")) continue;
			std::replace(launcher.begin(), launcher.end(), L'/', L'\\');
			std::wstring exePath = homeDir + L"\\" + launcher;
			if (IsExistingFile(exePath)) return exePath;
		}
	}

	WIN32_FIND_DATAW findData;
	HANDLE hFind = FindFirstFileExW((homeDir + L"\\bin\\*64.exe").c_str(), FindExInfoBasic, &findData,
									FindExSearchNameMatch, nullptr, 0);
	if (hFind == INVALID_HANDLE_VALUE) return L"";
	std::wstring result;
	do {
		if (_wcsicmp(findData.cFileName, L"jetbrains_client64.exe") == 0) continue;
		result = homeDir + L"\\bin\\" + findData.cFileName;
		break;
	} while (FindNextFileW(hFind, &findData));
	FindClose(hFind);
	return result;
}

// IDE 会在 LocalAppData\<Vendor>\<Product><Version>\.home 中记录安装目录
inline std::wstring FindIdeExecutableFromSystemDir(const std::wstring& systemDir) {
	std::ifstream file(systemDir + L"\\.home", std::ios::binary);
	if (!file) return L"";
	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	if (content.size() >= 3 && content.compare(0, 3, "\xEF\xBB\xBF") == 0) content.erase(0, 3);
	const size_t end = content.find_last_not_of(" \t\r\n");
	if (end == std::string::npos) return L"";
	content.resize(end + 1);
	return FindIdeExecutable(utf8_to_wide(content));
}

// 根据 Roaming 下的配置目录找到 IDE 启动程序
// 同版本的 LocalAppData 目录没有记录时，退而使用该 IDE 其他版本记录的安装目录（版本高者优先）
inline std::wstring ResolveIdeExecutable(const std::wstring& localAppData, const std::wstring& vendor,
										const std::wstring& dirName, const std::wstring& name) {
	if (localAppData.empty()) return L"";
	const std::wstring vendorDir = localAppData + L"\\" + vendor;
	std::wstring exePath = FindIdeExecutableFromSystemDir(vendorDir + L"\\" + dirName);
	if (!exePath.empty()) return exePath;

	std::vector<std::pair<std::vector<unsigned long>, std::wstring>> others;
	ForEachSubDirectory(vendorDir, [&](const std::wstring& systemDir) {
		std::wstring otherName;
		std::vector<unsigned long> version;
		ParseIdeDirName(systemDir.substr(systemDir.find_last_of(L'\\') + 1), otherName, version);
		if (otherName == name) others.emplace_back(std::move(version), systemDir);
	});
	std::sort(others.begin(), others.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
	for (const auto& other : others) {
		exePath = FindIdeExecutableFromSystemDir(other.second);
		if (!exePath.empty()) return exePath;
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

	// Path structure: AppData\Roaming\<Vendor>\<ProductCode><Version>\options\recentProjects.xml
	// Example: AppData\Roaming\Google\AndroidStudio2021.1\options\recentProjects.xml
	// 直接遍历两层目录，比通过 Everything SDK 搜索整个 Roaming 快得多，且不依赖 Everything 运行
	struct Candidate {
		JetBrainsIDE ide;
		std::wstring vendor;
		std::wstring dirName;
		std::vector<unsigned long> version;
	};
	// Map to store unique IDE installations by product name
	std::map<std::wstring, Candidate> ideMap;

	ForEachSubDirectory(roamingPath, [&](const std::wstring& vendorDir) {
		ForEachSubDirectory(vendorDir, [&](const std::wstring& productDir) {
			const std::wstring xmlPath = productDir + LR"(\options\recentProjects.xml)";
			if (!IsExistingFile(xmlPath)) return;

			// 去掉目录名末尾的数字和点，CLion2025.1 -> CLion, {2025, 1}
			const std::wstring dirName = productDir.substr(productDir.find_last_of(L'\\') + 1);
			std::wstring name;
			std::vector<unsigned long> version;
			ParseIdeDirName(dirName, name, version);
			if (name.empty()) return;

			// 同一 IDE 存在多个版本时，取版本号最高的那个（旧版本的 xml 格式可能不同）
			auto it = ideMap.find(name);
			if (it != ideMap.end() && it->second.version >= version) return;

			Candidate& candidate = ideMap[name];
			candidate.ide.name = name;
			candidate.ide.recentProjectsXmlPath = xmlPath;
			candidate.vendor = vendorDir.substr(vendorDir.find_last_of(L'\\') + 1);
			candidate.dirName = dirName;
			candidate.version = std::move(version);
		});
	});

	// Convert map to vector, 并获取每个 IDE 的启动程序及其系统图标索引
	const std::wstring localAppData = GetKnownFolder(FOLDERID_LocalAppData);
	ides.reserve(ideMap.size());
	for (auto& pair : ideMap) {
		Candidate& candidate = pair.second;
		candidate.ide.exePath = ResolveIdeExecutable(localAppData, candidate.vendor, candidate.dirName, candidate.ide.name);
		candidate.ide.iconIndex = GetSysImageIndex(candidate.ide.exePath);
		ides.push_back(std::move(candidate.ide));
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
