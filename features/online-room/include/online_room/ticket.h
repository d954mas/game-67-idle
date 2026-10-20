#ifndef ONLINE_ROOM_TICKET_H
#define ONLINE_ROOM_TICKET_H

// Version 2 tickets (features/online-room/CONTRACT.md): the
// metaserver's signed permission for one account to enter one room, and the
// room's own credential toward the metaserver. 34 bytes, little-endian:
//   version u8 (2) | kind u8 | subject u64 | room_id u32 | expires_at u32 | mac[16]
//   mac = the first 16 bytes of HMAC-SHA256(key, the 18 bytes before it)
// A player ticket's subject is the account key (the player id); a room
// ticket's is the room's incarnation (its start time in ms).

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if !defined(ONLINE_ROOM_PLAYERS_MAX)
#error "the online_room target defines ONLINE_ROOM_PLAYERS_MAX"
#endif

#define ONLINE_TICKET_VERSION 2U
#define ONLINE_TICKET_KIND_PLAYER 0x54U
#define ONLINE_TICKET_KIND_PAGE_ENTRY 0x50U
#define ONLINE_TICKET_KIND_ROOM 0x52U
#define ONLINE_TICKET_KEY_SIZE 32U
#define ONLINE_TICKET_MAC_SIZE 16U
#define ONLINE_TICKET_SIGNED_SIZE 18U
#define ONLINE_TICKET_SIZE (ONLINE_TICKET_SIGNED_SIZE + ONLINE_TICKET_MAC_SIZE)
// Clock disagreement tolerated with the metaserver, each way.
#define ONLINE_TICKET_SKEW_SECONDS 5U
// A room bearer's life; the metaserver refuses one longer than 300 s.
#define ONLINE_TICKET_BEARER_SECONDS 60U
// A bearer: the ticket's bytes in lowercase hex.
#define ONLINE_TICKET_BEARER_CHARS (2U * ONLINE_TICKET_SIZE)

typedef struct online_ticket {
    uint8_t kind;
    uint64_t subject;
    uint32_t room_id;
    uint32_t expires_at; // unix seconds
} online_ticket;

typedef enum online_ticket_result {
    ONLINE_TICKET_OK,
    ONLINE_TICKET_MALFORMED,  // wrong size or version
    ONLINE_TICKET_WRONG_KIND, // not a player ticket
    ONLINE_TICKET_FORGED,     // mac mismatch
    ONLINE_TICKET_WRONG_ROOM,
    ONLINE_TICKET_EXPIRED,
    ONLINE_TICKET_NO_SUBJECT,
    ONLINE_TICKET_REPEATED, // accepted before, and not expired yet
    ONLINE_TICKET_CAPPED,   // its account holds as many live tickets as one may
    ONLINE_TICKET_FULL,     // no room left to remember one more
    ONLINE_TICKET_RESULT_COUNT
} online_ticket_result;

void online_ticket_encode(const online_ticket *ticket, const uint8_t key[ONLINE_TICKET_KEY_SIZE], uint8_t out[ONLINE_TICKET_SIZE]);
// A player ticket for this room, checked in the contract's order: size and
// version, kind, mac (constant time), room, expiry (with the skew), subject.
online_ticket_result online_ticket_verify(const uint8_t key[ONLINE_TICKET_KEY_SIZE], uint32_t room_id, const uint8_t *data, size_t size,
    uint32_t now, online_ticket *out);
// The room's bearer for a metaserver call: a room ticket minted now.
void online_ticket_bearer(const uint8_t key[ONLINE_TICKET_KEY_SIZE], uint32_t room_id, uint64_t incarnation, uint32_t now,
    char out[ONLINE_TICKET_BEARER_CHARS + 1U]);
// Key file contents: 64 hex digits, trailing whitespace ignored.
bool online_ticket_key_parse(const char *text, uint8_t key[ONLINE_TICKET_KEY_SIZE]);

// Tickets that seated someone and are still inside their expiry (plus the
// skew): a repeat of one is refused, so a ticket seen on the wire cannot seat
// anyone again. One account holds at most ONLINE_TICKET_PER_ACCOUNT of them, so
// no account can fill the memory for the others; a memory full anyway
// refuses rather than forgets, and counts it.
#define ONLINE_TICKET_SEEN_MAX (16U * ONLINE_ROOM_PLAYERS_MAX)
#define ONLINE_TICKET_PER_ACCOUNT 4U
typedef struct online_ticket_seen {
    uint8_t macs[ONLINE_TICKET_SEEN_MAX][ONLINE_TICKET_MAC_SIZE];
    uint64_t subjects[ONLINE_TICKET_SEEN_MAX];
    uint32_t until[ONLINE_TICKET_SEEN_MAX]; // 0: a free entry
    uint64_t full; // tickets refused for a full memory
} online_ticket_seen;

// Verifies as online_ticket_verify, then refuses a repeat, a capped account or a
// full memory, without remembering anything: a ticket counts as used only
// once it seats someone (online_ticket_remember). `out` names every genuine
// ticket, refused or not.
online_ticket_result online_ticket_check(online_ticket_seen *seen, const uint8_t key[ONLINE_TICKET_KEY_SIZE], uint32_t room_id, const uint8_t *data,
    size_t size, uint32_t now, online_ticket *out);
// Remembers a ticket online_ticket_check passed at the same `now`.
void online_ticket_remember(online_ticket_seen *seen, const uint8_t *data, const online_ticket *ticket, uint32_t now);
// Check and remember at once.
online_ticket_result online_ticket_admit(online_ticket_seen *seen, const uint8_t key[ONLINE_TICKET_KEY_SIZE], uint32_t room_id, const uint8_t *data,
    size_t size, uint32_t now, online_ticket *out);

#endif
