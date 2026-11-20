#ifndef FEATURES_STEAM_PLATFORM_STEAM_NET_LINK_CLOSES_H
#define FEATURES_STEAM_PLATFORM_STEAM_NET_LINK_CLOSES_H

#include <stdbool.h>
#include <stdint.h>

/* The table of closes in hand, as pure decisions: which connection to close and with which
   code, which listen socket to close, when an entry leaves. Connections and sockets are opaque
   numbers (never zero), time is passed in, memory is fixed. steam_net_link_closing.cpp performs
   what these functions ask for and nothing else, so every rule here is tested without Steam.

   Steam's own linger is never asked for. A lingering connection dies without a word when its
   listen socket closes or the platform stops, and its peer then waits for a timeout; with the
   port hosted again the peer stayed connected and silent for as long as it was watched (45 s).
   A close that can wait no longer goes out at once instead, so its peer always reads the code
   and loses only what was not delivered. */

#ifdef __cplusplus
extern "C" {
#endif

enum { STEAM_NET_LINK_CLOSES_MAX = 32, STEAM_NET_LINK_ADDRESS_BYTES = 20 };

/* A listen socket's local address, compared byte for byte. */
typedef struct steam_net_link_address {
    uint8_t bytes[STEAM_NET_LINK_ADDRESS_BYTES];
} steam_net_link_address_t;

typedef struct steam_net_link_close_entry {
    bool used;
    bool told; /* Steam was asked to close it; only the hold remains */
    bool socket_orphaned; /* its host link is destroyed: the last entry on it closes it */
    uint32_t conn;
    uint32_t socket; /* zero for a guest */
    steam_net_link_address_t local;
    int code;
    int32_t timeout_ms;
    int64_t deadline_us;
    int64_t release_us;
} steam_net_link_close_entry_t;

typedef struct steam_net_link_closes {
    steam_net_link_close_entry_t entries[STEAM_NET_LINK_CLOSES_MAX];
} steam_net_link_closes_t;

typedef enum steam_net_link_close_do {
    STEAM_NET_LINK_DO_CLOSE = 1,  /* CloseConnection(handle, code), without Steam's linger */
    STEAM_NET_LINK_DO_FREE,       /* the peer already left: free the handle, no reason */
    STEAM_NET_LINK_DO_CLOSE_SOCKET, /* CloseListenSocket(handle) */
} steam_net_link_close_do_t;

typedef struct steam_net_link_close_action {
    steam_net_link_close_do_t what;
    uint32_t handle;
    int code;
} steam_net_link_close_action_t;

typedef struct steam_net_link_close_actions {
    steam_net_link_close_action_t items[2 * STEAM_NET_LINK_CLOSES_MAX + 4];
    uint32_t count;
} steam_net_link_close_actions_t;

/* What Steam says of a connection waiting for its acknowledgement. */
typedef enum steam_net_link_close_state {
    STEAM_NET_LINK_CLOSE_PENDING = 1, /* something is still queued or unacknowledged */
    STEAM_NET_LINK_CLOSE_ACKED,
    STEAM_NET_LINK_CLOSE_GONE, /* Steam no longer has it */
} steam_net_link_close_state_t;

typedef struct steam_net_link_close_probe {
    steam_net_link_close_state_t state;
    int32_t ping_ms;
} steam_net_link_close_probe_t;

typedef steam_net_link_close_probe_t (*steam_net_link_close_probe_fn)(void *user, uint32_t conn);

/* A close handed over at `now`: wait until `now + close_wait_us` for the acknowledgement. A full
   table first cuts the entry due first: it is closed at once. */
void steam_net_link_closes_take(steam_net_link_closes_t *closes, uint32_t conn, int code, int32_t timeout_ms, int64_t close_wait_us, uint32_t socket,
                                const steam_net_link_address_t *local, int64_t now_us, steam_net_link_close_actions_t *out);

/* Steam reported `conn` ended by the peer or by a problem: true when it was an entry waiting
   for its acknowledgement, which is then freed. */
bool steam_net_link_closes_peer_left(steam_net_link_closes_t *closes, uint32_t conn, steam_net_link_close_actions_t *out);

/* Once a frame: entries acknowledged or past their deadline close with their code, entries
   Steam no longer has are freed, holds that ran out leave. */
void steam_net_link_closes_frame(steam_net_link_closes_t *closes, int64_t now_us, steam_net_link_close_probe_fn probe, void *user, steam_net_link_close_actions_t *out);

/* The host link of `socket` is destroyed: true when an entry still needs the socket, which the
   last of them then closes. */
bool steam_net_link_closes_keep_socket(steam_net_link_closes_t *closes, uint32_t socket);

/* A destroyed host's socket kept for its closes: newcomers there are turned away. */
bool steam_net_link_closes_turns_away(const steam_net_link_closes_t *closes, uint32_t socket);

/* Hosting again on `local`: its kept entries are cut (those still waiting are closed at once)
   and their socket closed. True when there were any. */
bool steam_net_link_closes_take_port(steam_net_link_closes_t *closes, const steam_net_link_address_t *local, steam_net_link_close_actions_t *out);

bool steam_net_link_closes_any(const steam_net_link_closes_t *closes);

/* The platform stopped: Steam's shutdown already cut every connection and socket. */
void steam_net_link_closes_forget(steam_net_link_closes_t *closes);

/* Wait for the certificate, dial now, or give up (TRANSPORT): the deadline comes first, so a
   certificate that lands in the frame the deadline passes is too late. */
typedef enum steam_net_link_dial {
    STEAM_NET_LINK_DIAL_WAIT = 1,
    STEAM_NET_LINK_DIAL_NOW,
    STEAM_NET_LINK_DIAL_GIVE_UP,
} steam_net_link_dial_t;

typedef enum steam_net_link_certificate {
    STEAM_NET_LINK_CERTIFICATE_PENDING = 1,
    STEAM_NET_LINK_CERTIFICATE_CURRENT,
    STEAM_NET_LINK_CERTIFICATE_FAILED,
} steam_net_link_certificate_t;

steam_net_link_dial_t steam_net_link_dial_decision(int64_t now_us, int64_t deadline_us, steam_net_link_certificate_t certificate);

#ifdef __cplusplus
}
#endif

#endif
