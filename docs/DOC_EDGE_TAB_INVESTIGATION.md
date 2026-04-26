# Edge 标签页索引排查记录

日期：2026-04-27

## 背景

本次目标是在 `RunningApp` 插件中，为“运行中的窗口”增加一个可选功能：

- 开启插件设置项后
- 将 Microsoft Edge 的标签页也作为可搜索项加入索引
- 选中后尝试切换到对应标签页

当前相关代码入口：

- `plugins/RunningApp/RunningAppPlugin.cpp`
- `plugins/RunningApp/RunningAppAction.hpp`
- `util/ImmersiveAppViewTraverser.hpp`

## 本次结论

本次开发没有最终完成“稳定索引 Edge 标签页”这个功能。

但已经确认了两件重要的事：

1. 旧实现失败的核心原因不是搜索逻辑本身，而是 undocumented `ImmersiveShell` 接口的版本兼容问题。
2. 在当前开发环境的 Windows 10 19045 上，`IApplicationViewCollection` 的新 GUID 可以成功通过 `QueryService` 获取，`GetViews` 也可以成功返回系统 view 列表。

也就是说：

- “拿不到 `IApplicationViewCollection`” 这个第一层阻塞已经被定位并修正
- 但“为什么最终没有成功拿到可用的 Edge 标签页结果”仍然没有彻底收尾

## 最初失败现象

`TODO.md` 中记录的原始失败日志是：

```txt
CoCreateInstance: 0x00000000
QueryService IApplicationViewCollection: 0x80004001
```

这说明：

- `CLSID_ImmersiveShell` 可以成功实例化
- 但是对 `IServiceProvider::QueryService(...)` 的调用失败

错误码：

- `0x80004001`
- 含义：`E_NOTIMPL`

## 根因分析

排查后确认，本机系统为：

- Windows 10 Pro
- Build 19045

而 `IApplicationViewCollection` / `IApplicationView` 这套 undocumented 接口并不是单一稳定版本：

- 老版本 Windows 10 使用一套 GUID / vtable
- Windows 10 1809+ 使用另一套 GUID / vtable

之前代码的问题主要有两类：

1. `IApplicationViewCollection` 仍然使用旧 GUID
2. `IApplicationView` 仍然按旧的 `IUnknown` 形态定义，而在较新系统上实际更接近 `IInspectable` 形态

这会带来两个直接后果：

1. `QueryService` 可能直接失败
2. 即使拿到对象，后续 vtable 偏移也可能错位，导致调用异常或返回值不可信

## 本次已做的修正

### 1. 新增适配层

新增文件：

- `util/ImmersiveAppViewTraverser.hpp`

这个文件当前负责：

- 根据系统 build 判定使用老接口还是新接口
- 提供统一的遍历入口 `TraverseImmersiveApplicationViews(...)`
- 封装以下操作
  - `GetShowInSwitchers`
  - `GetThumbnailWindow`
  - `GetAppUserModelId`
  - `SwitchTo`

### 2. 按系统版本切换 GUID

当前逻辑：

- `build <= 17134` 使用旧版 `IApplicationViewCollection` GUID
- `build > 17134` 使用新版 `IApplicationViewCollection` GUID

当前 Windows 10 19045 会走新版路径。

### 3. `RunningAppAction` 改为保存通用 COM 对象

`plugins/RunningApp/RunningAppAction.hpp` 里，原先直接保存某个固定版本的 `IApplicationView` 指针。

现在改成：

- 保存 `IUnknown`
- 额外保存 `isModernApplicationView`
- 执行时再根据版本调用正确的 `SwitchTo`

这样可以避免在 action 层绑定死某一个接口布局。

### 4. 插件侧把版本信息传递下去

`plugins/RunningApp/RunningAppPlugin.cpp` 中：

- `appendEdgeTabs(...)` 创建 `RunningAppAction` 时
- 会把 `viewInfo.view` 和 `viewInfo.isModernView` 一并保存

## 本次验证结果

### 1. 工程构建

已执行：

```cmd
cmd.exe /c .\scripts\run_build.cmd
```

结果：

- 构建成功
- `RunningAppPlugin.dll` 成功生成

### 2. 本机 COM 验证

为了避免只凭仓库代码推断，本次还在当前环境直接做了最小化验证。

验证点一：

- 使用 `CLSID_ImmersiveShell`
- 在 Windows 10 19045 上
- 使用新版 `IApplicationViewCollection` GUID
- `QueryService` 成功

验证结果：

```txt
QueryService success
```

验证点二：

- 对获取到的 `IApplicationViewCollection` 调用 `GetViews`

验证结果：

```txt
GetViews success, count=55
```

这个结果非常关键，说明：

- 新 GUID 路线在当前系统上是通的
- `ImmersiveShell -> IApplicationViewCollection -> GetViews` 至少已经工作

## 为什么仍然没有最终成功

虽然第一层 COM 问题已经解决，但“Edge 标签页索引最终没有成功”仍然可能卡在下面几层中的某一层：

### 1. `GetViews()` 返回的是系统 view，但不一定每个 Edge 标签页都会以可切换 view 暴露

需要进一步确认：

- 当前安装的 Edge 版本
- 当前 Edge 标签页是否启用了系统级 Alt+Tab 集成[config_folder_plugin.json](../cmake-build-release-visual-studio/plugins/config_folder_plugin.json)
- Edge 标签页在当前系统设置下是否真的作为独立 application view 出现

### 2. 过滤条件可能过严

当前代码会过滤：

- `GetShowInSwitchers == true`
- `GetThumbnailWindow(...)` 成功且句柄非空
- `processPath` 最终识别为 `msedge.exe`
- `title` 非空

任何一层不满足，都会导致看不到结果。

### 3. “view 是 Edge 的窗口”不等于“view 是 Edge 的标签页”

即使能拿到 Edge 的 application view：

- 也可能只有窗口级视图
- 不一定会展开到单标签页粒度

### 4. 执行切换未实测

本次确认了接口能拿到、工程能编过，但没有完成一轮“真实 Edge 标签页列表展示 + 选择 + 切换”的端到端功能验证。

所以当前状态仍然属于：

- 接口兼容性修正完成
- 功能闭环尚未完成

## 下次继续开发建议

建议按下面顺序继续，而不是直接继续改 UI 或搜索逻辑。

### 第一步：把遍历结果完整打日志

优先在 `TraverseImmersiveApplicationViews(...)` 或 `appendEdgeTabs(...)` 附近记录每个 view 的：

- `title`
- `processPath`
- `appUserModelId`
- `showInSwitchers`
- `thumbnailHwnd`
- 是否被过滤掉

目标不是马上修，而是先看清楚：

- 有没有 Edge 相关 view
- 它们是在什么环节被过滤掉

如果没有日志，后续只能盲猜。

### 第二步：确认系统和 Edge 的 Alt+Tab 行为

要人工确认两件事：

1. 当前系统 Alt+Tab 是否真的展示 Edge 标签页
2. 当前 Edge 设置是否允许标签页与 Windows 集成

如果系统本身就没有把标签页暴露到 Alt+Tab，那么这条 `IApplicationViewCollection` 路线很可能只能拿到 Edge 窗口，而不是标签页。

### 第三步：临时放宽过滤条件

为了排查，建议临时取消或放宽以下过滤：

- `showInSwitchers`
- `title.empty()`
- `IsEdgeProcessPath(processPath)`

先输出全部 view，再手动观察哪些 view 其实属于 Edge。

### 第四步：如果 `IApplicationView` 路线只能拿到窗口级视图，准备备用方案

如果验证后发现：

- 这条路线只能拿到 Edge 窗口
- 不能稳定拿到单 tab 粒度

则下次应考虑改为“窗口级 + UI Automation 辅助”的混合方案，例如：

- 先找到 Edge 窗口
- 再用 UI Automation 枚举 tab strip
- 用标签页标题做索引

这条路线复杂度更高，但如果系统层没有暴露单 tab view，就不能继续死磕 `IApplicationViewCollection`。

## 当前文件状态

本次与 Edge 标签页排查相关的代码主要在：

- `util/ImmersiveAppViewTraverser.hpp`
- `plugins/RunningApp/RunningAppAction.hpp`
- `plugins/RunningApp/RunningAppPlugin.cpp`

这些代码已经包含：

- 版本兼容分支
- 新旧接口适配
- Edge 标签页索引开关接入

下次继续时，优先从 `util/ImmersiveAppViewTraverser.hpp` 的遍历结果日志开始，不要重复从 GUID 兼容问题重新查起。

## 一句话总结

这次没有做成最终功能，但已经确认并修掉了第一层阻塞：

- 旧版 `IApplicationViewCollection` / `IApplicationView` 定义不适用于当前 Windows 10 19045

下次开发的重点不再是“为什么 `QueryService` 失败”，而是：

- 当前系统是否真的把 Edge 标签页暴露为可枚举 view
- 以及这些 view 在现有过滤逻辑中是如何被筛掉的
