# 高 DPI 适配

## 总体方案

- 进程本身保持 **DPI 不感知**（`common/app.manifest` 不声明 `dpiAware`）。Windows 7 / 8.1 上，以及尚未适配的插件窗口
  （截图工具的覆盖层等）仍由系统整体拉伸，行为和以前完全一样。
- **次级窗口用简单方案自行适配**（不考虑 Win7）：设置窗口、皮肤编辑器、重命名对话框，以及 Folder 插件的索引管理器 /
  已排除项 / 快捷方式修复 / 自动化动作组编辑器、WebSearch 的搜索引擎管理器、SmallToolBox 的二维码窗口，都在各自的创建函数里进入
  `Dpi::ThreadContextScope(Dpi::PerMonitorV2Context())`，初始尺寸按 `Dpi::PrimaryMonitorDpi()` / `Dpi::MonitorDpiForWindow(parent)`
  换算；`WM_CREATE` 里用 `Dpi::WindowDpi(hwnd)` 得到窗口 DPI，布局常量（都是 96 DPI 下的值）经 `Dpi::Scale` 换算，字体用
  `Dpi::CreateScaledFont` 创建；`WM_DPICHANGED` 里先 `Dpi::ApplySuggestedRect` 移到建议矩形，然后要么重建字体并用
  `Dpi::RescaleChildWindows` 等比缩放子控件和 ListView 列宽（布局靠 `WM_SIZE` 的窗口自动重排），要么像设置窗口那样销毁全部
  子控件按新 DPI 重建（`CreateSettingsWindowChildren`）。自绘控件（`view/CustomButtonHelper.hpp`、`SwitchViewPainter.hpp`、
  `CustomComboBox.hpp`、`CustomEdit.hpp`、`HotkeyEditView.hpp`）绘制时按 `Dpi::WindowDpi(hWnd)` 缩放线宽、边距和图标，
  它们共用的字体由 `InitializeCustomButtonResources(dpi)` 按设置窗口的 DPI 重建。
  皮肤编辑器的 WebBrowser 控件在 `DOCHOSTUIFLAG_DPI_AWARE` 宿主里不会自己缩放页面，页面加载完成后按窗口 DPI 设置
  `OLECMDID_OPTICAL_ZOOM`（`SkinEditorHost::ApplyZoom`）。
- 试过的死路：进程级 GDI 缩放（`SetProcessDpiAwarenessContext(UNAWARE_GDISCALED)`；manifest 的 `gdiScaling` 甚至不会改变线程
  上下文）只让直接画在窗口 DC 上的 GDI 文字变清晰，双缓冲到内存位图的自绘控件仍然模糊，而 IE WebBrowser 控件会被放大两次
  （自身按 DPI 缩放后再被系统拉伸）且点击位置错位。
- **主窗口**（搜索框 + 列表）在 Windows 10 1703+ 上以 `DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2` 创建
  （`CandyLauncher.cpp` 的 `MainWindowInitInstance`，通过 `Dpi::ThreadContextScope` 只对创建主窗口和子控件的那段代码生效），
  之后自己按所在显示器的 DPI 缩放皮肤。
- 所有 Win10 的 DPI API 通过 `GetProcAddress` 动态加载（`util/DpiUtil.hpp`），exe 不直接导入，Win7 仍能启动；
  系统不支持时所有作用域都是空操作，主窗口保持不感知。

## 渲染：只在烘焙时缩放，每帧仍是 BitBlt

皮肤 json 里的所有尺寸、位置、字号都是 **96 DPI 下的值**。`refreshSkin`（`manager/SkinHelper.hpp`）按
`g_mainWindowDpi` 用 `MainDpiScale` / `MainDpiScaleF`（`common/GlobalState.hpp`）换算后再创建控件尺寸、字体、
预乘 DIB 背景图层，所以：

- `WM_PAINT` 路径没有任何改动，依旧是一次 `BitBlt` / `AlphaBlend`（见 `manager/BackgroundLayerPainter.hpp`）。
- 九宫格图片 `RenderNinePatchToSize(..., scale)` 的固定段（圆角、边框）也按比例放大并做高质量重采样，`scale == 1` 时输出与之前完全一致。
- 列表图标尺寸 `LISTITEM_ICON_SIZE = 48 * dpi / 96`。系统图标由 `ListViewSystemIconWarmer` 的工作线程用
  `SHIL_JUMBO`（256px）缩小生成预乘 DIB 并按图标索引缓存（`util/IconScaleUtil.hpp`）；没有 256px 版本的图标退回
  48px 放大。插件给的 `icon_bitmap` 在 `SharedIconBitmap::ScaledFor` 里按需缩放一次并缓存。96 DPI 下这些路径都不参与，
  仍直接 `ImageList_Draw` / `AlphaBlend`。

## DPI 何时变化

`applyMainWindowDpi(dpi)`（`manager/SkinHelper.hpp`）：DPI 变了就重新 `refreshSkin`（不显示窗口）。触发点：

1. **弹出前**：`util/MainTools.hpp` 的各个 `showMainWindowIn*` 在算位置之前调用 `g_prepareMainWindowForMonitor(hMonitor)`，
   先按目标显示器 DPI 重载皮肤，这样 `MAIN_WINDOW_WIDTH/HEIGHT` 已经是最终尺寸，窗口位置一次算对。
2. **`WM_DPICHANGED`**（拖到另一台显示器、系统缩放设置改变）：DPI 与当前不同才重载皮肤并移到系统建议的位置；
   弹出前已经处理过的情况下 DPI 相同，直接忽略。

## 混合模式下的坐标空间（最容易踩坑的地方）

线程的 DPI 上下文决定 `GetWindowRect` / `SetWindowPos` / `GetCursorPos` 等的坐标空间。系统在调用某个窗口的窗口过程时会
临时切到该窗口的上下文，所以主窗口消息里拿到的是物理像素；但从 `wWinMain`、钩子回调、设置窗口里操作主窗口时线程是不感知的，
得到的是虚拟化坐标。规则：

- 操作主窗口（定位、测量、`refreshSkin`、保存位置）：`Dpi::ThreadContextScope scope(Dpi::ContextOfWindow(g_mainHwnd))`。
  已加在 `refreshSkin`、`MyMoveWindow`、`ShowMainWindowSimple`、`HideWindow`、`SaveWindowRectToRegistry`。
- 从主窗口消息里调用插件：`Dpi::ThreadContextScope unaware(Dpi::UnawareContext())`，已加在 `PluginManager` 的
  `DispatchActionExecute` / `DispatchSendHotKey` / `DispatchItem*` / `DispatchSettingItemExecute`；
  已自行适配的插件窗口在自己的创建函数里再进入 `PerMonitorV2Context` 作用域，不感知的只有尚未适配的插件窗口。
- 传给插件的屏幕坐标要在不感知上下文里取：`NM_RCLICK` 在该作用域里重新 `GetCursorPos`；列表项中心、拖拽起点用
  `Dpi::ChildClientPointToThreadScreen` 换算（实测系统会按调用方上下文换算 `GetWindowRect`，物理 (3000,500) 750x750 在不感知进程里是 (2853,450) 500x500）。
- `GetScrollBarInfo` 的 `dxyLineButton` 在 Per-Monitor 子窗口里不随 DPI 缩放（150% 下仍报 17，实际 26），
  而且弹出前按目标显示器 DPI 重载皮肤时窗口还在旧显示器上，现场测量得到的也是旧值；
  所以列宽按 `Dpi::SystemMetricForDpi(SM_CXVSCROLL, g_mainWindowDpi)` 计算（96/120/144/192 DPI 下为 17/21/26/34），否则列表会多出横向滚动条。
- `Dpi::MonitorDpi` 的返回值取决于调用线程的上下文，不感知线程永远得到 96，要在主窗口上下文里调用。

## 验证方法

本机 DISPLAY3（x >= 2560）是 150%。把主窗口 `SetWindowPos` 到 (3000,500) 再移回 (100,100)，日志应出现
`Main window DPI 144 -> 96` / `96 -> 144`，窗口尺寸 750x750 / 500x500（skin_default），列表无横向滚动条，
用 `PrintWindow(PW_RENDERFULLCONTENT)` 截图确认文字、图标、九宫格边框都已放大且清晰。

## 尚未自行适配的部分

ScreenTools 的截图覆盖层、崩溃对话框、调试用的 Shell32 图标查看器仍由系统拉伸。要迁移某个窗口时按上面的简单方案：
创建处进入 `PerMonitorV2Context` 作用域，布局常量、字体按 `Dpi::WindowDpi(hwnd)` 换算，处理 `WM_DPICHANGED`。
主程序调用插件时线程处于不感知上下文（`PluginManager` 的各 `Dispatch*`），所以插件窗口必须在自己的创建函数里进入 Per-Monitor 作用域。

## 验证次级窗口

把窗口移到 150% 的 DISPLAY3（物理 x >= 2560）上：`GetDpiForWindow` 应变为 144，窗口按系统建议矩形放大 1.5 倍（设置窗口 640x680 -> 960x1020），
用 Per-Monitor 线程 BitBlt 屏幕区域截图，文字、开关、列表应清晰且布局按比例放大；皮肤编辑器页面应整体放大 1.5 倍而不是 2.25 倍。
