# cmake/PluginCommon.cmake
# 用于统一插件构建设置

# 要求 C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录（确保插件都输出到同一个 plugins 文件夹）
set_target_properties(${PROJECT_NAME} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/plugins"
		RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/plugins"
		LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/plugins"
		LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/plugins"
		ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/plugins"
		ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/plugins"
)

# Windows 特定定义
if (WIN32)
	target_compile_definitions(${PROJECT_NAME} PRIVATE BUILDING_PLUGIN_DLL)
	target_compile_options(${PROJECT_NAME} PRIVATE /wd4251 /wd4275 /bigobj)
	target_link_libraries(${PROJECT_NAME} PRIVATE Shlwapi)
endif ()

# Debug 定义
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_definitions(${PROJECT_NAME} PRIVATE _DEBUG)
endif ()
