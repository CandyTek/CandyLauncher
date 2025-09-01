@echo off
call "E:\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
cmake -S . -B cmake-build-debug-ninja-vs -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
