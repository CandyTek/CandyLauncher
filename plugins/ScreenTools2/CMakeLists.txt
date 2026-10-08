cmake_minimum_required(VERSION 3.16)
project(ScreenToolsPlugin2 LANGUAGES C CXX)

set(WIN_ICONV_DIR ${CMAKE_SOURCE_DIR}/3rdparty/win-iconv)
add_library(screen_tools_iconv STATIC ${WIN_ICONV_DIR}/win_iconv.c)
target_include_directories(screen_tools_iconv PUBLIC ${WIN_ICONV_DIR})

set(ZBAR_DIR ${CMAKE_SOURCE_DIR}/3rdparty/zbar-windows)
file(READ ${ZBAR_DIR}/config-internal/config-win.h ZBAR_WINDOWS_CONFIG)
string(REPLACE "#define HAVE_UNISTD_H 1" "#undef HAVE_UNISTD_H" ZBAR_WINDOWS_CONFIG "${ZBAR_WINDOWS_CONFIG}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/config-win.h "${ZBAR_WINDOWS_CONFIG}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/config.h "#include \"config-win.h\"\n")
file(GLOB ZBAR_DECODER_SOURCES ${ZBAR_DIR}/zbar/decoder/*.c)
file(GLOB ZBAR_QR_SOURCES ${ZBAR_DIR}/zbar/qrcode/*.c)
# This zbar revision can move an unavailable iconv descriptor into the current
# slot while reordering QR text encodings. Skip it before calling iconv().
file(READ ${ZBAR_DIR}/zbar/qrcode/qrdectxt.c ZBAR_QR_TEXT_SOURCE)
set(ZBAR_QR_ICONV_CALL "                                            err = iconv(enc_list[ei], &in,")
string(FIND "${ZBAR_QR_TEXT_SOURCE}" "${ZBAR_QR_ICONV_CALL}" ZBAR_QR_ICONV_POSITION)
if(ZBAR_QR_ICONV_POSITION EQUAL -1)
	message(FATAL_ERROR "The zbar QR text decoder changed; review the iconv patch")
endif()
string(REPLACE "${ZBAR_QR_ICONV_CALL}"
	"                                            if (enc_list[ei] == (iconv_t)-1)\n                                                continue;\n${ZBAR_QR_ICONV_CALL}"
	ZBAR_QR_TEXT_SOURCE "${ZBAR_QR_TEXT_SOURCE}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/qrdectxt.c "${ZBAR_QR_TEXT_SOURCE}")
list(REMOVE_ITEM ZBAR_QR_SOURCES ${ZBAR_DIR}/zbar/qrcode/qrdectxt.c)
list(APPEND ZBAR_QR_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/qrdectxt.c)
add_library(screen_tools_zbar STATIC
	${ZBAR_DIR}/zbar/config.c
	${ZBAR_DIR}/zbar/decoder.c
	${ZBAR_DIR}/zbar/error.c
	${ZBAR_DIR}/zbar/image.c
	${ZBAR_DIR}/zbar/img_scanner.c
	${ZBAR_DIR}/zbar/misc.c
	${ZBAR_DIR}/zbar/refcnt.c
	${ZBAR_DIR}/zbar/scanner.c
	${ZBAR_DIR}/zbar/sqcode.c
	${ZBAR_DIR}/zbar/symbol.c
	${ZBAR_DECODER_SOURCES}
	${ZBAR_QR_SOURCES})
target_compile_definitions(screen_tools_zbar PRIVATE HAVE_CONFIG_H _CRT_SECURE_NO_WARNINGS)
target_include_directories(screen_tools_zbar PRIVATE
	${CMAKE_CURRENT_BINARY_DIR} ${ZBAR_DIR}/zbar ${ZBAR_DIR}/zbar/decoder ${ZBAR_DIR}/zbar/qrcode)
target_include_directories(screen_tools_zbar PUBLIC ${ZBAR_DIR}/include)
target_link_libraries(screen_tools_zbar PRIVATE screen_tools_iconv winmm)

add_library(screen_tools_qr STATIC QrDecoder.cpp)
target_link_libraries(screen_tools_qr PUBLIC screen_tools_zbar)

add_library(${PROJECT_NAME} SHARED ScreenToolsPlugin.cpp)
include(${CMAKE_SOURCE_DIR}/plugins/PluginCommon.cmake)
target_link_libraries(${PROJECT_NAME} PRIVATE screen_tools_qr gdi32 user32 shell32 gdiplus)
