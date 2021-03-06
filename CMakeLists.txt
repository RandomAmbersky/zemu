cmake_minimum_required (VERSION 3.7)
project (ZEmu)

if (NOT USE_SDL1)
    # CMake should be able to find SDL2 without this module, but I'm not sure what about Windows.
    # So this module is used mainly for compatibility with old-way and for Windows.
    list (APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake-sdl2)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    find_program (RUBY_COMMAND "ruby")

    if (RUBY_COMMAND)
        message ("Ruby found")
    else ()
        message (FATAL_ERROR "Ruby not found (required to patch executable for Mojave when SDL1 is used)")
    endif ()
endif ()

if (USE_ANALYZE)
    find_program (PVS_STUDIO_ANALYZER_COMMAND "pvs-studio-analyzer")
    find_program (PLOG_CONVERTER_COMMAND "plog-converter")

    if (PVS_STUDIO_ANALYZER_COMMAND AND PLOG_CONVERTER_COMMAND)
        message ("PVS-Studio files found")
    else ()
        message (FATAL_ERROR "PVS-Studio files not found (required to perform static analysis)")
    endif ()

    set (CMAKE_EXPORT_COMPILE_COMMANDS On)
elseif (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release)
endif ()

if (NOT USE_DEFAULT_FIND_FRAMEWORK)
    # Fix to be able to compile on macOS Mojave
    set (CMAKE_FIND_FRAMEWORK LAST)
endif ()

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")

if (UNIX AND (NOT APPLE) AND CMAKE_COMPILER_IS_GNUCC)
    # For Boost.Process
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif ()

set (ZEMU_VERSION_MAJOR 1)
set (ZEMU_VERSION_MINOR 0)
set (CMAKE_INSTALL_PREFIX /opt)

include (TestBigEndian)
test_big_endian (BIG_ENDIAN)

configure_file (
    "${PROJECT_SOURCE_DIR}/ZEmuConfig.h.in"
    "${PROJECT_BINARY_DIR}/ZEmuConfig.h"
)

include_directories (${PROJECT_BINARY_DIR})

option (USE_Z80EX "Use external Z80EX library for z80 emulation" Off)
set (ZAME_Z80_PATH "${PROJECT_SOURCE_DIR}/zame_z80" CACHE PATH "Path to zame z80 library")
set (Z80EX_PATH "${PROJECT_SOURCE_DIR}/../z80ex" CACHE PATH "Path to Z80Ex library")

if (USE_Z80EX)
    include_directories ("${Z80EX_PATH}/include")
    set (Z80EX_BINARY_DIR "${PROJECT_BINARY_DIR}/lib_z80ex")
    make_directory (${Z80EX_BINARY_DIR})
    set (Z80EX_STATIC_ONLY true)
    add_subdirectory (${Z80EX_PATH} ${Z80EX_BINARY_DIR})
    link_directories (${Z80EX_BINARY_DIR})
else ()
    set (ZAME_Z80_BINARY_DIR "${PROJECT_BINARY_DIR}/lib_zame_z80")
    set (ZAME_SRC_OUTPUT_DIR "${ZAME_Z80_BINARY_DIR}-src")
    include_directories (${ZAME_SRC_OUTPUT_DIR})
    add_subdirectory (${ZAME_Z80_PATH} ${ZAME_Z80_BINARY_DIR})
    link_directories (${ZAME_Z80_BINARY_DIR})
endif ()

find_package (Boost REQUIRED COMPONENTS filesystem)
include_directories (${Boost_INCLUDE_DIR})

if (USE_SDL1)
    if (WIN32 AND NOT CMAKE_COMPILER_IS_GNUCC)
        if (NOT ENV{SDLDIR})
            set (ENV{SDLDIR} "${PROJECT_BINARY_DIR}/SDL")
            message ("ENV{SDLDIR}=$ENV{SDLDIR}")
        endif ()
    endif ()

    find_package (SDL REQUIRED)

    if (SDL_FOUND)
        message ("SDL found")
        include_directories (${SDL_INCLUDE_DIR})
    else ()
        message (FATAL_ERROR "SDL not found")
    endif ()
else ()
    if (WIN32 AND NOT CMAKE_COMPILER_IS_GNUCC)
        if (NOT ENV{SDL2_DIR})
            set (ENV{SDL2_DIR} "${PROJECT_BINARY_DIR}/SDL2")
            message ("ENV{SDL2_DIR}=$ENV{SDL2_DIR}")
        endif ()
    endif ()

    find_package (SDL2 REQUIRED)

    if (SDL2_FOUND)
        message ("SDL2 found")
        include_directories (${SDL2_INCLUDE_DIR})
    else ()
        message (FATAL_ERROR "SDL2 not found")
    endif ()
endif ()

include_directories ("${PROJECT_SOURCE_DIR}/src")

file (GLOB_RECURSE SOURCES
    src/*.cpp
    src/*.h
)

if (WIN32)
    if (CMAKE_COMPILER_IS_GNUCC)
        set (CMAKE_RC_COMPILER_INIT windres)
        enable_language (RC)
        set (CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff <FLAGS> <DEFINES> <SOURCE> <OBJECT>")
    endif ()

    set (SOURCES ${SOURCES} "${PROJECT_SOURCE_DIR}/src/windows/zemu.rc")
endif ()

add_executable (zemu WIN32 ${SOURCES})

if (USE_Z80EX)
    target_link_libraries (zemu z80ex-static z80ex_dasm-static)
else ()
    target_link_libraries (zemu z80ex_wrapper)
endif ()

target_link_libraries (zemu ${Boost_LIBRARIES})

if (USE_SDL1)
    target_link_libraries (zemu ${SDL_LIBRARY})
else ()
    target_link_libraries (zemu ${SDL2_LIBRARY})
endif ()

if (WIN32)
    target_link_libraries (zemu winmm shlwapi)
endif ()

if (USE_SDL1 AND ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_custom_command (
        TARGET zemu
        POST_BUILD
        COMMAND ${RUBY_COMMAND} "${CMAKE_SOURCE_DIR}/osx-mojave-fix.rb" "${CMAKE_BINARY_DIR}/zemu"
    )
endif ()

if (USE_ANALYZE)
    add_custom_command (
        TARGET zemu
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/analysis"
    )

    add_custom_command (
        TARGET zemu
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/analysis"
    )

    add_custom_command (
        TARGET zemu
        POST_BUILD
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMAND ${PVS_STUDIO_ANALYZER_COMMAND} analyze -o analysis.log -i -j 2 -f compile_commands.json
        VERBATIM
    )

    add_custom_command (
        TARGET zemu
        POST_BUILD
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMAND ${PLOG_CONVERTER_COMMAND} -a "GA;64;OP;CS;MISRA" -t fullhtml -o analysis analysis.log
        VERBATIM
    )
endif ()

# Visual Studio file grouping

source_group (zemu src/*)
source_group (devices src/devices/*)
source_group (devices/border src/devices/border/*)
source_group (devices/covox src/devices/covox/*)
source_group (devices/extport src/devices/extport/*)
source_group (devices/gsound src/devices/gsound/*)
source_group (devices/joystick src/devices/joystick/*)
source_group (devices/keyboard src/devices/keyboard/*)
source_group (devices/mmanager src/devices/mmanager/*)
source_group (devices/mouse src/devices/mouse/*)
source_group (devices/trdos src/devices/trdos/*)
source_group (devices/tsfm src/devices/tsfm/*)
source_group (images src/images/*)
source_group (lib_ay src/lib_ay/*)
source_group (lib_saa1099 src/lib_saa1099/*)
source_group (lib_simpleini src/lib_simpleini/*)
source_group (lib_wd1793 src/lib_wd1793/*)
source_group (lib_ym2203 src/lib_ym2203/*)
source_group (renderer src/renderer/*)
source_group (sound src/sound/*)
source_group (tape src/tape/*)
source_group (windows src/windows/*)

# Install

INSTALL_PROGRAMS (/zemu FILES ./zemu)
INSTALL_FILES (/zemu/arc FILES ./extras/arc/)
INSTALL_FILES (/zemu/boot FILES ./extras/boot/)
INSTALL_FILES (/zemu/roms FILES ./extras/roms/)

INSTALL_FILES (/zemu FILES
    ./LICENSE
    ./CHANGELOG.md
    ./README.md
    ./extras/keys.config
    ./extras/zemu.ini
    ./src/lib_saa1099/LICENCE-SAA1099
    ./src/lib_simpleini/LICENSE-CONVERTUTF
    ./src/lib_simpleini/LICENSE-SIMPLEINI
    ./src/lib_ym2203/LICENSE-YM2203
)
