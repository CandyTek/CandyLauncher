cmake_minimum_required(VERSION 3.16)
project(FolderPlugin)

# 插件源文件
set(PLUGIN_SOURCES
		FolderPlugin.cpp
		FolderPluginConfigUtils.hpp
		FileAction.hpp
		../Plugin.hpp
		FolderPluginData.hpp
		FileSystemTraverserWrapper.hpp
		ShortcutRepairTool.hpp
		IndexManagerState.hpp
		IndexManagerLogic.hpp
		IndexManagerWindow.hpp
		UWPAppsTraverserWrapper.hpp
		../../util/MainTools.hpp
		../../util/BitmapUtil.hpp
		../../util/FileSystemTraverser.hpp
		../../util/FileUtil.hpp
		ContextMenuHelper.hpp
)
add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

# 引用公共 CMake 模块
include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)

# 链接 Shlwapi 库（PathFileExistsW 等 API）
target_link_libraries(${PROJECT_NAME} PRIVATE Shlwapi)

# 每次都初始化构建程序的用户配置
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND $<$<NOT:$<CONFIG:Release>>:${CMAKE_COMMAND}>
		$<$<NOT:$<CONFIG:Release>>:-E>
		$<$<NOT:$<CONFIG:Release>>:copy_if_different>
		$<$<NOT:$<CONFIG:Release>>:${CMAKE_SOURCE_DIR}/plugins/Folder/config_folder_plugin.json>
		$<$<NOT:$<CONFIG:Release>>:$<TARGET_FILE_DIR:${PROJECT_NAME}>>
		COMMAND_EXPAND_LISTS
		VERBATIM
)
