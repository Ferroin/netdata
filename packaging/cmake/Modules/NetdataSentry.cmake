# Macros and functions to handle Sentry
#
# Copyright (c) 2024 Netdata Inc.
# SPDX-License-Identifier: GPL-3.0-or-later

function(netdata_vendor_sentry_native)
    if(ENABLE_SENTRY)
        include(FetchContent)

        # ignore debhelper
        set(FETCHCONTENT_FULLY_DISCONNECTED Off)

        set(SENTRY_VERSION 0.6.6)
        set(SENTRY_BACKEND "breakpad")
        set(SENTRY_BUILD_SHARED_LIBS OFF)

        FetchContent_Declare(
                sentry
                URL https://github.com/getsentry/sentry-native/releases/download/${SENTRY_VERSION}/sentry-native.zip
                URL_HASH SHA256=7a98467c0b2571380a3afc5e681cb13aa406a709529be12d74610b0015ccde0c
        )
        FetchContent_MakeAvailable(sentry)
    endif()
endfunction()

macro(netdata_add_sentry_files _var)
    if(ENABLE_SENTRY)
        list(APPEND ${_var}
            src/daemon/sentry-native/sentry-native.c
            src/daemon/sentry-native/sentry-native.h
        )
    endif()
endmacro()
