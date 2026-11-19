#include "coop_transport.h"

#include "net_ws_client.h"
#include "net_ws_server.h"

#include <stdlib.h>
#include <string.h>

/* The shipped transport: network-core's WebSocket server and client behind the coop interfaces.
   This file is the only place in the module that knows network-core exists. */

typedef struct WsServer {
    net_ws_server_t *ws;
    coop_link_events_t events;
} WsServer;

typedef struct WsClient {
    net_ws_client_t *ws;
    coop_link_events_t events;
} WsClient;

static void ws_on_connect(void *user, uint32_t client, const uint8_t *ticket, size_t ticket_size) {
    (void)ticket;
    (void)ticket_size;
    WsServer *server = user;
    if (server->events.on_open != NULL) server->events.on_open(server->events.user, client);
}

static void ws_on_message(void *user, uint32_t client, const uint8_t *data, size_t size) {
    WsServer *server = user;
    if (server->events.on_message != NULL) server->events.on_message(server->events.user, client, data, size);
}

static void ws_on_disconnect(void *user, uint32_t client, net_ws_close_reason_t reason) {
    (void)reason;
    WsServer *server = user;
    if (server->events.on_close != NULL) server->events.on_close(server->events.user, client, 0);
}

static bool ws_server_start(void *state, const coop_transport_config_t *config, const coop_link_events_t *events) {
    WsServer *server = state;
    server->events = *events;
    net_ws_server_config_t ws = {0};
    ws.port = config->port;
    ws.max_clients = config->max_clients;
    ws.max_message_bytes = config->max_message_bytes;
    ws.send_queue_bytes = 64U * 1024U;
    ws.max_messages_per_second = 240;
    ws.max_message_burst = 240;
    ws.handshake_timeout_ms = 5000;
    ws.ping_idle_s = config->ping_idle_s;
    ws.hangup_idle_s = config->hangup_idle_s;
    ws.protocol_version = config->protocol_version;
    ws.user = server;
    ws.on_connect = ws_on_connect;
    ws.on_message = ws_on_message;
    ws.on_disconnect = ws_on_disconnect;
    server->ws = net_ws_server_create(&ws);
    return server->ws != NULL;
}

static void ws_server_stop(void *state) {
    WsServer *server = state;
    if (server->ws != NULL) net_ws_server_destroy(server->ws);
    free(server);
}

static void ws_server_service(void *state, uint32_t timeout_ms) {
    WsServer *server = state;
    net_ws_server_service(server->ws, timeout_ms);
}

static bool ws_server_send(void *state, uint32_t peer, const uint8_t *data, size_t size) {
    WsServer *server = state;
    return net_ws_server_send(server->ws, peer, data, size);
}

static bool ws_server_send_latest(void *state, uint32_t peer, const uint8_t *data, size_t size) {
    WsServer *server = state;
    size_t replaced = 0;
    return net_ws_server_send_latest(server->ws, peer, data, size, &replaced);
}

static uint16_t ws_server_port(const void *state) {
    const WsServer *server = state;
    return server->ws != NULL ? net_ws_server_port(server->ws) : 0;
}

coop_server_transport_t coop_transport_ws_server(void) {
    WsServer *server = calloc(1, sizeof *server);
    return (coop_server_transport_t){
        .state = server,
        .start = ws_server_start,
        .stop = ws_server_stop,
        .service = ws_server_service,
        .send = ws_server_send,
        .send_latest = ws_server_send_latest,
        .port = ws_server_port,
    };
}

static void ws_client_on_message(void *user, const uint8_t *data, size_t size, double received_at) {
    (void)received_at;
    WsClient *client = user;
    if (client->events.on_message != NULL) client->events.on_message(client->events.user, 0, data, size);
}

static void ws_client_on_close(void *user, uint16_t code) {
    WsClient *client = user;
    if (client->events.on_close != NULL) client->events.on_close(client->events.user, 0, code);
}

static bool ws_client_start(void *state, const char *endpoint, const coop_transport_config_t *config,
                            const coop_link_events_t *events) {
    WsClient *client = state;
    client->events = *events;
    net_ws_client_config_t ws = {0};
    ws.url = endpoint;
    ws.protocol_version = config->protocol_version;
    ws.max_message_bytes = config->max_message_bytes;
    ws.receive_queue_bytes = 16U * 1024U;
    ws.receive_queue_messages = 64;
    ws.overflow_policy = NET_WS_OVERFLOW_DROP_OLDEST;
    ws.send_queue_bytes = 8U * 1024U;
    ws.user = client;
    ws.on_message = ws_client_on_message;
    ws.on_close = ws_client_on_close;
    client->ws = net_ws_client_create(&ws);
    return client->ws != NULL;
}

static void ws_client_stop(void *state) {
    WsClient *client = state;
    if (client->ws != NULL) net_ws_client_destroy(client->ws);
    free(client);
}

static void ws_client_service(void *state, uint32_t timeout_ms) {
    WsClient *client = state;
    net_ws_client_service(client->ws, timeout_ms);
}

static bool ws_client_send(void *state, const uint8_t *data, size_t size) {
    WsClient *client = state;
    return net_ws_client_send(client->ws, data, size);
}

coop_client_transport_t coop_transport_ws_client(void) {
    WsClient *client = calloc(1, sizeof *client);
    return (coop_client_transport_t){
        .state = client,
        .start = ws_client_start,
        .stop = ws_client_stop,
        .service = ws_client_service,
        .send = ws_client_send,
    };
}
