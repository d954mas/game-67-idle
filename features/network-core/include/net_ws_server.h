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
    /* PEM files; both set = the server speaks wss:// itself, which needs
       the library built with NETWORK_CORE_WITH_TLS (creation fails
       otherwise). Both NULL = plain ws://, a proxy's job to wrap. */
    const char *tls_cert_path;
    const char *tls_key_path;
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
    /* Liveness, server-owned because browsers cannot send pings. Every
       ping_idle_s after the last pong a protocol ping goes out; a client
       whose pong has not arrived hangup_idle_s after that same pong is
       dropped with NET_WS_CLOSE_PEER (the peer sees a bare close). Data
       frames do not count as life, only pongs, so the pong window is
       hangup_idle_s - ping_idle_s and must be positive. 0 = no pings and
       no idle drop at all. */
    uint16_t ping_idle_s;
    uint16_t hangup_idle_s;
    uint32_t protocol_version;
    void *user;
    /* `ticket` is what the client put in its HELLO, NET_HELLO_TICKET_MAX
       bytes at most, never NULL, empty when the client sent none, valid
       for the call. */
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

/* Queues one non-empty binary message. True means immutable local
   admission, not kernel or network delivery. False means the message is
   empty, the client is unavailable, its queue is full, or a direct write
   failed and it is terminal-closing as NET_WS_CLOSE_SLOW; the id stays valid
   until on_disconnect fires. */
bool net_ws_server_send(net_ws_server_t *server, uint32_t client, const uint8_t *data, size_t size);
/* True only when an authenticated live client has no local queued or partial
   output, so an immutable stream frame can be admitted. It says nothing
   about bytes already accepted by the kernel or network. */
bool net_ws_server_send_ready(const net_ws_server_t *server, uint32_t client);
/* Queues a message that supersedes every queued message of its kind (a
   newer full state): the queued ones whose first byte, the application's
   type, equals this one's are dropped first, so a client that cannot
   drain gets the newest when it can, not a backlog of stale ones, while
   messages of other kinds keep their place. Nothing mid-write is touched;
   the transport keeps a partial write itself. True means replaceable local
   admission, not kernel or network delivery. A later
   send_latest() of the same kind may replace it. `replaced` (may be NULL)
   receives how many queued messages this one superseded. False as for
   net_ws_server_send. */
bool net_ws_server_send_latest(net_ws_server_t *server, uint32_t client, const uint8_t *data, size_t size,
    size_t *replaced);
/* Bytes still queued for the client, so the application can decide to
   coalesce (skip a snapshot) instead of piling up. 0 for an unknown client. */
size_t net_ws_server_queued_bytes(const net_ws_server_t *server, uint32_t client);
/* Seconds since the kernel last received data on the client's connection,
   read from the socket: what a message delivered in this pass has waited
   since it arrived, so a server that services its sockets on its own
   schedule still knows when each message came in. At the kernel's tick
   resolution (1 or 4 ms). 0 for an unknown client; -1 where the platform
   cannot tell — net_ws_server_reports_arrival() says so beforehand, and a
   caller that needs arrival times then services as messages arrive. */
double net_ws_server_receive_age(const net_ws_server_t *server, uint32_t client);
bool net_ws_server_reports_arrival(void);
/* Flushes what is queued, then closes with the application's code
   (NET_CLOSE_APP, or a game-defined 4100-4999). on_disconnect fires from a
   later service call with NET_WS_CLOSE_APP. */
void net_ws_server_close(net_ws_server_t *server, uint32_t client, uint16_t code);
/* Ends a terminal stream as NET_CLOSE_SLOW without draining application
   output. Ordinary application closes retain their drain-first behavior. */
void net_ws_server_close_slow(net_ws_server_t *server, uint32_t client);

#endif
