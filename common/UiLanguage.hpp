#pragma once

#include <windows.h>
#include <cwchar>
#include <iterator>

// 与宿主资源 DLL 无关的轻量语言判断，给同时被主程序和插件 DLL 编译的公共头（RenameDialog、ToolTipHelper、
// MainTools、LogUtil 等）使用。主程序加载语言 DLL 后把语言代码写进本进程的环境变量，插件 DLL 在同一进程里
// 即可读到用户设置的界面语言；主程序尚未初始化（启动早期）或变量不存在时退回系统 UI 语言。
constexpr const wchar_t* kUiLanguageEnvVar = L"CANDYLAUNCHER_UI_LANG";

inline bool UiLanguageIsChinese() {
	wchar_t buffer[16] = {};
	const DWORD len = GetEnvironmentVariableW(kUiLanguageEnvVar, buffer, static_cast<DWORD>(std::size(buffer)));
	if (len > 0 && len < std::size(buffer)) {
		return wcsncmp(buffer, L"zh", 2) == 0;
	}
	return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE;
}

// 按当前界面语言在中英文字面量之间选择；完整的字符串表请优先用 common/I18n.hpp 的 LoadI18nString
inline const wchar_t* TrZhEn(const wchar_t* zh, const wchar_t* en) {
	return UiLanguageIsChinese() ? zh : en;
}
