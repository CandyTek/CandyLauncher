# TODO: NodeJsPlugin 兼容性与功能增强

## 1. 架构兼容性 (node-host.js)
- [ ] **完善 Mock API**：
    - [ ] 实现 `GetSetting`：允许 JS 插件读取主程序的设置或插件目录下的配置文件。
    - [ ] 实现 `Log`：将 JS 内部日志通过 `process.stderr` 转发到 C++。
    - [ ] 实现 `OnSettingChanged` 回调支持。
- [ ] **环境变量支持**：模拟 Flow Launcher 的环境变量（如 `PYTHONPATH`, `FLOW_LAUNCHER_API_VER` 等），增加兼容性。
- [ ] **进程生命周期**：对于 CLI 模式插件，增加超时强制 Kill 机制，防止僵尸进程。

## 2. 核心功能增强 (C++ & JS)
- [ ] **设置系统集成**：
    - [ ] 将 C++ 的 `IPluginHost::GetSettingsMap` 暴露给 `node-host.js`。
    - [ ] 支持在 CandyLauncher 设置界面动态修改 JS 插件的配置。
- [ ] **图标加载优化**：
    - [ ] 支持 `relative:` 前缀路径解析。
    - [ ] 支持 Base64 图片数据（ImageData）的直接解析。
    - [ ] 增加图标缓存机制，避免频繁从磁盘加载。
- [ ] **热重载**：监控 `scripts/` 目录变化，实现 JS 插件自动重载无需重启 CandyLauncher。

## 3. 插件特定修复 (CustomCommands)
- [ ] **配置文件加载**：修改 `node-host.js`，使 `GetSetting("shortcuts")` 能读取 `CustomCommands/shortcuts.json`（如果存在）。
- [ ] **通配符处理**：优化 C++ 层的关键词匹配，确保 `*` 类型的插件能正确接收到所有流量。

## 4. 自动化 (Developer Experience)
- [ ] **自动安装依赖**：检测到 `package.json` 且缺少 `node_modules` 时，提示用户或自动运行 `npm install`。
- [ ] **模板工具**：提供一个简单的 `create-plugin.js` 脚本，帮助用户快速生成兼容 CandyLauncher 的 JS 插件。
