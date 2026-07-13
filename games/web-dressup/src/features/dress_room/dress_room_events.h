#ifndef FEATURES_DRESS_ROOM_EVENTS_H
#define FEATURES_DRESS_ROOM_EVENTS_H
// feature-layer: L2

#include "game_event_desc.h"
#include "hash/nt_hash.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum dress_room_reveal_outcome {
    DRESS_ROOM_REVEAL_DISCOVERY = 0,
    DRESS_ROOM_REVEAL_REMIX = 1,
    DRESS_ROOM_REVEAL_REPLAY = 2,
} dress_room_reveal_outcome_t;

typedef struct dress_room_ev_awakening_start {
    int32_t recipe_index; /* 0..5 */
    int32_t support_mask; /* 0..7, confirmation state only */
    int32_t look_slot;    /* 0..2, or 3 when full/unsaved */
    int32_t round_index;  /* 1..8 MVP measurement window */
    unsigned char recipe_known;
    unsigned char look_known;
} dress_room_ev_awakening_start_t;

typedef struct dress_room_ev_recipe_reveal {
    int32_t recipe_index; /* 0..5 */
    int32_t support_mask; /* 0..7, confirmation state only */
    int32_t look_slot;    /* 0..2, or 3 when full/unsaved */
    int32_t round_index;  /* 1..8 MVP measurement window */
    int32_t outcome;      /* dress_room_reveal_outcome_t */
    int32_t recipes_found; /* 0..6 */
    int32_t looks_found;   /* 0..18 */
} dress_room_ev_recipe_reveal_t;

typedef struct dress_room_ev_lookbook_open {
    int32_t recipes_found; /* 0..6 */
    int32_t looks_found;   /* 0..18 */
} dress_room_ev_lookbook_open_t;

typedef struct dress_room_ev_collection_mastery {
    int32_t milestone;     /* one of 1, 3, 6 */
    int32_t recipes_found; /* 1..6 */
} dress_room_ev_collection_mastery_t;

nt_hash64_t dress_room_ev_awakening_start_type(void);
nt_hash64_t dress_room_ev_recipe_reveal_type(void);
nt_hash64_t dress_room_ev_lookbook_open_type(void);
nt_hash64_t dress_room_ev_collection_mastery_type(void);

extern const game_event_desc_t *const dress_room_ev_descs[];
extern const int dress_room_ev_desc_count;

void dress_room_events_register(void);
void dress_room_events_emit_awakening_start(int recipe_index, int support_mask, int look_slot,
                                            int round_index,
                                            bool recipe_known, bool look_known);
void dress_room_events_emit_recipe_reveal(int recipe_index, int support_mask, int look_slot,
                                          int round_index,
                                          dress_room_reveal_outcome_t outcome,
                                          int recipes_found, int looks_found);
void dress_room_events_emit_lookbook_open(int recipes_found, int looks_found);
void dress_room_events_emit_collection_mastery(int milestone, int recipes_found);

#endif
