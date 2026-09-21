#ifndef NETWORK_CORE_NET_WS_CLIENT_H
#define NETWORK_CORE_NET_WS_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* One WebSocket connection to a room. Native builds drive libcurl's
   WebSocket (or libwebsockets where no libcurl is built) on a thread of
   their own, the browser build drives the page's own WebSocket; in all
   the socket is read the moment the network delivers, so every message
   carries its exact arrival time, and callbacks fire only from inside
   net_ws_client_service(), on the caller's thread, so the game reads the
   network at one point of its frame.

   The client sends HELLO with its protocol version as the first frame; the
   server closes with NET_CLOSE_VERSION when the versions differ.

   net_ws_client_destroy() may be called from inside a callback: the client
   finishes the pass and frees itself afterwards. */

typedef struct net_ws_client_t net_ws_client_t;

typedef enum net_ws_client_state_t {
    NET_WS_CLIENT_CONNECTING,
    NET_WS_CLIENT_OPEN,
    NET_WS_CLIENT_CLOSED,
} net_ws_client_state_t;

/* What happens when messages arrive faster than the game services them. */
typedef enum net_ws_overflow_policy_t {
    /* Close with NET_CLOSE_SLOW; the game sees on_close and decides. Right
       for delta streams, where a lost message poisons everything after it. */
    NET_WS_OVERFLOW_CLOSE,
    /* Drop the oldest queued messages and keep the newest. Right for full
       snapshots, where only the latest one matters. */
    NET_WS_OVERFLOW_DROP_OLDEST,
} net_ws_overflow_policy_t;

typedef struct net_ws_client_config_t {
    const char *url;                /* ws://host:port/path, or wss:// where the build speaks TLS */
    const char *subprotocol;        /* Sec-WebSocket-Protocol, NULL for none */
    /* Accept any certificate: a stand signing with its own CA. Native
       libcurl builds only; the browser trusts what the browser trusts. */
    bool tls_insecure;
    uint32_t protocol_version;
    /* Opaque bytes carried in HELLO for the server's on_connect; at most
       NET_HELLO_TICKET_MAX, copied at create. */
    const uint8_t *ticket;
    size_t ticket_size;
    uint32_t max_message_bytes;     /* inbound, after fragment reassembly */
    /* Inbound messages held until service(); must hold at least one
       message (max_message_bytes + 12). */
    uint32_t receive_queue_bytes;
    net_ws_overflow_policy_t overflow_policy;
    uint32_t send_queue_bytes;      /* outbound messages waiting for the socket */
    void *user;
    void (*on_open)(void *user);
    /* `received_at` is net_ws_client_clock() at the moment the socket
       delivered the message, not the moment service() replays it. */
    void (*on_message)(void *user, const uint8_t *data, size_t size, double received_at);
    /* `code` is the peer's close code, or 0 when the socket dropped. */
    void (*on_close)(void *user, uint16_t code);
} net_ws_client_config_t;

/* Monotonic seconds; the clock `received_at` is stamped with, for the
   caller's own send-side timestamps. */
double net_ws_client_clock(void);

/* NULL only for an invalid config or an unusable URL. A refused connection
   is reported later through on_close from the first service() call. */
net_ws_client_t *net_ws_client_create(const net_ws_client_config_t *config);
void net_ws_client_destroy(net_ws_client_t *client);
/* May already read OPEN or CLOSED before the matching callback has been
   delivered by service(); the callbacks are the ordered account. */
net_ws_client_state_t net_ws_client_state(const net_ws_client_t *client);

/* Delivers the callbacks queued since the last call and returns at once;
   `timeout_ms` is accepted for the server's sake and ignored. */
void net_ws_client_service(net_ws_client_t *client, uint32_t timeout_ms);

/* Queues one non-empty binary message. False when the connection is not
   open, the message is empty or the send queue is full. */
bool net_ws_client_send(net_ws_client_t *client, const uint8_t *data, size_t size);

#endif
