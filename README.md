#CandyLauncher

CandyLauncher is a lightweight Windows shortcut launcher written in C++, imitating Wox, and quickly launches the actions in each plug-in through search

[![English badge](https://img.shields.io/badge/%E8%8B%B1%E6%96%87-English-blue)](./README.md)
[![简体中文 badge](https://img.shields.io/badge/%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87-Simplified%20Chinese-blue)](./README_ZH.md)

## Core goals of software

- Quickly display matching results
- Lower memory usage
- Support fuzzy search and pinyin matching
- Support Windows7+

## Function introduction

- The main panel can be called out or hidden through custom hotkeys
- Quickly execute items in a list, run as administrator or open a folder
- Support reading `config_folder_plugin.json` and scanning directories
- Theme skin and window effects can be adjusted through configuration files
- Support loading various custom plugins

## Build method

### Visual Studio 2022

- Installation components: Desktop development using C++ (including Windows SDK, MSVC toolset, CMake integration)
- Open the warehouse root folder
- Select one of the configurations to generate (ninja-msvc is recommended)
- Select CandyLauncher.exe as the run item and click the Run button

### CLions

- Open the warehouse root folder
- Do not use Cmake preset, use the default Debug configuration, and manually select the tool chain and generator (Visual Studio + Ninja recommended)
- Select CandyLauncher as the run item and click the run button

### VsCode + Plugin (CMake Tools + C/C++)

- Open the warehouse root folder
- Shortcut key Ctrl+Shift+P, Cmake: select configuration preset
- Select one of the configurations to generate (ninja-msvc is recommended)
- Click to generate in the bottom status bar
- Click the run button and select CandyLauncher as the run item

### Command line (Native Tools Command Prompt for VS 2022)

Open a terminal in the root directory of the repository

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


### Command line

Check MinGW using command

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
