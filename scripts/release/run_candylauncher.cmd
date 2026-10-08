@echo off
chcp 65001 >nul
cd /d "%~dp0..\.."

for /f "tokens=3 delims=," %%I in ('wmic process where "name='CandyLauncher.exe'" get ExecutablePath^,ProcessId /format:csv 2^>nul ^| findstr /i "cmake-build-release-ninja-vs"') do (
    for /f "delims=" %%A in ("%%I") do (
        taskkill /F /PID %%A /T
    )
)

cd cmake-build-release-ninja-vs

@REM :: 删除旧日志
del output_log.txt 2>nul

@REM :: 启动 CandyLauncher.exe，3 秒后强制结束
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
"$exePath = Join-Path (Get-Location) 'CandyLauncher.exe'; $logPath = Join-Path (Get-Location) 'output_log.txt'; $p = Start-Process -FilePath $exePath -RedirectStandardOutput $logPath -NoNewWindow -PassThru; Start-Sleep 3; try { Stop-Process -Id $p.Id -Force } catch {}; Wait-Process -Id $p.Id -ErrorAction SilentlyContinue; if (Test-Path $logPath) { Get-Content $logPath -Encoding UTF8}"

cd ..
