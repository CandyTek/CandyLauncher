# Standalone project, not included by the root CMakeLists.txt. Works with MSVC, MinGW GCC or clang.
#   cmake -S plugins/ExampleC -B plugins/ExampleC/build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build plugins/ExampleC/build
# Copy build/ExampleCPlugin.dll into the "plugins" folder next to CandyLauncher.exe.
# The DLL must have the same bitness as CandyLauncher.exe (the default debug build is x86).
cmake_minimum_required(VERSION 3.16)
project(ExampleCPlugin C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(ExampleCPlugin SHARED ExampleCPlugin.c)
# Only the C ABI header is needed: plugins/sdk/CandyPluginApi.h
target_include_directories(ExampleCPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_compile_definitions(ExampleCPlugin PRIVATE UNICODE _UNICODE)
set_target_properties(ExampleCPlugin PROPERTIES PREFIX "")

if (MSVC)
	target_compile_options(ExampleCPlugin PRIVATE /utf-8)
else ()
	# No dependency on libgcc DLLs
	target_link_options(ExampleCPlugin PRIVATE -static-libgcc)
endif ()
