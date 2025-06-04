cmake_minimum_required(VERSION 3.31)
project(WindowsProject1)

#set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD 17)  # 或者选择其他 C++ 标准，如 14、20
include_directories(.)

add_executable(WindowsProject1
        ActionBase.cpp
        framework.h
        json.hpp
        ListedRunnerPlugin.cpp
        ListedRunnerPlugin.h
        Resource.h
        RunCommandAction.cpp
        targetver.h
        WindowsProject1.cpp
        WindowsProject1.h
        PinyinHelper.h
        ListViewManager.cpp
        ListViewManager.h
        MainTools.hpp
        GlassHelper.hpp
        TrayMenuManager.h
        TrayMenuManager.cpp
        RefreshAction.cpp
        DataKeeper.hpp
        EditHelper.hpp

)

add_definitions(-DUNICODE -D_UNICODE)

# 设置 GUI 子系统（只有当你定义了 WinMain 时才这样设置）
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -municode -mwindows")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
# 检查是否是 Visual Studio 2022
if (MSVC AND CMAKE_GENERATOR MATCHES "Visual Studio 17")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # 检查是否是 MinGW
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 10.0)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -municode -mwindows")
    endif()
endif()

# 关闭优化
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")

set(CMAKE_GENERATOR_PLATFORM x64)  # 或者 x86，依据需要设置
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

# 链接 GDI+ 和 Windows 控件库
# 链接 Windows 所需库
target_link_libraries(WindowsProject1
        gdiplus
        comctl32
        kernel32
        user32
        gdi32
        winspool
        shell32
        ole32
        oleaut32
        uuid
        comdlg32
        advapi32
        shlwapi
)

#include_directories("E:/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.42.34433/atlmfc/include")

