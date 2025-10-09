cmake_minimum_required(VERSION 3.16)
project(RegistryPlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 插件源文件
set(PLUGIN_SOURCES
	RegistryPlugin.cpp
	../Plugin.hpp
	RegistryAction.hpp
	RegistryPluginData.hpp
	RegistryEntry.hpp
	RegistryUtil.hpp
)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

# 引用公共 CMake 模块
include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)

# 链接 Windows 库
target_link_libraries(${PROJECT_NAME} PRIVATE advapi32)
