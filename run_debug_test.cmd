@echo off
call "E:\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
ctest -C Debug --test-dir cmake-build-debug-ninja-vs
