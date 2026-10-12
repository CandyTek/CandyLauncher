# Plugin API ABI Notes

插件接口从 2.0 起是纯 C ABI。主程序与插件 DLL 之间只通过 `plugins/sdk/CandyPluginApi.h` 交互，
不再传递任何 C++ 类型（`std::wstring`、`std::vector`、`shared_ptr`、`nlohmann::json`、虚函数表）。
因此插件可以用任意编译器 / C 运行时构建，旧版 `IPlugin` / `IPluginHost` 已删除。

## 文件

| 文件 | 作用 |
|---|---|
| `plugins/sdk/CandyPluginApi.h` | 纯 C 契约：`CL_PluginApi`（插件入口表）、`CL_HostApi`（主程序服务表）、各结构体 |
| `plugins/sdk/CandyPluginSdk.hpp` | 仅头文件的 C++ 封装，编译进插件：`candy::Plugin`、`candy::Host`、`candy::ActionSet<T>`、`CANDY_PLUGIN_EXPORT` |
| `plugins/PluginActionProxy.hpp` | 主程序侧：`PluginActionProxy`（插件条目的副本）、`CL_PluginContext`（不透明句柄的实现） |
| `plugins/PluginHostApi.hpp` | 主程序侧：`CL_HostApi` 每个函数的实现 |
| `plugins/PluginManager.hpp` | 加载、校验、分发 |
| `plugins/PluginI18n.hpp` | 主程序侧：插件字符串表（2.3 本地化），见 [DOC_PLUGIN_I18N.md](DOC_PLUGIN_I18N.md) |

## 加载流程

1. `LoadLibraryW` 后查找唯一导出 `CandyPlugin_GetApi`，没有则不是插件（旧版 C++ 插件会被跳过并记录日志）。
2. 校验 `api_major == CL_API_VERSION_MAJOR`、表大小、`create` / `destroy` / `package_name`。
3. 元数据（名称、版本、包名、默认设置 JSON）是表里的静态字符串，探测插件目录时不需要创建实例。
4. `create(handle, hostApi)` 返回实例指针 `self`，之后所有回调都带 `self`；卸载时 `destroy(self)`。

## 核心模型：主程序持有条目，插件持有 user_data

- 插件用 `set_actions(handle, set_id, descs, count, flags)` 发布一个条目集合，主程序把字符串、图标复制成
  `PluginActionProxy`。代理不含任何指向插件代码的指针，插件卸载后列表里残留的条目也能安全绘制。
- 带 `CL_SET_GLOBAL_SEARCH` 的集合参与主搜索，其余集合只能通过拦截输入（`sink_add_matches` / `sink_add_items`）
  或 `show_action_set` 显示。
- 执行、热键、右键、拖拽回调只把 `user_data` 交还给插件。SDK 的 `ActionSet<T>` 把
  集合 id / 发布代数 / 下标编码进 `user_data`，`Find` 会拒绝旧代数的过期条目。
- 主程序在每次按键时只合并缓存的代理（不再每次按键回调插件拉取条目）。
- `match_text` 为空时主程序对 title 做小写 + 拼音处理；插件已自行处理（例如书签插件并行处理）时设置
  `CL_ACTION_MATCH_TEXT_VERBATIM`。
- `icon_bitmap` 由主程序复制，插件保留所有权；`icon_index < 0` 且有 `icon_path` 时主程序延迟解析系统图标，
  同一次发布中相同路径共享一个解析结果。
- 代理的 `ownerSerial` 对应每次加载的唯一序号，插件卸载重载后旧条目不会派发给新实例。

## 版本记录

| 版本 | 追加内容 |
|---|---|
| 2.0 | 初始纯 C ABI |
| 2.1 | `CL_HostApi::set_clipboard_text`（SDK `Host::SetClipboardText`，旧主程序上返回 false）；`CL_PluginApi::get_item_copy_text`（SDK `Plugin::GetItemCopyText`，输入框无选中文本时 Ctrl+C 复制插件提供的文本） |
| 2.2 | `CL_ActionDesc::stable_key / icon_key`；`CL_HostApi::set_trigger_keywords / sink_add_descs / post_results / notify_actions_changed / get_current_query_id / sink_take_over / request_refresh`；`CL_PluginApi::flags / on_query / on_query_cancel / on_setting_changed`；详见下文 "2.2 机制" |
| 2.3 | 本地化：`CL_HostApi::get_ui_language / get_string`；`CL_PluginApi::on_language_changed`；`CL_CatalogEntry::description`；SDK `Host::GetUiLanguage / Tr`、`Plugin::Tr / OnLanguageChanged`、`candy::Format`。字符串表在 `plugins/lang/<DLL 名>/<语言>.json`，详见 [DOC_PLUGIN_I18N.md](DOC_PLUGIN_I18N.md) |

## 2.2 机制

所有 2.2 追加项在旧主程序上静默退化：SDK 的 `Host` 封装用 `Host::Has(&CL_HostApi::xxx)` 判断主程序表大小，
不存在时返回 false / 不做任何事；主程序调用插件新回调前用 `CL_PLUGIN_HAS`。

### 触发关键字（按键路由）

`set_trigger_keywords(h, keywords, count, flags)`（SDK `Host::SetTriggerKeywords`）。设置后主程序只在输入以某个关键字
开头（不区分大小写）时才调用该插件的 `on_user_input` / `on_intercept_input` / `on_query`。
`CL_TRIGGER_ALSO_GLOBAL` 表示没有关键字匹配时也调用（计算器自动识别算式这类场景）。
从未调用过的插件行为不变：每次输入都会被调用。书签插件在 `OnUserSettingsLoaded` 里把激活命令注册为关键字。

### 临时结果（不进集合）

`sink_add_descs(sink, h, descs, count)`：在 `on_intercept_input` 里直接把只为本次输入生成的条目加入结果，
主程序不做拼音处理也不保存到任何集合。插件要自己保管 `user_data` 指向的对象到下一次输入为止，
SDK 的 `candy::TransientSet<T>` 负责这一点（`ResultSink::AddTransient(set, items)`，`set.Find(userData)`）。
`TransientSet` 的集合 id 与 `ActionSet` 共用 `user_data` 高 16 位，不能重复。

### 异步查询

每次按键是一个查询，主程序先 `on_query_cancel(上一个 id)` 再 `on_query(self, query_id, input, arg)`
（同样受触发关键字约束，在 `on_intercept_input` 之后调用）。插件在任意线程完成后调用
`post_results(h, query_id, descs, count, is_final)`：主程序在调用线程上复制条目，投递到 UI 线程追加到当前列表；
`query_id` 已过期时返回 0 并丢弃。可以分多批投递；最后一个参数是 `CL_RESULTS_FINAL | CL_RESULTS_REPLACE` 组合，
`CL_RESULTS_REPLACE` 让这批结果替换当前列表（例如先显示 "搜索中" 占位项，结果到了再整体替换）而不是追加。
SDK：`TransientSet::PostResults(host, queryId, items, isFinal, restart, replaceList)`。
`get_current_query_id` 可用于工作线程中途检查是否该放弃。
纯异步插件在 `on_intercept_input` 里调用 `sink_take_over`（SDK `ResultSink::TakeOver()`）即可在没有条目的情况下接管列表，
主程序不再跑常规搜索，之后的 `post_results` 填充它。

### 插件主动请求刷新

`request_refresh(h)`（SDK `Host::RequestRefresh()`，任意线程）：主程序再次调用该插件的 `refresh_actions`
（带 `CL_PLUGIN_REFRESH_ANY_THREAD` 时在它的工作线程上）并刷新可见列表。设置保存后主程序本来就会刷新所有插件，
这个接口用于插件自己发现数据变化（文件监视、服务状态变化等）的场合。
`ActionSet::Entries()` / `UserDataAt(i)` 给出当前发布的 `user_data`，配合 `ResultSink::AddItems` 可以只显示集合的一个子集。

### 工作线程刷新

`CL_PluginApi::flags` 含 `CL_PLUGIN_REFRESH_ANY_THREAD`（SDK `CANDY_PLUGIN_EXPORT_EX(..., Flags)`）时，
`refresh_actions` 在该插件自己的工作线程上运行（主程序已 `CoInitializeEx(MTA)`），各插件并行，启动不再串行等待。
刷新函数里只能用标注 "Any thread" 的主程序服务（`set_actions`、设置读取、`traverse_files`、日志等）。
刷新进行中再次请求刷新会合并为一次后续运行；卸载插件前主程序会等待工作线程结束。
在工作线程上发布完集合后主程序自动刷新可见列表；其它后台发布请调用 `notify_actions_changed`。

### 设置项变更通知

`on_setting_changed(self, key)`：主程序对比上一次的 "<package_name>.*" 值快照，只对变化的键调用，然后才调用
`on_user_settings_loaded`。插件通过 `set_setting_json` 自己改的值不会回调。

### 稳定标识与共享图标

- `stable_key`：刷新后不变的条目标识（路径、URL、id）。主程序保存在代理上，供后续的使用频率排序 / 固定条目等功能使用。
- `icon_key`：相同 key 的条目共享一份主程序侧位图，只有第一个条目需要给 `icon_bitmap`。
  不用 key 时每个条目仍各复制一份（GDI 句柄上限约 1 万，大集合务必用 key 或 `icon_path`）。

### 主程序侧的性能改动

- 主搜索列表只在插件发布 / 优先级 / 加载状态变化后重建（`PluginManager::ActionsVersion()`），不再每次按键合并。
- 集合是不可变快照（`shared_ptr<const PluginActionSet>`），`sink_add_matches` / `sink_add_items` 不再复制整个集合，
  `sink_add_items` 用发布时建好的 `user_data` 索引。
- 小写 + 拼音处理结果按原文缓存（`PluginHostApi.hpp` 的 `MatchTextCache`，拼音方案切换时清空），
  重复发布未变化的标题几乎零成本。
- 插件按优先级（高在前）再按加载顺序分发；插件 id 不再在卸载后重编号。
- 每次按键回调超过 50 ms 的插件会被记录（每插件最多 5 秒一次）。
- Everything SDK 的查询有进程级互斥，多个插件线程同时 `traverse_files` 是安全的。
- 插件在工作线程读取设置时与 `g_settings_map` 的重建互斥（`g_settingsMapMutex`）。

### 清单文件

每个插件必须在 DLL 旁边提供 `<文件名>.plugin.json`（源码里放在插件目录下，`PluginCommon.cmake` 自动复制），
主程序不加载 DLL 就能把插件列入目录（禁用的插件完全不执行代码）、生成设置页并显示本地化的名称：

```json
{
	"package_name": "com.example.plugin",
	"name": "ExamplePlugin",
	"description": "Simple example plugin",
	"version": "1.0.0",
	"default_setting_json": { "...": "也可以是字符串" },
	"keywords": ["ex "],
	"min_host_api_minor": 3,
	"strings": { "zh-CN": { "plugin": { "name": "示例插件", "description": "简单示例插件" } }, "en-US": { "...": "" } }
}
```

`package_name` 必填；DLL 加载后以 DLL 表为准，包名 / 版本不一致时记录警告。内置插件的默认设置项都在清单里
（`CANDY_PLUGIN_EXPORT` 传 `nullptr`）。没有清单时记录警告并退回探测 DLL。
`strings` 的格式和用法见 [DOC_PLUGIN_I18N.md](DOC_PLUGIN_I18N.md)。

### 热安装 / 热更新

主程序监视 `plugins` 目录（`ReadDirectoryChangesW`，1 秒去抖）：

- 新 DLL：加入目录并（未被禁用时）加载、刷新；设置列表随之重建。
- DLL 被删除 / 重命名走开：卸载并移出目录。
- DLL 时间戳变化：卸载后重新加载。

已加载的 DLL 被锁定不能覆盖，但可以重命名；更新程序可以 "重命名旧文件 → 写入新文件"。
或者打开 `pref_plugin_shadow_copy`：主程序从 `plugins\.shadow\<名>.<时间戳>.dll` 加载副本，原文件随时可覆盖，
卸载时删除副本。副本模式下插件用 `GetModuleFileName` 找自己旁边的资源会指向 `.shadow`，
需要相对资源的插件应从清单或设置项取路径。

## 修改接口的规则

1. **只在结构体末尾追加字段**，不要改动、删除或重排已有字段（包括 `CL_HostApi`、`CL_PluginApi`、所有 `CL_*` 结构体）。
2. 追加字段后提升 `CL_API_VERSION_MINOR`。读取新字段前用 `CL_HAS_FIELD(ptr, Type, field)` 判断对方的结构体是否足够大。
   - 主程序调用插件新增的回调：`CL_PLUGIN_HAS(info, field)`。
   - 插件调用主程序新增的服务：`CL_HAS_FIELD(host.Api(), CL_HostApi, field)`。
   - 数组参数（如 `CL_ActionDesc*`）以首元素的 `size` 作为步长读取。
3. 只有无法兼容时才提升 `CL_API_VERSION_MAJOR`，主程序会拒绝主版本不同的插件。
4. 跨边界只用：定长整数、`int32_t` 布尔、`const wchar_t*`（UTF-16，NUL 结尾，只在调用期间有效）、Win32 句柄、
   POD 结构体、`函数指针 + void* ctx` 回调。返回字符串一律用 `CL_StrSink`，内存永远不跨边界释放。
5. 异常不能穿过边界：SDK 的 `PluginBridge` 包住每个插件入口，`PluginHostApi.hpp` 的 `Guard` 包住每个主程序入口。
6. 除注释标明 "Any thread" 的函数外，主程序服务只能在 UI 线程调用。`set_actions` 系列、`post_results`、
   `notify_actions_changed`、设置读取可以在工作线程调用。
7. 插件通过 `register_app_command` 注册的回调在卸载时由主程序自动移除。

## 旧插件移植清单（IPlugin → candy::Plugin）

| 旧 | 新 |
|---|---|
| `class X : public IPlugin` + `CreatePlugin/DestroyPlugin/GetPluginApiVersion` | `class X : public candy::Plugin` + `CANDY_PLUGIN_EXPORT(X, name, desc, version, pkg, defaultJson)` |
| `GetPluginName()` 等元数据、`DefaultSettingJson()` | `CANDY_PLUGIN_EXPORT` 参数（静态字符串） |
| `Initialize(IPluginHost*)` / `OnPluginIdChange` | `Initialize()`，用 `GetHost()` 访问主程序；不再需要 pluginId |
| `class XAction : public BaseAction` | `class XAction : public candy::ActionBase`（字段 `title/subtitle/matchText/iconPath/iconIndex/iconBitmap`） |
| `GetTextMatchActions()` 返回列表 | `RefreshActions()` 里 `ActionSet<XAction>::Publish(GetHost(), items)` |
| `InterceptInputShowResultsDirectly` + `GetSuccessfullyMatchingTextActions` | `OnInterceptInput(input, sink)` + `sink.AddMatches(set.Id(), query)` |
| `ShowResultsDerectly(list)` | 发布一个不带 `CL_SET_GLOBAL_SEARCH` 的集合，再 `GetHost().ShowActionSet(id)` |
| `dynamic_pointer_cast<XAction>(action)` | `actionSet.Find(userData)` |
| `GetSettingsMap().at(key).stringValue / boolValue / intValue / stringArr` | `GetString / GetBool / GetInt / GetStringArray(L"key")` |
| `SetPluginSettingValue(id, key, json)` | `SetSettingJson(L"key", L"<json>")` |
| `GetTheProcessedMatchingText` | `ProcessMatchText`，或留空 `matchText` 让主程序处理 |
| `GetAppLaunchActionCallbacks().at(k)` / `RegisterAppLaunchActionCallback` | `InvokeAppCommand(L"k")` / `RegisterAppCommand(L"k", fn, ctx)` |
| `TraverseFilesForEverythingSDK` / `SearchPossibleTargets` | `TraverseFiles(candy::TraverseOptions, visitor)` / `SearchFiles(query, max)` |
| `BeginOleDragDropFiles` / `BeginOleDragDropData` | `BeginDrag(candy::DragData)` |
| `PluginTaskDone` / `ChangeEditTextText` / `GetEditTextText` / `GetCurrectArgText` / `MyShowWindow` | `TaskDone` / `SetEditText` / `GetEditText` / `GetActionArg` / `ShowMainWindow` |
| `GetPluginCatalogEntries` / `SetPluginEnabled`（插件浏览器） | `GetPluginCatalog` / `SetPluginEnabled` |
| `OnSettingItemExecute(const SettingItem*, HWND)` | `OnSettingItemExecute(const std::wstring& key, HWND)` |

插件不能再包含 `common/GlobalState.hpp`、`plugins/BaseLaunchAction.hpp` 等主程序头文件中的全局状态；
`util/` 下的纯工具头文件仍可使用（它们编译进插件自身）。

`plugins/PluginTest` 通过 Mock `CL_HostApi` 直接驱动插件的 C 入口，可用于验证移植后的插件。

## 兼容性测试

`PluginTest` 也可以加载任意插件 DLL：`PluginTest.exe <plugin.dll> [--host-minor=0]`。
`--host-minor=0` / `--host-minor=1` 让 Mock 主程序只提供 2.0 / 2.1 大小的 `CL_HostApi`，用来模拟旧主程序。
书签插件会用临时书签文件驱动全部入口（设置、刷新、拦截输入、热键、拖拽、2.1 的复制回调）。
ctest 里的 `PluginTestBookmarkDll` / `PluginTestBookmarkDllHost20` / `PluginTestBookmarkDllHost21` 就是这些模式；
`PluginTestHost21` 用示例插件验证 2.2 的 SDK 封装在 2.1 主程序上静默退化。

`scripts/run_build_bookmark_mingw.cmd [输出路径]` 用 MinGW GCC（libstdc++ + msvcrt）构建书签插件，
默认覆盖 `cmake-build-debug-ninja-vs/plugins/BookmarkPlugin.dll`，用来验证 ABI 不依赖 MSVC。
MinGW 默认取 CLion 自带的 `%LOCALAPPDATA%\Programs\CLion\bin\mingw\bin`，可用 `MINGW_BIN` 指定。
插件位数必须与主程序一致：CLion 自带的 MinGW 只有 x64，而默认构建的主程序是 x86。

## C / Rust 插件示例

两个示例都不在根 `CMakeLists.txt` 中，默认不会构建，需要时单独构建。
DLL 放到 `CandyLauncher.exe` 旁边的 `plugins` 目录即可；位数必须与主程序一致。

| 目录 | 语言 | 构建 |
|---|---|---|
| `plugins/ExampleC` | C99，只依赖 `CandyPluginApi.h` | `cmake -S plugins/ExampleC -B plugins/ExampleC/build -G Ninja -DCMAKE_BUILD_TYPE=Release` 后 `cmake --build plugins/ExampleC/build`（MSVC / MinGW / clang 均可） |
| `plugins/ExampleRust` | Rust，`src/api.rs` 是手写的完整绑定 | `cargo build --release`；x86 主程序用 `--target i686-pc-windows-msvc` |

示例演示了：发布全局搜索条目、读取设置项（激活命令）、拦截输入、执行、2.1 的复制回调，
以及调用 2.1 主程序服务前用 `CL_HAS_FIELD`（Rust 为 `cl_has_field!`）判断主程序表大小。
非 C++ 插件没有 SDK 的 `ActionSet`，需要自己管理 `user_data`；Rust 插件不能让 panic 穿过边界
（示例用 `catch_unwind` 并设置 `panic = "abort"`）。

