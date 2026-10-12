#include <windows.h>
#include <memory>
#include <string>

#include "ToolBoxAction.hpp"
#include "ToolBoxPluginData.hpp"
#include "ToolBoxUtil.hpp"
#include "util/StringUtil.hpp"

class SmallToolBoxPlugin : public candy::Plugin {
private:
	// 0 号集合：固定工具项，参与全局搜索；1 号：输入 "qrcode xxx" 时按键生成的临时结果
	candy::ActionSet<SmallToolBoxAction> allPluginActions{0};
	candy::TransientSet<SmallToolBoxAction> qrcodeResults{1};

	std::shared_ptr<SmallToolBoxAction> FindAction(const uint64_t userData) const {
		if (auto action = allPluginActions.Find(userData)) return action;
		return qrcodeResults.Find(userData);
	}

	// 生成并显示二维码窗口
	static bool ShowQrCodeForText(const std::string& textUtf8) {
		try {
			using namespace qrcodegen;
			QrCode qr = QrCode::encodeText(textUtf8.c_str(), QrCode::Ecc::MEDIUM);

			// 显示二维码窗口
			ShowQRCodeWindow(qr, textUtf8);
			return true;
		} catch (const std::exception& e) {
			std::wstring errorMsg = L"生成二维码失败: " + std::wstring(e.what(), e.what() + strlen(e.what()));
			MessageBoxW(NULL, errorMsg.c_str(), L"错误", MB_OK | MB_ICONERROR);
			return false;
		}
	}

public:
	SmallToolBoxPlugin() = default;
	~SmallToolBoxPlugin() override = default;

	// 不要在初始化里加载Actions，由主程序调用 RefreshActions
	bool Initialize() override {
		m_host = &GetHost();
		return true;
	}

	void RefreshActions() override {
		// 应该使用make_shared，而不是简单的ExampleAction，这样子iconBitmap 的图标才能正常显示
		// matchText 留空，由主程序根据 title 生成（小写 + 拼音）
		std::shared_ptr<SmallToolBoxAction> action1 = std::make_shared<SmallToolBoxAction>();
		action1->id = L"show_qrcode";
		action1->title = L"从剪贴板生成二维码";
		action1->subtitle = L"";
		action1->stableKey = action1->id;
		action1->iconBitmap = LoadShell32IconAsBitmap(15);

		std::shared_ptr<SmallToolBoxAction> action2 = std::make_shared<SmallToolBoxAction>();
		action2->id = L"color_converter";
		action2->title = L"颜色格式转换";
		action2->subtitle = L"#RRGGBB <-> R,G,B";
		action2->stableKey = action2->id;
		action2->iconBitmap = LoadShell32IconAsBitmap(174);

		allPluginActions.Publish(GetHost(), {action1, action2});
	}

	void OnUserSettingsLoaded() override {
		// 2.2 主程序只在输入以 "qrcode " 开头时调用 OnInterceptInput；旧主程序上照旧每次都调用
		GetHost().SetTriggerKeywords({L"qrcode "});
	}

	void Shutdown() override {
		allPluginActions.ClearLocal();
		m_host = nullptr;
	}

	// "qrcode xxx"：接管结果列表，显示只为本次输入生成的条目
	void OnInterceptInput(const std::wstring& input, candy::ResultSink& sink) override
	{
		if (StartsWith(input, L"qrcode "))
		{
			size_t spacePos = input.find(L' ');
			std::wstring query = input.substr(spacePos + 1);

			auto action = std::make_shared<SmallToolBoxAction>();
			action->id = L"gen_qrcode";
			action->title = L"生成二维码";
			action->subtitle = query;
			action->stableKey = action->id;
			sink.AddTransient(qrcodeResults, {action});
		}
	}

	bool OnActionExecute(const uint64_t userData, const std::wstring& arg) override {
		const auto exampleAction = FindAction(userData);
		if (!exampleAction) return false;

		if (exampleAction->id == L"show_qrcode") {
			// 从剪贴板获取文本
			std::string clipboardText = GetClipboardTextAsUTF8();
			if (clipboardText.empty()) {
				MessageBoxW(NULL, L"剪贴板为空或无法获取文本", L"提示", MB_OK | MB_ICONINFORMATION);
				return false;
			}

			// 生成二维码
			return ShowQrCodeForText(clipboardText);
		} else if (exampleAction->id == L"gen_qrcode") {
			// 获取文本
			if (exampleAction->subtitle.empty()) {
				return false;
			}

			// 生成二维码
			return ShowQrCodeForText(wide_to_utf8(exampleAction->subtitle));
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
			if (GetHost().SetClipboardText(utf8_to_wide(converted))) {
				std::wstring msg = L"已转换并写入剪贴板:\n" + utf8_to_wide(converted);
				MessageBoxW(NULL, msg.c_str(), L"成功", MB_OK | MB_ICONINFORMATION);
			} else {
				MessageBoxW(NULL, L"写入剪贴板失败", L"错误", MB_OK | MB_ICONERROR);
				return false;
			}
		}
		return true;
	}

	// Ctrl+C 复制 "qrcode xxx" 条目的文本
	bool GetItemCopyText(const uint64_t userData, std::wstring& text) override {
		const auto action = FindAction(userData);
		if (!action || action->id != L"gen_qrcode" || action->subtitle.empty()) return false;
		text = action->subtitle;
		return true;
	}
};

CANDY_PLUGIN_EXPORT(SmallToolBoxPlugin, L"Small Toolbox", L"A collection of small utilities", L"1.0.0", L"com.candytek.smalltoolbox",
					nullptr)
