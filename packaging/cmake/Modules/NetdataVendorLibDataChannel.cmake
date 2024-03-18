# Macros and functions to handle LibDataChannel
#
# Copyright (c) 2024 Netdata Inc.
# SPDX-License-Identifier: GPL-3.0-or-later

function(netdata_vendor_libdatachannel)
        include(FetchContent)

        # ignore debhelper
        set(FETCHCONTENT_FULLY_DISCONNECTED Off)

        set(PREFER_SYSTEM_LIB True)
        set(NO_MEDIA True)
        set(NO_WEBSOCKET True)

        set(HAVE_LIBDATACHANNEL True)

        FetchContent_Declare(libdatachannel
                GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
                GIT_TAG 0b1074a9effeb8d9d3f4eca704d3fe3d2f9bc7e5 # v0.20.2
        )
        FetchContent_MakeAvailable(libdatachannel)
endfunction()
