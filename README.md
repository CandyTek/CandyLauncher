# CandyLauncher

CandyLauncher 是一个用 C++ 编写的轻量级 Windows 快捷启动器，能够通过搜索快速启动程序或命令。

## 软件核心目标
- 提供快速的全局热键启动方式
- 支持模糊搜索和拼音匹配
- 通过配置文件扩展可执行命令
- 统一索引常规应用、UWP 应用及指定目录

## 功能介绍
- 主面板可通过自定义热键呼出或隐藏
- 搜索框实时过滤结果，支持模糊匹配
- 托盘图标与右键菜单便于常驻后台
- 在列表中快速执行项目、以管理员权限运行或打开文件夹
- 支持读取 `runner.json` 扫描目录和自定义命令
- 主题皮肤及窗口特效可通过配置文件调整

## TODO
- UWP 应用图标加载仍使用 `HBITMAP` 占位，需要完善其生命周期管理【F:ListedRunnerPlugin.cpp†L176-L182】
- 完善目录索引与拖放交互
- 补充设置界面的更多选项

## 构建方式
### Visual Studio 2022
1. 打开 `WindowsProject1.sln`。
2. 选择 Debug 或 Release 构建配置后生成解决方案即可运行。

### CLion
1. 使用 CLion 打开仓库根目录，读取 `CMakeLists.txt`。
2. 直接点击构建按钮即可完成编译运行。

### 命令行
```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```
