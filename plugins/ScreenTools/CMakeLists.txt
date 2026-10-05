cmake_minimum_required(VERSION 3.16)
project(ScreenToolsPlugin LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(zxing_cpp
	GIT_REPOSITORY https://github.com/zxing-cpp/zxing-cpp.git
	GIT_TAG d6068bcebeb8fd9f0d35a99b00d202be86a14dbe
	GIT_SHALLOW FALSE)
FetchContent_GetProperties(zxing_cpp)
if(NOT zxing_cpp_POPULATED)
	FetchContent_MakeAvailable(zxing_cpp)
endif()

# ZXING_READERS builds every format. This plugin needs only the QR decoder.
set(ZXING_SRC ${zxing_cpp_SOURCE_DIR}/core/src)
add_library(screen_tools_zxing_qr STATIC
	${ZXING_SRC}/Barcode.cpp
	${ZXING_SRC}/BarcodeFormat.cpp
	${ZXING_SRC}/BinaryBitmap.cpp
	${ZXING_SRC}/BitArray.cpp
	${ZXING_SRC}/BitMatrix.cpp
	${ZXING_SRC}/BitSource.cpp
	${ZXING_SRC}/CharacterSet.cpp
	${ZXING_SRC}/ConcentricFinder.cpp
	${ZXING_SRC}/Content.cpp
	${ZXING_SRC}/ECI.cpp
	${ZXING_SRC}/Error.cpp
	${ZXING_SRC}/GenericGF.cpp
	${ZXING_SRC}/GenericGFPoly.cpp
	${ZXING_SRC}/GlobalHistogramBinarizer.cpp
	${ZXING_SRC}/GridSampler.cpp
	${ZXING_SRC}/HRI.cpp
	${ZXING_SRC}/HybridBinarizer.cpp
	${ZXING_SRC}/PerspectiveTransform.cpp
	${ZXING_SRC}/ReedSolomonDecoder.cpp
	${ZXING_SRC}/ResultPoint.cpp
	${ZXING_SRC}/TextDecoder.cpp
	${ZXING_SRC}/Utf.cpp
	${ZXING_SRC}/qrcode/QRBitMatrixParser.cpp
	${ZXING_SRC}/qrcode/QRCodecMode.cpp
	${ZXING_SRC}/qrcode/QRDataBlock.cpp
	${ZXING_SRC}/qrcode/QRDecoder.cpp
	${ZXING_SRC}/qrcode/QRDetector.cpp
	${ZXING_SRC}/qrcode/QRErrorCorrectionLevel.cpp
	${ZXING_SRC}/qrcode/QRFormatInformation.cpp
	${ZXING_SRC}/qrcode/QRReader.cpp
	${ZXING_SRC}/qrcode/QRVersion.cpp
	${ZXING_SRC}/libzueci/zueci.c)
set_source_files_properties(${ZXING_SRC}/libzueci/zueci.c PROPERTIES COMPILE_DEFINITIONS ZUECI_EMBED_NO_TO_ECI)
target_include_directories(screen_tools_zxing_qr PUBLIC ${ZXING_SRC})
target_compile_definitions(screen_tools_zxing_qr PRIVATE ZXING_READERS)
target_compile_features(screen_tools_zxing_qr PUBLIC cxx_std_17)

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
