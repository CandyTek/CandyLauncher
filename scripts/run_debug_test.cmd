@echo off
chcp 65001 >nul
:: Use vswhere to find the latest VS installation path
set PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%
for /f "usebackq tokens=*" %%i in (`call "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set VS_PATH=%%i
)

:: call VsDevCmd.bat
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -no_logo

@REM cmake --build cmake-build-debug-ninja-vs --target SplitWordsTest --config Debug
@REM ctest -C Debug --test-dir cmake-build-debug-ninja-vs 
cmake --build cmake-build-debug-ninja-vs --target PluginTest --config Debug
ctest -C Debug --test-dir cmake-build-debug-ninja-vs -R PluginTest -V
