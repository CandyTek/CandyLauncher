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

@REM ScreenToolsQrTest
cmake --build cmake-build-release-ninja-vs --target PluginTest SplitWordsTest AutomationActionModelTest ScreenToolsZxingQrTest UrlUtilTest CalcEngineTest --config Release
if errorlevel 1 exit /b %errorlevel%
ctest -C Release --test-dir cmake-build-release-ninja-vs --output-on-failure
exit /b %errorlevel%
