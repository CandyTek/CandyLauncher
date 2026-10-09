@echo off
chcp 65001 >nul
cd /d "%~dp0.."
cd cmake-build-debug-ninja-vs

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
"$exePath = Join-Path (Get-Location) 'CandyLauncher.exe'; Start-Process -FilePath $exePath"
