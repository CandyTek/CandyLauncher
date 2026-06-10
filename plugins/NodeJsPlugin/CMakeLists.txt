cmake_minimum_required(VERSION 3.16)
project(NodeJsPlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 插件源文件
set(PLUGIN_SOURCES
    NodeJsAction.hpp
    NodeJsBridge.hpp
    NodeJsScriptManager.hpp
    NodeJsPluginData.hpp
    NodeJsPlugin.cpp
    ../Plugin.hpp
)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

# 引用公共 CMake 模块
include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)

# 复制 Node.js 宿主程序到输出目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/NodeJsPlugin"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/node-host.js"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/NodeJsPlugin"

        VERBATIM
)

# scripts 目录存在时才复制
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/scripts")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/NodeJsPlugin/scripts"
            VERBATIM
    )
endif()
