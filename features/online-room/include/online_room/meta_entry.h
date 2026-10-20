#ifndef ONLINE_ROOM_META_ENTRY_H
#define ONLINE_ROOM_META_ENTRY_H

// The client's way into a room through the metaserver
// (features/online-room/CONTRACT.md, "Entry"): a session (player
// id + secret) kept in the device's storage, and for every try a version 2
// ticket bought with it at POST /api/enter, which names the room's address
// and the account the room seats. The session is a guest's until a login
// attaches to its player. Only the metaserver's own word that it no longer
// knows the secret ({"error":"unknown_session"}) starts over as a new guest,
// and the old record is kept aside until the new guest has entered; any other
// 401 (a proxy, a broken deploy) fails the try and keeps the session. A
// ticket seats once, so the connection ladder (online_room/entry.h) buys a new one
// for every try.
//
// One machine, pumped per frame. The HTTP and the storage are the host's
// (the engine's client and the save storage); tests pass canned ones.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// A version 2 ticket's bytes (online_room/ticket.h holds its layout;
// a client includes no sized server header).
#define ONLINE_META_ENTRY_TICKET_SIZE 34U
#define ONLINE_META_ENTRY_URL_MAX 256U
#define ONLINE_META_ENTRY_SECRET_MAX 64U
#define ONLINE_META_ENTRY_LOGIN_MAX 24U
// The storage slot of the session with the build's own metaserver, shared
// with the first client so a device keeps its player across both. Another
// metaserver's session goes to a slot of its own (online_meta_entry_slot).
#define ONLINE_META_ENTRY_SLOT "guest"
#define ONLINE_META_ENTRY_SLOT_MAX 32U
// The session a start over replaced, kept until the new guest has entered.
#define ONLINE_META_ENTRY_PREVIOUS_SUFFIX "_previous"
// Past this an entry step counts the metaserver as down and the connection
// ladder takes over.
#define ONLINE_META_ENTRY_TIMEOUT_MS 5000U

// `begin` POSTs `url` with an optional JSON body and an optional bearer, giving
// up after `timeout_ms`; 0 when nothing was sent. `poll` is true once the call is over: `status` 0 for no answer,
// `body` malloc'd (NULL when empty) for the caller to free. `cancel`
// forgets a call.
typedef struct online_meta_entry_http {
    void *user;
    uint32_t (*begin)(void *user, const char *url, const char *bearer, const char *json, uint32_t timeout_ms);
    bool (*poll)(void *user, uint32_t handle, uint16_t *status, uint8_t **body, size_t *size);
    void (*cancel)(void *user, uint32_t handle);
} online_meta_entry_http;

// Stored text by slot: `load` returns a malloc'd string or NULL.
typedef struct online_meta_entry_storage {
    void *user;
    char *(*load)(void *user, const char *slot);
    void (*store)(void *user, const char *slot, const char *text);
} online_meta_entry_storage;

typedef enum online_meta_entry_state {
    ONLINE_META_ENTRY_IDLE,
    ONLINE_META_ENTRY_BUSY,
    ONLINE_META_ENTRY_READY,  // room_url and ticket name the room to connect to
    ONLINE_META_ENTRY_FAILED, // status and failure say why
} online_meta_entry_state;

typedef enum online_meta_entry_step {
    ONLINE_META_ENTRY_STEP_NONE,
    ONLINE_META_ENTRY_STEP_GUEST,
    ONLINE_META_ENTRY_STEP_ENTER,
} online_meta_entry_step;

typedef struct online_meta_entry {
    online_meta_entry_http http;
    online_meta_entry_storage storage;
    char meta_url[ONLINE_META_ENTRY_URL_MAX]; // http(s)://host[:port], no trailing slash
    bool foreign; // chosen by whoever wrote the page URL: its sessions are not stored
    bool page_entry; // the page has not yet received an authoritative seat
    char slot[ONLINE_META_ENTRY_SLOT_MAX];
    char previous[ONLINE_META_ENTRY_SLOT_MAX + sizeof ONLINE_META_ENTRY_PREVIOUS_SUFFIX];
    bool starting_over; // a guest replaces an unknown session; the old record waits in `previous`
    uint64_t player_id;
    char secret[ONLINE_META_ENTRY_SECRET_MAX];
    bool has_session;
    bool account; // the session belongs to a login
    bool signed_out; // an account's session was forgotten by the metaserver: this device is a guest again
    char login[ONLINE_META_ENTRY_LOGIN_MAX];
    char room_url[ONLINE_META_ENTRY_URL_MAX];
    uint32_t room_id;
    uint8_t ticket[ONLINE_META_ENTRY_TICKET_SIZE];
    size_t ticket_size;
    uint64_t ticket_expires_at; // unix seconds on the metaserver's clock
    online_meta_entry_state state;
    online_meta_entry_step step;
    uint32_t request;
    uint32_t hops;   // calls since begin, bounding the fallbacks
    uint16_t status; // of the last answer, 0 for none
    const char *failure;
} online_meta_entry;

// The storage slot of the session with `meta_url`: ONLINE_META_ENTRY_SLOT for the
// build's own metaserver (`home`), else one named by a hash of the url, so a
// developer's local metaserver never overwrites the session a device keeps
// with the real one.
void online_meta_entry_slot(const char *meta_url, const char *home, char out[ONLINE_META_ENTRY_SLOT_MAX]);
// Loads the stored session, if any, for `meta_url` from `slot` (NULL:
// ONLINE_META_ENTRY_SLOT); `foreign` says the page URL chose the metaserver,
// whose session then stays out of the storage.
void online_meta_entry_init(online_meta_entry *entry, const char *meta_url, bool foreign, const online_meta_entry_http *http,
    const online_meta_entry_storage *storage, const char *slot);
bool online_meta_entry_enabled(const online_meta_entry *entry);
// A ticket for a new try: an enter, or first a guest when no session is stored.
void online_meta_entry_begin(online_meta_entry *entry);
// Hears the call in flight.
online_meta_entry_state online_meta_entry_update(online_meta_entry *entry);
// One answer for the call in flight: `status` 0 for no answer.
void online_meta_entry_feed(online_meta_entry *entry, uint16_t status, const uint8_t *body, size_t size);
// Drops the call in flight; the machine goes IDLE.
void online_meta_entry_cancel(online_meta_entry *entry);

// The session a login issued: stored, and the next try enters with it; false
// when the secret is not one the metaserver mints.
bool online_meta_entry_set_identity(online_meta_entry *entry, uint64_t player_id, const char *secret, const char *login);
// The stored session's player got a login.
void online_meta_entry_note_login(online_meta_entry *entry, const char *login);
// Drops the stored session: the next try starts as a new guest.
void online_meta_entry_forget(online_meta_entry *entry);
// A JSON number that is a whole id, 1..2^53.
typedef struct cJSON cJSON;
bool online_meta_entry_json_id(const cJSON *node, uint64_t *out);

#endif
