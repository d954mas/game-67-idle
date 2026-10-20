#ifndef ONLINE_ROOM_MEMORY_STORE_H
#define ONLINE_ROOM_MEMORY_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "online_room/profile_store.h"

#if !defined(ONLINE_ROOM_PROFILE_TEXT_MAX)
#error "the online_room target defines ONLINE_ROOM_PROFILE_TEXT_MAX"
#endif

// The in-memory store, with the metaserver's verdicts: documents by account
// with CAS on rev, ownership by the room life that loaded last, and saves
// remembered by the id that wrote the current rev. Each room life is a view
// of it (online_memory_store_life). A load is answered at once unless
// `hold_loads`; a save is applied when first polled, unless `hold_saves`
// keeps it pending (a slow store); `fail_saves` fails every save applied
// while it is set (a store down, nothing written); `lose_answers` applies
// them and answers FAILED (landed, the answer lost); `size_limit` refuses
// larger documents.
#define ONLINE_MEMORY_STORE_ACCOUNTS 32U
#define ONLINE_MEMORY_STORE_REQUESTS 64U
#define ONLINE_MEMORY_STORE_LIVES 8U
// The owner and writer of a document put by online_memory_store_put.
#define ONLINE_MEMORY_STORE_ELSEWHERE 0xFFFFFFFFU

typedef struct online_memory_store online_memory_store;
typedef struct online_memory_life {
    online_memory_store *memory;
    uint32_t life;
} online_memory_life;

struct online_memory_store {
    uint64_t accounts[ONLINE_MEMORY_STORE_ACCOUNTS];
    int64_t revs[ONLINE_MEMORY_STORE_ACCOUNTS];
    size_t sizes[ONLINE_MEMORY_STORE_ACCOUNTS];
    uint32_t owners[ONLINE_MEMORY_STORE_ACCOUNTS];
    uint32_t writer_lives[ONLINE_MEMORY_STORE_ACCOUNTS];
    uint64_t writer_saves[ONLINE_MEMORY_STORE_ACCOUNTS];
    char docs[ONLINE_MEMORY_STORE_ACCOUNTS][ONLINE_ROOM_PROFILE_TEXT_MAX];
    struct {
        uint32_t id;
        uint32_t life;
        online_store_status status;
        online_store_answer answer;
        bool load;     // a load not run yet: account below
        bool save;     // a save not applied yet: account, if_rev, save_id and doc below
        uint64_t account;
        int64_t if_rev;
        uint64_t save_id;
        size_t size;
        char doc[ONLINE_ROOM_PROFILE_TEXT_MAX]; // a save's document, or a load's as it stood
    } requests[ONLINE_MEMORY_STORE_REQUESTS];
    online_memory_life lives[ONLINE_MEMORY_STORE_LIVES];
    uint32_t life_count;
    uint32_t next_id;
    bool fail_saves;
    bool hold_saves;
    bool hold_loads;
    bool lose_answers;
    size_t size_limit; // documents above it are refused, as too big; 0 for none
    uint64_t saves;
};

void online_memory_store_init(online_memory_store *memory);
// A new room life over the store: its own owner and writer identity.
void online_memory_store_life(online_memory_store *memory, online_profile_store *out);
// The stored document of `account` (NULL when none), for tests and tools.
const char *online_memory_store_doc(const online_memory_store *memory, uint64_t account, size_t *size, int64_t *rev);
// Stores `doc` as the account's current document at `rev`, as another room
// that loaded and saved it would: that room owns it now.
bool online_memory_store_put(online_memory_store *memory, uint64_t account, const char *doc, size_t size, int64_t rev);

#endif
