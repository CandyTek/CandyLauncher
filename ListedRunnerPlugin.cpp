#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib") // Link against the shell library
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Uuid.lib")

#include "ListedRunnerPlugin.h"
#include "DataKeeper.hpp"
#include "TraverseFilesHelper.hpp"
#include "MainTools.hpp"

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
#include <Everything.h>

#ifndef ESI_ALLITEMS
#define ESI_ALLITEMS 0x00000040
#endif


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



/// <summary>
/// Enumerates UWP applications from the AppsFolder and adds them to the actions list.
/// This is the C++ equivalent of the C# SpecificallyForGetCurrentUwpName2() and the subsequent loop.
/// </summary>
/// <param name="actions">The list of actions to add UWP apps to.</param>
static void LoadUwpApps(const TraverseOptions& options,std::vector<std::shared_ptr<RunCommandAction>>& actions)
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
								// 进行UWP列表项的筛选，排除，重命名
								std::wstring uwpAppName = pwszDisplayName;
								if (shouldExclude(options,uwpAppName)) {
									CoTaskMemFree(pwszParsingName);
									pShellItem->Release();
									continue;
								}
								if (const auto it = options.renameMap.find(uwpAppName); it != options.renameMap.end())
								{
									uwpAppName = it->second;
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
											uwpAppName, uwpCommand, uwpCommandS, hBitmap));
								}
								else
								{
									// 无法加载图标的后备
									actions.push_back(
										std::make_shared<RunCommandAction>(
											uwpAppName, uwpCommand, uwpCommandS, nullptr));
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
    MethodTimerStart();
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
	std::string utf8json = ReadUtf8File(configPath);
	
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
				
				TraverseOptions traverseOptions = getTraverseOptions(cmd);
				if (!traverseOptions.command.empty()) continue;

				if (cmd.contains("special_uwp"))
				{
					TraverseUwpApps(traverseOptions,actions);
				}
				else
				{
//					TraverseFiles(folderPath, traverseOptions, actions);
//                    TraverseFilesForEverything(folderPath, traverseOptions, actions);
					::TraverseFilesForEverythingSDK(traverseOptions.folder, traverseOptions, [&](const std::wstring& name,
																					 const std::wstring& fullPath,
																					 const std::wstring& parent,
																					 const std::wstring& ext)
					{
						const auto action = std::make_shared<RunCommandAction>(
								name, fullPath, false, true, parent
						);
						actions.push_back(action);
					});
				}

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
	//TraverseOptions noOptions;
	//TraverseRegistryApps(noOptions,actions);
	//TraversePATHExecutables(actions);
	// TODO: 解决这个崩溃问题
	TraversePATHExecutables2(actions);

	MethodTimerEnd(L"loadlist");
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

HICON ListedRunnerPlugin::GetFileIcon(const std::wstring& filePath, const bool largeIcon)
{
	SHFILEINFOW sfi = {0};;
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

	if (MyEndsWith(path, L".lnk"))
	{
		actualPath = GetShortcutTarget(path);
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
	const ULONG size = static_cast<ULONG>(stat.cbSize.QuadPart);

	buffer.resize(size);
	const LARGE_INTEGER liZero = {};
	pStream->Seek(liZero, STREAM_SEEK_SET, nullptr);
	ULONG read = 0;
	pStream->Read(buffer.data(), size, &read);
	pStream->Release();

	return buffer;
}


void ListedRunnerPlugin::TraverseUwpApps(
	const TraverseOptions& options,
	std::vector<std::shared_ptr<RunCommandAction>>& outActions
)
{
	// *** UWP应用程序加载逻辑 ***
	if (g_settings_map["pref_indexed_uwp_apps_enable"].boolValue)
	{
		if (IsWindows8OrGreater())
			try
			{
				LoadUwpApps(options,outActions);
			}
		catch (const std::exception& e)
		{
			std::wcerr << L"Failed to load UWP apps: " << Utf8ToWString(e.what()) << std::endl;
		}
	}
}

