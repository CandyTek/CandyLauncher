# 插件清单与本地化方案（Plugin API 2.3）

主程序自身的国际化已经完成：固定文案在 `lang/<语言>.dll` 资源 DLL 里（`common/I18n.hpp`），
`settings.json` 里的设置项标题用 `title_zh` / `title_en` 后缀键（`GetLocalizedJsonString`）。
本文说明插件的清单文件、插件如何接入同一套语言切换，以及迁移现有插件的约定。

## 设计原则

1. **每个插件必须提供一份清单 JSON**（`<DLL 名>.plugin.json`，放在 DLL 旁边）。元数据、默认设置项和
   各语言字符串都在这一个文件里，主程序不加载 DLL 就能把插件列入目录、生成设置页、显示本地化的名称。
2. **插件代码里只写 key 和默认文本**，翻译是清单里的数据。改翻译、加语言都不需要重新编译插件。
3. **主程序负责读表、回退和切换**。插件不关心语言什么时候变、文件在哪（影子副本模式下
   `GetModuleFileName` 指向 `.shadow`，插件自己找文件并不可靠）。
4. **跨 ABI 只增加两个 C 函数**（`get_ui_language` / `get_string`）和一个回调（`on_language_changed`），
   C / Rust 插件同样能用；C++ SDK 把它包成 `Tr(key, fallback)`。
5. 不做延迟加载：清单只用于目录、设置页和本地化，插件仍在启动时全部加载。

插件有三类文本，分别走三条路：

| 文本 | 来源 | 机制 |
|---|---|---|
| 目录元数据：插件名、描述（设置页插件开关标题、插件浏览器） | 清单 `name` / `description` | 清单 `strings` 里的保留键 `plugin.name` / `plugin.description`，`PluginCatalogInfo::DisplayName()` |
| 设置项标题、列表项 | 清单 `default_setting_json` | 沿用主程序规则：`title_zh` / `title_en` / `entries_zh` / `entries_en`（`ParseConfig`） |
| 运行时文本：条目标题、副标题、消息、菜单、窗口 | 插件代码 | `Tr(key, 默认文本)` → `CL_HostApi::get_string` → 清单 `strings` |

## 清单文件

源码里放在插件目录下（`plugins/Service/ServicePlugin.plugin.json`，文件名 = CMake 的 `project()` 名 =
DLL 名），`plugins/PluginCommon.cmake` 在构建后自动复制到 DLL 旁边；缺少清单时 CMake 配置阶段给出警告，
主程序运行时也会记录警告并退回加载 DLL 探测元数据（此时没有本地化名称）。

```json
{
	"package_name": "com.candytek.serviceplugin",
	"name": "Windows Services",
	"description": "Index and manage Windows services",
	"version": "1.1.0",
	"keywords": ["service "],
	"min_host_api_minor": 3,
	"default_setting_json": { "version": 1, "prefList": [ ... ] },
	"strings": {
		"zh-CN": {
			"plugin": { "name": "Windows 服务", "description": "索引和管理 Windows 服务" },
			"status": { "running": "运行中", "stopped": "已停止" },
			"msg": { "stop_ok": "成功停止服务: {0}" }
		},
		"en-US": {
			"plugin": { "name": "Windows Services", "description": "Index and manage Windows services" },
			"status": { "running": "Running", "stopped": "Stopped" },
			"msg": { "stop_ok": "Service stopped: {0}" }
		}
	}
}
```

| 字段 | 说明 |
|---|---|
| `package_name` | 必填，和 DLL 表里的一致（不一致时以 DLL 为准并记录警告） |
| `name` / `description` | 没有对应语言翻译时的默认文本，建议英文 |
| `version` | 和 DLL 表里的版本不一致时记录警告 |
| `keywords` | 信息性，真正的触发关键字仍由 `set_trigger_keywords` 决定 |
| `min_host_api_minor` | 主程序 API 小版本低于它时跳过该插件 |
| `default_setting_json` | 设置页的默认设置项，可以是对象或字符串。放在清单里后 `CANDY_PLUGIN_EXPORT` 传 `nullptr`；两处都有时以清单为准 |
| `strings` | `{ "<语言代码>": { ... } }`，见下文 |

### 字符串表格式

- 每个语言一个对象，嵌套对象用 `.` 拍平：`"action": { "hello": { "title": "Hi" } }` 定义了
  `action.hello.title`；也可以直接写 `"action.hello.title": "Hi"`，两种写法等价。非字符串的值忽略。
- 保留键：`plugin.name`、`plugin.description`。
- 语言代码不区分大小写，`_` 当作 `-`。
- 带参数的文本用 `{0}` `{1}` 占位，翻译可以调整顺序；插件用 `candy::Format(Tr(...), {a, b})` 填充。
- 可选：`plugins/lang/<DLL 名>/<语言>.json`（源码里 `plugins/<插件>/lang/`，CMake 自动复制）里的表
  优先级高于清单，适合翻译者单独维护的大表；内置插件目前全部只用清单。

### 回退链

`get_string` 按下面的顺序找第一个定义了该 key 的表：

1. 当前界面语言（`zh-CN`）
2. 同主语言的其它表（`zh-TW` → `zh-CN`）
3. `en-US`
4. 任意 `en-*`
5. 第一个表（按语言代码排序）
6. 都没有：返回 0，SDK 的 `Tr` 返回代码里的默认文本

所以翻译可以不完整：缺的 key 会落到英文，再缺就落到代码里的文本。

## ABI（2.3 追加，`plugins/sdk/CandyPluginApi.h`）

| 项 | 说明 |
|---|---|
| `CL_HostApi::get_ui_language(h, sink, ctx)` | 当前界面语言代码，如 `L"zh-CN"`。任意线程 |
| `CL_HostApi::get_string(h, key, sink, ctx)` | 按回退链查 key，找到返回 1 并通过 sink 给出文本；没有返回 0。任意线程 |
| `CL_PluginApi::on_language_changed(self, language)` | 用户切换语言后调用；之后主程序会 `refresh_actions` |
| `CL_CatalogEntry::description` | 插件浏览器拿到的描述，和 `name` 一样已本地化 |

C++ SDK（`CandyPluginSdk.hpp`）：

| 封装 | 作用 |
|---|---|
| `Host::GetUiLanguage()` | 旧主程序上返回空串 |
| `Host::Tr(key, fallback)` / `Plugin::Tr(key, fallback)` | 旧主程序或没有翻译时返回 `fallback` |
| `Plugin::OnLanguageChanged(language)` | 对应 `on_language_changed` |
| `candy::Format(pattern, {args})` | `{0}` `{1}` 占位符替换 |

`Tr` 可以在工作线程调用（`CL_PLUGIN_REFRESH_ANY_THREAD` 的 `RefreshActions` 里也能用）。

## 主程序实现

| 文件 | 内容 |
|---|---|
| `plugins/PluginI18n.hpp` | `PluginStringTable`（一个插件的所有语言）、`Load`（读清单 `strings` + 可选 lang 目录）、`Resolve`（按回退链拍平成当前语言的 key → 文本）、`PluginI18n::Language()`（`g_uiLanguageCode` 的线程安全镜像） |
| `PluginCatalogInfo::strings` | 目录项持有全部语言的表，`DisplayName()` / `DisplayDescription()` 用保留键 |
| `CL_PluginContext::strings` | 当前语言的拍平表，`std::atomic_load/store` 的 `shared_ptr`，`get_string` 无锁读取 |
| `PluginManager::ReadPluginManifest` | 读清单；缺清单时记录警告并退回 `ProbePluginCatalogInfo` |
| `PluginManager::SyncLanguageFromSettings` | `NotifyUserSettingsLoadDone` 开头检查 `g_uiLanguageCode` 是否变化：重读所有清单的字符串（翻译者改完文件切一次语言即可看到）、重建每个已加载插件的拍平表、调用 `on_language_changed` |

语言切换的完整流程（用户在设置里改 `pref_ui_language` 并保存）：

```
保存设置 → LoadSettingList → InitializeI18n（主程序资源 DLL 切换）
        → NotifyUserSettingsLoadDone → SyncLanguageFromSettings → on_language_changed
        → RefreshAllActions（插件在 RefreshActions 里重新 Tr，集合标题换语言）
        → 设置页重建（插件名走 DisplayName()，设置项标题走 title_zh / title_en）
```

启动时插件先于设置加载（`LoadAllPlugins` 在 `LoadSettingList` 之前），第一次 `SyncLanguageFromSettings`
只静默把表换成正确的语言，不调用 `on_language_changed`（此时还没有发布任何条目）。

主程序持有的是发布时复制的条目副本，所以标题必须重新发布才会变；`RefreshAllActions` 已经做了这一步，
插件通常不需要实现 `OnLanguageChanged`。需要实现的情况：插件自己缓存了翻译后的文本（右键菜单项、
打开着的窗口、`SetTriggerKeywords` 里带本地化的关键字等）。`TransientSet` 的结果每次输入都重新生成，不用管。

## 插件迁移约定

以 `plugins/Example`（全部机制）和 `plugins/Service`（真实插件）为样板。

1. **清单是单一来源**：名称、描述、版本、默认设置项、各语言字符串都写在清单里；
   `CANDY_PLUGIN_EXPORT` 的名称 / 描述只是没有翻译时的默认文本（英文），`DefaultSettingJson` 传 `nullptr`。
2. **代码里的默认文本用英文**，清单 `strings.zh-CN` 提供中文，`strings.en-US` 与默认文本一致
   （方便翻译者对照 key）。
3. **key 命名**：`plugin.*` 保留；其余建议 `action.<id>.title` / `action.<id>.subtitle`、`msg.*`、
   `menu.*`、`status.*`、`window.*`。key 只用 ASCII。
4. **带参数的文本**用占位符而不是字符串拼接：`Format(Tr(L"msg.stop_ok", L"Service stopped: {0}"), {name})`。
5. **`default_setting_json` 的标题**加 `title_en`（`title` 保持中文作为默认，和主程序 `settings.json` 一致）；
   `list` 类型的 `entries` 加 `entries_en`。
6. **输入语法不是界面文本**：`status:` / `状态:` 这类激活关键字两种都保留，不走 `Tr`。
7. 在工具头文件（没有 `candy::Plugin` 的自由函数）里需要翻译时，像 `ServicePluginData.hpp` 的
   `ServiceTr` 一样通过全局 `m_host` 调 `Host::Tr`，`m_host` 为空时返回默认文本。
8. 有自己 Win32 窗口的插件（Folder 的索引管理器 / 自动化编辑器 / 快捷方式修复）：窗口文本同样走
   `Tr`，并在 `OnLanguageChanged` 里关闭或重建已打开的窗口。

### 迁移进度

**清单、插件名 / 描述、默认设置项**：全部 25 个内置插件都已提供清单（`strings.zh-CN` / `strings.en-US`
里有 `plugin.name` / `plugin.description`，默认设置项已从 C++ 移入清单）。下表是其余运行时文本的进度：

| 插件 | 中文字面量数 | 状态 |
|---|---|---|
| Example | 21 | 已迁移（样板） |
| Service | 31 | 已迁移 |
| PluginBrowser | 8 | 已迁移（副标题改用 `CatalogEntry::description`） |
| Folder | 183（10 个文件，含 3 个窗口） | 待迁移，工作量最大 |
| CalcExprtk | 99（函数说明列表） | 待迁移 |
| ValueGenerator | 60 | 待迁移 |
| ScreenTools | 25 | 待迁移 |
| FeatureLaunch | 21 | 待迁移 |
| CherryTree | 18 | 待迁移 |
| SmallToolBox | 17 | 待迁移 |
| NodeJsPlugin / UnitConverter | 12 / 12 | 待迁移 |
| Registry / RunningApp / TextFile | 11 / 11 / 11 | 待迁移 |
| WindowsSearch / WebSearch | 10 / 9 | 待迁移 |
| Bookmark | 8 | 待迁移 |
| EverNote / BrowserHistory / JetbrainsWorkspaces | 7 / 6 / 6 | 待迁移 |
| OneNote / VSCodeWorkspaces / VisualStudio | 5 / 5 / 5 | 待迁移 |

## C / Rust 插件

```c
static const wchar_t* Tr(const CL_HostApi* host, CL_PluginHandle h, const wchar_t* key, const wchar_t* fallback,
						wchar_t* buf, size_t cap) {
	if (CL_HAS_FIELD(host, CL_HostApi, get_string) && host->get_string(h, key, CopyToBuf, &ctx)) return buf;
	return fallback;
}
```

`plugins/ExampleC` / `plugins/ExampleRust` 还没有清单，也没有演示 2.3；Rust 的 `src/api.rs` 需要在
`CL_HostApi` 末尾追加 `get_ui_language` / `get_string` 两个字段（追加不影响旧字段的偏移）。

## 测试

- `plugins/PluginTest`：Mock 主程序实现了 `get_ui_language` / `get_string`（`g_mock.strings`），示例插件检查
  "有表时返回翻译、缺 key 时回退默认文本、2.2 主程序上 `Tr` 始终返回默认文本"；
  `PluginTestHost22` 用 `--host-minor=2` 模拟 2.2 主程序。测试程序从 DLL 旁边的清单
  （内置示例插件用 `PLUGIN_TEST_EXAMPLE_MANIFEST`）读取默认设置项。
- 手工：设置 → 常规 → Language 切换后保存，主列表里 Example / Service 的条目标题、设置页里的插件名和设置项标题应同时切换。

## 已知限制 / 待办

- 插件目录监视只关注 `.dll` 和 `.plugin.json` 的新增 / 删除 / 修改时间；直接改清单里的翻译后，
  切换一次语言或重载插件即可生效。
- 设置项标题的后缀只有 `zh` / `en` 两种（`GetCurrentUiLanguageSuffix`）。以后加语言时要么扩展后缀，
  要么让 `ParseConfig` 支持 `"title": "@key"` 从字符串表取。
- 主程序 `settings.json` 里 `pref_ui_language` 的选项列表还是固定的，新语言需要同时加资源 DLL 和选项。
- 其余插件的运行时文本迁移见上表。
