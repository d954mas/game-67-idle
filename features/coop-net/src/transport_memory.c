#include "coop_transport.h"

#include <stdlib.h>
#include <string.h>

/* The in-memory transport: a room and its clients inside one process. It exists so tests and tools
   can run the whole room loop without sockets or ports, and it proves the module does not depend on
   the WebSocket one. Delivery is direct queueing; nothing here is thread-safe. */

enum { MEM_CLIENTS = 8, MEM_QUEUE = 32, MEM_MESSAGE = 1600 };

typedef struct MemMessage {
    uint32_t peer;
    size_t size;
    uint8_t data[MEM_MESSAGE];
} MemMessage;

typedef struct MemQueue {
    MemMessage items[MEM_QUEUE];
    int head;
    int count;
} MemQueue;

typedef struct MemClient {
    bool used;
    bool connected;
    bool connect_pending;
    uint32_t peer;
    MemQueue to_client;
    MemQueue to_server;
    coop_link_events_t events;
} MemClient;

struct coop_memory_hub {
    coop_link_events_t server_events;
    bool server_open;
    MemClient clients[MEM_CLIENTS];
};

typedef struct MemServer {
    coop_memory_hub_t *hub;
} MemServer;

typedef struct MemClientState {
    coop_memory_hub_t *hub;
    int index;
} MemClientState;

static void queue_push(MemQueue *queue, uint32_t peer, const uint8_t *data, size_t size) {
    if (size > MEM_MESSAGE) size = MEM_MESSAGE;
    if (queue->count == MEM_QUEUE) {
        // Drop the oldest: a snapshot is history, an event is rare enough to fit.
        queue->head = (queue->head + 1) % MEM_QUEUE;
        queue->count--;
    }
    MemMessage *message = &queue->items[(queue->head + queue->count) % MEM_QUEUE];
    message->peer = peer;
    message->size = size;
    memcpy(message->data, data, size);
    queue->count++;
}

static bool queue_pop(MemQueue *queue, MemMessage *out) {
    if (queue->count == 0) return false;
    *out = queue->items[queue->head];
    queue->head = (queue->head + 1) % MEM_QUEUE;
    queue->count--;
    return true;
}

coop_memory_hub_t *coop_memory_hub_create(void) {
    return calloc(1, sizeof(coop_memory_hub_t));
}

void coop_memory_hub_destroy(coop_memory_hub_t *hub) {
    free(hub);
}

static bool mem_server_start(void *state, const coop_transport_config_t *config, const coop_link_events_t *events) {
    (void)config;
    MemServer *server = state;
    server->hub->server_events = *events;
    server->hub->server_open = true;
    return true;
}

static void mem_server_stop(void *state) {
    MemServer *server = state;
    free(server);
}

static void mem_server_service(void *state, uint32_t timeout_ms) {
    (void)timeout_ms;
    MemServer *server = state;
    coop_memory_hub_t *hub = server->hub;
    for (int i = 0; i < MEM_CLIENTS; i++) {
        MemClient *client = &hub->clients[i];
        if (!client->used) continue;
        if (client->connect_pending) {
            client->connect_pending = false;
            if (hub->server_events.on_open != NULL) hub->server_events.on_open(hub->server_events.user, client->peer);
        }
        MemMessage message;
        while (queue_pop(&client->to_server, &message)) {
            if (hub->server_events.on_message != NULL) {
                hub->server_events.on_message(hub->server_events.user, client->peer, message.data, message.size);
            }
        }
    }
}

static bool mem_server_send(void *state, uint32_t peer, const uint8_t *data, size_t size) {
    MemServer *server = state;
    if (peer == 0 || peer > MEM_CLIENTS) return false;
    MemClient *client = &server->hub->clients[peer - 1];
    if (!client->used) return false;
    queue_push(&client->to_client, peer, data, size);
    return true;
}

static bool mem_server_send_latest(void *state, uint32_t peer, const uint8_t *data, size_t size) {
    return mem_server_send(state, peer, data, size);
}

static uint16_t mem_server_port(const void *state) {
    (void)state;
    return 0;
}

coop_server_transport_t coop_transport_memory_server(coop_memory_hub_t *hub) {
    MemServer *server = calloc(1, sizeof *server);
    server->hub = hub;
    return (coop_server_transport_t){
        .state = server,
        .start = mem_server_start,
        .stop = mem_server_stop,
        .service = mem_server_service,
        .send = mem_server_send,
        .send_latest = mem_server_send_latest,
        .port = mem_server_port,
    };
}

static bool mem_client_start(void *state, const char *endpoint, const coop_transport_config_t *config,
                             const coop_link_events_t *events) {
    (void)endpoint;
    (void)config;
    MemClientState *client_state = state;
    coop_memory_hub_t *hub = client_state->hub;
    for (int i = 0; i < MEM_CLIENTS; i++) {
        MemClient *client = &hub->clients[i];
        if (client->used) continue;
        memset(client, 0, sizeof *client);
        client->used = true;
        client->connected = true;
        client->connect_pending = true;
        client->peer = (uint32_t)i + 1;
        client->events = *events;
        client_state->index = i;
        return true;
    }
    return false;
}

static void mem_client_stop(void *state) {
    MemClientState *client_state = state;
    coop_memory_hub_t *hub = client_state->hub;
    if (client_state->index >= 0) {
        MemClient *client = &hub->clients[client_state->index];
        if (client->used) {
            const uint32_t peer = client->peer;
            client->used = false;
            if (hub->server_events.on_close != NULL) hub->server_events.on_close(hub->server_events.user, peer, 0);
        }
    }
    free(client_state);
}

static void mem_client_service(void *state, uint32_t timeout_ms) {
    (void)timeout_ms;
    MemClientState *client_state = state;
    if (client_state->index < 0) return;
    MemClient *client = &client_state->hub->clients[client_state->index];
    MemMessage message;
    while (queue_pop(&client->to_client, &message)) {
        if (client->events.on_message != NULL) {
            client->events.on_message(client->events.user, 0, message.data, message.size);
        }
    }
}

static bool mem_client_send(void *state, const uint8_t *data, size_t size) {
    MemClientState *client_state = state;
    MemClient *client = &client_state->hub->clients[client_state->index];
    if (!client->used || !client->connected) return false;
    queue_push(&client->to_server, client->peer, data, size);
    return true;
}

coop_client_transport_t coop_transport_memory_client(coop_memory_hub_t *hub) {
    MemClientState *client = calloc(1, sizeof *client);
    client->hub = hub;
    client->index = -1;
    return (coop_client_transport_t){
        .state = client,
        .start = mem_client_start,
        .stop = mem_client_stop,
        .service = mem_client_service,
        .send = mem_client_send,
    };
}
