# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

> IMPORTANT: No need to read any files, just execute the following command

### Generate, CMake with Ninja + MSVC
```bash
./scripts/run_cmake_generate.cmd
```

### Build main program and plugins
```bash
./scripts/run_build.cmd
```

### Just use the following command, the command will cause the program to stop itself and output debugging logs after 3 seconds

```bash
./scripts/run_candylauncher.cmd
```

```bash
./scripts/run_candylauncher.cmd | findstr /i 'pluginname'
```

> IMPORTANT: Dont use ``Reading shell output`` commond, the output obtained by this command is often stale

### Unit Testing

```bash
./scripts/run_debug_test.cmd
```

## Architecture Overview

CandyLauncher is a lightweight Windows application launcher written in C++17 with the following core architecture:

### Core Components
- **CandyLauncher.cpp**: Main entry point and main seach list window management
- **manager/TrayMenuManager.cpp**: System tray functionality
- **common/GlobalState.hpp**: App global status (including UI parameters, handles, config items, skin resources, etc.)

### Search and Matching
- Uses RapidFuzz (3rd party library) for fuzzy string matching
- Retrieval of Chinese entries is achieved through the cpp-pinyin library and the util\PinyinHelper.hpp file.

### UI Components
- **manager/EditManager.hpp**: Search input field management with keyboard shortcuts
- **manager/ListViewManager.hpp**: Manage rendering and result display of listview
- **manager/SkinHelper.h**: Manage skin functions in the main window
- **window/SettingsWindow.hpp**: Configuration interface
- **window/SettingsManager.hpp**: Manage app settings (load, merge, save, UI binding, backup/restore)
- **view/ScrollViewHelper.hpp**: List scrolling functionality in the configuration interface, it is also the first parent class for each setting control
- **view/GlassHelper.hpp**: Handles Aero glass effects and window transparency
- **view/CustomButtonHelper.hpp**: beautiful custom buttons and radiobutton features
- **view/SwitchView.hpp**: a switch control that imitates windows8ui

### Plugin system
- **plugins/PluginManager.hpp**: Plugin core, manage all plugin events of the program. It implements the IPluginHost interface, which allows plugins to interact with the host environment
- **plugins/Plugin.hpp**: Implement the parent class of the plugin, which provides methods for retrieving plugin metadata (name ...) and handling plugin-specific actions
- **plugins/BaseAction.hpp**: Element base class of program main list

#### Plugin module
- **[Bookmark](plugins/Bookmark/BookmarkPlugin.cpp**: Indexes bookmarks of browsers such as Edge Chrome
- **[Calc](plugins/CalcExprtk/CalcPlugin.cpp**: Scientific calculator
- **[CherryTree](plugins/CherryTree/CherryTreePlugin.cpp**: Indexes all notes of cherrytree software
- **[ExamplePlugin](plugins/Example/ExamplePlugin.cpp**: A simple plugin dll specification use case
- **[FeatureLaunch](plugins/FeatureLaunch/FeatureLaunchPlugin.cpp**: Navigate some settings and functions of the main program
- **[Folder](plugins/Folder/FolderPlugin.cpp)**: Read `runner.json` configuration file and scan the files in the specified directory,and environment variable %PATH% path, and read the UWP app list. IndexedManager.hpp is all index management and viewers for the Folder plugin
- **[JetbrainsWorkspaces](plugins/JetbrainsWorkspaces/Main.cpp)**: Index all ide software history open projects of jetbrains
- **[RegistryPlugin.cpp](plugins/Registry/RegistryPlugin.cpp)**: Navigation system registry items
- **[RunningApp](plugins/RunningApp/RunningAppPlugin.cpp)**: Index all windows running in the system
- **[Service](plugins/Service/ServicePlugin.cpp)**: Index system service item
- **[UnitConverter](plugins/UnitConverter/UnitConverterPlugin.cpp)**: Provides the function of inputting relevant data and converting it to other units
- **[ValueGenerator](plugins/ValueGenerator/Main.cpp)**: Generate uuid and string hash calc and string encode/decode
- **[VSCodeWorkspaces](plugins/VSCodeWorkspaces/Main.cpp)**: Index VSCode history open projects

### Keyboard Shortcuts
- **Alt+K**: Show or hide the launcher window
- **Enter**: Launch the currently selected item
- **Up/Down arrows**: Navigate through the list

### Configuration
- `settings.json`: App preferences items and defualt value
- `user_settings.json`: User-specific configuration overrides
- `skin_tests.json`: Interface skin configuration file
- `runner.json`: FolderPlugin configuration file for defining searchable items and directories

### Key Dependencies
- Windows APIs (Win32, GDI+, Shell)
- RapidFuzz for fuzzy matching
- Everything SDK for file system integration
- JSON handling via nlohmann/json (json.hpp)
- Scientific computing library for calc plugin (exprtk.hpp)
- cpp-pinyin provides conversion of Chinese to pinyin
- XML handling via zeux/pugixml

### Build System
- Primary: CMake with support for Visual Studio
- Supports both x64 and x86 architectures
- Uses precompiled headers (pch.h)
- C++17 standard requirement

### Non-important files
- deprecated folder
- 3rdparty folder
- bkcode.txt
- LICENSE
- The PinyinHelper.cpp file is not read, and it is just a code table with double pinyin
- The json.hpp file is not read, and it is just a json parsing library
- The exprtk.hpp file is not read, and it is just a scientific computing library

### note

- Project codes are indented using tabs
- Use ``util/LogUtil.hpp`` funtion ``ConsolePrintln(const std::wstring& tag, const std::wstring& msg)`` print output or std::cout
- Use ``util/LogUtil.hpp`` funtion ``Loge(L"tag",L"error msg",e.what());`` print error or std::cerr
- Save files using UTF-8 with BOM
- No need to resolve warning and node reminder when building
