#include "online_room/meta_nt_http.h"

#include <stdio.h>

#include "http/nt_http.h"

_Static_assert(NT_HTTP_MAX_REQUESTS >= ONLINE_META_TRANSPORT_SLOTS, "the room's HTTP holds every metaserver call it can have out at once");

// The metaserver over the engine's HTTP client: one engine request per call,
// pumped from the room loop, its answer handed over with its handle.

static void http_pump(void *user) {
    (void)user;
    nt_http_update();
}

static uint32_t http_begin(void *user, const char *url, const char *bearer, const void *body, size_t size, uint32_t timeout_ms) {
    (void)user;
    char authorization[128];
    (void)snprintf(authorization, sizeof authorization, "Bearer %s", bearer);
    const char *headers[] = {"Authorization", authorization};
    const nt_http_options_t options = {
        .method = "POST", .body = body, .body_size = (uint32_t)size, .content_type = "application/octet-stream",
        .headers = headers, .header_count = 1U, .timeout_ms = timeout_ms,
    };
    // The engine's invalid handle is 0, which the room reads as "not sent".
    return nt_http_request_ex(url, &options).id;
}

static bool http_poll(void *user, uint32_t handle, uint16_t *status, uint8_t **body, size_t *size) {
    (void)user;
    const nt_http_request_t request = {.id = handle};
    const nt_http_state_t state = nt_http_state(request);
    if (state != NT_HTTP_STATE_DONE && state != NT_HTTP_STATE_FAILED) { return false; }
    // A timeout mid-body still carries a status; only a finished exchange counts as answered.
    *status = state == NT_HTTP_STATE_DONE ? nt_http_status(request) : 0U;
    uint32_t taken = 0U;
    *body = nt_http_take_data(request, &taken);
    *size = taken;
    nt_http_free(request);
    return true;
}

static void http_cancel(void *user, uint32_t handle) {
    (void)user;
    nt_http_free((nt_http_request_t){.id = handle});
}

const online_meta_transport online_meta_nt_http = {.pump = http_pump, .begin = http_begin, .poll = http_poll, .cancel = http_cancel};
