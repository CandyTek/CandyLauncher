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
./scripts/run_build.cmd 2>&1 | tail -25
```

### Just use the following command, the command will cause the program to stop itself and output debugging logs after 3 seconds

```bash
./scripts/run_candylauncher.cmd
```

```bash
./scripts/run_candylauncher.cmd | grep -i 'pluginname'
```

```bash
./scripts/build_and_run.cmd
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
- **util/DpiUtil.hpp**: High-DPI support. The process stays DPI-unaware; the main window is Per-Monitor V2 and scales the skin at bake time (`g_mainWindowDpi`, `MainDpiScale`, `applyMainWindowDpi`); secondary windows (settings, skin editor, rename dialog, plugin windows) are Per-Monitor V2 too and use the simple per-window scheme (`Dpi::Scale`, `CreateScaledFont`, `RescaleChildWindows`, `ApplySuggestedRect`). Coordinate-space rules and verification: [DOC_DPI.md](docs/DOC_DPI.md)
- **view/CustomButtonHelper.hpp**: beautiful custom buttons and radiobutton features
- **view/SwitchView.hpp**: a switch control that imitates windows8ui

### Plugin system
- **plugins/sdk/CandyPluginApi.h**: Pure C plugin ABI (`CL_PluginApi` exported by plugins via `CandyPlugin_GetApi`, `CL_HostApi` provided by the host). Rules and porting guide: [DOC_PLUGIN_API_ABI_NOTES.md](docs/DOC_PLUGIN_API_ABI_NOTES.md)
- **plugins/sdk/CandyPluginSdk.hpp**: Header-only C++ layer compiled into plugins (`candy::Plugin`, `candy::Host`, `candy::ActionSet<T>`, `CANDY_PLUGIN_EXPORT`)
- **plugins/PluginManager.hpp**: Plugin core, loads plugins and dispatches events; `PluginHostApi.hpp` implements `CL_HostApi`, `PluginActionProxy.hpp` holds host-owned copies of plugin list items
- **plugins/BaseAction.hpp**: Element base class of program main list (host only, never crosses the DLL boundary)
- **plugins/PluginI18n.hpp**: Plugin string tables behind `CL_HostApi::get_string`; plugins call `Tr(key, fallback)`. Strings live in the `"strings"` object of each `<dll>.plugin.json` (all current plugins) or optionally in `plugins/lang/<dll>/<lang>.json`. Design and migration rules: [DOC_PLUGIN_I18N.md](docs/DOC_PLUGIN_I18N.md)

#### Plugin module
- **[AirPods](plugins/AirPods/AirPodsPlugin.cpp)**: Shows the battery level of connected Apple headphones
- **[Bookmark](plugins/Bookmark/BookmarkPlugin.cpp)**: Indexes bookmarks of browsers such as Edge Chrome
- **[BrowserHistory](plugins/BrowserHistory/BrowserHistoryPlugin.cpp)**: Indexes browser history (shares `sqlite3.dll` with CherryTree/EverNote)
- **[Calc](plugins/CalcExprtk/CalcPlugin.cpp)**: Scientific calculator
- **[CherryTree](plugins/CherryTree/CherryTreePlugin.cpp)**: Indexes all notes of cherrytree software
- **[EverNote](plugins/EverNote/EverNotePlugin.cpp)**: Indexes the local Evernote database
- **[ExamplePlugin](plugins/Example/ExamplePlugin.cpp)**: A simple plugin dll specification use case; `plugins/ExampleC` and `plugins/ExampleRust` are standalone C/Rust ABI examples
- **[FeatureLaunch](plugins/FeatureLaunch/FeatureLaunchPlugin.cpp)**: Navigate some settings and functions of the main program
- **[Folder](plugins/Folder/FolderPlugin.cpp)**: Read `config_folder_plugin.json` configuration file and scan the files in the specified directory,and environment variable %PATH% path, and read the UWP app list. IndexManagerWindow.hpp / IndexManagerLogic.hpp are the index management and viewers for the Folder plugin
- **[JetbrainsWorkspaces](plugins/JetbrainsWorkspaces/JbPlugin.cpp)**: Index all ide software history open projects of jetbrains
- **[NodeJsPlugin](plugins/NodeJsPlugin/NodeJsPlugin.cpp)**: Runs Wox / Flow Launcher style JavaScript plugins through Node.js (`node-host.js` bridge, scripts under `plugins/NodeJsPlugin/scripts`). Docs: [DOC_NODEJSPLUGIN.md](plugins/NodeJsPlugin/DOC_NODEJSPLUGIN.md)
- **[OneNote](plugins/OneNote/OneNotePlugin.cpp)**: Indexes local OneNote notebooks (built only when the OneNote type library is available)
- **[PluginBrowser](plugins/PluginBrowser/PluginBrowserPlugin.cpp)**: Lists installed plugins and toggles them with Enter
- **[Registry](plugins/Registry/RegistryPlugin.cpp)**: Navigation system registry items
- **[RunningApp](plugins/RunningApp/RunningAppPlugin.cpp)**: Index all windows running in the system
- **[ScreenTools](plugins/ScreenTools/ScreenToolsPlugin.cpp)**: Scrolling screenshots and QR code recognition (zxing-cpp)
- **[Service](plugins/Service/ServicePlugin.cpp)**: Index system service item
- **[SmallToolBox](plugins/SmallToolBox/ToolBoxPlugin.cpp)**: A collection of small utilities (QR code generation etc.)
- **[TextFile](plugins/TextFile/TextFilePlugin.cpp)**: Indexes text files in configured folders
- **[UnitConverter](plugins/UnitConverter/UnitConverterPlugin.cpp)**: Provides the function of inputting relevant data and converting it to other units
- **[ValueGenerator](plugins/ValueGenerator/ValueGeneratorPlugin.cpp)**: Generate uuid and string hash calc and string encode/decode
- **[VisualStudio](plugins/VisualStudio/VisualStudioPlugin.cpp)**: Index recently opened Visual Studio projects and solutions
- **[VSCodeWorkspaces](plugins/VSCodeWorkspaces/VscPlugin.cpp)**: Index VSCode history open projects
- **[WebSearch](plugins/WebSearch/WebSearchPlugin.cpp)**: Search the web with keyword shortcuts (`config_websearch_plugin.json`)
- **[WindowsSearch](plugins/WindowsSearch/WindowsSearchPlugin.cpp)**: Search files and folders through the Windows indexing service
- **[PluginTest](plugins/PluginTest)**: Host-side plugin ABI test harness (ctest), not a shipped plugin

### Keyboard Shortcuts
- **Alt+K**: Show or hide the launcher window
- **Enter**: Launch the currently selected item
- **Up/Down arrows**: Navigate through the list

### Configuration
- `settings.json`: App preferences items and defualt value
- `user_settings.json`: User-specific configuration overrides
- `skin_tests.json`: Interface skin configuration file
- `config_folder_plugin.json`: FolderPlugin configuration file for defining searchable items and directories

### Key Dependencies
- Windows APIs (Win32, GDI+, Shell)
- RapidFuzz for fuzzy matching
- Everything SDK for file system integration
- JSON handling via nlohmann/json (json.hpp)
- Scientific computing library for calc plugin (exprtk.hpp)
- cpp-pinyin provides conversion of Chinese to pinyin
- XML handling via zeux/pugixml
- lunasvg for high-quality SVG icon rendering
- cmark-gfm for URL detection through its autolink extension (`util/UrlUtil.hpp`)

### Build System
- Primary: CMake with support for Visual Studio
- Supports both x64 and x86 architectures
- Uses precompiled headers (pch.h)
- C++17 standard requirement
- App version is single-sourced from `project(CandyLauncher VERSION x.y.z)` in CMakeLists.txt; CMake generates `common/AppVersion.h` (`APP_VERSION_STRING`, `APP_VERSION_RC`, `APP_ARCH_STRING`) used by the .rc VERSIONINFO, the updater and the about page. Never hard-code a version elsewhere. GitHub release tags are `vX.Y.Z`.

### Non-important files
- deprecated folder
- 3rdparty folder
- bkcode.txt
- LICENSE
- The PinyinHelper.cpp file is not read, and it is just a code table with double pinyin
- The json.hpp file is not read, and it is just a json parsing library
- The exprtk.hpp file is not read, and it is just a scientific computing library
- The stb_image_write.h file is not read

### App Update System

read [DOC_APP_UPDATE.md](docs/DOC_APP_UPDATE.md)

### note

- Project codes are indented using tabs
- Use ``util/LogUtil.hpp`` funtion ``Logi(const std::wstring& tag, ...)`` print output
- Use ``util/LogUtil.hpp`` funtion ``Loge(const std::wstring& tag, ...)`` print error
- Save files using UTF-8 with BOM, `Get-Content` command requires parameter `-Encoding UTF8` to read the file.
- No need to resolve warning and node reminder when building
- The app is still under development.
- 
