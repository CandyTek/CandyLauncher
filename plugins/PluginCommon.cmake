# cmake/PluginCommon.cmake
# 用于统一插件构建设置

# 要求 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录（确保插件都输出到同一个 plugins 文件夹）
set_target_properties(${PROJECT_NAME} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
		ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

# Windows 特定定义
if (WIN32)
	target_compile_definitions(${PROJECT_NAME} PRIVATE BUILDING_PLUGIN_DLL)
#	target_compile_options(${PROJECT_NAME} PRIVATE /wd4251 /wd4275 /bigobj)
endif ()

# Debug 定义
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_definitions(${PROJECT_NAME} PRIVATE _DEBUG)
endif ()

# 清单文件（必需）：<DLL 名>.plugin.json 放在 DLL 旁边，主程序不加载 DLL 也能读到元数据、默认设置和
# 各语言字符串（docs/DOC_PLUGIN_I18N.md）
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.plugin.json")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.plugin.json"
			"$<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.plugin.json"
	)
else ()
	message(WARNING "${PROJECT_NAME}: missing ${PROJECT_NAME}.plugin.json (plugin manifest is required)")
endif ()

# 可选的语言文件：插件源码目录下的 lang/<语言代码>.json 复制到 plugins/lang/<DLL 名>/，
# 优先级高于清单里的 strings，适合翻译者单独维护的大表
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lang")
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_directory
			"${CMAKE_CURRENT_SOURCE_DIR}/lang"
			"$<TARGET_FILE_DIR:${PROJECT_NAME}>/lang/$<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>"
	)
endif ()
