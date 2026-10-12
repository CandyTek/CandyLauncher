cmake_minimum_required(VERSION 3.16)
project(ScreenToolsPlugin LANGUAGES CXX)

# ZXing QR-only static library (shared with tests/ScreenToolsZxingQrTest).
include(${CMAKE_CURRENT_SOURCE_DIR}/ZxingQr.cmake)

add_library(${PROJECT_NAME} SHARED ScreenToolsPlugin.cpp QrDecoder.cpp)
include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)
target_link_libraries(${PROJECT_NAME} PRIVATE screen_tools_zxing_qr gdi32 user32 shell32 gdiplus
	libcmark-gfm-extensions_static libcmark-gfm_static)
target_compile_definitions(${PROJECT_NAME} PRIVATE CMARK_GFM_STATIC_DEFINE CMARK_GFM_EXTENSIONS_STATIC_DEFINE)
target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_SOURCE_DIR}/3rdparty/cmark-gfm/src
	${CMAKE_SOURCE_DIR}/3rdparty/cmark-gfm/extensions
	${CMAKE_BINARY_DIR}/3rdparty/cmark-gfm/src
	${CMAKE_BINARY_DIR}/3rdparty/cmark-gfm/extensions)
