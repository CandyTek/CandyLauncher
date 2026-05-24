# NodeJsPlugin for CandyLauncher

NodeJsPlugin 是 CandyLauncher 的一个桥接插件，允许开发者使用 JavaScript 或 TypeScript 编写自定义插件。它通过一个持久化的 Node.js 宿主进程与主程序进行 JSON-RPC 通信，兼顾了开发效率与系统稳定性。

兼容 Wox 和 FlowLauncher 的js ts插件，js直接放入指定文件夹即可使用，ts插件目前需要手动 npm install/ run-script build

## 特性

- **高性能桥接**：采用异步 I/O 和 JSON-RPC 协议，查询过程不阻塞主 UI 线程。
- **多格式支持**：
  - **Standard Module**: 支持导出 `init`, `query`, `execute` 方法的模块化插件。
  - **Flow Launcher 兼容**: 直接支持大量的 Flow Launcher 社区插件（CLI 模式）。
- **进程隔离**：JS 插件运行在独立的 Node.js 进程中，即使脚本崩溃也不会影响 CandyLauncher 主程序。
- **错误捕获**：自动捕获并转发 JS 层的 `stderr` 日志到主程序的调试控制台。

## 目录结构

```text
plugins/NodeJsPlugin/
├── NodeJsPlugin.dll        # C++ 桥接插件主程序
├── node-host.js            # Node.js 宿主服务脚本 (RPC 服务端)
├── NodeJsBridge.hpp        # 通信逻辑
└── scripts/                # JS 插件存放目录
    ├── PluginA/
    │   ├── plugin.json     # 插件元数据
    │   └── index.js        # 入口文件
    └── PluginB/            # Flow Launcher 格式插件
        ├── plugin.json
        └── main.js
```

## 插件开发指南

### 1. 配置文件 (plugin.json)

每个 JS 插件必须包含一个 `plugin.json` 文件。

**标准格式示例：**
```json
{
  "Id": "unique-uuid",
  "Name": "My JS Plugin",
  "Description": "Doing awesome things with JS",
  "Author": "YourName",
  "Version": "1.0.0",
  "Runtime": "nodejs",
  "Entry": "index.js",
  "TriggerKeywords": ["myjs"]
}
```

**Flow Launcher 兼容格式：**
```json
{
  "ID": "unique-uuid",
  "ActionKeyword": "hn",
  "Name": "HelloWorld",
  "ExecuteFileName": "main.js",
  "Language": "javascript"
}
```

### 2. 编写脚本

#### 模块化模式 (推荐)
导出 `query` 函数处理搜索请求。

```javascript
// index.js
module.exports = {
    init: async () => {
        // 初始化逻辑
    },
    query: async (ctx, payload) => {
        const query = payload.Search;
        return [{
            Title: `You searched for ${query}`,
            SubTitle: "Click to copy",
            ActionData: { text: query }
        }];
    },
    execute: async (actionData) => {
        // 执行动作
        console.log("Executing:", actionData.text);
    }
};
```

#### CLI 模式 (Flow Launcher 风格)
程序通过命令行参数接收 JSON 请求，并将结果打印到 `stdout`。

```javascript
// main.js
const request = JSON.parse(process.argv[2]);
if (request.method === "query") {
    console.log(JSON.stringify({
        "result": [{
            "Title": "Hello Flow Launcher",
            "Subtitle": "Arguments: " + request.parameters[0]
        }]
    }));
}
```

## 调试与日志

- 插件内部使用 `console.log` 或 `console.error` 打印的内容会被桥接器捕获。
- 日志在 CandyLauncher 的调试日志窗口中以 `[NodeJsBridge][JS]` 前缀显示。
- 如果插件未按预期工作，请检查 `node-host.js` 是否正确检测到插件入口。

## 系统要求

- 操作系统：Windows 7+
- 环境：系统中需安装 [Node.js](https://nodejs.org/) 并将其添加到环境变量 `PATH` 中。
