cmake_minimum_required(VERSION 3.16)
project(TenkaiDanmaku VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC OFF)

# ── Qt6 ──────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Widgets WebSockets Network)

# ── libqrencode ──────────────────────────────────────────────────────────
# Option 1: system-installed (vcpkg / brew / apt)
# Option 2: set -DQRENCODE_DIR=... to point at libqrencode source tree
set(QRENCODE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/libqrencode-4.1.1"
    CACHE PATH "Path to libqrencode source directory")

if(EXISTS "${QRENCODE_DIR}/qrencode.c")
    message(STATUS "Using bundled libqrencode at ${QRENCODE_DIR}")
    add_library(qrencode STATIC
        ${QRENCODE_DIR}/qrencode.c
        ${QRENCODE_DIR}/qrinput.c
        ${QRENCODE_DIR}/qrspec.c
        ${QRENCODE_DIR}/mqrspec.c
        ${QRENCODE_DIR}/rsecc.c
        ${QRENCODE_DIR}/bitstream.c
        ${QRENCODE_DIR}/mask.c
        ${QRENCODE_DIR}/mmask.c
        ${QRENCODE_DIR}/split.c
    )
    set_target_properties(qrencode PROPERTIES C_STANDARD 99)
    target_include_directories(qrencode PUBLIC ${QRENCODE_DIR})
    target_compile_definitions(qrencode PRIVATE
        MAJOR_VERSION=4 MINOR_VERSION=1 MICRO_VERSION=1
        VERSION="4.1.1"
        HAVE_SDL=0
        $<$<CONFIG:Release>:STATIC_IN_RELEASE=static>
        $<$<CONFIG:Debug>:STATIC_IN_RELEASE=>
    )
    if(MSVC)
        target_compile_definitions(qrencode PRIVATE
            strcasecmp=_stricmp
            strncasecmp=_strnicmp
            _CRT_SECURE_NO_WARNINGS
            _CRT_NONSTDC_NO_DEPRECATE
        )
    endif()
else()
    find_library(QRENCODE_LIB qrencode libqrencode REQUIRED)
    find_path(QRENCODE_INCLUDE_DIR qrencode.h REQUIRED)
    add_library(qrencode SHARED IMPORTED)
    set_target_properties(qrencode PROPERTIES
        IMPORTED_LOCATION "${QRENCODE_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${QRENCODE_INCLUDE_DIR}"
    )
endif()

# ── Source files ─────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/Logger.cpp
    src/NetworkHelper.cpp
    src/QRCodeGenerator.cpp
    src/SimpleHttpServer.cpp
    src/WsServer.cpp
    src/DanmakuWindow.cpp
    src/ControlPanel.cpp
    src/ConfigDialog.cpp
    src/TrayManager.cpp
)

set(HEADERS
    src/Logger.h
    src/NetworkHelper.h
    src/QRCodeGenerator.h
    src/SimpleHttpServer.h
    src/WsServer.h
    src/DanmakuWindow.h
    src/ControlPanel.h
    src/ConfigDialog.h
    src/TrayManager.h
)

# ── Executable ───────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::WebSockets
    Qt6::Network
    qrencode
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

# MSVC specific
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# MinGW specific
if(MINGW)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
