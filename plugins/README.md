# CandyLauncher 插件开发指南

本文档介绍如何为 CandyLauncher 创建新插件，包括完整的开发步骤、关键概念和重要 API。

---

## 📋 目录

1. [插件架构概述](#插件架构概述)
2. [快速开始](#快速开始)
3. [核心概念](#核心概念)
4. [创建插件步骤](#创建插件步骤)
5. [重要 API 参考](#重要-api-参考)
6. [最佳实践](#最佳实践)
7. [性能优化建议](#性能优化建议)
8. [示例插件分析](#示例插件分析)

---

## 插件架构概述

CandyLauncher 使用动态链接库（DLL）方式加载插件。每个插件是一个独立的 DLL，实现 `IPlugin` 接口。

### 核心组件

```
plugins/
├── Plugin.hpp              # 插件接口定义（IPlugin, IPluginHost）
├── BaseAction.hpp          # Action基类（搜索结果条目）
├── PluginCommon.cmake      # 插件通用构建配置
├── YourPlugin/             # 你的插件目录
│   ├── YourPlugin.cpp      # 插件主入口
│   ├── YourAction.hpp      # 自定义Action类
│   ├── YourPluginData.hpp  # 插件全局变量
│   ├── YourUtil.hpp        # 工具函数（可选）
│   └── CMakeLists.txt      # 构建配置
└── ...
```

### 插件生命周期

```
加载阶段:
1. 主程序扫描 plugins/ 目录
2. 加载 DLL 并调用 CreatePlugin()
3. 调用 Initialize(IPluginHost*)
4. 调用 OnPluginIdChange(pluginId)

配置阶段:
5. 主程序加载用户设置
6. 调用 OnUserSettingsLoadDone()
7. 调用 RefreshAllActions()

运行阶段:
8. 用户输入 -> OnUserInput()
9. 搜索匹配 -> GetTextMatchActions() 或 InterceptInputShowResultsDirectly()
10. 执行操作 -> OnActionExecute()

卸载阶段:
11. 调用 Shutdown()
12. 调用 DestroyPlugin()
```

---

## 快速开始

### 参考示例插件

**推荐参考**: `plugins/BrowserHistory/` - 这是一个功能完整、结构清晰的插件示例。

**文件结构**:

```
plugins/BrowserHistory/
├── BrowserHistoryPlugin.cpp       # 主入口（继承 IPlugin）
├── BrowserHistoryAction.hpp       # Action类定义（继承 BaseAction）
├── BrowserHistoryPluginData.hpp   # 全局变量定义（m_host, m_pluginId等）
├── BrowserHistoryUtil.hpp         # 工具函数（数据加载逻辑）
└── CMakeLists.txt                 # 构建配置
```

**学习要点**:

1. 查看 `BrowserHistoryPlugin.cpp` 学习插件主类结构和生命周期
2. 查看 `BrowserHistoryAction.hpp` 学习如何定义自定义 Action
3. 查看 `BrowserHistoryPluginData.hpp` 学习如何管理全局变量
4. 查看 `BrowserHistoryUtil.hpp` 学习如何组织数据加载逻辑
5. 查看 `CMakeLists.txt` 学习构建配置

---

## 核心概念

### 1. IPlugin 接口

所有插件必须实现 `IPlugin` 接口。

#### 必须实现的方法

| 方法                       | 用途   | 注意事项                       |
|--------------------------|------|----------------------------|
| `GetPluginName()`        | 插件名称 | 显示给用户                      |
| `GetPluginPackageName()` | 包名   | 唯一标识，建议使用反向域名              |
| `GetPluginVersion()`     | 版本号  | 语义化版本号                     |
| `GetPluginDescription()` | 描述   | 简短说明插件功能                   |
| `Initialize(host)`       | 初始化  | 保存 host 指针，不要在这里加载 Actions |
| `OnPluginIdChange(id)`   | ID分配 | 保存插件ID，用于标识 Actions        |
| `Shutdown()`             | 清理资源 | 释放内存，关闭句柄等                 |
| `OnActionExecute()`      | 执行操作 | 用户按回车时调用                   |

#### 可选方法（重要）

| 方法                                    | 用途      | 性能建议                |
|---------------------------------------|---------|---------------------|
| `DefaultSettingJson()`                | 默认配置    | 返回 JSON 格式的配置定义     |
| `OnUserSettingsLoadDone()`            | 配置加载完成  | 读取用户设置              |
| `RefreshAllActions()`                 | 刷新动作列表  | 启动时调用，可以耗时操作        |
| `GetTextMatchActions()`               | 获取可搜索项  | **不要在这里懒加载**，直接返回列表 |
| `InterceptInputShowResultsDirectly()` | 拦截输入    | **必须快速响应**（<50ms）   |
| `OnUserInput()`                       | 用户输入监听  | 可用于预加载              |
| `OnMainWindowShow()`                  | 窗口显示/隐藏 | 可用于状态更新             |

### 2. BaseAction 类

`BaseAction` 是搜索结果条目的基类，每个插件需要继承它创建自己的 Action 类。

**参考文件**: `plugins/BrowserHistory/BrowserHistoryAction.hpp`

**关键要点**:

- 必须重写: `getTitle()`, `getSubTitle()`, `getIconFilePath()` 等方法
- 必须设置: `pluginId` 和 `matchText` 字段
- 可以添加自定义字段存储插件特定数据（如 URL、路径等）
- 使用 `m_host->GetTheProcessedMatchingText()` 处理 matchText（支持拼音搜索）
- 使用 `GetSysImageIndex()` 设置 iconIndex 提升图标加载性能

### 3. IPluginHost 接口

`IPluginHost` 提供主程序的功能供插件调用。

---

## 创建插件步骤

### 以 BrowserHistory 为模板创建新插件

#### Step 1: 可以复制并重命名 BrowserHistory 插件，或者自己手动创建文件

```bash
# 复制 BrowserHistory 作为模板
cp -r plugins/BrowserHistory plugins/YourPlugin
cd plugins/YourPlugin
```

#### Step 2: 修改文件名和内容

按照以下对应关系重命名和修改文件：

| BrowserHistory 文件              | 你的插件文件               | 说明         |
|--------------------------------|----------------------|------------|
| `BrowserHistoryPluginData.hpp` | `YourPluginData.hpp` | 全局变量定义     |
| `BrowserHistoryAction.hpp`     | `YourAction.hpp`     | Action 类定义 |
| `BrowserHistoryUtil.hpp`       | `YourUtil.hpp`       | 工具函数（可选）   |
| `BrowserHistoryPlugin.cpp`     | `YourPlugin.cpp`     | 插件主类       |
| `CMakeLists.txt`               | `CMakeLists.txt`     | 构建配置       |

#### Step 3: 修改关键内容

**3.1 YourPluginData.hpp**

- 参考 `BrowserHistoryPluginData.hpp`
- 定义 `m_host` 和 `m_pluginId`
- 添加你的配置变量（如 maxResults, startStr 等）

**3.2 YourAction.hpp**

- 参考 `BrowserHistoryAction.hpp`
- 修改类名为 `YourAction`
- 添加你需要的自定义字段（如 url, path, customData 等）

**3.3 YourPlugin.cpp**

- 参考 `BrowserHistoryPlugin.cpp`
- 修改插件名称、包名、版本、描述
- 实现 `RefreshAllActions()` 加载数据逻辑
- 实现 `OnActionExecute()` 执行操作逻辑
- 可选实现 `InterceptInputShowResultsDirectly()` 或 `GetTextMatchActions()`

**3.4 CMakeLists.txt**

- 参考 `BrowserHistory/CMakeLists.txt`
- 修改 `project(YourPlugin)`
- 更新 `PLUGIN_SOURCES` 中的文件名

#### Step 4: 集成到主项目

编辑主 `CMakeLists.txt`，添加：

```cmake
add_subdirectory(plugins/YourPlugin)
list(APPEND PLUGIN_DEPENDENCIES YourPlugin)
```

#### Step 5: 编译和测试

```bash
./scripts/run_cmake_generate.cmd
./scripts/run_build.cmd
./scripts/run_candylauncher.cmd
```

---

## 重要 API 参考

### IPluginHost 方法

#### 常用 IPluginHost 方法

| 方法                                      | 用途                   | 参考位置                                               |
|-----------------------------------------|----------------------|----------------------------------------------------|
| `ShowMessage()`                         | 显示消息框                | BrowserHistoryPlugin.cpp                           |
| `GetSettingsMap()`                      | 获取配置项                | BrowserHistoryPlugin.cpp: OnUserSettingsLoadDone() |
| `GetTheProcessedMatchingText()`         | 处理匹配文本（含拼音）          | BrowserHistoryUtil.hpp                             |
| `GetSuccessfullyMatchingTextActions()`  | 执行模糊搜索               | BrowserHistoryPlugin.cpp                           |
| `TraverseFilesSimpleForEverythingSDK()` | 遍历文件（Everything SDK） | Folder 插件                                          |
| `ChangeEditTextText()`                  | 修改搜索框文本              | CalcExprtk 插件                                      |
| `LoadResIconAsBitmap()`                 | 加载资源图标               | FeatureLaunch 插件                                   |

**详细用法**: 请参考 `plugins/Plugin.hpp` 中的 IPluginHost 接口定义和各插件的实现。

---

### 常用工具函数

**参考文件**: `util/StringUtil.hpp`, `util/LogUtil.hpp`, `util/BitmapUtil.hpp`

**常用函数**:

- `utf8_to_wide()` / `wide_to_utf8()` - 编码转换
- `MyStartsWith2()` / `MyTrim()` - 字符串操作
- `ConsolePrintln()` / `Loge()` - 调试和日志
- `GetSysImageIndex()` - 获取图标索引（提升性能）

**用法示例**: 查看 BrowserHistoryPlugin.cpp 中的实际使用

---

## 最佳实践

### 1. 性能优化

#### ✅ DO - 正确做法

**参考**: `BrowserHistoryPlugin.cpp` 和 `BrowserHistoryUtil.hpp`

- ✅ 在 `RefreshAllActions()` 中预加载所有数据（可耗时）
- ✅ `GetTextMatchActions()` 直接返回 allActions（不做计算）
- ✅ `InterceptInputShowResultsDirectly()` 使用 `GetSuccessfullyMatchingTextActions()` 快速搜索
- ✅ 使用 `GetSysImageIndex()` 设置图标索引（避免重复加载文件）
- ✅ 限制结果数量（建议不超过 1000）

#### ❌ DON'T - 错误做法

- ❌ 不要在 `GetTextMatchActions()` 中懒加载数据（会阻塞 UI）
- ❌ 不要在 `InterceptInputShowResultsDirectly()` 中遍历大量数据
- ❌ 不要每次都重新加载图标文件（应使用 iconIndex）

**详细示例**: 对照 BrowserHistory 插件的实现理解这些要点

### 2. 内存管理

- 使用 `shared_ptr` 管理 Action 生命周期
- 在 `Shutdown()` 中清理所有资源
- `RefreshAllActions()` 先 clear 再加载

**参考**: BrowserHistoryPlugin.cpp 的 Shutdown() 和 RefreshAllActions()

### 3. 配置管理

- 使用语义化配置键名: `com.yourcompany.yourplugin.feature_name`
- 在 `DefaultSettingJson()` 中定义配置项和默认值
- 在 `OnUserSettingsLoadDone()` 中读取用户配置

**参考**: BrowserHistoryPlugin.cpp 的配置处理

### 4. 错误处理

- 使用 `dynamic_pointer_cast` 安全转换 Action
- 检查 `m_host` 指针有效性
- 使用 try-catch 处理异常
- 使用 `Loge()` 记录错误

**参考**: BrowserHistoryUtil.hpp 的异常处理

### 5. 搜索优化

- 预处理 matchText: `m_host->GetTheProcessedMatchingText()`
- 使用主程序的模糊匹配: `GetSuccessfullyMatchingTextActions()`
- 限制结果数量，避免返回过多结果

**参考**: BrowserHistoryPlugin.cpp 的搜索实现

---

## 性能优化建议

### 1. 响应时间要求

| 方法                                  | 最大响应时间  | 说明           |
|-------------------------------------|---------|--------------|
| `InterceptInputShowResultsDirectly` | < 50ms  | 用户输入响应，超过会卡顿 |
| `OnUserInput`                       | < 10ms  | 每次按键都调用      |
| `OnActionExecute`                   | < 200ms | 执行操作，可以稍慢    |
| `RefreshAllActions`                 | 无限制     | 启动时调用，可以耗时   |

### 2. 性能测试

使用计时器测试性能：

```cpp
    MethodTimerStart();  // 开始计时

    auto results = SearchData(input);

    MethodTimerEnd(L"InterceptInputShowResultsDirectly");  // 结束并输出耗时
```

### 3. 优化策略

#### A. 索引预建立

在 `RefreshAllActions()` 中建立索引，搜索时直接查询索引（O(1) 或 O(log n)）

#### B. 限制结果数量

- 早期退出循环（达到最大结果数）
- 提示用户输入更多字符缩小范围

**参考**: Registry 插件的结果限制策略

#### C. 懒加载策略

- 空查询返回提示信息
- 短查询限制较少结果
- 长查询返回完整结果

**参考**: Registry 插件的 InterceptInputShowResultsDirectly() 实现

#### D. 异步加载（高级）

使用线程异步加载数据，避免阻塞主线程。需要注意线程安全（使用 mutex）。

---

## 示例插件分析

### 推荐学习路径

#### 1. BrowserHistory 插件 - 浏览器历史（推荐入门）

**学习重点**:

- 标准的五文件结构（Data, Action, Util, Plugin, CMake）
- SQLite 数据库只读访问（`?immutable=1`）
- 配置项定义和读取
- 图标索引缓存优化
- SQL 查询结果限制

**文件位置**: `plugins/BrowserHistory/`

---

#### 2. Registry 插件 - 注册表导航（学习高级优化）

**学习重点**:

- 空查询优化（不枚举大量数据）
- 结果数量限制策略
- 完全匹配优先返回
- 动态前缀拦截 (`InterceptInputShowResultsDirectly`)

**文件位置**: `plugins/Registry/`

---

#### 3. FeatureLaunch 插件 - 功能启动（学习主程序交互）

**学习重点**:

- 使用主程序回调 (`GetAppLaunchActionCallbacks()`)
- 静态数据定义（不需要 RefreshAllActions）
- 资源图标加载 (`LoadResIconAsBitmap()`)

**文件位置**: `plugins/FeatureLaunch/`

---

#### 4. Folder 插件 - 文件索引（学习 Everything SDK）

**学习重点**:

- Everything SDK 文件遍历
- JSON 配置文件解析
- 环境变量处理
- UWP 应用索引
- 复杂的索引管理（`IndexedManager.hpp`）

**文件位置**: `plugins/Folder/`

---

### 其他插件

| 插件名                 | 学习要点             | 复杂度 |
|---------------------|------------------|-----|
| Bookmark            | 多浏览器书签索引和Json    | 中   |
| CalcExprtk          | 科学计算库集成          | 中   |
| CherryTree          | SQLite           | 中   |
| RunningApp          | Windows API 窗口枚举 | 低   |
| JetbrainsWorkspaces | XML 解析           | 中   |
| VSCodeWorkspaces    | JSON 数据库读取       | 低   |
| UnitConverter       | 静态数据和计算逻辑        | 低   |
| ValueGenerator      | UUID 生成和编码转换     | 低   |

---

## 配置项定义

### DefaultSettingJson 格式

**参考**: BrowserHistoryPlugin.cpp 的 `DefaultSettingJson()` 方法

### 支持的类型

| 类型          | C++ 读取方式                       | 示例           |
|-------------|--------------------------------|--------------|
| `string`    | `settings.at(key).stringValue` | `"hello"`    |
| `bool`      | `settings.at(key).boolValue`   | `true`       |
| `long`      | `settings.at(key).intValue`    | `42`         |
| `stringArr` | `settings.at(key).stringArr`   | `["a", "b"]` |

**详细示例**: 查看 BrowserHistory、Registry、Folder 等插件的配置定义

---

## 常见问题 (FAQ)

### Q1: 插件加载失败怎么办？

**A**: 检查以下几点：

1. DLL 是否在 `plugins/` 目录
2. 是否导出了三个必需函数：`CreatePlugin`, `DestroyPlugin`, `GetPluginApiVersion`
3. 查看主程序日志输出
4. 使用调试器检查是否有异常

### Q2: 为什么搜索很慢？

**A**:

1. 不要在 `InterceptInputShowResultsDirectly` 中做耗时操作
2. 使用 `m_host->GetSuccessfullyMatchingTextActions()` 而不是自己实现搜索
3. 限制结果数量（建议不超过 50-100）
4. 使用 `MethodTimerStart/End` 测量耗时

### Q3: 图标显示不正确？

**A**:

1. 确保 `iconPath` 指向有效的文件（.exe, .ico, .dll）
2. 使用 `GetSysImageIndex()` 设置 `iconIndex` 提升性能
3. 或返回自定义 `HBITMAP` 从 `getIconBitmap()`

### Q4: 如何调试插件？

**A**:

```cpp
// 1. 使用控制台输出
ConsolePrintln(L"YourPlugin", L"Debug info: " + someValue);

// 2. 使用错误日志
Loge(L"YourPlugin", L"Error occurred");

// 3. 使用 Visual Studio 附加到进程
// 设置断点，然后附加到 CandyLauncher.exe

// 4. 使用计时器
MethodTimerStart();
SomeOperation();
MethodTimerEnd(L"SomeOperation");
```

### Q5: 如何处理中文？

**A**:

- 所有字符串使用 `std::wstring`（L"中文文本"）
- 配置文件 UTF-8 使用 `utf8_to_wide()` 转换
- matchText 使用 `GetTheProcessedMatchingText()` 自动支持拼音搜索

**参考**: BrowserHistoryUtil.hpp 的字符串处理

---

## 调试技巧

### 1. 使用计时器

`MethodTimerStart()` 和 `MethodTimerEnd(L"方法名")` - 测量代码执行时间

### 2. 输出调试信息

`ConsolePrintln(L"插件名", L"调试信息")` - 控制台输出

### 3. 错误日志

`Loge(L"插件名", L"错误信息", e.what())` - 记录异常

**参考**: util/LogUtil.hpp 和 BrowserHistoryUtil.hpp 的使用

---

## 总结

### 创建插件的推荐流程

1. ✅ **复制 BrowserHistory 插件作为模板**
2. ✅ **重命名文件和类名**（YourPlugin, YourAction, YourPluginData）
3. ✅ **修改插件元数据**（名称、包名、版本、描述）
4. ✅ **实现数据加载逻辑**（RefreshAllActions）
5. ✅ **实现执行逻辑**（OnActionExecute）
6. ✅ **更新 CMakeLists.txt**
7. ✅ **集成到主项目并测试**

### 关键原则

- **性能第一**: InterceptInputShowResultsDirectly 必须快（<50ms）
- **预加载数据**: 在 RefreshAllActions 中加载，而非搜索时
- **使用工具**: 利用 IPluginHost 提供的工具函数
- **限制结果**: 避免返回成千上万条结果
- **内存安全**: 使用 shared_ptr 管理对象生命周期
- **用户体验**: 提供清晰的标题、副标题和图标

### 学习路径

**第一步**: 仔细阅读 `plugins/BrowserHistory/` 的所有文件
**第二步**: 参考 Registry 插件学习性能优化
**第三步**: 参考 FeatureLaunch 学习主程序交互
**第四步**: 参考 Folder 插件学习 Everything SDK
**第五步**: 查看 `plugins/Plugin.hpp` 理解完整 API

---

**快乐编码！** 🚀
