# Plugin API ABI Notes

本文记录 CandyLauncher 插件接口变更时的注意事项。重点是 `IPlugin` 这类由主程序和插件 DLL 共同使用的 C++ 虚接口。

## 这次右键菜单问题的原因

文件夹插件新增了右键 API 后，普通右键和 `Shift + 右键` 都没有弹出菜单。最终修复的关键点是：

- `IPlugin` 增加了新的虚函数，主程序通过 `IPlugin*` 调用插件 DLL。
- C++ 虚函数依赖 vtable 顺序，向接口中间插入虚函数会改变后续虚函数槽位。
- 如果主程序和某个插件 DLL 没有完全同步编译，或者旧插件仍按旧 vtable 布局运行，主程序可能调用到错误的虚函数槽位。
- 表现可能不是崩溃，而是“事件看似没触发”“回调返回 false”“菜单不显示”等难排查现象。
- 所以新增的右键虚函数要放到 `IPlugin` 末尾，并通过 `GetPluginApiVersion()` 做能力判断。

本次修复还增加了右键路径日志，用于确认链路：

- `RightClick`: 主窗口是否收到 `NM_RCLICK`，命中的列表项 index 和 action/pluginId。
- `PluginManager`: 是否找到插件、插件 API version、是否派发到插件。
- `FolderPlugin`: 是否进入文件夹插件右键回调。
- `ShellMenu`: Windows Shell 右键菜单创建和弹出过程。
- `MyMenu`: 自定义 `Shift + 右键` 菜单创建和弹出过程。

## 以后修改插件接口的规则

### 1. 新虚函数只能追加到接口末尾

不要把新的虚函数插到 `IPlugin` 已有虚函数中间，也不要调整已有虚函数顺序。

推荐：

```cpp
class IPlugin {
public:
	virtual bool OldMethodA() = 0;
	virtual bool OldMethodB() = 0;

	// 新增接口放末尾
	virtual bool NewMethod() {
		return false;
	}
};
```

避免：

```cpp
class IPlugin {
public:
	virtual bool OldMethodA() = 0;

	// 不要插到中间，会改变后续 vtable 槽位
	virtual bool NewMethod() {
		return false;
	}

	virtual bool OldMethodB() = 0;
};
```

### 2. 新能力必须提升 Plugin API version

如果主程序要调用新增虚函数，插件的 `GetPluginApiVersion()` 必须返回新的版本号。

示例：

```cpp
PLUGIN_EXPORT int GetPluginApiVersion() {
	return 2;
}
```

主程序派发前必须判断：

```cpp
if (!info.loaded || !info.plugin || info.apiVersion < 2) {
	return false;
}

return info.plugin->NewMethod(...);
```

不要在未判断 version 的情况下直接调用新虚函数。

### 3. 主程序和插件 DLL 必须一起编译和部署

只改主程序或只改某个插件 DLL 都可能造成 ABI 不一致。涉及 `plugins/Plugin.hpp`、`IPluginHost`、`BaseAction` 这类跨 DLL 类型时，必须重新编译主程序和所有受影响插件。

推荐使用项目脚本：

```bat
cmd.exe /c .\scripts\run_build.cmd
```

如果只临时编译了单个目标，要确认最终运行目录里的 `CandyLauncher.exe` 和 `plugins/*.dll` 都是同一轮接口头文件编出来的。

### 4. 接口参数要保持 ABI 简单

跨 DLL 虚接口里尽量避免复杂 STL 类型作为长期 ABI。当前项目已经使用了 `std::shared_ptr<BaseAction>`、`std::wstring` 等 C++ 类型，因此更要保证主程序和插件使用同一编译器、同一运行时、同一头文件版本。

新增接口时优先考虑：

- 用已有项目类型，不随意引入新的模板-heavy 参数。
- 参数只传必要数据。
- 对新增能力提供默认空实现，避免所有插件都必须立即实现。
- 用 API version 判断能力，不用猜测插件是否实现了某个新增虚函数。

### 5. 新事件链路要先打最小日志

新增主程序到插件的事件派发时，先覆盖这几个点：

- 主窗口是否收到原始 Win32 消息。
- 命中的 action 是否有效。
- action 的 `pluginId` 是否能找到已加载插件。
- 插件 API version 是否满足。
- 插件回调是否进入。
- 回调内部关键系统 API 是否失败。

右键菜单这类问题尤其要记录 `TrackPopupMenu` / `TrackPopupMenuEx` 的返回值，以及 Shell API 的 `HRESULT`。

## 右键 API 当前约定

`IPlugin` 末尾提供：

```cpp
virtual bool OnItemRightClick(const std::shared_ptr<BaseAction>& action, HWND parentHwnd, POINT screenPt);
virtual bool OnItemShiftRightClick(const std::shared_ptr<BaseAction>& action, HWND parentHwnd, POINT screenPt);
```

约定：

- 返回 `true` 表示插件已处理。
- 返回 `false` 表示插件不处理该 action。
- 插件内部要先 `dynamic_pointer_cast` 到自己认识的 action 类型。
- 当前右键能力要求 `GetPluginApiVersion() >= 2`。

文件夹插件当前行为：

- 普通右键：调用 Windows Shell 菜单。
- `Shift + 右键`：调用 CandyLauncher 自定义菜单。

---

# 公共头文件和共享类型注意事项

• 风险级别分两类：

1. Plugin.hpp 这类跨 DLL 虚接口：最高风险
   IPlugin / IPluginHost / 任何通过虚函数跨 DLL 调用的接口，属于 ABI 契约。改虚函数顺序、在中间插虚函数、改参数类型、改返回类型，都可能让旧插件 DLL 调错 vtable 槽位。

结果可能是：

- 直接崩溃
- 调到错误函数
- 返回奇怪值
- 看起来像“插件没响应”
- 某些机器正常，某些机器异常

所以这类必须用 GetPluginApiVersion()、末尾追加虚函数、主程序按 version 判断能力。

2. ../../util/BaseTools.hpp 这类 header-only 公共工具：也有风险，但主要是“行为/布局/二进制依赖”风险
   如果插件只是把 BaseTools.hpp 编译进自己的 DLL，主程序运行时不会直接调用插件 DLL 里的 BaseTools 函数，也不会共享这些 inline 函数的二进制实现。那么主程序改了 BaseTools.hpp 后，旧插件 DLL 不会因为这个改动自动崩溃。

但有几个例外很重要。

如果 BaseTools.hpp 里定义的是普通 inline 工具函数，比如字符串转换、路径处理、剪贴板工具，旧插件 DLL 会继续使用它当时编译进去的旧代码。主程序改了实现后，旧插件不会自动变成新行为。这通常不会崩，但可能出现新旧行为不一致。

如果 BaseTools.hpp 里定义或引用了跨 DLL 共享的数据结构，例如 BaseAction、SettingItem、TraverseOptions，那就要小心。插件创建对象，主程序读取对象字段，这种结构体/类的内存布局就是 ABI 的一部分。改字段顺序、删字段、改字段类型、改虚函数、改继承关系，都可能让旧插件 DLL 和新主程序
对同一块内存的理解不一致。

结果可能是：

- 读错字段
- dynamic_pointer_cast 失败
- 虚析构/虚函数调用异常
- 字符串/vector/shared_ptr 生命周期异常
- 崩溃

最危险的公共类型
这些比普通工具函数更应该当作插件 ABI：

- plugins/Plugin.hpp
- plugins/BaseAction.hpp
- plugins/BaseLaunchAction.hpp
- model/SettingItem.hpp
- model/TraverseOptions.hpp
- model/FileInfo.hpp
- 任何插件返回给主程序、主程序保存/读取/调用的类型
- 任何主程序通过 IPluginHost 传给插件的引用、指针、容器

结论
BaseTools.hpp 本身如果只是工具函数，主程序改方法实现后，旧插件 DLL 通常不会因此崩溃，只是旧插件继续用旧逻辑。

但如果改的是插件和主程序共同传递的类/结构体/虚类，旧插件 DLL 就可能崩溃。即使文件不叫 Plugin.hpp，只要它参与跨 DLL 对象传递，就是 ABI。

建议规则

1. Plugin.hpp、BaseAction.hpp、model/*.hpp 里被插件和主程序共同使用的类型，都按 ABI 文件处理。
2. 给共享结构体新增字段时，只追加到末尾，不改已有字段顺序和类型。
3. 给共享类新增虚函数时，只追加到末尾，并提升 plugin API version。
4. 不要让插件和主程序跨 DLL 互相 delete 对方 new 出来的复杂对象，除非约定清楚谁创建谁销毁。
5. 修改公共头后，默认要求主程序和所有插件 DLL 全量重新编译。
6. 如果以后要支持旧插件 DLL 长期兼容，最好把 C++ STL/类 ABI 收缩成 C 风格接口或稳定的纯数据 ABI。
