# Functions for handling our unconditionally required libraries.
#
# Copyright (c) 2024 Netdata Inc.
# SPDX-License-Identifier: GPL-3.0-or-later

macro(netdata_check_required_libraries)
        set(NETDATA_REQUIRED_LIBS ZLIB;LIBUV)

        if(MACOS)
                find_package(ZLIB)
                set(ZLIB_LDFLAGS "ZLIB::ZLIB")
        else()
                pkg_check_modules(ZLIB REQUIRED zlib)
                pkg_check_modules(UUID REQUIRED uuid)
                list(APPEND NETDATA_REQUIRED_LIBS UUID)
        endif()

        pkg_check_modules(LIBUV REQUIRED libuv)
endmacro()

macro(netdata_add_required_libraries)
        foreach(L LISTS NETDATA_REQUIRED_LIBS)
                target_include_directories(libnetdata BEFORE PUBLIC ${${L}_INCLUDE_DIRS})
                target_compile_definitions(libnetdata PUBLIC ${${L}_CFLAGS_OTHER})
                target_link_libraries(libnetdata PUBLIC ${${L}_LDFLAGS})
        endforeach()
endmacro()
