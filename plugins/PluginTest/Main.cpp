#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fcntl.h>
#include <io.h>
#include <unordered_map>

#include "util/StringUtil.hpp"
#include "util/LogUtil.hpp"
#include "plugins/Plugin.hpp"
#include "model/SettingItem.hpp"
#include "util/MyJsonUtil.hpp"

// 声明插件导出函数
extern "C" {
__declspec(dllexport) IPlugin* CreatePlugin();
__declspec(dllexport) void DestroyPlugin(IPlugin* plugin);
__declspec(dllexport) int GetPluginApiVersion();
}

static void TestLog(const std::string& msg) {
	std::cout << msg << std::endl;
}

static void TestLog(const std::wstring& msg) {
	TestLog(wide_to_utf8(msg));
}

// 简单的 Mock PluginHost 用于测试
class MockPluginHost : public IPluginHost {
private:
	std::unordered_map<std::string, std::function<void()>> callbacks;

public:
	std::unordered_map<std::string, SettingItem> settingsMap;
	
	void ShowMessage(const std::wstring& title, const std::wstring& message) override {
		TestLog(L"[Message] " + title + L": " + message);
	}

	const std::unordered_map<std::string, SettingItem>& GetSettingsMap() override {
		return settingsMap;
	}

	const std::unordered_map<std::string, std::function<void()>>& GetAppLaunchActionCallbacks() override {
		return callbacks;
	}

	std::wstring GetTheProcessedMatchingText(const std::wstring& source) override {
		// 简单返回原文本（实际应该处理拼音等）
		return source;
	}

	void TraverseFilesSimpleForEverythingSDK(
		const std::wstring& folderPath,
		bool recursive,
		const std::vector<std::wstring>& extensions,
		const std::wstring& nameFilter,
		std::function<void(const std::wstring&, const std::wstring&, const std::wstring&, const std::wstring&)> callback) override {
		// Mock implementation
	}

	void TraverseFilesForEverythingSDK(
		const std::wstring& folderPath,
		const TraverseOptions& options,
		std::function<void(const std::wstring&, const std::wstring&, const std::wstring&, const std::wstring&)>&& callback) override {
		// Mock implementation
	}

	void ChangeEditTextText(const std::wstring& text) override {
		TestLog(L"[ChangeText] " + text);
	}

	std::vector<std::shared_ptr<BaseAction>> GetSuccessfullyMatchingTextActions(
		const std::wstring& input, const std::vector<std::shared_ptr<BaseAction>>& actions) override {
		// 简单的匹配实现
		return actions;
	}

	std::vector<FileInfo> SearchPossibleTargets(const std::wstring& fileName, uint64_t resultCount) override {
		return {};
	}

	HBITMAP LoadResIconAsBitmap(int nResID, int cx, int cy) override {
		return nullptr;
	}

	void ShowSimpleToast(const std::wstring& title, const std::wstring& msg) override {
		TestLog(L"[Toast] " + title + L": " + msg);
	}

	std::wstring& GetEditTextText() override {
		std::wstring temp;
		return temp;
	}
};

int main() {
	// 不要设置这两个，宽窄字符输出混用的情况下都会造成崩溃，测试不通过
	// _setmode(_fileno(stdout), _O_U8TEXT);
	// _setmode(_fileno(stdout), _O_WTEXT);

	// 设置这两行即可，全流程使用窄字符流
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	TestLog(L"Plugin Test");
	TestLog(L"=========================");

	try {
		// 创建 mock host
		MockPluginHost mockHost;

		// 创建插件
		IPlugin* plugin = CreatePlugin();
		if (!plugin) {
			std::cerr << "Failed to create plugin" << std::endl;
			return 1;
		}

		// 初始化插件
		if (!plugin->Initialize(&mockHost)) {
			std::cerr << "Failed to initialize plugin" << std::endl;
			DestroyPlugin(plugin);
			return 1;
		}

		// 输出插件信息
		TestLog(L"Plugin Name: " + (plugin->GetPluginName()));
		TestLog(L"Plugin Version: " + plugin->GetPluginVersion());
		TestLog(L"Plugin Package: " + plugin->GetPluginPackageName());
		TestLog(L"Description: " + plugin->GetPluginDescription());
		TestLog(L"=========================");

		// 测试设置json文本
		std::vector<SettingItem> settings2 = ParseConfig(wide_to_utf8(plugin->DefaultSettingJson()), 0);
		std::function<void(std::vector<SettingItem>&)> loadSettingsToMap =
			[&](std::vector<SettingItem>& settings) {
				for (auto& item : settings) {
					if (MyEndsWith2(item.key,".start_str")) {
						item.stringValue = "";
					}
					mockHost.settingsMap[item.key] = item;
					if ((item.type == "expand" || item.type == "expandswitch") && !item.children.empty()) {
						loadSettingsToMap(item.children);
					}
				}
		};
		loadSettingsToMap(settings2);
		TestLog(L"Setting items size: " + std::to_wstring(mockHost.settingsMap.size()));

		// 加载用户设置
		plugin->OnUserSettingsLoadDone();

		// 刷新所有动作
		TestLog(L"Refreshing actions...");

		try {
			plugin->RefreshAllActions();
			TestLog(L"Refresh completed.");
		} catch (const std::exception& e) {
			std::cerr << "[RefreshAllActions]" << "Exception during refresh: " << e.what() << std::endl;
			return 1;
		}

		// 获取动作列表
		auto actions = plugin->GetTextMatchActions();
		TestLog(L"Total actions loaded: " + std::to_wstring(actions.size()));

		// 显示前几个动作
		const int displayCount = std::min(5, static_cast<int>(actions.size()));
		if (displayCount > 0) {
			TestLog(L"\nFirst " + std::to_wstring(displayCount) + L" actions:");
			for (int i = 0; i < displayCount; i++) {
				TestLog(L"  [" + std::to_wstring(i + 1) + L"] " + actions[i]->getTitle());
				TestLog(L"      " + actions[i]->getSubTitle());
			}
		}

		// 关闭插件
		plugin->Shutdown();
		DestroyPlugin(plugin);

		TestLog(L"\n✅✅ Test completed successfully!");
		return 0;
	} catch (const std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
		return 1;
	} catch (...) {
		std::cerr << "Unknown exception occurred" << std::endl;
		return 1;
	}
}
