#include "net_ws_client.h"

#include "net_codec.h"
#include "net_queue.h"

#include <emscripten/websocket.h>

#include <stdlib.h>
#include <string.h>

/* Browser events land between frames. Messages are queued and replayed from
   service() so the game sees the network at one point of its frame, exactly
   like the native client; open and close are single flags outside the ring,
   so no overflow policy can ever discard them. */
struct net_ws_client_t {
    net_ws_client_config_t config;
    EMSCRIPTEN_WEBSOCKET_T socket;
    net_ws_client_state_t state;
    net_queue_t messages;
    uint8_t *scratch;
    bool open_pending;
    bool close_pending;
    uint16_t close_code;
    bool in_service;
    bool destroy_requested;
    bool destroying;
};

static void mark_closed(net_ws_client_t *client, uint16_t code) {
    if (client->state == NET_WS_CLIENT_CLOSED) { return; }
    client->state = NET_WS_CLIENT_CLOSED;
    client->close_pending = true;
    client->close_code = code;
}

static EM_BOOL on_open(int type, const EmscriptenWebSocketOpenEvent *event, void *user) {
    (void)type;
    (void)event;
    net_ws_client_t *client = (net_ws_client_t *)user;
    uint8_t hello[NET_HELLO_SIZE];
    net_writer_t writer;
    net_writer_init(&writer, hello, sizeof hello);
    net_hello_encode(&writer, client->config.protocol_version);
    emscripten_websocket_send_binary(client->socket, hello, sizeof hello);
    client->state = NET_WS_CLIENT_OPEN;
    client->open_pending = true;
    return EM_TRUE;
}

static EM_BOOL on_message(int type, const EmscriptenWebSocketMessageEvent *event, void *user) {
    (void)type;
    net_ws_client_t *client = (net_ws_client_t *)user;
    if (client->state != NET_WS_CLIENT_OPEN) { return EM_TRUE; }
    if (event->isText || event->numBytes == 0U || event->numBytes > client->config.max_message_bytes) {
        emscripten_websocket_close(client->socket, NET_CLOSE_FORMAT, "bad frame");
        mark_closed(client, NET_CLOSE_FORMAT);
        return EM_TRUE;
    }
    while (!net_queue_push(&client->messages, event->data, event->numBytes)) {
        if (client->config.overflow_policy != NET_WS_OVERFLOW_DROP_OLDEST) {
            emscripten_websocket_close(client->socket, NET_CLOSE_SLOW, "receive queue full");
            mark_closed(client, NET_CLOSE_SLOW);
            return EM_TRUE;
        }
        /* The queue holds at least one message, so a pop always makes room. */
        net_queue_pop(&client->messages);
    }
    return EM_TRUE;
}

static EM_BOOL on_close(int type, const EmscriptenWebSocketCloseEvent *event, void *user) {
    (void)type;
    net_ws_client_t *client = (net_ws_client_t *)user;
    mark_closed(client, event->wasClean ? event->code : 0U);
    return EM_TRUE;
}

static EM_BOOL on_error(int type, const EmscriptenWebSocketErrorEvent *event, void *user) {
    (void)type;
    (void)event;
    /* The browser always follows an error with a close event. */
    (void)user;
    return EM_TRUE;
}

static void free_client(net_ws_client_t *client) {
    if (client->socket > 0) {
        emscripten_websocket_close(client->socket, 1000, "bye");
        emscripten_websocket_delete(client->socket);
    }
    free(client->scratch);
    net_queue_free(&client->messages);
    free(client);
}

net_ws_client_t *net_ws_client_create(const net_ws_client_config_t *config) {
    if (config == NULL || config->url == NULL || config->max_message_bytes == 0U ||
        config->receive_queue_bytes < config->max_message_bytes + 4U ||
        !emscripten_websocket_is_supported()) {
        return NULL;
    }
    net_ws_client_t *client = (net_ws_client_t *)calloc(1U, sizeof *client);
    if (client == NULL) { return NULL; }
    client->config = *config;
    client->state = NET_WS_CLIENT_CONNECTING;
    client->scratch = (uint8_t *)malloc(config->max_message_bytes);
    if (client->scratch == NULL || !net_queue_init(&client->messages, config->receive_queue_bytes)) {
        free_client(client);
        return NULL;
    }
    EmscriptenWebSocketCreateAttributes attributes;
    emscripten_websocket_init_create_attributes(&attributes);
    attributes.url = config->url;
    attributes.protocols = config->subprotocol;
    attributes.createOnMainThread = EM_TRUE;
    client->socket = emscripten_websocket_new(&attributes);
    if (client->socket <= 0) {
        free_client(client);
        return NULL;
    }
    emscripten_websocket_set_onopen_callback(client->socket, client, on_open);
    emscripten_websocket_set_onmessage_callback(client->socket, client, on_message);
    emscripten_websocket_set_onclose_callback(client->socket, client, on_close);
    emscripten_websocket_set_onerror_callback(client->socket, client, on_error);
    return client;
}

void net_ws_client_destroy(net_ws_client_t *client) {
    if (client == NULL) { return; }
    client->destroying = true;
    if (client->in_service) {
        client->destroy_requested = true;
        return;
    }
    free_client(client);
}

net_ws_client_state_t net_ws_client_state(const net_ws_client_t *client) {
    return client->state;
}

void net_ws_client_service(net_ws_client_t *client, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (client->destroying) { return; }
    client->in_service = true;
    if (client->open_pending) {
        client->open_pending = false;
        if (client->config.on_open != NULL) { client->config.on_open(client->config.user); }
    }
    size_t size;
    while (!client->destroying && (size = net_queue_front_size(&client->messages)) > 0U) {
        net_queue_front_copy(&client->messages, client->scratch, size);
        net_queue_pop(&client->messages);
        if (client->config.on_message != NULL) {
            client->config.on_message(client->config.user, client->scratch, size);
        }
    }
    if (!client->destroying && client->close_pending) {
        client->close_pending = false;
        if (client->config.on_close != NULL) { client->config.on_close(client->config.user, client->close_code); }
    }
    client->in_service = false;
    if (client->destroy_requested) { free_client(client); }
}

bool net_ws_client_send(net_ws_client_t *client, const uint8_t *data, size_t size) {
    if (client->state != NET_WS_CLIENT_OPEN || size == 0U || size > UINT32_MAX) { return false; }
    return emscripten_websocket_send_binary(client->socket, (void *)data, (uint32_t)size) ==
        EMSCRIPTEN_RESULT_SUCCESS;
}
