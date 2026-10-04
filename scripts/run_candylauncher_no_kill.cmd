@echo off
chcp 65001 >nul
cd /d "%~dp0.."
cd cmake-build-debug-ninja-vs

@REM :: 启动 CandyLauncher.exe，3 秒后强制结束
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
"$exePath = Join-Path (Get-Location) 'CandyLauncher.exe'; Start-Process -FilePath $exePath"
