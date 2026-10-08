@echo off
chcp 65001 >nul
:: Use vswhere to find the latest VS installation path
set PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%
for /f "usebackq tokens=*" %%i in (`call "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set VS_PATH=%%i
)

:: call VsDevCmd.bat
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -no_logo

@REM ScreenToolsQrTest
cmake --build cmake-build-debug-ninja-vs --target PluginTest SplitWordsTest AutomationActionModelTest ScreenToolsZxingQrTest UrlUtilTest CalcEngineTest --config Debug
if errorlevel 1 exit /b %errorlevel%
ctest -C Debug --test-dir cmake-build-debug-ninja-vs --output-on-failure
exit /b %errorlevel%
