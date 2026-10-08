@echo off
chcp 65001 >nul
:: Use vswhere to find the latest VS installation path
set PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%
for /f "usebackq tokens=*" %%i in (`call "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set VS_PATH=%%i
)

:: call VsDevCmd.bat (force x64, otherwise the x86 CRT may be linked)
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -no_logo -arch=amd64 -host_arch=amd64
cd /d "%~dp0..\.."
cmake -S . -B cmake-build-release-ninja-vs -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl --no-warn-unused-cli

:: Reuse the debug user settings so the release build has the same plugin configuration
if not exist "cmake-build-release-ninja-vs\user_settings.json" if exist "cmake-build-debug-ninja-vs\user_settings.json" (
    copy /y "cmake-build-debug-ninja-vs\user_settings.json" "cmake-build-release-ninja-vs\user_settings.json" >nul
)
