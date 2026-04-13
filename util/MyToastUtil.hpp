#pragma once
#if !defined(__MINGW32__)
#include <wintoastlib.h>

#include "LogUtil.hpp"

static const wchar_t* APP_ID = L"com.candytek.candylauncher.ToastSample";


// WinToast Handler
class CandyToastHandler final : public WinToastLib::IWinToastHandler {
public:
	void toastActivated() const override {
		ConsolePrintln(L"WinToast", L"通知被激活");
	}

	void toastActivated(int actionIndex) const override {
		ConsolePrintln(L"WinToast", L"通知动作被激活: " + std::to_wstring(actionIndex));
	}

	void toastActivated(std::wstring response) const override {
		ConsolePrintln(L"WinToast", L"通知响应: " + response);
	}

	void toastDismissed(WinToastDismissalReason state) const override {
		switch (state) {
		case UserCanceled: ConsolePrintln(L"WinToast", L"用户取消了通知");
			break;
		case TimedOut: ConsolePrintln(L"WinToast", L"通知超时");
			break;
		case ApplicationHidden: ConsolePrintln(L"WinToast", L"应用隐藏了通知");
			break;
		default: ConsolePrintln(L"WinToast", L"通知被忽略");
			break;
		}
	}

	void toastFailed() const override {
		ConsolePrintln(L"WinToast", L"通知显示失败");
	}
};

// 初始化 WinToast
// 返回 true 表示初始化成功，false 表示失败
inline bool InitializeWinToast() {
	using namespace WinToastLib;

	if (!WinToast::instance()->isCompatible()) {
		ConsolePrintln(L"WinToast", L"系统不兼容 WinToast");
		return false;
	}

	WinToast::instance()->setAppName(L"CandyLauncher");
	const auto aumi = WinToast::configureAUMI(L"CandyTek", L"CandyLauncher", L"Launcher", L"1.0");
	WinToast::instance()->setAppUserModelId(aumi);

	WinToast::WinToastError error;
	const auto succeeded = WinToast::instance()->initialize(&error);
	if (!succeeded) {
		ConsolePrintln(L"WinToast", L"初始化失败,错误代码: " + std::to_wstring(static_cast<int>(error)));
		return false;
	}

	ConsolePrintln(L"WinToast", L"初始化成功");
	return true;
}

// 显示简单的 Toast 通知
// title: 通知标题（第一行）
// message: 通知内容（第二行）
// 返回通知ID，失败返回 -1
static INT64 MyShowSimpleToast(const std::wstring& title, const std::wstring& message) {
	if (!isToastAvailable) {
		return -1;
	}
	using namespace WinToastLib;

	if (!WinToast::instance()->isInitialized()) {
		ConsolePrintln(L"WinToast", L"WinToast 未初始化，无法显示通知");
		return -1;
	}

	CandyToastHandler* handler = new CandyToastHandler();
	WinToastTemplate templ = WinToastTemplate(WinToastTemplate::Text02);
	templ.setTextField(title, WinToastTemplate::FirstLine);
	templ.setTextField(message, WinToastTemplate::SecondLine);

	WinToast::WinToastError error;
	const auto toast_id = WinToast::instance()->showToast(templ, handler, &error);
	if (toast_id < 0) {
		ConsolePrintln(L"WinToast", L"显示通知失败,错误代码: " + std::to_wstring(static_cast<int>(error)));
		delete handler;
		return -1;
	}

	// ConsolePrintln(L"WinToast", L"通知已显示,ID: " + std::to_wstring(toast_id));
	return toast_id;
}
#endif
