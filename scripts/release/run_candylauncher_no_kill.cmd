@echo off
chcp 65001 >nul
cd /d "%~dp0..\.."
cd cmake-build-release-ninja-vs

@REM :: 启动 CandyLauncher.exe
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
"$exePath = Join-Path (Get-Location) 'CandyLauncher.exe'; Start-Process -FilePath $exePath"
