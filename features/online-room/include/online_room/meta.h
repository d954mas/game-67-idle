#ifndef ONLINE_ROOM_META_H
#define ONLINE_ROOM_META_H

// The room's calls to the metaserver (features/online-room/CONTRACT.md):
// the profile store (online_room/profile_store.h) over
// POST /rooms/profiles/{account}/load and POST /rooms/profiles/save
// (every save of a tick in one call), and the room's status over
// POST /rooms/status, each under a version 2 room bearer minted per call.
// The HTTP itself is a transport the host binds (the engine's client);
// tests answer from a table.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "online_room/profile_store.h"
#include "online_room/ticket.h"

#if !defined(ONLINE_ROOM_PARTING_MAX) || !defined(ONLINE_ROOM_PROFILE_TEXT_MAX)
#error "the online_room target defines ONLINE_ROOM_PARTING_MAX and ONLINE_ROOM_PROFILE_TEXT_MAX"
#endif

// `begin` returns a handle, 0 when the request could not start. `poll` is
// true once the request is over: `status` is the HTTP status, 0 for no
// answer (a transport failure, a timeout); `body` is malloc'd (NULL when
// empty), the caller frees it, and the handle is dead. `cancel` forgets a
// request whose answer nobody wants.
typedef struct online_meta_transport {
    void *user;
    void (*pump)(void *user);
    uint32_t (*begin)(void *user, const char *url, const char *bearer, const void *body, size_t size, uint32_t timeout_ms);
    bool (*poll)(void *user, uint32_t handle, uint16_t *status, uint8_t **body, size_t *size);
    void (*cancel)(void *user, uint32_t handle);
} online_meta_transport;

// Every call waits this long on the wire; the profile store's user gives each
// up at its own deadline as well.
#define ONLINE_META_TIMEOUT_MS 3000U
#define ONLINE_META_URL_MAX 192U
// Requests out at once: every seat's load or save, every last save (up to
// ONLINE_ROOM_PARTING_MAX), a status.
#define ONLINE_META_REQUESTS (ONLINE_ROOM_PLAYERS_MAX + ONLINE_ROOM_PARTING_MAX + 4U)
// The saves one call carries, and what they can weigh: each entry is
// account, if_rev, save id, size, then the document. The metaserver takes
// up to 96 entries in one call.
#define ONLINE_META_BATCH_MAX (ONLINE_ROOM_PLAYERS_MAX + ONLINE_ROOM_PARTING_MAX)
#define ONLINE_META_BATCH_ENTRY_HEAD 28U
#define ONLINE_META_BATCH_BYTES (ONLINE_META_BATCH_MAX * (ONLINE_META_BATCH_ENTRY_HEAD + ONLINE_ROOM_PROFILE_TEXT_MAX))
// What the transport must hold at once for no call to wait: every request,
// and the status. The room's HTTP is built with at least this many slots
// (src/meta_nt_http.c asserts it).
#define ONLINE_META_TRANSPORT_SLOTS (ONLINE_META_REQUESTS + 1U)
// The metaserver's cap on a status body.
#define ONLINE_META_STATUS_MAX_BODY 4096U

typedef struct online_meta_config {
    const online_meta_transport *transport;
    const char *base_url; // http(s)://host:port, no trailing slash
    uint8_t key[ONLINE_TICKET_KEY_SIZE];
    uint32_t room_id;
    uint64_t incarnation; // this process's start time in ms, never 0
    // Unix seconds for the bearers; NULL for the system clock.
    uint32_t (*clock)(void *user);
    void *clock_user;
} online_meta_config;

typedef enum online_meta_call {
    ONLINE_META_FREE,
    ONLINE_META_LOAD,
    ONLINE_META_SAVE,
    ONLINE_META_STATUS,
} online_meta_call;

typedef struct online_meta_stats {
    uint64_t calls, unauthorized, unexpected, status_refused;
} online_meta_stats;

// One call of saves: its transport handle and, once over, its answer,
// kept until every save in it has heard its own entry.
typedef struct online_meta_batch {
    uint32_t handle;  // 0 for a free entry
    uint32_t waiting; // saves in it not yet heard
    bool over;
    uint16_t status;
    uint8_t *body;
    size_t size;
} online_meta_batch;
typedef struct online_meta {
    online_meta_config config;
    char base_url[ONLINE_META_URL_MAX];
    struct {
        uint32_t id; // the store's request id, 0 for a free entry
        uint32_t handle;
        uint8_t call;
        // A save: its batch (index + 1; 0 while the call is not sent yet) and its entry in it.
        uint32_t batch;
        uint32_t entry;
    } requests[ONLINE_META_REQUESTS];
    online_meta_batch batches[ONLINE_META_REQUESTS];
    // The saves given since the last send.
    uint8_t pending[ONLINE_META_BATCH_BYTES];
    size_t pending_size;
    uint32_t pending_count;
    uint32_t next_id;
    uint32_t status_request; // the status in flight, 0 for none
    uint32_t status_round_trip_ms;
    uint64_t status_sent_ms;
    online_meta_stats stats;
} online_meta;

// The metaserver's url as the room is started with it. The authority is what
// follows "://" up to the first of / ? # \; a user:password@ in it ends at its
// last @, and the host is matched whole.
// Whether the room may send its bearer to `url`: https://, or http:// to this
// machine (127.0.0.1, localhost, [::1]) or the compose service `meta`, or any
// http:// when `insecure`.
bool online_meta_url_allowed(const char *url, bool insecure);
// `url` without its user:password@, as a log may show it.
void online_meta_url_loggable(const char *url, char *out, size_t cap);

// False when the base url does not fit.
bool online_meta_init(online_meta *meta, const online_meta_config *config);
// The profile store over this metaserver.
void online_meta_store(online_meta *meta, online_profile_store *out);
// Sends the room's status body unless one is still out; `now_ms` measures
// its round trip. False when it could not start.
bool online_meta_status_send(online_meta *meta, const char *body, size_t size, uint64_t now_ms);
// Hears the status in flight; a refused one (400: the room is fenced) is
// counted and logged.
void online_meta_status_poll(online_meta *meta, uint64_t now_ms);

#endif
