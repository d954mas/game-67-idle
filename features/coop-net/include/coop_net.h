#ifndef COOP_NET_H
#define COOP_NET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "coop_transport.h"

/* Reusable authoritative co-op rooms: seats, input mailboxes with sequence numbers, a fixed tick,
   snapshot fan-out and the client side of the same wire. The world stays in the game: a room calls
   the game to step it and to fill the snapshot payload, a client hands the payload to the game.
   Transport is features/network-core; this module adds only the room and session rules. */

enum { COOP_NET_PROTOCOL_VERSION = 1, COOP_NET_SEATS_MAX = 8, COOP_NET_SNAPSHOT_MAX = 1600 };

/* Room closes with this code when every seat is taken. */
enum { COOP_NET_CLOSE_FULL = 4100, COOP_NET_CLOSE_FORMAT = 4101 };

typedef struct coop_room coop_room_t;

typedef struct coop_room_callbacks {
    void *user;
    /* A player took a seat. The game spawns its heroine here. */
    void (*on_join)(void *user, int seat);
    void (*on_leave)(void *user, int seat);
    /* One input of one seat arrived. `seq` is the seat's own counter; `data` is the game's payload. */
    void (*on_input)(void *user, int seat, uint32_t seq, const uint8_t *data, size_t size);
    /* The room advances its world by dt. Called once per pump, before the snapshot. */
    void (*on_tick)(void *user, float dt);
    /* Fill the game's part of the snapshot. `acks[seat]` is the last input the room applied. */
    size_t (*write_snapshot)(void *user, const uint32_t *acks, int seats, uint8_t *out, size_t capacity);
} coop_room_callbacks_t;

/* Room configuration. Zero fields take the module's defaults, so a consumer sets only what it
   changes. Sizes are bytes and never exceed the compiled ceilings below. */
typedef struct coop_room_config {
    uint16_t port;               /* 0 asks the OS for a free port */
    uint32_t max_clients;        /* 0: COOP_NET_SEATS_MAX */
    uint32_t max_message_bytes;  /* 0: 256 */
    uint32_t snapshot_bytes;     /* 0: COOP_NET_SNAPSHOT_MAX */
    uint32_t protocol_version;   /* 0: COOP_NET_PROTOCOL_VERSION */
    uint16_t ping_idle_s;        /* 0: no pings from the room */
    uint16_t hangup_idle_s;      /* 0: no idle drop */
    /* NULL takes the shipped WebSocket transport. */
    const coop_server_transport_t *transport;
} coop_room_config_t;

/* port 0 asks the OS for a free port. NULL when the port cannot be bound. */
coop_room_t *coop_room_create(const coop_room_config_t *config, const coop_room_callbacks_t *callbacks);
/* The original shorthand: a room with default limits and the given port and seats. */
coop_room_t *coop_room_start(uint16_t port, uint32_t max_clients, const coop_room_callbacks_t *callbacks);
void coop_room_stop(coop_room_t *room);
uint16_t coop_room_port(const coop_room_t *room);
int coop_room_seats(const coop_room_t *room);
/* Services the sockets without waiting, ticks the world and sends every client the new snapshot. */
void coop_room_pump(coop_room_t *room, float dt);
/* Sends one game event to every seated client; the transport delivers it reliably and in order. */
bool coop_room_broadcast(coop_room_t *room, const uint8_t *data, size_t size);
/* The last input sequence the room applied for a seat, 0 when the seat never sent one. */
uint32_t coop_room_ack(const coop_room_t *room, int seat);

typedef struct coop_client coop_client_t;

typedef struct coop_client_callbacks {
    void *user;
    void (*on_welcome)(void *user, int seat, uint32_t tick);
    /* `acks[seat]` tells a client which of its inputs the snapshot already includes. */
    void (*on_snapshot)(void *user, uint32_t tick, const uint32_t *acks, int seats, const uint8_t *payload,
                        size_t size);
    /* One game event from the room. Reliable and ordered with the other events, not with snapshots. */
    void (*on_event)(void *user, const uint8_t *data, size_t size);
    void (*on_close)(void *user, uint16_t code);
} coop_client_callbacks_t;

typedef struct coop_client_config {
    const char *url;
    uint32_t protocol_version;  /* 0: COOP_NET_PROTOCOL_VERSION */
    uint32_t snapshot_bytes;    /* 0: COOP_NET_SNAPSHOT_MAX */
    /* NULL takes the shipped WebSocket transport. */
    const coop_client_transport_t *transport;
} coop_client_config_t;

coop_client_t *coop_client_create(const coop_client_config_t *config, const coop_client_callbacks_t *callbacks);
/* The original shorthand: connect with default limits. */
coop_client_t *coop_client_connect(const char *url, const coop_client_callbacks_t *callbacks);
void coop_client_stop(coop_client_t *client);
/* Pumps the socket; callbacks fire on the caller's thread. */
void coop_client_service(coop_client_t *client);
/* Queues one input payload; the client adds its sequence number. */
bool coop_client_send_input(coop_client_t *client, const uint8_t *data, size_t size);
/* The sequence number the last queued input got; keys the game's prediction history. */
uint32_t coop_client_seq(const coop_client_t *client);
int coop_client_seat(const coop_client_t *client);
/* True once the room has seated this client. */
bool coop_client_ready(const coop_client_t *client);

#endif
