﻿#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib") // Link against the shell library
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Uuid.lib")

#include "ListedRunnerPlugin.h"
#include "DataKeeper.hpp"

#include <shobjidl.h>
#include <windows.h>
#include <fstream>
#include <iostream>
#include <shlobj.h>
#include <objbase.h>
#include <combaseapi.h> // for CreateStreamOnHGlobal
#include <sstream> // for CreateStreamOnHGlobal
#include <codecvt>
#include <objidl.h>
#include <gdiplus.h>

#include <filesystem>
#include <propkey.h>  // For property keys (optional but good practice)
#include <shlwapi.h>       // For Path* functions (optional)
#include <propvarutil.h>   // Optional for PROPVARIANT helpers
#include <VersionHelpers.h>  // 添加这个头文件

// 不能少
#include <shellapi.h>

#ifndef ESI_ALLITEMS
#define ESI_ALLITEMS 0x00000040
#endif


namespace fs = std::filesystem;

static bool FolderExists(const std::wstring& folderPath)
{
	return std::filesystem::exists(folderPath) && std::filesystem::is_directory(folderPath);
}

static std::unordered_map<std::string, std::function<void()>> callbackFunctions;

ListedRunnerPlugin::ListedRunnerPlugin()
{
}

ListedRunnerPlugin::ListedRunnerPlugin(const std::unordered_map<std::string, std::function<void()>>& callbackFunction1)
{
	callbackFunctions = callbackFunction1;
	configPath = EXE_FOLDER_PATH + L"\\runner.json";
	LoadConfiguration();
}

const std::vector<std::shared_ptr<RunCommandAction>>& ListedRunnerPlugin::GetActions() const
{
	return actions;
}

static std::wstring Utf8ToWString(const std::string& str)
{
	if (str.empty()) return {};

	const int wideLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
	if (wideLen == 0) throw std::runtime_error("MultiByteToWideChar failed");

	std::wstring wstr(wideLen - 1, 0); // -1 去掉 null terminator
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], wideLen);

	return wstr;
}

static std::string WStringToUtf8(const std::wstring& wstr)
{
	if (wstr.empty()) return "";

	const int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (size_needed == 0) throw std::runtime_error("WideCharToMultiByte failed");

	std::string str(size_needed - 1, 0); // -1 去除 null terminator
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size_needed, nullptr, nullptr);

	return str;
}


/// <summary>
/// Enumerates UWP applications from the AppsFolder and adds them to the actions list.
/// This is the C++ equivalent of the C# SpecificallyForGetCurrentUwpName2() and the subsequent loop.
/// </summary>
/// <param name="actions">The list of actions to add UWP apps to.</param>
static void LoadUwpApps(std::vector<std::shared_ptr<RunCommandAction>>& actions)
{
	if (FAILED(CoInitialize(NULL)))
	{
		return;
	}

	IKnownFolderManager* pKnownFolderManager = nullptr;
	if (FAILED(
		CoCreateInstance(CLSID_KnownFolderManager, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pKnownFolderManager))))
	{
		CoUninitialize();
		return;
	}

	IShellItem* pAppsFolderItem = nullptr;
	if (SUCCEEDED(SHGetKnownFolderItem(FOLDERID_AppsFolder, KF_FLAG_DEFAULT, NULL, IID_PPV_ARGS(&pAppsFolderItem))))
	{
		IShellFolder* pDesktopFolder = nullptr;
		if (SUCCEEDED(SHGetDesktopFolder(&pDesktopFolder)))
		{
			LPITEMIDLIST pidl = nullptr;
			if (SUCCEEDED(SHGetIDListFromObject(pAppsFolderItem, &pidl)))
			{
				IShellFolder* pAppsFolderShellFolder = nullptr;
				if (SUCCEEDED(pDesktopFolder->BindToObject(pidl, NULL, IID_PPV_ARGS(&pAppsFolderShellFolder))))
				{
					IEnumIDList* pEnumIDList = nullptr;
					if (SUCCEEDED(
						pAppsFolderShellFolder->EnumObjects(NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &pEnumIDList)))
					{
						LPITEMIDLIST pidlItem = nullptr;
						ULONG fetched = 0;
						while (pEnumIDList->Next(1, &pidlItem, &fetched) == S_OK)
						{
							IShellItem* pShellItem = nullptr;
							if (SUCCEEDED(
								SHCreateItemWithParent(pidl, pAppsFolderShellFolder, pidlItem, IID_PPV_ARGS(&pShellItem)
								)))
							{
								// 处理每个 ShellItem
								LPWSTR pwszParsingName = nullptr;
								if (FAILED(pShellItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &pwszParsingName)))
								{
									pShellItem->Release();
									continue;
								}

								// UWP应用程序通常具有“！”以他们的解析名称。
								if (wcschr(pwszParsingName, L'!') == nullptr)
								{
									CoTaskMemFree(pwszParsingName);
									pShellItem->Release();
									continue;
								}

								LPWSTR pwszDisplayName = nullptr;
								if (FAILED(pShellItem->GetDisplayName(SIGDN_NORMALDISPLAY, &pwszDisplayName)))
								{
									CoTaskMemFree(pwszParsingName);
									pShellItem->Release();
									continue;
								}

								HBITMAP hBitmap = nullptr;
								IShellItemImageFactory* pImageFactory = nullptr;
								// 大小可以调整。 256x256是一个很好的高质量尺寸。
								SIZE size = {LISTITEM_ICON_SIZE, LISTITEM_ICON_SIZE};
								if (SUCCEEDED(pShellItem->QueryInterface(IID_PPV_ARGS(&pImageFactory))))
								{
									// SIIGBF_RESIZETOFIT 即使没有确切的尺寸，也可以确保我们获得图像。
									// SIIGBF_ICONONLY 防止获得文档的缩略图预览。
									pImageFactory->GetImage(size, SIIGBF_RESIZETOFIT | SIIGBF_ICONONLY, &hBitmap);
									pImageFactory->Release();
								}

								// 构造命令字符串以启动UWP应用程序
								std::wstring uwpCommand = L"shell:AppsFolder\\";
								uwpCommand += pwszParsingName;

								std::wstring uwpCommandS = L"";
								uwpCommandS += pwszParsingName;

								// 为UWP应用程序创建一个新的RunCommandaction。
								if (hBitmap != nullptr)
								{
									// TODO: 使用HBITMAP并管理其生命周期
									actions.push_back(
										std::make_shared<RunCommandAction>(
											pwszDisplayName, uwpCommand, uwpCommandS, hBitmap));
								}
								else
								{
									// 无法加载图标的后备
									actions.push_back(
										std::make_shared<RunCommandAction>(
											pwszDisplayName, uwpCommand, uwpCommandS, nullptr));
								}

								CoTaskMemFree(pwszDisplayName);
								CoTaskMemFree(pwszParsingName);
								pShellItem->Release();
							}
							CoTaskMemFree(pidlItem);
						}
						pEnumIDList->Release();
					}
					pAppsFolderShellFolder->Release();
				}
				CoTaskMemFree(pidl);
			}
			pDesktopFolder->Release();
		}
		pAppsFolderItem->Release();
	}
	CoUninitialize();
}


void ListedRunnerPlugin::LoadConfiguration()
{
	actions.clear();
	// std::shared_ptr<ActionNormal> refreshAction = ;
	// actions.push_back(refreshAction);
	actions.push_back(std::make_shared<ActionNormal>(L"刷新列表", L"刷新运行配置", EXE_FOLDER_PATH + L"\\refresh.ico",
													callbackFunctions["refreshList"]));
	actions.push_back(std::make_shared<ActionNormal>(L"退出软件", L"退出软件", EXE_FOLDER_PATH + L"\\refresh.ico",
													callbackFunctions["quit"]));
	actions.push_back(std::make_shared<ActionNormal>(L"重启软件", L"重启软件", EXE_FOLDER_PATH + L"\\refresh.ico",
													callbackFunctions["restart"]));
	actions.push_back(std::make_shared<ActionNormal>(L"软件设置", L"打开本软件设置界面", EXE_FOLDER_PATH + L"\\refresh.ico",
													callbackFunctions["settings"]));
	std::ifstream in((configPath.data())); // 用 std::ifstream 而不是 std::wifstream
	if (!in)
	{
		std::wcerr << L"配置文件不存在：" << configPath << std::endl;
		return;
	}

	// 读取整个文件内容（UTF-8 编码）
	std::ostringstream buffer;
	buffer << in.rdbuf();
	std::string utf8json = buffer.str();

	// 解析 JSON
	nlohmann::json configJson;
	try
	{
		configJson = nlohmann::json::parse(utf8json); // 解析 UTF-8 的 std::string
	}
	catch (const nlohmann::json::parse_error& e)
	{
		std::wcerr << L"JSON 解析错误：" << Utf8ToWString(e.what()) << std::endl;
		return;
	}

	for (const nlohmann::basic_json<>& cmd : configJson)
	{
		try
		{
			if (!cmd.is_object())
			{
				std::wcerr << L"配置项不是一个对象，跳过。" << std::endl;
				continue;
			}

			if (!cmd.contains("command") || !cmd["command"].is_string())
			{
				std::wcerr << L"JSON 配置缺失或类型错误：'command' 字段，跳过该项。" << std::endl;
				continue;
			}

			std::wstring title = Utf8ToWString(cmd["command"].get<std::string>());

			if (cmd.contains("exePath"))
			{
				if (!cmd["exePath"].is_string())
				{
					std::wcerr << L"'exePath' 字段类型错误，跳过该项。" << std::endl;
					continue;
				}

				std::wstring path = Utf8ToWString(cmd["exePath"].get<std::string>());
				auto action = std::make_shared<RunCommandAction>(title, path);
				actions.push_back(action);
			}
			else if (cmd.contains("folder"))
			{
				if (!cmd["folder"].is_string())
				{
					std::wcerr << L"'folder' 字段类型错误，跳过该项。" << std::endl;
					continue;
				}

				std::wstring folderPath = Utf8ToWString(cmd["folder"].get<std::string>());
				if (!FolderExists(folderPath))
				{
					std::wcerr << L"指定的文件夹不存在：" << folderPath << std::endl;
					continue;
				}

				ListedRunnerPlugin::TraverseOptions traverseOptions;
				traverseOptions.extensions = {L".exe", L".lnk"};
				traverseOptions.recursive = true;

				if (cmd.contains("excludes") && cmd["excludes"].is_array())
				{
					for (const auto& name : cmd["excludes"])
					{
						if (name.is_string())
						{
							traverseOptions.excludeNames.insert(Utf8ToWString(name.get<std::string>()));
						}
					}
				}

				if (cmd.contains("exclude_words") && cmd["exclude_words"].is_array())
				{
					for (const auto& word : cmd["exclude_words"])
					{
						if (word.is_string())
						{
							traverseOptions.excludeWords.insert(Utf8ToWString(word.get<std::string>()));
						}
					}
				}

				if (cmd.contains("rename_sources") && cmd["rename_sources"].is_array() &&
					cmd.contains("rename_targets") && cmd["rename_targets"].is_array())
				{
					const auto& sources = cmd["rename_sources"];
					const auto& targets = cmd["rename_targets"];
					size_t count = (((sources.size()) < (targets.size())) ? (sources.size()) : (targets.size()));

					for (size_t i = 0; i < count; ++i)
					{
						if (sources[i].is_string() && targets[i].is_string())
						{
							std::wstring src = Utf8ToWString(sources[i].get<std::string>());
							std::wstring dst = Utf8ToWString(targets[i].get<std::string>());
							traverseOptions.renameMap[src] = dst;
						}
					}
				}

				TraverseFiles(folderPath, traverseOptions, actions);
			}
			else
			{
				std::wcerr << L"配置项缺少 'exePath' 或 'folder' 字段，跳过。" << std::endl;
			}
		}
		catch (const std::exception& e)
		{
			std::wcerr << L"处理 JSON 配置时出现异常：" << Utf8ToWString(e.what()) << std::endl;
			continue;
		}
	}

	// *** UWP应用程序加载逻辑 ***
	if (settingsMap["pref_indexed_uwp_apps_enable"].defValue.get<int>() == 1)
	{
		if (IsWindows8OrGreater())
			try
			{
				LoadUwpApps(actions);
			}
			catch (const std::exception& e)
			{
				std::wcerr << L"Failed to load UWP apps: " << Utf8ToWString(e.what()) << std::endl;
			}
	}
}


std::vector<std::shared_ptr<RunCommandAction>> ListedRunnerPlugin::Search(const std::wstring& query) const
{
	std::vector<std::shared_ptr<RunCommandAction>> results;
	const std::vector<std::wstring> words = SplitWords(query);

	for (const std::shared_ptr<RunCommandAction>& action : actions)
	{
		bool match = true;
		for (const auto& word : words)
		{
			if (action->GetTitle().find(word) == std::wstring::npos)
			{
				match = false;
				break;
			}
		}
		if (match)
		{
			results.push_back(action);
			if (results.size() >= 7) break;
		}
	}

	return results;
}

std::vector<std::wstring> ListedRunnerPlugin::SplitWords(const std::wstring& input)
{
	std::vector<std::wstring> result;
	size_t start = 0, end = 0;
	while ((end = input.find(L' ', start)) != std::wstring::npos)
	{
		if (end > start)
			result.push_back(input.substr(start, end - start));
		start = end + 1;
	}
	if (start < input.size())
		result.push_back(input.substr(start));
	return result;
}

std::wstring ListedRunnerPlugin::GetShortcutTarget(const std::wstring& lnkPath)
{
	CoInitialize(nullptr);

	std::wstring target;

	IShellLink* psl = nullptr;
	if (SUCCEEDED(
		CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, reinterpret_cast<void**>(&psl)
		)))
	{
		IPersistFile* ppf = nullptr;
		if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&ppf))))
		{
			if (SUCCEEDED(ppf->Load(lnkPath.c_str(), STGM_READ)))
			{
				WCHAR szPath[MAX_PATH];
				WIN32_FIND_DATA wfd = {0};
				if (SUCCEEDED(psl->GetPath(szPath, MAX_PATH, &wfd, SLGP_UNCPRIORITY)))
				{
					target = szPath;
				}
			}
			ppf->Release();
		}
		psl->Release();
	}

	CoUninitialize();
	return target;
}

HICON ListedRunnerPlugin::GetFileIcon(const std::wstring& filePath, const bool largeIcon)
{
	SHFILEINFOW sfi = { 0 };;
	UINT flags = SHGFI_ICON | SHGFI_USEFILEATTRIBUTES;
	flags |= largeIcon ? SHGFI_LARGEICON : SHGFI_SMALLICON;

	SHGetFileInfoW(filePath.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), flags);
	return sfi.hIcon;
}

static HBITMAP IconToBitmap(HICON hIcon)
{
	if (!hIcon) return nullptr;

	ICONINFO iconInfo;
	GetIconInfo(hIcon, &iconInfo);

	return iconInfo.hbmColor;
}

HBITMAP ListedRunnerPlugin::GetIconFromPath(const std::wstring& path)
{
	std::wstring actualPath = path;

	if (path.size() > 4 && path.substr(path.size() - 4) == L".lnk")
	{
		actualPath = ListedRunnerPlugin::GetShortcutTarget(path);
	}

	HICON hIcon = GetFileIcon(actualPath, true);
	HBITMAP hBitmap = IconToBitmap(hIcon);
	DestroyIcon(hIcon);
	return hBitmap;
}

void ListedRunnerPlugin::WriteDefaultConfig(const std::wstring& path)
{
	using json = nlohmann::json;

	json jArray = json::array();
	jArray.push_back({
		{"Command", "ProgramData开始菜单"},
		{"FolderPath", R"(C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs)"},
		{"IsSearchSubFolder", true},
		{"ExcludeNameWordArr", {"uninstall", "帮助", "help"}}
	});

	// std::ofstream out(path);

	std::ofstream out;
	out.open(path.c_str()); // ✅ 使用 c_str() 得到 const wchar_t*

	out << jArray.dump(4); // Pretty-print JSON
	out.close();
}

// Helper: 获取 PNG 编码器 CLSID
static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT num = 0, size = 0;
	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0) return -1;

	Gdiplus::ImageCodecInfo* pImageCodecInfo = static_cast<Gdiplus::ImageCodecInfo*>(malloc(size));
	if (!pImageCodecInfo) return -1;

	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT i = 0; i < num; ++i)
	{
		if (wcscmp(pImageCodecInfo[i].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[i].Clsid;
			free(pImageCodecInfo);
			return i;
		}
	}

	free(pImageCodecInfo);
	return -1;
}

static std::vector<BYTE> HBitmapToByteArray(HBITMAP hBitmap)
{
	std::vector<BYTE> buffer;
	if (!hBitmap) return buffer;

	Gdiplus::Bitmap bmp(hBitmap, nullptr);
	CLSID pngClsid;
	GetEncoderClsid(L"image/png", &pngClsid);

	IStream* pStream = nullptr;
	CreateStreamOnHGlobal(nullptr, TRUE, &pStream);

	bmp.Save(pStream, &pngClsid, nullptr);

	STATSTG stat;
	pStream->Stat(&stat, STATFLAG_NONAME);
	const ULONG size = stat.cbSize.QuadPart;

	buffer.resize(size);
	const LARGE_INTEGER liZero = {};
	pStream->Seek(liZero, STREAM_SEEK_SET, nullptr);
	ULONG read = 0;
	pStream->Read(buffer.data(), size, &read);
	pStream->Release();

	return buffer;
}


void ListedRunnerPlugin::TraverseFiles(
	const std::wstring& folderPath,
	const ListedRunnerPlugin::TraverseOptions& options,
	std::vector<std::shared_ptr<RunCommandAction>>& outActions
)
{
	if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) return;

	auto extMatch = [&](const std::wstring& ext) -> bool
	{
		if (options.extensions.empty()) return true;
		return std::any_of(options.extensions.begin(), options.extensions.end(),
							[&](const std::wstring& ex) { return _wcsicmp(ext.c_str(), ex.c_str()) == 0; });
	};

	auto shouldExclude = [&](const std::wstring& name) -> bool
	{
		std::wstring nameLower = name;
		std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::towlower);
		if (options.excludeNames.find(nameLower) != options.excludeNames.end()) return true;

		for (const auto& word : options.excludeWords)
		{
			if (nameLower.find(word) != std::wstring::npos) return true;
		}
		return false;
	};

	auto addFile = [&](const fs::path& path)
	{
		std::wstring filename = path.stem().wstring(); // without extension

		if (shouldExclude(filename)) return;

		// 重命名映射
		if (const auto it = options.renameMap.find(filename); it != options.renameMap.end())
		{
			filename = it->second;
		}

		const auto action = std::make_shared<RunCommandAction>(
			filename, path.wstring(), false, true, path.parent_path().wstring()
		);

		outActions.push_back(action);
	};

	if (options.recursive)
	{
		for (const auto& entry : fs::recursive_directory_iterator(folderPath))
		{
			if (!entry.is_regular_file()) continue;

			const auto ext = entry.path().extension().wstring();
			if (!extMatch(ext)) continue;

			addFile(entry.path());
		}
	}
	else
	{
		for (const auto& entry : fs::directory_iterator(folderPath))
		{
			if (!entry.is_regular_file()) continue;

			const auto ext = entry.path().extension().wstring();
			if (!extMatch(ext)) continue;

			addFile(entry.path());
		}
	}
}
