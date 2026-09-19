#ifndef NETWORK_CORE_NET_WS_SERVER_H
#define NETWORK_CORE_NET_WS_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Single-threaded WebSocket server. Everything happens inside
   net_ws_server_service(): the caller's simulation thread owns both the
   sockets and the callbacks, so no lock guards the world.

   A connection becomes a client only after a valid HELLO with the configured
   protocol version; anything else is closed with a NET_CLOSE_* code before the
   application hears about it. Client ids are never reused inside one server
   lifetime, so a stale id can never address a newer connection.

   net_ws_server_destroy() may be called from inside a callback: the server
   finishes the pass and frees itself afterwards. */

typedef struct net_ws_server_t net_ws_server_t;

typedef enum net_ws_close_reason_t {
    NET_WS_CLOSE_PEER,      /* peer closed, the socket dropped or a timeout hit */
    NET_WS_CLOSE_PROTOCOL,  /* text frame, oversize or malformed message */
    NET_WS_CLOSE_RATE,      /* inbound message rate exceeded */
    NET_WS_CLOSE_SLOW,      /* outbound queue overflowed */
    NET_WS_CLOSE_APP,       /* net_ws_server_close() */
} net_ws_close_reason_t;

typedef struct net_ws_server_config_t {
    const char *bind_address;         /* NULL binds every interface */
    uint16_t port;                    /* 0 picks an ephemeral port */
    const char *subprotocol;          /* Sec-WebSocket-Protocol clients must request; NULL = none */
    uint32_t max_clients;
    uint32_t max_message_bytes;       /* inbound, after fragment reassembly */
    uint32_t send_queue_bytes;        /* outbound per client, incl. 4-byte prefixes */
    /* Inbound per client, token bucket: burst capacity equals the rate, so
       one second of silence buys one second of messages. 0 = unlimited. */
    uint32_t max_messages_per_second;
    /* A connection that has not sent HELLO, or a close the peer does not
       acknowledge, is dropped after this long. 0 = 10 s. */
    uint32_t handshake_timeout_ms;
    /* Liveness, server-owned because browsers cannot send pings: after
       ping_idle_s without inbound traffic a protocol ping goes out, and a
       client silent for hangup_idle_s (pongs count) is dropped with
       NET_WS_CLOSE_DROPPED. 0 = no pings. */
    uint16_t ping_idle_s;
    uint16_t hangup_idle_s;
    uint32_t protocol_version;
    void *user;
    /* `ticket` is what the client put in its HELLO, NET_HELLO_TICKET_MAX
       bytes at most, valid for the call. */
    void (*on_connect)(void *user, uint32_t client, const uint8_t *ticket, size_t ticket_size);
    void (*on_message)(void *user, uint32_t client, const uint8_t *data, size_t size);
    void (*on_disconnect)(void *user, uint32_t client, net_ws_close_reason_t reason);
} net_ws_server_config_t;

/* NULL when the port cannot be bound. */
net_ws_server_t *net_ws_server_create(const net_ws_server_config_t *config);
void net_ws_server_destroy(net_ws_server_t *server);
uint16_t net_ws_server_port(const net_ws_server_t *server);
/* Connections that completed HELLO. */
uint32_t net_ws_server_client_count(const net_ws_server_t *server);

/* Runs accept, reads, writes and callbacks, returning after `timeout_ms`
   at the latest so a fixed tick can share the thread. */
void net_ws_server_service(net_ws_server_t *server, uint32_t timeout_ms);

/* Queues one non-empty binary message. False means the message is empty,
   the client is unknown, or its queue is full and it is being closed as
   NET_WS_CLOSE_SLOW; the id stays valid until on_disconnect fires. */
bool net_ws_server_send(net_ws_server_t *server, uint32_t client, const uint8_t *data, size_t size);
/* Bytes still queued for the client, so the application can decide to
   coalesce (skip a snapshot) instead of piling up. 0 for an unknown client. */
size_t net_ws_server_queued_bytes(const net_ws_server_t *server, uint32_t client);
/* Flushes what is queued, then closes with the application's code
   (NET_CLOSE_APP, or a game-defined 4100-4999). on_disconnect fires from a
   later service call with NET_WS_CLOSE_APP. */
void net_ws_server_close(net_ws_server_t *server, uint32_t client, uint16_t code);

#endif
