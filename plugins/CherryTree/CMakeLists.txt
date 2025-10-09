cmake_minimum_required(VERSION 3.16)
project(CherryTreePlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 插件源文件
set(PLUGIN_SOURCES
		CherryTreePlugin.cpp
		../Plugin.hpp
		CherryTreeAction.hpp
		CherryTreePluginData.hpp
)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

# 引用公共 CMake 模块
include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)

# 包含 SQLite3 头文件路径
target_include_directories(${PROJECT_NAME} PRIVATE
		${CMAKE_SOURCE_DIR}/3rdparty/sqlite-amalgamation
)

# 链接 SQLite3 库
target_link_libraries(${PROJECT_NAME} PRIVATE SQLite3)
