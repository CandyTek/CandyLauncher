@echo off
chcp 65001 >nul
cd /d "%~dp0"

run_build.cmd && cd /d "%~dp0" && run_candylauncher.cmd
