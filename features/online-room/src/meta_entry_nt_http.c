#include "online_room/meta_entry_nt_http.h"

#include <stdio.h>
#include <string.h>

#include "http/nt_http.h"

static uint32_t http_begin(void *user, const char *url, const char *bearer, const char *json, uint32_t timeout_ms) {
    (void)user;
    char header[96];
    const char *headers[2] = {"Authorization", header};
    uint32_t header_count = 0U;
    if (bearer != NULL) {
        (void)snprintf(header, sizeof header, "Bearer %s", bearer);
        header_count = 1U;
    }
    const nt_http_options_t options = {.method = "POST", .body = json, .body_size = json != NULL ? (uint32_t)strlen(json) : 0U,
        .content_type = json != NULL ? "application/json" : NULL, .headers = headers, .header_count = header_count,
        .timeout_ms = timeout_ms};
    // The engine's invalid handle is 0, which the entry reads as "not sent".
    const uint32_t id = nt_http_request_ex(url, &options).id;
    // The header holds the session's secret; the engine has its own copy.
    volatile char *bytes = header;
    for (size_t index = 0U; index < sizeof header; ++index) { bytes[index] = 0; }
    return id;
}

static bool http_poll(void *user, uint32_t handle, uint16_t *status, uint8_t **body, size_t *size) {
    (void)user;
    const nt_http_request_t request = {.id = handle};
    const nt_http_state_t state = nt_http_state(request);
    if (state != NT_HTTP_STATE_DONE && state != NT_HTTP_STATE_FAILED) { return false; }
    *status = state == NT_HTTP_STATE_DONE ? nt_http_status(request) : 0U;
    uint32_t taken = 0U;
    *body = state == NT_HTTP_STATE_DONE ? nt_http_take_data(request, &taken) : NULL;
    *size = taken;
    nt_http_free(request);
    return true;
}

static void http_cancel(void *user, uint32_t handle) {
    (void)user;
    nt_http_free((nt_http_request_t){.id = handle});
}

const online_meta_entry_http online_meta_entry_nt_http = {.begin = http_begin, .poll = http_poll, .cancel = http_cancel};
