# Copyright 2020 Michael Rodriguez
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
# OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

find_package(Qt5 COMPONENTS Widgets REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (CMAKE_VERSION VERSION_LESS "3.7.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

set(SRCS emulator.cpp main.cpp main_window.cpp pstest.cpp)
set(HDRS emulator.h main_window.h pstest.h)

set(DEBUG_SRCS debug/bios_calls.cpp debug/gpu.cpp debug/tty_log.cpp)
set(DEBUG_HDRS debug/bios_calls.h debug/gpu.h debug/tty_log.h)

source_group("Source Files\\debug" FILES ${DEBUG_SRCS})
source_group("Header Files\\debug" FILES ${DEBUG_HDRS})

add_executable(pstest ${SRCS} ${HDRS} ${DEBUG_SRCS} ${DEBUG_HDRS})

set_target_properties(pstest PROPERTIES
                      CXX_STANDARD 17
                      CXX_STANDARD_REQUIRED YES
                      CXX_EXTENSIONS ON)

target_link_libraries(pstest ps Qt5::Widgets)

target_compile_options(pstest PRIVATE
                       $<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:
                       -Wall -Wextra -Wno-gnu-case-range -Wno-old-style-cast -Wno-c++98-compat>)

target_compile_definitions(pstest PRIVATE LIBPS_DEBUG)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT pstest)