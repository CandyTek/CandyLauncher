# CandyLauncher

CandyLauncher 是一个用 C++ 编写的轻量级 Windows 快捷启动器，仿 wox，通过搜索快速启动各插件里的行为

## 软件核心目标

- 极速显示匹配结果
- 较低的内存占用
- 支持模糊搜索和拼音匹配
- 支持 Windows7+

## 功能介绍

- 主面板可通过自定义热键呼出或隐藏
- 在列表中快速执行项目、以管理员权限运行或打开文件夹
- 支持读取 `config_folder_plugin.json` 扫描目录
- 主题皮肤及窗口特效可通过配置文件调整

## note

- 使用 UTF-8 with BOM 保存文件

## 构建方式

### Visual Studio 2022

- 安装组件：使用 C++ 的桌面开发（含 Windows SDK、MSVC 工具集、CMake 集成）
- 打开仓库根目录文件夹
- 选择其中一项配置进行生成（推荐 ninja-msvc）
- 运行项选择 CandyLauncher.exe，点击运行按钮

### CLion

- 打开仓库根目录文件夹
- 不使用 Cmake预设，使用默认的Debug配置，手动选择工具链、生成器（推荐 Visual Studio + Ninja）
- 运行项选择 CandyLauncher，点击运行按钮

### VsCode + 插件 (CMake Tools + C/C++)

- 打开仓库根目录文件夹
- 快捷键 Ctrl+Shift+P，Cmake: 选择配置预设
- 选择其中一项配置进行生成（推荐 ninja-msvc）
- 底部状态栏点击生成
- 点击运行按钮，运行项选择 CandyLauncher

### 命令行 (Native Tools Command Prompt for VS 2022)

在仓库根目录打开终端

#### CMake + Ninja + MSVC

```bash
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build --config Debug --target CandyLauncher
ctest -C Debug --test-dir build
```

#### CMake + MSVC

```commandline
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug --target CandyLauncher
ctest -C Debug --test-dir build
```


### 命令行

使用命令检查 MinGW

```commandline
where gcc
where g++
```

#### CMake + Ninja + MinGW-w64

```commandline
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=C:\path_to\ninja.exe
cmake --build build --config Debug
ctest -C Debug --test-dir build
```

#### CMake + MinGW-w64

```commandline
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build --config Debug
ctest -C Debug --test-dir build
```
