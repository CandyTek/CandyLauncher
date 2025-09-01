@echo off
call "E:\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
cmake --build cmake-build-debug-ninja-vs --config Debug --target WindowsProject1 -j 18
