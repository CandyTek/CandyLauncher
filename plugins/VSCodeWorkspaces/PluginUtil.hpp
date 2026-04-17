#pragma once

#include "PluginAction.hpp"
#include "PluginData.hpp"
#include <vector>
#include <memory>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <Windows.h>
#include <ShlObj.h>
#include <regex>
#include <map>

#include "util/BitmapUtil.hpp"
#include "util/LogUtil.hpp"
#include "util/StringUtil.hpp"
#include "util/json.hpp"

using json = nlohmann::json;

// Structure to hold VSCode instance information
struct VSCodeInstance {
	std::wstring version; // e.g., "Code", "Code - Insiders"
	std::wstring executablePath; // Full path to the executable
	std::wstring appDataPath; // AppData path for this instance
};

// Enum for workspace environment types
enum class WorkspaceEnvironment {
	Local = 1,
	Codespaces = 2,
	RemoteWSL = 3,
	RemoteSSH = 4,
	DevContainer = 5,
	RemoteTunnel = 6,
};

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

// Get the LocalAppData path
inline std::wstring GetLocalAppDataPath() {
	wchar_t* path = nullptr;
	if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path) == S_OK) {
		std::wstring result(path);
		CoTaskMemFree(path);
		return result;
	}
	return L"";
}

// Parse RFC 3986 URI
struct Rfc3986Uri {
	std::string scheme;
	std::string authority;
	std::string path;
	std::string query;
	std::string fragment;

	static Rfc3986Uri Parse(const std::string& uriString) {
		Rfc3986Uri result;
		// Regex pattern from RFC 3986
		std::regex uriRegex(R"(^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?)");
		std::smatch match;

		if (std::regex_match(uriString, match, uriRegex)) {
			result.scheme = match[2];
			result.authority = match[4];
			result.path = match[5];
			result.query = match[7];
			result.fragment = match[9];
		}

		return result;
	}
};

// Parse VSCode authority to get workspace environment
inline std::pair<WorkspaceEnvironment, std::string> ParseVSCodeAuthority(const std::string& authority) {
	static std::map<std::string, WorkspaceEnvironment> environmentTypes = {
		{"", WorkspaceEnvironment::Local},
		{"ssh-remote", WorkspaceEnvironment::RemoteSSH},
		{"wsl", WorkspaceEnvironment::RemoteWSL},
		{"vsonline", WorkspaceEnvironment::Codespaces},
		{"dev-container", WorkspaceEnvironment::DevContainer},
		{"tunnel", WorkspaceEnvironment::RemoteTunnel},
	};

	std::string remoteName = authority;
	std::string machineName = "";

	size_t pos = authority.find('+');
	if (pos != std::string::npos) {
		remoteName = authority.substr(0, pos);
		machineName = authority.substr(pos + 1);
	}

	auto it = environmentTypes.find(remoteName);
	WorkspaceEnvironment env = (it != environmentTypes.end()) ? it->second : WorkspaceEnvironment::Local;

	return {env, machineName};
}

// URL decode function
inline std::string UrlDecode(const std::string& str) {
	std::string result;
	for (size_t i = 0; i < str.length(); ++i) {
		if (str[i] == '%' && i + 2 < str.length()) {
			int value;
			std::sscanf(str.substr(i + 1, 2).c_str(), "%x", &value);
			result += static_cast<char>(value);
			i += 2;
		} else if (str[i] == '+') {
			result += ' ';
		} else {
			result += str[i];
		}
	}
	return result;
}

// Check if path exists
inline bool DoesPathExist(const std::wstring& path, WorkspaceEnvironment env) {
	if (env == WorkspaceEnvironment::Local) {
		DWORD attrs = GetFileAttributesW(path.c_str());
		return (attrs != INVALID_FILE_ATTRIBUTES);
	}
	// For non-local environments, assume path exists
	return true;
}

// Parse VSCode URI to extract workspace information
inline std::shared_ptr<PluginAction> ParseVSCodeUri(
	const std::string& uri,
	const std::string& authority,
	const VSCodeInstance& instance,
	const int iconFilePathIndex,
	bool isWorkspace = false) {
	if (uri.empty()) {
		return nullptr;
	}

	// Parse the URI
	Rfc3986Uri rfc3986Uri = Rfc3986Uri::Parse(UrlDecode(uri));

	if (rfc3986Uri.path.empty()) {
		return nullptr;
	}

	// Get workspace environment from authority
	auto [workspaceEnv, machineName] = ParseVSCodeAuthority(
		authority.empty() ? rfc3986Uri.authority : authority);

	std::string path = rfc3986Uri.path;

	// Remove preceding '/' from local (Windows) path
	if (workspaceEnv == WorkspaceEnvironment::Local && !path.empty() && path[0] == '/') {
		path = path.substr(1);
	}

	std::wstring widePath = utf8_to_wide(path);

	// Check if path exists (only for local paths)
	if (!DoesPathExist(widePath, workspaceEnv)) {
		return nullptr;
	}

	// Get folder name from path
	std::wstring folderName;
	size_t lastSlash = widePath.find_last_of(L"\\/");
	if (lastSlash != std::wstring::npos) {
		folderName = widePath.substr(lastSlash + 1);
	} else {
		folderName = widePath;
	}

	// Check for root drive case (C:\)
	if (folderName.empty() && widePath.length() >= 2 && widePath[1] == L':') {
		folderName = widePath.substr(0, 1);
	}

	// Create action
	auto action = std::make_shared<PluginAction>();

	// Build title
	std::wstring title = folderName;
	if (isWorkspace) {
		// Remove .code-workspace extension from title
		size_t extPos = title.rfind(L".code-workspace");
		if (extPos != std::wstring::npos) {
			title = title.substr(0, extPos) + L" (Workspace)";
		}
	}

	if (workspaceEnv != WorkspaceEnvironment::Local) {
		std::wstring envStr;
		switch (workspaceEnv) {
		case WorkspaceEnvironment::RemoteSSH: envStr = L"SSH";
			break;
		case WorkspaceEnvironment::RemoteWSL: envStr = L"WSL";
			break;
		case WorkspaceEnvironment::Codespaces: envStr = L"Codespaces";
			break;
		case WorkspaceEnvironment::DevContainer: envStr = L"Dev Container";
			break;
		case WorkspaceEnvironment::RemoteTunnel: envStr = L"Tunnel";
			break;
		default: envStr = L"Remote";
			break;
		}

		if (!machineName.empty()) {
			title += L" - " + utf8_to_wide(machineName);
		}
		title += L" (" + envStr + L")";
	}

	action->title = title;
	action->subTitle = widePath;
	action->iconFilePath = instance.executablePath;
	action->iconFilePathIndex = iconFilePathIndex;
	action->projectPath = widePath;

	// Store original URI for launching
	action->originalUri = utf8_to_wide(uri);
	action->isWorkspaceFile = isWorkspace;

	// Build match text
	std::wstring matchText = title + L" " + widePath;
	action->matchText = m_host->GetTheProcessedMatchingText(matchText);

	return action;
}

// Parse storage.json file
inline std::vector<std::shared_ptr<PluginAction>> ParseStorageJson(
	const std::wstring& jsonPath,
	const VSCodeInstance& instance) {
	std::vector<std::shared_ptr<PluginAction>> results;

	try {
		int iconFilePathIndex = GetSysImageIndex(instance.executablePath);
		std::filesystem::path p(jsonPath);
		std::ifstream file(p, std::ios::binary);
		if (!file.is_open()) {
			return results;
		}

		json data;
		file >> data;
		file.close();

		// Handle newest format (profileAssociations.workspaces) - VSCode v1.75+
		if (data.contains("profileAssociations") && data["profileAssociations"].is_object()) {
			auto profileAssoc = data["profileAssociations"];
			if (profileAssoc.contains("workspaces") && profileAssoc["workspaces"].is_object()) {
				auto workspaces = profileAssoc["workspaces"];
				for (auto it = workspaces.begin(); it != workspaces.end(); ++it) {
					std::string uri = it.key();
					if (!uri.empty()) {
						// Determine if it's a workspace file
						bool isWorkspaceFile = uri.find(".code-workspace") != std::string::npos;

						auto action = ParseVSCodeUri(uri, "", instance, iconFilePathIndex, isWorkspaceFile);
						if (action) {
							results.push_back(action);
						}
					}
				}
			}
		}

		// Handle old format (openedPathsList)
		if (data.contains("openedPathsList")) {
			auto openedPathsList = data["openedPathsList"];

			// Handle older format (workspaces3)
			if (openedPathsList.contains("workspaces3") && openedPathsList["workspaces3"].is_array()) {
				for (const auto& workspaceUri : openedPathsList["workspaces3"]) {
					if (workspaceUri.is_string()) {
						auto action = ParseVSCodeUri(workspaceUri.get<std::string>(), "", instance, iconFilePathIndex);
						if (action) {
							results.push_back(action);
						}
					}
				}
			}

			// Handle newer format (entries)
			if (openedPathsList.contains("entries") && openedPathsList["entries"].is_array()) {
				for (const auto& entry : openedPathsList["entries"]) {
					if (!entry.is_object()) continue;

					bool isWorkspaceFile = false;
					std::string uri;
					std::string remoteAuthority = entry.value("remoteAuthority", "");

					// Check if it's a workspace file or folder
					if (entry.contains("workspace") && entry["workspace"].is_object()) {
						auto workspace = entry["workspace"];
						if (workspace.contains("configPath")) {
							isWorkspaceFile = true;
							uri = workspace["configPath"].get<std::string>();
						}
					}

					if (!isWorkspaceFile && entry.contains("folderUri")) {
						uri = entry["folderUri"].get<std::string>();
					}

					if (!uri.empty()) {
						auto action = ParseVSCodeUri(uri, remoteAuthority, instance, iconFilePathIndex, isWorkspaceFile);
						if (action) {
							results.push_back(action);
						}
					}
				}
			}
		}
	} catch (const std::exception& e) {
		// Log error but continue
		ConsolePrintln(L"Error parsing storage.json: " + utf8_to_wide(e.what()));
	}

	return results;
}

// Parse state.vscdb SQLite database
inline std::vector<std::shared_ptr<PluginAction>> ParseStateVscdb(
	const std::wstring& dbPath,
	const VSCodeInstance& instance) {
	std::vector<std::shared_ptr<PluginAction>> results;

	// Note: This would require SQLite library integration
	// For now, we'll skip this and rely on storage.json
	// In a full implementation, you would:
	// 1. Open SQLite connection
	// 2. Query: SELECT value FROM ItemTable WHERE key LIKE 'history.recentlyOpenedPathsList'
	// 3. Parse the JSON value similar to storage.json entries

	return results;
}

// Find VSCode instances by looking in PATH environment variable
inline std::vector<VSCodeInstance> FindVSCodeInstances() {
	std::vector<VSCodeInstance> instances;

	std::wstring roamingPath = GetRoamingAppDataPath();
	std::wstring localPath = GetLocalAppDataPath();

	// Get PATH environment variable
	wchar_t* pathEnv = _wgetenv(L"PATH");
	if (!pathEnv) {
		return instances;
	}

	std::wstring pathStr(pathEnv);
	std::wstringstream ss(pathStr);
	std::wstring path;

	while (std::getline(ss, path, L';')) {
		// Look for paths containing "VS Code" or "VSCodium"
		std::wstring lowerPath = path;
		std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

		if (lowerPath.find(L"vs code") == std::wstring::npos &&
			lowerPath.find(L"vscodium") == std::wstring::npos &&
			lowerPath.find(L"vscode") == std::wstring::npos &&
			lowerPath.find(L"microsoft vs code") == std::wstring::npos) {
			continue;
		}

		// Check if directory exists
		DWORD attrs = GetFileAttributesW(path.c_str());
		if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
			continue;
		}

		// Get parent directory (Code.exe is usually in parent of bin)
		std::wstring parentPath = path;
		size_t lastSlash = parentPath.find_last_of(L"\\/");
		if (lastSlash != std::wstring::npos) {
			std::wstring lastDir = parentPath.substr(lastSlash + 1);
			std::transform(lastDir.begin(), lastDir.end(), lastDir.begin(), ::tolower);

			// If current path is bin directory, use parent
			if (lastDir == L"bin") {
				parentPath = parentPath.substr(0, lastSlash);
			}
		}

		// Look for Code.exe in parent directory first
		std::vector<std::wstring> pathsToCheck = {parentPath, path};

		for (const auto& checkPath : pathsToCheck) {
			WIN32_FIND_DATAW findData;
			std::wstring searchPath = checkPath + L"\\*.exe";
			HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);

			if (hFind != INVALID_HANDLE_VALUE) {
				do {
					std::wstring fileName = findData.cFileName;
					std::wstring lowerName = fileName;
					std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

					// Skip code-tunnel.exe and other utilities
					if (lowerName.find(L"code-tunnel.exe") != std::wstring::npos ||
						lowerName.find(L"unins") != std::wstring::npos) {
						continue;
					}

					// Look for Code.exe, code-insiders.exe, etc.
					if ((lowerName == L"code.exe" ||
						lowerName == L"code-insiders.exe" ||
						lowerName == L"code-exploration.exe" ||
						lowerName == L"codium.exe" ||
						lowerName == L"codium-insiders.exe")) {
						VSCodeInstance instance;
						instance.executablePath = checkPath + L"\\" + fileName;

						// Determine version
						std::wstring version = L"Code";
						if (lowerName.find(L"insiders") != std::wstring::npos) {
							version = L"Code - Insiders";
						} else if (lowerName.find(L"exploration") != std::wstring::npos) {
							version = L"Code - Exploration";
						} else if (lowerName.find(L"codium") != std::wstring::npos) {
							if (lowerName.find(L"insiders") != std::wstring::npos) {
								version = L"VSCodium - Insiders";
							} else {
								version = L"VSCodium";
							}
						}

						instance.version = version;

						// Check for portable data directory
						std::wstring portableData = checkPath + L"\\data";
						DWORD portableAttrs = GetFileAttributesW(portableData.c_str());
						if (portableAttrs != INVALID_FILE_ATTRIBUTES && (portableAttrs & FILE_ATTRIBUTE_DIRECTORY)) {
							instance.appDataPath = portableData + L"\\user-data";
						} else {
							instance.appDataPath = roamingPath + L"\\" + version;
						}

						instances.push_back(instance);
						break; // Found Code.exe, move to next PATH entry
					}
				} while (FindNextFileW(hFind, &findData));
				FindClose(hFind);

				// If found an instance, break the inner loop
				if (!instances.empty() && instances.back().executablePath.find(checkPath) != std::wstring::npos) {
					break;
				}
			}
		}
	}

	// If no instances found in PATH, try default installation location
	if (instances.empty()) {
		std::wstring defaultPath = localPath + L"\\Programs\\Microsoft VS Code\\Code.exe";
		DWORD attrs = GetFileAttributesW(defaultPath.c_str());
		if (attrs != INVALID_FILE_ATTRIBUTES) {
			VSCodeInstance instance;
			instance.executablePath = defaultPath;
			instance.version = L"Code";
			instance.appDataPath = roamingPath + L"\\Code";
			instances.push_back(instance);
		}
	}

	return instances;
}

// Get all VSCode workspace actions
inline std::vector<std::shared_ptr<BaseAction>> GetAllVSCodeWorkspaces() {
	std::vector<std::shared_ptr<BaseAction>> results;

	if (!m_host) {
		return results;
	}

	// Find all VSCode instances
	const std::vector<VSCodeInstance> instances = FindVSCodeInstances();
	// 用来去除重复项
	std::unordered_set<std::wstring> configSet;

	// Parse each instance's storage files
	for (const auto& instance : instances) {
		// Try storage.json in User/globalStorage
		std::wstring storageJsonPath = instance.appDataPath + L"\\User\\globalStorage\\storage.json";
		if (!configSet.insert(storageJsonPath).second) continue;
		DWORD attrs = GetFileAttributesW(storageJsonPath.c_str());
		if (attrs != INVALID_FILE_ATTRIBUTES) {
			auto storageResults = ParseStorageJson(storageJsonPath, instance);
			results.insert(results.end(),
							std::make_move_iterator(storageResults.begin()),
							std::make_move_iterator(storageResults.end()));
		}

		// Try state.vscdb (would need SQLite support)
		std::wstring vscdbPath = instance.appDataPath + L"\\User\\globalStorage\\state.vscdb";
		attrs = GetFileAttributesW(vscdbPath.c_str());
		if (attrs != INVALID_FILE_ATTRIBUTES) {
			auto dbResults = ParseStateVscdb(vscdbPath, instance);
			results.insert(results.end(),
							std::make_move_iterator(dbResults.begin()),
							std::make_move_iterator(dbResults.end()));
		}
	}

	return results;
}
