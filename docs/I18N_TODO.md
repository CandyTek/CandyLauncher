国际化初步已完成实现，英文和中文


可以。对你这个 Win32 项目，**最实用的做法**是：

* 主程序 EXE 只保留代码
* 每种语言一个 **resource-only DLL**
* 所有窗口标题、按钮文字、Tab 名称、菜单、图标、对话框模板都放进 DLL
* 启动时按用户语言加载对应 DLL
* 之后统一从这个 DLL 里 `LoadStringW` / `LoadImage` / `DialogBoxParamW`

这套方案和 Win32/MUI 的思路一致：把“代码”和“可本地化资源”拆开。微软文档明确支持用一个核心 EXE 配多语言资源 DLL；如果 DLL 只用于取资源，推荐用 `LoadLibraryEx` 的资源/数据文件方式加载。([Microsoft Learn][1])

你当前代码里已经有明显适合改造的点：`tabNameMap` 现在是把中文直接写在代码里，而且类型也不对；更合理的是把 Tab 文本改成资源 ID，再从语言 DLL 读取。你贴出来的代码里，窗口标题 `"设置"`、按钮 `"保存"` / `"取消"`、以及 Tab 文案 `"插件"` 都还是硬编码状态。

## 一套最小可落地的结构

目录可以这样：

```text
MyApp.exe
lang/
  zh-CN.dll
  en-US.dll
  ja-JP.dll
```

资源 ID 统一定义在一个共享头文件里，比如 `ResourceIds.h`：

```cpp
#pragma once

#define IDS_APP_TITLE      1000
#define IDS_BTN_SAVE       1001
#define IDS_BTN_CANCEL     1002
#define IDS_TAB_PLUGIN     1003
#define IDS_TAB_NORMAL     1004
#define IDS_TAB_FEATURE    1005

#define IDI_APP_SMALL      2000
#define IDD_SETTINGS       3000
```

注意这里的 **资源 ID 必须在所有语言 DLL 里保持一致**。同一个 ID，在中文 DLL 里对应中文，在英文 DLL 里对应英文。

---

## 第一步：做资源 DLL

微软的做法是新建一个 DLL 项目，把 `.rc` 等资源加进去；resource-only DLL 本质上就是“只含资源的 DLL”。([Microsoft Learn][2])

### `zh-CN.rc`

```rc
#include "ResourceIds.h"

LANGUAGE LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED

STRINGTABLE
BEGIN
    IDS_APP_TITLE   "设置"
    IDS_BTN_SAVE    "保存"
    IDS_BTN_CANCEL  "取消"
    IDS_TAB_PLUGIN  "插件"
    IDS_TAB_NORMAL  "常规"
    IDS_TAB_FEATURE "功能"
END

IDI_APP_SMALL ICON "res\\app.ico"
```

### `en-US.rc`

```rc
#include "ResourceIds.h"

LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US

STRINGTABLE
BEGIN
    IDS_APP_TITLE   "Settings"
    IDS_BTN_SAVE    "Save"
    IDS_BTN_CANCEL  "Cancel"
    IDS_TAB_PLUGIN  "Plugin"
    IDS_TAB_NORMAL  "General"
    IDS_TAB_FEATURE "Features"
END

IDI_APP_SMALL ICON "res\\app.ico"
```

如果你还要本地化菜单、对话框模板、加速键表，也都放进去。

---

## 第二步：主程序保存一个“当前语言模块句柄”

你不要再直接用 `g_hInst` 去加载文本资源，而是加一个专门的语言模块句柄：

```cpp
inline HMODULE g_hLangRes = nullptr;
```

加载语言 DLL：

```cpp
bool LoadLanguageModule(const std::wstring& dllPath)
{
    if (g_hLangRes) {
        FreeLibrary(g_hLangRes);
        g_hLangRes = nullptr;
    }

    g_hLangRes = LoadLibraryExW(
        dllPath.c_str(),
        nullptr,
        LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE
    );

    return g_hLangRes != nullptr;
}
```

`LoadLibraryEx` 这类资源加载方式适合“只访问 DLL 中的数据或资源”；同时，用这种方式加载的模块不能拿来 `GetProcAddress` 查函数地址，因为它不是按可执行 DLL 的方式使用。([Microsoft Learn][3])

---

## 第三步：封装读字符串

`LoadStringW` 可以从指定模块读取字符串资源。([Microsoft Learn][4])

```cpp
std::wstring LoadResString(UINT id)
{
    HINSTANCE hMod = g_hLangRes ? g_hLangRes : GetModuleHandleW(nullptr);

    wchar_t buf[256] = {};
    int len = LoadStringW(hMod, id, buf, _countof(buf));
    if (len <= 0) {
        return L"";
    }
    return std::wstring(buf, len);
}
```

如果你担心字符串较长，可以做动态扩容版；但大多数按钮/标题 256 已够。

---

## 第四步：把你现在的硬编码 map 改成“key → 资源 ID”

你现在是：

```cpp
static std::map<std::string, std::string> tabNameMap = {
    {"plugin", "插件",IDI_SMALL},
};
```

这段本身就不合法，而且也不适合国际化。更合理的是：

```cpp
struct TabMeta {
    UINT textResId;
    UINT iconResId;
};

static std::map<std::string, TabMeta> tabNameMap = {
    {"plugin",  {IDS_TAB_PLUGIN,  IDI_APP_SMALL}},
    {"normal",  {IDS_TAB_NORMAL,  IDI_APP_SMALL}},
    {"feature", {IDS_TAB_FEATURE, IDI_APP_SMALL}},
};
```

然后你在创建 Tab 按钮时，改成：

```cpp
for (size_t i = 0; i < subPageTabs.size(); ++i) {
    std::wstring label;
    UINT iconId = IDI_APP_SMALL;

    auto it = tabNameMap.find(subPageTabs[i]);
    if (it != tabNameMap.end()) {
        label = LoadResString(it->second.textResId);
        iconId = it->second.iconResId;
    } else {
        label = utf8_to_wide(subPageTabs[i]);
    }

    HICON myIcon = (HICON)LoadImageW(
        g_hLangRes ? g_hLangRes : g_hInst,
        MAKEINTRESOURCEW(iconId),
        IMAGE_ICON,
        16, 16,
        LR_DEFAULTCOLOR
    );

    HWND hTabBtn = CreateEnhancedButton(
        hwnd,
        (2000 + i),
        label,
        0, tabY, tabBtnWidth, 48,
        (HMENU)(2000 + i),
        BTN_NORMAL,
        myIcon,
        TEXT_LEFT
    );

    hTabButtons.push_back(hTabBtn);
    tabY += 48;
}
```

这样 `plugin` 这个业务 key 永远不变，只是显示文本随 DLL 改变。

---

## 第五步：把窗口标题、按钮文字都换成资源读取

你代码里现在是：

```cpp
L"设置"
L"保存"
L"取消"
```

这些都应该改。你贴出来的 `CreateWindowExW` 和保存/取消按钮创建位置都可以直接替换。

例如：

```cpp
g_settingsHwnd = CreateWindowExW(
    WS_EX_ACCEPTFILES,
    L"SettingsWndClass",
    LoadResString(IDS_APP_TITLE).c_str(),
    dw_style,
    x, y,
    SETTINGS_WINDOW_WIDTH, SETTINGS_WINDOW_HEIGHT,
    hParent,
    nullptr,
    hInstance,
    nullptr
);
```

保存按钮：

```cpp
CreateEnhancedButton(
    hwnd, 1, LoadResString(IDS_BTN_SAVE),
    SETTINGS_WINDOW_WIDTH - 200, SETTINGS_WINDOW_HEIGHT - 80,
    80, 35, (HMENU)9999, BTN_PRIMARY
);
```

取消按钮：

```cpp
CreateEnhancedButton(
    hwnd, 1, LoadResString(IDS_BTN_CANCEL),
    SETTINGS_WINDOW_WIDTH - 110, SETTINGS_WINDOW_HEIGHT - 80,
    80, 35, (HMENU)9995, BTN_NORMAL
);
```

---

## 第六步：程序启动时决定加载哪种语言

最简单的是配置文件指定：

```json
{
  "ui_language": "en-US"
}
```

启动时：

```cpp
void InitLanguage()
{
    std::wstring lang = L"zh-CN"; // 从配置读取，或根据系统语言判断
    std::wstring path = L".\\lang\\" + lang + L".dll";

    if (!LoadLanguageModule(path)) {
        // 回退到内置资源或默认语言 DLL
        LoadLanguageModule(L".\\lang\\zh-CN.dll");
    }
}
```

如果没加载成功，也可以退回 EXE 自带默认资源。

---

## 第七步：切换语言时怎么刷新

如果你的设置窗口已经打开，语言切换后要做两件事：

1. 重新加载新的资源 DLL
2. 重建窗口文本或直接重建整个界面

对你这个项目，最稳的是“**重新创建设置窗口 UI**”。因为你本来就有 `RefreshSettingsUI` 和控件重建逻辑。

切换语言流程可以是：

```cpp
ChangeLanguage("en-US")
{
    LoadLanguageModule(L".\\lang\\en-US.dll");

    // 更新窗口标题
    SetWindowTextW(g_settingsHwnd, LoadResString(IDS_APP_TITLE).c_str());

    // 最稳妥：销毁并重建相关控件
    RefreshSettingsUI(g_settingsHwnd, tabContainers[currentSubPageIndex]);

    // 左侧 Tab、底部按钮也要重新设置文本
}
```

如果控件很多，甚至直接关掉设置窗口再重新打开都可以，逻辑更简单。

---

## 对话框、菜单、图标也能这么用

这不只是字符串表。

### 1. 图标

从语言 DLL 读：

```cpp
HICON hIcon = (HICON)LoadImageW(
    g_hLangRes,
    MAKEINTRESOURCEW(IDI_APP_SMALL),
    IMAGE_ICON,
    16, 16,
    LR_DEFAULTCOLOR
);
```

### 2. 菜单

如果菜单模板也在 DLL 里：

```cpp
HMENU hMenu = LoadMenuW(g_hLangRes, MAKEINTRESOURCEW(IDR_MAINMENU));
SetMenu(hwnd, hMenu);
DrawMenuBar(hwnd);
```

### 3. 对话框模板

如果设置窗口未来改成资源对话框：

```cpp
DialogBoxParamW(
    g_hLangRes,
    MAKEINTRESOURCEW(IDD_SETTINGS),
    hParent,
    SettingsDlgProc,
    0
);
```

也就是说，**谁需要资源，就传 `g_hLangRes`，不要默认只用 EXE 的 `g_hInst`**。

---

## 一个你会立刻踩到的点

如果你用 `LOAD_LIBRARY_AS_DATAFILE` 或同类 flag 加载 DLL，就不要把它当普通 DLL 调函数。微软文档明确说，`GetProcAddress` 不能用于这种方式加载的模块。([Microsoft Learn][5])

所以你的语言 DLL 应该是：

* **只放资源**
* **不放业务函数**
* 主程序只拿它做 `LoadStringW` / `FindResource` / `LoadImageW`

---

## 一个更工程化的建议

你现在项目里很多地方是直接：

* `utf8_to_wide(item.title)`
* `CreateWindowW(..., L"保存", ...)`
* `MessageBoxW(..., L"确定要重置所有设置为默认值吗？", L"确认重置", ...)`

像这种地方，应该逐步收敛成两类：

### 类 1：固定 UI 文案

全部改资源 ID：

```cpp
MessageBoxW(
    hwnd,
    LoadResString(IDS_RESET_CONFIRM_TEXT).c_str(),
    LoadResString(IDS_RESET_CONFIRM_TITLE).c_str(),
    MB_YESNO | MB_ICONQUESTION
);
```

### 类 2：配置驱动文本

如果 `item.title` 本身来自 JSON / 配置文件，那就不要直接存中文，而是存资源 key 或资源 ID。例如：

```json
{
  "key": "pref_backup_settings",
  "title_res": 5001
}
```

然后：

```cpp
std::wstring text = LoadResString(item.titleResId);
```

否则你后面会发现：UI 框架已经国际化了，但配置文件里的标题还是中文，语言还是切不干净。

---

## 给你一个最小可用骨架

### `I18n.h`

```cpp
#pragma once
#include <windows.h>
#include <string>

inline HMODULE g_hLangRes = nullptr;

inline bool LoadLanguageModule(const std::wstring& dllPath)
{
    if (g_hLangRes) {
        FreeLibrary(g_hLangRes);
        g_hLangRes = nullptr;
    }

    g_hLangRes = LoadLibraryExW(
        dllPath.c_str(),
        nullptr,
        LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE
    );

    return g_hLangRes != nullptr;
}

inline HINSTANCE GetResModule()
{
    return g_hLangRes ? g_hLangRes : GetModuleHandleW(nullptr);
}

inline std::wstring LoadResString(UINT id)
{
    wchar_t buf[256] = {};
    int len = LoadStringW(GetResModule(), id, buf, _countof(buf));
    return len > 0 ? std::wstring(buf, len) : L"";
}
```

### 启动时

```cpp
LoadLanguageModule(L".\\lang\\zh-CN.dll");
```

### 使用时

```cpp
SetWindowTextW(hwnd, LoadResString(IDS_APP_TITLE).c_str());
```

---

## 你现在最适合的改造顺序

别一次全改，按这个顺序就行：

1. **先做 `LoadLanguageModule` + `LoadResString`**
2. **先把窗口标题 / 保存 / 取消 / Tab 文案改掉**
3. **再把 MessageBox 文案改掉**
4. **再把菜单、图标、对话框模板迁到 DLL**
5. **最后处理 `g_settings_ui` / JSON 里的 title 字段**

这样风险最低。

## 结论

你要用“多语言资源 DLL”，核心就三件事：

* **资源统一编号**
* **启动时加载当前语言 DLL**
* **所有 UI 文案都从该模块读取，而不是硬编码**

对你当前代码，第一刀就是把：

* `L"设置"`
* `L"保存"`
* `L"取消"`
* `tabNameMap` 里的中文文本

全部替换成资源 ID + `LoadResString()`。你贴出的这段代码正好就是最先该改的区域。

我可以下一条直接给你一份 **按你这段 Settings 窗口代码改好的示例版本**，包括：

* `ResourceIds.h`
* `I18n.h`
* `tabNameMap` 重构
* `ShowSettingsWindow` / `WM_CREATE` 的具体替换代码

[1]: https://learn.microsoft.com/en-us/windows/win32/intl/development-of-mui-applications?utm_source=chatgpt.com "Development of MUI Applications - Win32 apps"
[2]: https://learn.microsoft.com/en-us/cpp/build/creating-a-resource-only-dll?view=msvc-170&utm_source=chatgpt.com "Creating a resource-only DLL"
[3]: https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexa?utm_source=chatgpt.com "LoadLibraryExA function (libloaderapi.h) - Win32 apps"
[4]: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-loadstringw?utm_source=chatgpt.com "LoadStringW function (winuser.h) - Win32 apps"
[5]: https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getprocaddress?utm_source=chatgpt.com "GetProcAddress function (libloaderapi.h) - Win32 apps"
