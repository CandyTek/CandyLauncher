@echo off
chcp 65001 >nul
:: Build the Release configuration and produce dist\CandyLauncher-v<version>-<arch>.zip
:: Usage: scripts\release\run_package.cmd            (generate if needed, build, test, package)
::        scripts\release\run_package.cmd --no-build (package the existing Release build only)

set PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%
for /f "usebackq tokens=*" %%i in (`call "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set VS_PATH=%%i
)

:: call VsDevCmd.bat (force x64, otherwise the x86 CRT may be linked)
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -no_logo -arch=amd64 -host_arch=amd64
cd /d "%~dp0..\.."

set BUILD_DIR=cmake-build-release-ninja-vs

if /i "%~1"=="--no-build" goto :package

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    call "%~dp0run_cmake_generate.cmd"
    if errorlevel 1 exit /b %errorlevel%
)

:: Stop a running instance from this build dir so the exe can be replaced
for /f "tokens=3 delims=," %%I in ('wmic process where "name='CandyLauncher.exe'" get ExecutablePath^,ProcessId /format:csv 2^>nul ^| findstr /i "%BUILD_DIR%"') do (
    for /f "delims=" %%A in ("%%I") do (
        taskkill /F /PID %%A /T
    )
)

cmake --build %BUILD_DIR% --config Release --target CandyLauncher SkinEditor -j 18
if errorlevel 1 exit /b %errorlevel%

call "%~dp0run_test.cmd"
if errorlevel 1 (
    echo [package] tests failed, aborting
    exit /b 1
)

:package
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0package.ps1" -BuildDir "%BUILD_DIR%" -OutDir dist
exit /b %errorlevel%
