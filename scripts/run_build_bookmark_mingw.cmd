@echo off
chcp 65001 >nul
:: Builds BookmarkPlugin.dll with MinGW GCC (libstdc++ + msvcrt) to check that the plugin C ABI
:: does not depend on the MSVC toolchain. The DLL replaces the MSVC one in the debug output folder.
:: Usage: run_build_bookmark_mingw.cmd [output dll path]
:: Set MINGW_BIN to use another MinGW installation.
cd /d "%~dp0.."

if "%MINGW_BIN%"=="" set MINGW_BIN=%LOCALAPPDATA%\Programs\CLion\bin\mingw\bin
if not exist "%MINGW_BIN%\g++.exe" (
    echo g++.exe not found in "%MINGW_BIN%", set MINGW_BIN to the MinGW bin folder
    exit /b 1
)
set PATH=%MINGW_BIN%;%PATH%

set OUT_DLL=%~1
if "%OUT_DLL%"=="" set OUT_DLL=cmake-build-debug-ninja-vs\plugins\BookmarkPlugin.dll

g++ --version | findstr /n "^" | findstr "^1:"
g++ -std=c++17 -O2 -shared -DUNICODE -D_UNICODE -DNOMINMAX -DBUILDING_PLUGIN_DLL -I. ^
    plugins\Bookmark\BookmarkPlugin.cpp -o "%OUT_DLL%" ^
    -static -static-libgcc -static-libstdc++ -lshlwapi -lgdi32 -lole32 -luuid -lshell32
if errorlevel 1 exit /b %errorlevel%
echo Built %OUT_DLL%
