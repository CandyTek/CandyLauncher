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

for /f "tokens=3 delims=," %%I in ('wmic process where "name='CandyLauncher.exe'" get ExecutablePath^,ProcessId /format:csv 2^>nul ^| findstr /i "cmake-build-release-ninja-vs"') do (
    for /f "delims=" %%A in ("%%I") do (
        taskkill /F /PID %%A /T
    )
)
:: Execute the build
cmake --build cmake-build-release-ninja-vs --config Release --target CandyLauncher -j 18
