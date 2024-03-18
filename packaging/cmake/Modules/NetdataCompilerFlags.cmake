# Functions for handling of extra compiler flags.
#
# Copyright (c) 2024 Netdata Inc.
# SPDX-License-Identifier: GPL-3.0-or-later

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

# Construct a pre-processor safe name
#
# This takes a specified value, and assigns the generated name to the
# specified target.
function(netdata_make_cpp_safe_name value target)
        string(REPLACE "-" "_" tmp "${value}")
        string(REPLACE "=" "_" tmp "${tmp}")
        set(${target} "${tmp}" PARENT_SCOPE)
endfunction()

# Conditionally add an extra compiler flag to C and C++ flags.
#
# If the language flags already match the `match` argument, skip this flag.
# Otherwise, check for support for `flag` and if support is found,
# add it to the global compile and link options.
function(netdata_add_simple_extra_compiler_flag match flag)
        set(CMAKE_REQUIRED_FLAGS "-Werror")

        netdata_make_cpp_safe_name("${flag}" flag_name)

        if(NOT ${CMAKE_C_FLAGS} MATCHES ${match})
                check_c_compiler_flag("${flag}" HAVE_C_${flag_name})
                if(HAVE_C_${flag_name})
                        add_compile_options($<$<COMPILE_LANGUAGE:C>:${flag}>)
                        add_link_options($<$<COMPILE_LANGUAGE:C>:${flag}>)
                endif()
        endif()

        if(NOT ${CMAKE_CXX_FLAGS} MATCHES ${match})
                check_cxx_compiler_flag("${flag}" HAVE_CXX_${flag_name})
                if(HAVE_CXX_${flag_name})
                        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:${flag}>)
                        add_link_options($<$<COMPILE_LANGUAGE:CXX>:${flag}>)
                endif()
        endif()
endfunction()

# Same as add_simple_extra_compiler_flag, but check for a second flag if the
# first one is unsupported.
function(netdata_add_double_extra_compiler_flag match flag1 flag2)
        set(CMAKE_REQUIRED_FLAGS "-Werror")

        netdata_make_cpp_safe_name("${flag1}" flag1_name)
        netdata_make_cpp_safe_name("${flag2}" flag2_name)

        set(_c_flag "")
        set(_cxx_flag "")

        if(NOT ${CMAKE_C_FLAGS} MATCHES ${match})
                check_c_compiler_flag("${flag1}" HAVE_C_${flag1_name})
                if(HAVE_C_${flag1_name})
                        set(_c_flag "${flag1}")
                else()
                        check_c_compiler_flag("${flag2}" HAVE_C_${flag2_name})
                        if(HAVE_C_${flag2_name})
                                set(_c_flag "${flag2}")
                        endif()
                endif()
        endif()

        if(NOT ${CMAKE_CXX_FLAGS} MATCHES ${match})
                check_cxx_compiler_flag("${flag1}" HAVE_CXX_${flag1_name})
                if(HAVE_CXX_${flag1_name})
                        set(_cxx_flag "${flag1}")
                else()
                        check_cxx_compiler_flag("${flag2}" HAVE_CXX_${flag2_name})
                        if(HAVE_CXX_${flag2_name})
                                set(_cxx_flag "${flag2}")
                        endif()
                endif()
        endif()
        add_compile_options($<$<COMPILE_LANGUAGE:C>:${_c_flag}>)
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:${_cxx_flag}>)
        add_link_options($<$<COMPILE_LANGUAGE:C>:${_c_flag}>)
        add_link_options($<$<COMPILE_LANGUAGE:CXX>:${_cxx_flag}>)
endfunction()

function(netdata_add_extra_hardening_flags)
        netdata_add_double_extra_compiler_flag("stack-protector" "-fstack-protector-strong" "-fstack-protector")
        netdata_add_double_extra_compiler_flag("_FORTIFY_SOURCE" "-D_FORTIFY_SOURCE=3" "-D_FORTIFY_SOURCE=2")
        netdata_add_simple_extra_compiler_flag("stack-clash-protection" "-fstack-clash-protection")
        netdata_add_simple_extra_compiler_flag("-fcf-protection" "-fcf-protection=full")
        netdata_add_simple_extra_compiler_flag("branch-protection" "-mbranch-protection=standard")
endfunction()

function(netdata_add_extra_optimization_flags)
        foreach(FLAG function-sections data-sections)
                netdata_add_simple_extra_compiler_flag("${FLAG}" "-f${FLAG}")
        endforeach()
endfunction()

function(netdata_add_extra_general_flags)
        add_simple_extra_compiler_flag("-Wbuiltin-macro-redefined" "-Wno-builtin-macro-redefined")
endfunction()
