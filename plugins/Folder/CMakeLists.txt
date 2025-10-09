cmake_minimum_required(VERSION 3.16)
project(FolderPlugin)

# 插件源文件
set(PLUGIN_SOURCES
		FolderPlugin.cpp
		RunnerConfigUtils.hpp
		FileAction.hpp
		../Plugin.hpp
		FolderPluginData.hpp
		FileSystemTraverserWrapper.hpp
		ShortcutRepairTool.hpp
		IndexManager.hpp
		UWPAppsTraverserWrapper.hpp
)
add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

# 引用公共 CMake 模块
include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)

# 链接 Shlwapi 库（PathFileExistsW 等 API）
target_link_libraries(${PROJECT_NAME} PRIVATE Shlwapi)
