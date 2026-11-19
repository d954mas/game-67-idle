#ifndef COOP_TRANSPORT_H
#define COOP_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The replaceable part of a co-op session. A room and a client talk to a transport only through
   these two interfaces; the shipped WebSocket implementation is one of them, a Steam or an
   in-memory transport is another. Nothing here knows a game. */

enum { COOP_TRANSPORT_MESSAGE_MAX = 1600 };

typedef struct coop_transport_config {
    uint16_t port;               /* server: 0 asks the OS for a free port */
    uint32_t max_clients;
    uint32_t max_message_bytes;
    uint32_t protocol_version;
    uint16_t ping_idle_s;
    uint16_t hangup_idle_s;
} coop_transport_config_t;

typedef struct coop_link_events {
    void *user;
    /* A peer finished the transport handshake and may be addressed by `peer`. */
    void (*on_open)(void *user, uint32_t peer);
    void (*on_message)(void *user, uint32_t peer, const uint8_t *data, size_t size);
    void (*on_close)(void *user, uint32_t peer, uint16_t code);
} coop_link_events_t;

typedef struct coop_server_transport {
    void *state;
    bool (*start)(void *state, const coop_transport_config_t *config, const coop_link_events_t *events);
    void (*stop)(void *state);
    /* Pumps the transport without waiting when timeout_ms is 0. */
    void (*service)(void *state, uint32_t timeout_ms);
    bool (*send)(void *state, uint32_t peer, const uint8_t *data, size_t size);
    /* Sends replacing any message this peer has not read: snapshots are history, not a queue. */
    bool (*send_latest)(void *state, uint32_t peer, const uint8_t *data, size_t size);
    uint16_t (*port)(const void *state);
} coop_server_transport_t;

typedef struct coop_client_transport {
    void *state;
    bool (*start)(void *state, const char *endpoint, const coop_transport_config_t *config,
                  const coop_link_events_t *events);
    void (*stop)(void *state);
    void (*service)(void *state, uint32_t timeout_ms);
    bool (*send)(void *state, const uint8_t *data, size_t size);
} coop_client_transport_t;

/* The shipped WebSocket transport. state is allocated by the factory and freed by stop. */
coop_server_transport_t coop_transport_ws_server(void);
coop_client_transport_t coop_transport_ws_client(void);

/* In-memory transport for tests and tools: a room and its clients in one process, no sockets.
   One hub per test; destroy it after every transport built from it was stopped. */
typedef struct coop_memory_hub coop_memory_hub_t;
coop_memory_hub_t *coop_memory_hub_create(void);
void coop_memory_hub_destroy(coop_memory_hub_t *hub);
coop_server_transport_t coop_transport_memory_server(coop_memory_hub_t *hub);
coop_client_transport_t coop_transport_memory_client(coop_memory_hub_t *hub);

#endif
