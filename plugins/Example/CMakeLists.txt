cmake_minimum_required(VERSION 3.16)
project(ExamplePlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 插件源文件
set(PLUGIN_SOURCES
        ExamplePlugin.hpp
        ExampleAction.hpp
        ExamplePlugin.cpp
        ../Plugin.h
)

# 创建动态库
add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

# 设置输出目录
#set_target_properties(${PROJECT_NAME} PROPERTIES
#    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
#    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
#    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
#)

# 设置输出目录到构建目录的 plugins 子文件夹
set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

# Windows特定设置
if(WIN32)
    # 设置DLL导出
    target_compile_definitions(${PROJECT_NAME} PRIVATE BUILDING_PLUGIN_DLL)

    # 禁用一些警告
    target_compile_options(${PROJECT_NAME} PRIVATE /wd4251 /wd4275)
endif()

# 设置调试信息
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE _DEBUG)
endif()
