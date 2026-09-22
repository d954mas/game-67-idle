#include "net_ws_client.h"
#include "net_codec.h"

#include <emscripten/emscripten.h>

#include <stdint.h>
#include <stddef.h>

static net_ws_client_t *client;
static uint32_t messages;
static uint32_t closes;
static uint32_t failure;
static uint32_t passes;

static void received(void *user, const uint8_t *data, size_t size, double received_at) {
    (void)user;
    (void)received_at;
    if (size != 2U || data[1] != messages) { failure = 1U; }
    messages += 1U;
}

static void closed(void *user, uint16_t code) {
    (void)user;
    if (code != NET_CLOSE_SLOW || messages != 64U) { failure = 1U; }
    closes += 1U;
    net_ws_client_destroy(client);
    client = NULL;
}

EMSCRIPTEN_KEEPALIVE void fixture_start(const char *url) {
    const net_ws_client_config_t config = {
        .url = url, .protocol_version = 1U, .max_message_bytes = 2U,
        .receive_queue_bytes = 1024U, .receive_queue_messages = 64U,
        .service_message_limit = 16U, .send_queue_bytes = 64U,
        .on_message = received, .on_close = closed,
    };
    client = net_ws_client_create(&config);
    if (client == NULL) { failure = 1U; }
}

EMSCRIPTEN_KEEPALIVE void fixture_service(void) {
    if (client != NULL && net_ws_client_state(client) == NET_WS_CLIENT_CLOSED) {
        const uint32_t before = messages;
        net_ws_client_service(client, 0U);
        passes += 1U;
        if (messages - before > 16U) { failure = 1U; }
    }
}

EMSCRIPTEN_KEEPALIVE int fixture_done(void) { return closes == 1U || failure != 0U; }
EMSCRIPTEN_KEEPALIVE int fixture_passed(void) { return failure == 0U && closes == 1U && messages == 64U && passes >= 4U; }
