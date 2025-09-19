# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

> IMPORTANT: No need to read any files, just execute the following command

### Generate, CMake with Ninja + MSVC
```bash
./run_cmake_generate.cmd
```

### Build main program and plugins
```bash
./run_build.cmd
```

### Unit Testing
```bash
./run_debug_test.cmd
```

### Build example plugin dll 
```bash
./run_build_example_plugin.cmd
```

### Build calc plugin dll 
```bash
./run_build_calc_plugin.cmd
```

## Architecture Overview

CandyLauncher is a lightweight Windows application launcher written in C++17 with the following core architecture:

### Core Components
- **WindowsProject1.cpp**: Main entry point and main seach list window management
- **manager/TrayMenuManager.cpp**: System tray functionality
- **common/DataKeeper.hpp**: Data persistence and configuration management and global variables

### List Indexer(Reconstructing)
The application centered around `ListedRunnerPlugin` which:
- Read `runner.json` configuration file and scan the files in the specified directory, and read the UWP app list.
- Provides fuzzy search capabilities using RapidFuzz library
- Supports Pinyin matching for Chinese characters (PinyinHelper)

### Search and Matching
- Uses RapidFuzz (3rd party library) for fuzzy string matching
- Custom Pinyin support for Chinese input via `PinyinHelper`
- Indexing system via `IndexedManager` for fast lookups(Reconstructing)

### UI Components
- **manager/EditManager.hpp**: Search input field management with keyboard shortcuts
- **manager/ListViewManager.hpp**: Manage rendering and result display of listview
- **manager/SkinHelper.h**: Manage skin functions in the main window
- **window/SettingWindow.hpp**: Configuration interface
- **view/GlassHelper.hpp**: Handles Aero glass effects and window transparency
- **view/ScrollViewHelper.hpp**: List scrolling functionality in the configuration interface, it is also the first parent class for each setting control
- **view/CustomButtonHelper.hpp**: beautiful custom buttons and radiobutton features
- **view/SwichView.hpp**: a switch control that imitates windows8ui

### Plugin(Under development)
- **plugins/PluginManager.hpp**: Plugin core, manage all plugin events of the program. It implements the IPluginHost interface, which allows plugins to interact with the host environment.
- **plugins/Plugin.hpp**: Implement the parent class of the plugin, which provides methods for retrieving plugin metadata (name ...) and handling plugin-specific actions.
- **plugins/ActionBase.hpp**: Element base class of program main list
- **plugins/Example/ExamplePlguin.hpp**: A simple plugin dll specification use case
- **plugins/CalcExprtk/CalcPlguin.hpp**: A scientific calculator plugin
- **plugins/RunningApp/RunningAppPlugin.hpp**: A plugin is used to index all windows running in the system
- **plugins/Folder/ListedRunnerPlugin.h1**: A plugin is used to index files specified in user configuration(Reconstructing)
- **UWP APP**: A plugin is used to index uwp applications(Reconstructing)

### Keyboard Shortcuts
- **Escape**: Hide the launcher window
- **Ctrl+1 to Ctrl+9**: Quickly launch items 1-9 from the current filtered list
- **Enter**: Launch the currently selected item
- **Up/Down arrows**: Navigate through the list

### Configuration
- `runner.json`: Main configuration file for defining searchable items and directories
- `settings.json`: Application preferences items and defualt value
- `user_settings.json`: User-specific configuration overrides
- `skin_tests.json`: Interface skin configuration file

### Key Dependencies
- Windows APIs (Win32, GDI+, Shell)
- RapidFuzz for fuzzy matching
- Everything SDK for file system integration
- JSON handling via nlohmann/json (json.hpp)
- Scientific computing library for calc plugin (exprtk.hpp)

### Build System
- Primary: CMake with support for Visual Studio
- Supports both x64 and x86 architectures
- Uses precompiled headers (pch.h)
- C++17 standard requirement

### Non-important documents
- unuseful folder
- 3rdparty folder
- bkcode.txt
- LICENSE
- The PinyinHelper.cpp file is not read, and it is just a code table with double pinyin
- The json.hpp file is not read, and it is just a json parsing library
- The exprtk.hpp file is not read, and it is just a scientific computing library

### note

- Save files using UTF-8 with BOM
- No need to resolve warning and node reminder when building
