# CandyLauncher

CandyLauncher 是一个用 C++ 编写的轻量级 Windows 快捷启动器，能够通过搜索快速启动程序。

## 软件核心目标
- 极速显示匹配结果
- 极低的内存占用
- 支持模糊搜索和拼音匹配
- 支持 Windows7+

## 功能介绍
- 主面板可通过自定义热键呼出或隐藏
- 在列表中快速执行项目、以管理员权限运行或打开文件夹
- 支持读取 `runner.json` 扫描目录和自定义命令
- 主题皮肤及窗口特效可通过配置文件调整

## TODO
- UWP 应用图标加载仍使用 `HBITMAP` 占位，需要完善其生命周期管理【F:ListedRunnerPlugin.cpp†L176-L182】
- 完善目录索引与拖放交互
- 补充设置界面的更多选项

## note

- 使用 UTF-8 with BOM 保存文件

## 构建方式
### Visual Studio 2022
- 安装组件：使用 C++ 的桌面开发（含 Windows SDK、MSVC 工具集、CMake 集成）
- 打开 ```WindowsProject1.sln```
- 选择 x64 / x86 与 Debug / Release，生成解决方案即可运行

### CLion
- 使用 CLion 打开仓库根目录，读取 `CMakeLists.txt`。
- 点击运行按钮编译运行。

### VsCode + 插件 (CMake Tools + C/C++)
- 使用 VsCode 打开仓库根目录，选择其中一项配置进行生成，注意相关的依赖和环境变量
- 点击运行按钮编译运行

### 命令行 (Native Tools Command Prompt for VS 2022)
#### CMake + MSVC
```commandline
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
ctest -C Debug --test-dir build
```

#### CMake + Ninja + MSVC
```commandline
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
cmake --build build --config Debug
ctest -C Debug --test-dir build
```

### 命令行
#### CMake + MinGW-w64
使用命令检查 MinGW
```commandline
where gcc
where g++
```

```commandline
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build --config Debug
ctest -C Debug --test-dir build
```

#### CMake + Ninja + MinGW-w64
使用命令检查 MinGW
```commandline
where gcc
where g++
```

```commandline
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=C:\path_to\ninja.exe
cmake --build build --config Debug
ctest -C Debug --test-dir build
```

#### MSBuild
```commandline
msbuild WindowsProject1.sln /t:Build /p:Configuration=Debug /p:Platform=x64
```
