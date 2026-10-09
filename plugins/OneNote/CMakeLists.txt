cmake_minimum_required(VERSION 3.16)
project(OneNotePlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 插件源文件
set(PLUGIN_SOURCES
		OneNotePlugin.cpp
		../Plugin.hpp
		OneNoteAction.hpp
		OneNotePluginData.hpp
		OneNoteUtil.hpp
		FindOneNoteUtil.hpp
)

# Generate the OneNote type library header once instead of using #import in the source,
# because #import rewrites the .tlh on every compile and the DLL would always be rebuilt.
set(ONENOTE_TLH_DIR "${CMAKE_CURRENT_BINARY_DIR}/typelib")
set(ONENOTE_TLH "${ONENOTE_TLH_DIR}/OneNoteLib.tlh")
add_custom_command(
		OUTPUT "${ONENOTE_TLH}"
		COMMAND ${CMAKE_COMMAND}
		-DCOMPILER=${CMAKE_CXX_COMPILER}
		-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/OneNoteTypeLib.cpp
		-DWORK_DIR=${ONENOTE_TLH_DIR}/work
		-DOUT=${ONENOTE_TLH}
		-P "${CMAKE_CURRENT_SOURCE_DIR}/GenerateTypeLib.cmake"
		DEPENDS OneNoteTypeLib.cpp GenerateTypeLib.cmake
		COMMENT "Generating OneNoteLib.tlh"
		VERBATIM
)
list(APPEND PLUGIN_SOURCES "${ONENOTE_TLH}")

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE "${ONENOTE_TLH_DIR}")

# 引用公共 CMake 模块
include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)

# 链接 COM 和 XML 库
target_link_libraries(${PROJECT_NAME} PRIVATE ole32 oleaut32 msxml6)
