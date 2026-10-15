// CandyUwpHelper.exe：短命的 Shell 枚举进程，按命令行参数枚举一种索引源，
// 结果按 util/UwpHelperProtocol.hpp 的格式写到 stdout 后退出：
//   uwp（默认）        shell:AppsFolder 里的打包应用 + 48px 图标
//   control_panel      控制面板全部项目 + 48px 图标（相同图标只传一份，按像素哈希共享）
//   windows_settings   "设置" 应用常用页面（名称来自系统 MUI 资源，图标由主程序按 SystemSettings.exe 延迟解析）
// Folder 插件（util/UwpHelperClient.hpp）通过匿名管道启动并读取它；这样 WinRT / StateRepository / MRT 以及
// 控制面板的几十个 applet DLL 只在本进程里加载，不会常驻主程序。无窗口、不依赖主程序任何头文件以外的全局状态。

#include <windows.h>
#include <objbase.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../util/SystemSettingsEnumerator.hpp"
#include "../util/UwpAppsEnumerator.hpp"
#include "../util/UwpHelperProtocol.hpp"

namespace {

enum ExitCode : int {
	kOk = 0,
	kBadMode = 1,
	kNoStdout = 2,
	kComInitFailed = 3,
	kWriteFailed = 4,
};

bool WriteAll(const HANDLE out, const std::vector<uint8_t>& data) {
	size_t written = 0;
	while (written < data.size()) {
		const DWORD chunk = static_cast<DWORD>((std::min<size_t>)(data.size() - written, 1 << 20));
		DWORD done = 0;
		if (!WriteFile(out, data.data() + written, chunk, &done, nullptr) || done == 0) return false;
		written += done;
	}
	return true;
}

void CollectUwpApps(std::vector<UwpHelper::Item>& items) {
	const std::vector<UwpApps::Entry> entries = UwpApps::Enumerate();
	std::vector<std::wstring> parsingNames;
	parsingNames.reserve(entries.size());
	for (const auto& entry : entries) parsingNames.push_back(entry.parsingName);

	std::vector<HBITMAP> bitmaps;
	UwpApps::LoadIcons(parsingNames, bitmaps);

	items.reserve(entries.size());
	for (size_t i = 0; i < entries.size(); ++i) {
		UwpHelper::Item item;
		item.name = entries[i].displayName;
		item.target = entries[i].parsingName;
		if (bitmaps[i]) {
			UwpApps::CopyDibPixels(bitmaps[i], item.icon);
			DeleteObject(bitmaps[i]);
		}
		items.push_back(std::move(item));
	}
}

void CollectControlPanel(std::vector<UwpHelper::Item>& items) {
	std::vector<SystemSettings::Item> entries;
	std::unordered_map<int, HBITMAP> icons;
	SystemSettings::EnumerateControlPanel(entries, &icons);

	// 同一图像列表索引的像素只复制、哈希一次；同一 key 只在第一次出现时带像素
	std::unordered_map<int, std::wstring> keyByIndex;
	std::unordered_map<std::wstring, UwpApps::DibPixels> pixelsByKey;
	for (auto& [index, bitmap] : icons) {
		UwpApps::DibPixels pixels;
		if (bitmap) {
			UwpApps::CopyDibPixels(bitmap, pixels);
			DeleteObject(bitmap);
		}
		if (pixels.empty()) continue;
		std::wstring key = UwpHelper::IconKeyForPixels(pixels, L"cp:");
		pixelsByKey.try_emplace(key, std::move(pixels));
		keyByIndex.emplace(index, std::move(key));
	}

	items.reserve(entries.size());
	for (auto& entry : entries) {
		UwpHelper::Item item;
		item.name = std::move(entry.name);
		item.target = std::move(entry.target);
		if (const auto key = keyByIndex.find(entry.iconIndex); key != keyByIndex.end()) {
			item.iconKey = key->second;
			if (const auto pixels = pixelsByKey.find(key->second); pixels != pixelsByKey.end()) {
				item.icon = std::move(pixels->second);
				pixelsByKey.erase(pixels);
			}
		}
		items.push_back(std::move(item));
	}
}

void CollectWindowsSettings(std::vector<UwpHelper::Item>& items) {
	std::vector<SystemSettings::Item> entries;
	SystemSettings::EnumerateWindowsSettings(entries);
	items.reserve(entries.size());
	for (auto& entry : entries) {
		UwpHelper::Item item;
		item.name = std::move(entry.name);
		item.target = std::move(entry.target);
		items.push_back(std::move(item));
	}
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR commandLine, int) {
	const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
	if (out == nullptr || out == INVALID_HANDLE_VALUE) return kNoStdout;

	std::wstring mode = commandLine ? commandLine : L"";
	while (!mode.empty() && (mode.back() == L' ' || mode.back() == L'\t')) mode.pop_back();
	while (!mode.empty() && (mode.front() == L' ' || mode.front() == L'\t')) mode.erase(mode.begin());
	if (mode.empty()) mode = UwpHelper::kModeUwp;

	if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) return kComInitFailed;

	std::vector<UwpHelper::Item> items;
	if (mode == UwpHelper::kModeUwp) {
		CollectUwpApps(items);
	} else if (mode == UwpHelper::kModeControlPanel) {
		CollectControlPanel(items);
	} else if (mode == UwpHelper::kModeWindowsSettings) {
		CollectWindowsSettings(items);
	} else {
		CoUninitialize();
		return kBadMode;
	}

	std::vector<uint8_t> payload;
	UwpHelper::Serialize(items, payload);
	const bool ok = WriteAll(out, payload);
	FlushFileBuffers(out);

	CoUninitialize();
	return ok ? kOk : kWriteFailed;
}
