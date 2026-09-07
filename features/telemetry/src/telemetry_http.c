#include "features/telemetry/telemetry.h"

#include "http/nt_http.h"

#include <stdlib.h>

/* The engine's nt_http as the module's transport. Handles are the request
 * ids boxed as pointers, so the module never sees the engine's types. */

#define TELEMETRY_HTTP_TIMEOUT_MS 10000U

static void *http_send(const char *url, const char *body, uint32_t length, void *userdata) {
    (void)userdata;
    const nt_http_options_t opts = {
        .method = "POST",
        .body = body,
        .body_size = length,
        .content_type = "application/json",
        .timeout_ms = TELEMETRY_HTTP_TIMEOUT_MS,
    };
    const nt_http_request_t req = nt_http_request_ex(url, &opts);
    if (req.id == 0U) return NULL;
    nt_http_request_t *box = malloc(sizeof *box);
    if (box == NULL) {
        nt_http_free(req);
        return NULL;
    }
    *box = req;
    return box;
}

static int http_poll(void *handle, void *userdata) {
    (void)userdata;
    const nt_http_request_t req = *(const nt_http_request_t *)handle;
    nt_http_update();
    const nt_http_state_t state = nt_http_state(req);
    if (state == NT_HTTP_STATE_PENDING || state == NT_HTTP_STATE_DOWNLOADING) return 0;
    if (state != NT_HTTP_STATE_DONE) return -1;
    const uint16_t status = nt_http_status(req);
    return status >= 200U && status < 300U ? 1 : -1;
}

static void http_release(void *handle, void *userdata) {
    (void)userdata;
    nt_http_request_t *box = handle;
    nt_http_free(*box);
    free(box);
}

const telemetry_transport_t *telemetry_http_transport(void) {
    static const telemetry_transport_t transport = {
        .send = http_send,
        .poll = http_poll,
        .release = http_release,
        .userdata = NULL,
    };
    return &transport;
}
