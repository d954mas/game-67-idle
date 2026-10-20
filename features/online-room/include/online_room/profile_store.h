#ifndef ONLINE_ROOM_PROFILE_STORE_H
#define ONLINE_ROOM_PROFILE_STORE_H

// The profile store a room loads and saves its players' documents through
// (features/online-room/CONTRACT.md): the metaserver's (online_room/meta.h),
// or the in-memory one (online_room/memory_store.h).

#include <stddef.h>
#include <stdint.h>

typedef enum online_store_status {
    ONLINE_STORE_PENDING,
    ONLINE_STORE_DONE,       // load: the document (none: size 0 at rev 0). save: landed at `rev`
    ONLINE_STORE_CONFLICT,   // save: the stored rev is not if_rev and nothing was written; `rev` is the stored one
    ONLINE_STORE_SUPERSEDED, // another room, or a newer life of this one, loaded the profile since
    ONLINE_STORE_FAILED,     // an error or no answer (a save may have landed or not), or no such account
    ONLINE_STORE_REFUSED,    // the store never takes this call: the document too big, or a malformed call
} online_store_status;

typedef struct online_store_answer {
    size_t size; // load: the document's bytes
    int64_t rev;
    // CONFLICT: the save id that wrote the stored rev, when this room life
    // wrote it; 0 for any other writer.
    uint64_t writer;
} online_store_answer;

// A request is 0 when it could not start. A save stores `doc` as rev
// if_rev + 1 only while the stored rev is if_rev (0: none stored yet) and
// this room life still owns the profile (it loaded it last). `save_id` is not
// 0, unique within this room life, and the same only on a byte-identical
// retry; a CONFLICT is never this attempt landing, even when it names it. A cancelled request is never answered; a save may still land.
// `send` puts every save given since the last send on its way, together;
// a save is heard only after it. NULL for a store whose saves go at once.
typedef struct online_profile_store {
    void *user;
    uint32_t (*load)(void *user, uint64_t account);
    uint32_t (*save)(void *user, uint64_t account, int64_t if_rev, uint64_t save_id, const char *doc, size_t size);
    online_store_status (*poll)(void *user, uint32_t request, char *doc, size_t cap, online_store_answer *answer);
    void (*cancel)(void *user, uint32_t request);
    void (*send)(void *user);
} online_profile_store;

#endif
