@echo off
chcp 65001 >nul

cd cmake-build-debug-ninja-vs

@REM :: 删除旧日志
del output_log.txt 2>nul

@REM :: 启动 CandyLauncher.exe，3 秒后强制结束
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
"$exePath = Join-Path (Get-Location) 'CandyLauncher.exe'; $logPath = Join-Path (Get-Location) 'output_log.txt'; $p = Start-Process -FilePath $exePath -RedirectStandardOutput $logPath -NoNewWindow -PassThru; Start-Sleep 3; try { Stop-Process -Id $p.Id -Force } catch {}; Wait-Process -Id $p.Id -ErrorAction SilentlyContinue; if (Test-Path $logPath) { Get-Content $logPath -Encoding UTF8}"

@REM echo ================== CandyLauncher 输出 ==================
@REM type output_log.txt
@REM echo ========================================================
