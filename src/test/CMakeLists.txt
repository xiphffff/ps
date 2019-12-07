# Copyright 2019 Michael Rodriguez
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

set(SRCS directx.cpp main.cpp main_window.cpp pstest.cpp)
set(HDRS directx.h main_window.h pstest.h)

set(FRAMEWORK_SRCS framework/file_dialog.cpp
                   framework/hex_editor.cpp
                   framework/ini_file.cpp
                   framework/menu.cpp
                   framework/menu_item.cpp
                   framework/window.cpp)

set(FRAMEWORK_HDRS framework/file_dialog.h
                   framework/hex_editor.h
                   framework/ini_file.h
                   framework/menu.h
                   framework/menu_item.h
                   framework/window.h)

source_group("Source Files\\framework" FILES ${FRAMEWORK_SRCS})
source_group("Header Files\\framework" FILES ${FRAMEWORK_HDRS})

add_executable(pstest WIN32 ${SRCS} ${HDRS} ${FRAMEWORK_SRCS} ${FRAMEWORK_HDRS})

set_target_properties(pstest PROPERTIES
                      CXX_STANDARD 17
                      CXX_STANDARD_REQUIRED YES
                      CXX_EXTENSIONS ON)

target_compile_options(pstest PRIVATE
                       $<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:GNU>>:
                       -Wall -Wextra -Wno-gnu-case-range -Wno-old-style-cast -Wno-c++98-compat -Wno-c++98-compat-pedantic>)

target_compile_definitions(pstest PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN UNICODE OEMRESOURCE)
target_link_libraries(pstest ps)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT pstest)