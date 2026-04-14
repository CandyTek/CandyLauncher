#include "../Plugin.hpp"
#include <windows.h>
#include <memory>
#include <string>

#include "ToolBoxAction.hpp"
#include "ToolBoxPluginData.hpp"
#include "ToolBoxUtil.hpp"

class SmallToolBoxPlugin : public IPlugin {
private:
	std::vector<std::shared_ptr<BaseAction>> allPluginActions;
	std::vector<std::shared_ptr<BaseAction>> dynamicExampleActions;

public:
	SmallToolBoxPlugin() = default;
	~SmallToolBoxPlugin() override = default;

	std::wstring GetPluginName() const override {
		return L"SmallToolBoxPlugin";
	}

	std::wstring GetPluginPackageName() const override {
		return L"com.candytek.smalltoolbox";
	}

	std::wstring GetPluginVersion() const override {
		return L"1.0.0";
	}

	std::wstring GetPluginDescription() const override {
		return L"小工具箱";
	}

	// 不要在初始化里加载Actions，由主程序调用 RefreshAllActions
	bool Initialize(IPluginHost* host) override {
		m_host = host;
		return m_host != nullptr;
	}
	
	void OnPluginIdChange(const uint16_t pluginId) override {
		m_pluginId = pluginId;
	}


	void RefreshAllActions() override {
		if (!m_host) return;

		allPluginActions.clear();
		// 应该使用make_shared，而不是简单的ExampleAction，这样子iconBitmap 的图标才能正常显示
		std::shared_ptr<SmallToolBoxAction> action1 = std::make_shared<SmallToolBoxAction>();
		action1->id = L"show_qrcode";
		action1->title = L"生成二维码";
		action1->subTitle = L"qrcode:www.example.com";
		action1->matchText = m_host->GetTheProcessedMatchingText(action1->title);
		action1->iconBitmap = LoadShell32IconAsBitmap(15);
		allPluginActions.push_back(action1);

		std::shared_ptr<SmallToolBoxAction> action2 = std::make_shared<SmallToolBoxAction>();
		action2->id = L"color_converter";
		action2->title = L"颜色格式转换";
		action2->subTitle = L"#RRGGBB <-> R,G,B";
		action2->matchText = m_host->GetTheProcessedMatchingText(action2->title);
		action2->iconBitmap = LoadShell32IconAsBitmap(174);
		allPluginActions.push_back(action2);

		// std::shared_ptr<ExampleAction> action2 = std::make_shared<ExampleAction>();
		// action2->id = L"ExamplePlugin_time";
		// action2->title = L"显示时间";
		// action2->subTitle = L"显示当前系统时间";
		// action2->matchText = m_host->GetTheProcessedMatchingText(action2->title);
		// action2->iconBitmap = LoadShell32IconAsBitmap(265);
		// allPluginActions.push_back((action2));
		//
		// std::shared_ptr<ExampleAction> action3 = std::make_shared<ExampleAction>();
		// action3->id = L"ExamplePlugin_time_en";
		// action3->title = L"Show Time";
		// action3->subTitle = L"Show current system time";
		// action3->matchText = m_host->GetTheProcessedMatchingText(action3->title);
		// action3->iconBitmap = LoadShell32IconAsBitmap(265);
		// allPluginActions.push_back((action3));
	}

	std::wstring DefaultSettingJson() override {
		return LR"(
{
}

   )";
	}

	void OnUserSettingsLoadDone() override {
		// auto it = settings_map.find("pref_pinyin_mode");
		// if (it != settings_map.end()) {
		// 	pinyinType = it->second.stringValue;
		// }
	}

	void Shutdown() override {
		if (m_host) {
			allPluginActions.clear();
		}
		m_host = nullptr;
	}

	std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() override {
		if (!m_host) return {};

		return allPluginActions;
	}
	

	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring& arg) override {
		if (!m_host) return false;
		auto exampleAction = std::dynamic_pointer_cast<SmallToolBoxAction>(action);
		if (!exampleAction) return false;

		if (exampleAction->id == L"show_qrcode") {
			// 从剪贴板获取文本
			std::string clipboardText = GetClipboardTextAsUTF8();
			if (clipboardText.empty()) {
				MessageBoxW(NULL, L"剪贴板为空或无法获取文本", L"提示", MB_OK | MB_ICONINFORMATION);
				return false;
			}

			// 生成二维码
			try {
				using namespace qrcodegen;
				QrCode qr = QrCode::encodeText(clipboardText.c_str(), QrCode::Ecc::MEDIUM);

				// 显示二维码窗口
				ShowQRCodeWindow(qr, clipboardText);
			} catch (const std::exception& e) {
				std::wstring errorMsg = L"生成二维码失败: " + std::wstring(e.what(), e.what() + strlen(e.what()));
				MessageBoxW(NULL, errorMsg.c_str(), L"错误", MB_OK | MB_ICONERROR);
				return false;
			}
		}
		else if (exampleAction->id == L"color_converter") {
			// 从剪贴板获取颜色值
			std::string clipboardText = GetClipboardTextAsUTF8();
			if (clipboardText.empty()) {
				MessageBoxW(NULL, L"剪贴板为空或无法获取文本", L"提示", MB_OK | MB_ICONINFORMATION);
				return false;
			}

			// 去除首尾空格
			std::string trimmed = TrimString(clipboardText);
			std::string converted;

			// 尝试转换颜色格式
			if (IsHexColor(trimmed)) {
				// #RRGGBB -> R,G,B
				converted = HexToRgb(trimmed);
			} else if (IsRgbColor(trimmed)) {
				// R,G,B -> #RRGGBB
				converted = RgbToHex(trimmed);
			} else {
				MessageBoxW(NULL, L"剪贴板内容不是有效的颜色格式\n支持格式:\n  #RRGGBB (如 #888800)\n  R,G,B (如 150,100,0)",
					L"提示", MB_OK | MB_ICONINFORMATION);
				return false;
			}

			// 写回剪贴板
			if (SetClipboardText(converted)) {
				std::wstring msg = L"已转换并写入剪贴板:\n" + utf8_to_wide(converted);
				MessageBoxW(NULL, msg.c_str(), L"成功", MB_OK | MB_ICONINFORMATION);
			} else {
				MessageBoxW(NULL, L"写入剪贴板失败", L"错误", MB_OK | MB_ICONERROR);
				return false;
			}
		}
		return true;
	}
	
};

PLUGIN_EXPORT IPlugin* CreatePlugin() {
	return new SmallToolBoxPlugin();
}

PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) {
	delete plugin;
}

PLUGIN_EXPORT int GetPluginApiVersion() {
	return 1;
}
