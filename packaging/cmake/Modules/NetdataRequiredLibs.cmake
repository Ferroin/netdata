# Functions for handling our unconditionally required libraries.
#
# Copyright (c) 2024 Netdata Inc.
# SPDX-License-Identifier: GPL-3.0-or-later

include(CheckCSourceCompiles)
include(CheckFunctionExists)

macro(_handle_libm)
        list(APPEND CMAKE_REQUIRED_LIBRARIES m)
        check_c_source_compiles("
        #include <math.h>

        int main() {
            double x = 1.0;
            log10(1.0);
            return(0);
        }
        " HAVE_LOG10)

        if(HAVE_LOG10)
                set(LINK_LIBM True CACHE BOOL "" FORCE)
        else()
                list(REMOVE_ITEM CMAKE_REQUIRED_LIBRARIES m)
                unset(HAVE_LOG10)
                check_function_exists(log10 HAVE_LOG10)

                if(NOT HAVE_LOG10)
                        message(FATAL_ERROR "Math functions are not available with or without libm.")
                endif()
        endif()
endmacro()

macro(netdata_check_required_libraries)
        _handle_libm()

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
