# Macros and functions to handle H2O
#
# Copyright (c) 2024 Netdata Inc.
# SPDX-License-Identifier: GPL-3.0-or-later

function(netdata_add_h2o_library)
        include(NetdataTLS)

        add_library(h2o STATIC
                src/web/server/h2o/libh2o/deps/cloexec/cloexec.c
                src/web/server/h2o/libh2o/deps/libgkc/gkc.c
                src/web/server/h2o/libh2o/deps/libyrmcds/close.c
                src/web/server/h2o/libh2o/deps/libyrmcds/connect.c
                src/web/server/h2o/libh2o/deps/libyrmcds/recv.c
                src/web/server/h2o/libh2o/deps/libyrmcds/send.c
                src/web/server/h2o/libh2o/deps/libyrmcds/send_text.c
                src/web/server/h2o/libh2o/deps/libyrmcds/socket.c
                src/web/server/h2o/libh2o/deps/libyrmcds/strerror.c
                src/web/server/h2o/libh2o/deps/libyrmcds/text_mode.c
                src/web/server/h2o/libh2o/deps/picohttpparser/picohttpparser.c
                src/web/server/h2o/libh2o/lib/common/cache.c
                src/web/server/h2o/libh2o/lib/common/file.c
                src/web/server/h2o/libh2o/lib/common/filecache.c
                src/web/server/h2o/libh2o/lib/common/hostinfo.c
                src/web/server/h2o/libh2o/lib/common/http1client.c
                src/web/server/h2o/libh2o/lib/common/memcached.c
                src/web/server/h2o/libh2o/lib/common/memory.c
                src/web/server/h2o/libh2o/lib/common/multithread.c
                src/web/server/h2o/libh2o/lib/common/serverutil.c
                src/web/server/h2o/libh2o/lib/common/socket.c
                src/web/server/h2o/libh2o/lib/common/socketpool.c
                src/web/server/h2o/libh2o/lib/common/string.c
                src/web/server/h2o/libh2o/lib/common/time.c
                src/web/server/h2o/libh2o/lib/common/timeout.c
                src/web/server/h2o/libh2o/lib/common/url.c
                src/web/server/h2o/libh2o/lib/core/config.c
                src/web/server/h2o/libh2o/lib/core/configurator.c
                src/web/server/h2o/libh2o/lib/core/context.c
                src/web/server/h2o/libh2o/lib/core/headers.c
                src/web/server/h2o/libh2o/lib/core/logconf.c
                src/web/server/h2o/libh2o/lib/core/proxy.c
                src/web/server/h2o/libh2o/lib/core/request.c
                src/web/server/h2o/libh2o/lib/core/token.c
                src/web/server/h2o/libh2o/lib/core/util.c
                src/web/server/h2o/libh2o/lib/handler/access_log.c
                src/web/server/h2o/libh2o/lib/handler/chunked.c
                src/web/server/h2o/libh2o/lib/handler/compress.c
                src/web/server/h2o/libh2o/lib/handler/compress/gzip.c
                src/web/server/h2o/libh2o/lib/handler/errordoc.c
                src/web/server/h2o/libh2o/lib/handler/expires.c
                src/web/server/h2o/libh2o/lib/handler/fastcgi.c
                src/web/server/h2o/libh2o/lib/handler/file.c
                src/web/server/h2o/libh2o/lib/handler/headers.c
                src/web/server/h2o/libh2o/lib/handler/mimemap.c
                src/web/server/h2o/libh2o/lib/handler/proxy.c
                src/web/server/h2o/libh2o/lib/handler/redirect.c
                src/web/server/h2o/libh2o/lib/handler/reproxy.c
                src/web/server/h2o/libh2o/lib/handler/throttle_resp.c
                src/web/server/h2o/libh2o/lib/handler/status.c
                src/web/server/h2o/libh2o/lib/handler/headers_util.c
                src/web/server/h2o/libh2o/lib/handler/status/events.c
                src/web/server/h2o/libh2o/lib/handler/status/requests.c
                src/web/server/h2o/libh2o/lib/handler/http2_debug_state.c
                src/web/server/h2o/libh2o/lib/handler/status/durations.c
                src/web/server/h2o/libh2o/lib/handler/configurator/access_log.c
                src/web/server/h2o/libh2o/lib/handler/configurator/compress.c
                src/web/server/h2o/libh2o/lib/handler/configurator/errordoc.c
                src/web/server/h2o/libh2o/lib/handler/configurator/expires.c
                src/web/server/h2o/libh2o/lib/handler/configurator/fastcgi.c
                src/web/server/h2o/libh2o/lib/handler/configurator/file.c
                src/web/server/h2o/libh2o/lib/handler/configurator/headers.c
                src/web/server/h2o/libh2o/lib/handler/configurator/proxy.c
                src/web/server/h2o/libh2o/lib/handler/configurator/redirect.c
                src/web/server/h2o/libh2o/lib/handler/configurator/reproxy.c
                src/web/server/h2o/libh2o/lib/handler/configurator/throttle_resp.c
                src/web/server/h2o/libh2o/lib/handler/configurator/status.c
                src/web/server/h2o/libh2o/lib/handler/configurator/http2_debug_state.c
                src/web/server/h2o/libh2o/lib/handler/configurator/headers_util.c
                src/web/server/h2o/libh2o/lib/http1.c
                src/web/server/h2o/libh2o/lib/tunnel.c
                src/web/server/h2o/libh2o/lib/http2/cache_digests.c
                src/web/server/h2o/libh2o/lib/http2/casper.c
                src/web/server/h2o/libh2o/lib/http2/connection.c
                src/web/server/h2o/libh2o/lib/http2/frame.c
                src/web/server/h2o/libh2o/lib/http2/hpack.c
                src/web/server/h2o/libh2o/lib/http2/scheduler.c
                src/web/server/h2o/libh2o/lib/http2/stream.c
                src/web/server/h2o/libh2o/lib/http2/http2_debug_state.c
        )

        target_include_directories(h2o BEFORE PUBLIC
                "${CMAKE_SOURCE_DIR}/src/web/server/h2o/libh2o/include"
                "${CMAKE_SOURCE_DIR}/src/web/server/h2o/libh2o/deps/cloexec"
                "${CMAKE_SOURCE_DIR}/src/web/server/h2o/libh2o/deps/brotli/enc"
                "${CMAKE_SOURCE_DIR}/src/web/server/h2o/libh2o/deps/golombset"
                "${CMAKE_SOURCE_DIR}/src/web/server/h2o/libh2o/deps/libgkc"
                "${CMAKE_SOURCE_DIR}/src/web/server/h2o/libh2o/deps/libyrmcds"
                "${CMAKE_SOURCE_DIR}/src/web/server/h2o/libh2o/deps/klib"
                "${CMAKE_SOURCE_DIR}/src/web/server/h2o/libh2o/deps/neverbleed"
                "${CMAKE_SOURCE_DIR}/src/web/server/h2o/libh2o/deps/picohttpparser"
                "${CMAKE_SOURCE_DIR}/src/web/server/h2o/libh2o/deps/picotest"
                "${CMAKE_SOURCE_DIR}/src/web/server/h2o/libh2o/deps/yaml/include"
                "${CMAKE_SOURCE_DIR}/src/web/server/h2o/libh2o/deps/yoml"
        )

        target_compile_options(h2o PRIVATE
                -Wno-all -Wno-extra
                -Wno-shadow
                -Wno-deprecated-declarations
                -Wformat
        )

        target_compile_definitions(h2o PUBLIC H2O_USE_LIBUV=0)

        netdata_check_tls_libraries()
        netdata_add_tls_libraries_to_target(h2o PRIVATE)
endfunction()

function(netdata_add_h2o_to_target _target)
        target_sources(${_target} PRIVATE
                src/web/server/h2o/http_server.c
                src/web/server/h2o/http_server.h
                src/web/server/h2o/h2o_utils.c
                src/web/server/h2o/h2o_utils.h
                src/web/server/h2o/streaming.c
                src/web/server/h2o/streaming.h
                src/web/server/h2o/connlist.c
                src/web/server/h2o/connlist.h
        )

        target_link_libraries(${_target} PRIVATE h2o)
endfunction()
