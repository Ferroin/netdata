// SPDX-License-Identifier: GPL-3.0-or-later

#include "api_v3_calls.h"

int api_v3_me(RRDHOST *host __maybe_unused, struct web_client *w, char *url __maybe_unused) {
    BUFFER *wb = w->response.data;
    buffer_reset(wb);
    buffer_json_initialize(wb, "\"", "\"", 0, true, BUFFER_JSON_OPTIONS_MINIFY);

    const char *auth;
    switch(w->user_auth.method) {
        case USER_AUTH_METHOD_CLOUD:
            auth = "cloud";
            break;

        case USER_AUTH_METHOD_BEARER:
            auth = "bearer";
            break;

        case USER_AUTH_METHOD_GOD:
            auth = "god";
            break;

        default:
        case USER_AUTH_METHOD_NONE:
            auth = "none";
            break;
    }
    buffer_json_member_add_string(wb, "auth", auth);

    buffer_json_member_add_uuid(wb, "cloud_account_id", w->user_auth.cloud_account_id.uuid);
    buffer_json_member_add_string(wb, "client_name", w->user_auth.client_name);
    http_access2buffer_json_array(wb, "access", w->user_auth.access);
    buffer_json_member_add_string(wb, "user_role", http_id2user_role(w->user_auth.user_role));

    buffer_json_finalize(wb);
    return HTTP_RESP_OK;
}
