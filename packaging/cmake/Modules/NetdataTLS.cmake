# Macros to handle libcrypto and TLS support
#
# Copyright (c) 2024 Netdata Inc.
# SPDX-License-Identifier: GPL-3.0-or-later

macro(netdata_check_tls_libraries)
        set(ENABLE_OPENSSL True)
        pkg_check_modules(OPENSSL openssl)

        if(NOT OPENSSL_FOUND)
                if(MACOS)
                        execute_process(COMMAND
                                        brew --prefix --installed openssl
                                        RESULT_VARIABLE BREW_OPENSSL
                                        OUTPUT_VARIABLE BREW_OPENSSL_PREFIX
                                        OUTPUT_STRIP_TRAILING_WHITESPACE)

                        if((BREW_OPENSSL NOT EQUAL 0) OR (NOT EXISTS "${BREW_OPENSSL_PREFIX}"))
                                message(FATAL_ERROR "OpenSSL (or LibreSSL) is required for building Netdata, but could not be found.")
                        endif()

                        set(OPENSSL_INCLUDE_DIRS "${BREW_OPENSSL_PREFIX}/include")
                        set(OPENSSL_CFLAGS_OTHER "")
                        set(OPENSSL_LDFLAGS "-L${BREW_OPENSSL_PREFIX}/lib;-lssl;-lcrypto")
                else()
                        message(FATAL_ERROR "OpenSSL (or LibreSSL) is required for building Netdata, but could not be found.")
                endif()
        endif()

        if(NOT MACOS)
                pkg_check_modules(CRYPTO libcrypto)
        endif()
endmacro()

macro(netdata_add_crypto_libraries _target _vis)
        if(NOT _vis)
                set(_vis PRIVATE)
        endif()

        target_include_directories(${_target} BEFORE ${_vis} ${CRYPTO_INCLUDE_DIRS})
        target_compile_options(${_target} ${_vis} ${CRYPTO_CFLAGS_OTHER})
        target_link_libraries(${_target} ${_vis} ${CRYPTO_LIBRARIES})
endmacro()

macro(netdata_add_tls_libraries _target _vis)
        if(NOT _vis)
                set(_vis PRIVATE)
        endif()

        target_include_directories(${_target} BEFORE ${_vis} ${OPENSSL_INCLUDE_DIRS})
        target_compile_options(${_target} ${_vis} ${OPENSSL_CFLAGS_OTHER})
        target_link_libraries(${_target} ${_vis} ${OPENSSL_LIBRARIES})
endmacro()
