// 独立的皮肤编辑器程序 SkinEditor.exe：启动后直接打开皮肤编辑器窗口，关闭窗口即退出
// 与 CandyLauncher.exe 放在同一目录，读取同目录下的 skins 文件夹与 user_settings.json

#include <windows.h>
#include <fstream>

#include "common/GlobalState.hpp"
#include "common/I18n.hpp"
#include "util/json.hpp"
#include "window/SkinEditorWindow.hpp"

// 只读取编辑器用到的几项用户设置，不加载完整的设置系统与插件
static void LoadSkinEditorSettings() {
	nlohmann::json userConfig = nlohmann::json::object();
	std::ifstream in(USER_SETTINGS_PATH, std::ios::binary);
	if (in) {
		try {
			userConfig = nlohmann::json::parse(in, nullptr, true, true);
		} catch (const std::exception& e) {
			Loge(L"SkinEditorApp", L"读取 user_settings.json 失败: ", e.what());
		}
	}
	if (!userConfig.is_object()) userConfig = nlohmann::json::object();

	g_uiLanguageCode = ResolveUiLanguageCode(&userConfig);

	auto readString = [&](const char* key, const std::string& def) {
		const auto it = userConfig.find(key);
		return it != userConfig.end() && it->is_string() ? it->get<std::string>() : def;
	};
	auto readBool = [&](const char* key, bool def) {
		const auto it = userConfig.find(key);
		return it != userConfig.end() && it->is_boolean() ? it->get<bool>() : def;
	};
	// 默认值与 common/settings.json 保持一致
	g_currectSkinFilePath = utf8_to_wide(readString("pref_skin", "default"));
	g_settings_map["pref_search_box_placeholder"].stringValue = readString("pref_search_box_placeholder", "please input text...");
	g_settings_map["pref_search_box_placeholder_use_theme"].boolValue = readBool("pref_search_box_placeholder_use_theme", true);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
	SetCurrentDirectoryW(EXE_FOLDER_PATH.c_str());
	if (FAILED(OleInitialize(nullptr))) return -1;

	g_hInst = hInstance;
	g_skinEditorQuitOnClose = true;
	LoadSkinEditorSettings();
	ShowSkinEditorWindow();
	if (!g_skinEditorHwnd) {
		OleUninitialize();
		return -1;
	}

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0)) {
		if (SkinEditorPreTranslateMessage(&msg)) continue;
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	OleUninitialize();
	return static_cast<int>(msg.wParam);
}
