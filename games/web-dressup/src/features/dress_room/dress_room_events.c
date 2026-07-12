#include "features/dress_room/dress_room_events.h"

#include "game_events.h"

#include <stddef.h>

#define FIELD_COUNT(fields) ((int)(sizeof(fields) / sizeof((fields)[0])))

static int bounded(int value, int maximum) {
    if (value < 0) return 0;
    return value > maximum ? maximum : value;
}

#define EVENT_TYPE(fn_name, event_name)          \
    nt_hash64_t fn_name(void) {                  \
        static nt_hash64_t type;                 \
        if (!type.value) type = nt_hash64_str(event_name); \
        return type;                             \
    }

EVENT_TYPE(dress_room_ev_awakening_start_type, "runway.awakening_start")
EVENT_TYPE(dress_room_ev_recipe_reveal_type, "runway.recipe_reveal")
EVENT_TYPE(dress_room_ev_lookbook_open_type, "runway.lookbook_open")
EVENT_TYPE(dress_room_ev_collection_mastery_type, "runway.collection_mastery")

static const game_event_field_t s_start_fields[] = {
    {"recipe_index", GAME_EVENT_FT_INT, (uint32_t)offsetof(dress_room_ev_awakening_start_t, recipe_index), 0u},
    {"support_mask", GAME_EVENT_FT_INT, (uint32_t)offsetof(dress_room_ev_awakening_start_t, support_mask), 0u},
    {"look_slot", GAME_EVENT_FT_INT, (uint32_t)offsetof(dress_room_ev_awakening_start_t, look_slot), 0u},
    {"round_index", GAME_EVENT_FT_INT, (uint32_t)offsetof(dress_room_ev_awakening_start_t, round_index), 0u},
    {"recipe_known", GAME_EVENT_FT_BOOL, (uint32_t)offsetof(dress_room_ev_awakening_start_t, recipe_known), 0u},
    {"look_known", GAME_EVENT_FT_BOOL, (uint32_t)offsetof(dress_room_ev_awakening_start_t, look_known), 0u},
};

static const game_event_field_t s_reveal_fields[] = {
    {"recipe_index", GAME_EVENT_FT_INT, (uint32_t)offsetof(dress_room_ev_recipe_reveal_t, recipe_index), 0u},
    {"support_mask", GAME_EVENT_FT_INT, (uint32_t)offsetof(dress_room_ev_recipe_reveal_t, support_mask), 0u},
    {"look_slot", GAME_EVENT_FT_INT, (uint32_t)offsetof(dress_room_ev_recipe_reveal_t, look_slot), 0u},
    {"round_index", GAME_EVENT_FT_INT, (uint32_t)offsetof(dress_room_ev_recipe_reveal_t, round_index), 0u},
    {"outcome", GAME_EVENT_FT_INT, (uint32_t)offsetof(dress_room_ev_recipe_reveal_t, outcome), 0u},
    {"recipes_found", GAME_EVENT_FT_INT, (uint32_t)offsetof(dress_room_ev_recipe_reveal_t, recipes_found), 0u},
    {"looks_found", GAME_EVENT_FT_INT, (uint32_t)offsetof(dress_room_ev_recipe_reveal_t, looks_found), 0u},
};

static const game_event_field_t s_lookbook_fields[] = {
    {"recipes_found", GAME_EVENT_FT_INT, (uint32_t)offsetof(dress_room_ev_lookbook_open_t, recipes_found), 0u},
    {"looks_found", GAME_EVENT_FT_INT, (uint32_t)offsetof(dress_room_ev_lookbook_open_t, looks_found), 0u},
};

static const game_event_field_t s_mastery_fields[] = {
    {"milestone", GAME_EVENT_FT_INT, (uint32_t)offsetof(dress_room_ev_collection_mastery_t, milestone), 0u},
    {"recipes_found", GAME_EVENT_FT_INT, (uint32_t)offsetof(dress_room_ev_collection_mastery_t, recipes_found), 0u},
};

static const game_event_desc_t s_start_desc = {
    "runway.awakening_start", sizeof(dress_room_ev_awakening_start_t), s_start_fields, FIELD_COUNT(s_start_fields)};
static const game_event_desc_t s_reveal_desc = {
    "runway.recipe_reveal", sizeof(dress_room_ev_recipe_reveal_t), s_reveal_fields, FIELD_COUNT(s_reveal_fields)};
static const game_event_desc_t s_lookbook_desc = {
    "runway.lookbook_open", sizeof(dress_room_ev_lookbook_open_t), s_lookbook_fields, FIELD_COUNT(s_lookbook_fields)};
static const game_event_desc_t s_mastery_desc = {
    "runway.collection_mastery", sizeof(dress_room_ev_collection_mastery_t), s_mastery_fields, FIELD_COUNT(s_mastery_fields)};

const game_event_desc_t *const dress_room_ev_descs[] = {
    &s_start_desc, &s_reveal_desc, &s_lookbook_desc, &s_mastery_desc,
};
const int dress_room_ev_desc_count = FIELD_COUNT(dress_room_ev_descs);

void dress_room_events_register(void) {
    game_event_register_type_name(dress_room_ev_awakening_start_type(), "runway.awakening_start");
    game_event_register_type_name(dress_room_ev_recipe_reveal_type(), "runway.recipe_reveal");
    game_event_register_type_name(dress_room_ev_lookbook_open_type(), "runway.lookbook_open");
    game_event_register_type_name(dress_room_ev_collection_mastery_type(), "runway.collection_mastery");
}

void dress_room_events_emit_awakening_start(int recipe_index, int support_mask, int look_slot,
                                            int round_index,
                                            bool recipe_known, bool look_known) {
    const dress_room_ev_awakening_start_t event = {
        bounded(recipe_index, 5), bounded(support_mask, 7), bounded(look_slot, 3),
        bounded(round_index - 1, 7) + 1,
        recipe_known ? 1u : 0u, look_known ? 1u : 0u};
    (void)game_event_emit(dress_room_ev_awakening_start_type(), &event, sizeof event, _Alignof(dress_room_ev_awakening_start_t));
}

void dress_room_events_emit_recipe_reveal(int recipe_index, int support_mask, int look_slot,
                                          int round_index,
                                          dress_room_reveal_outcome_t outcome,
                                          int recipes_found, int looks_found) {
    const int safe_outcome = outcome < DRESS_ROOM_REVEAL_DISCOVERY || outcome > DRESS_ROOM_REVEAL_REPLAY
                                 ? DRESS_ROOM_REVEAL_REPLAY : (int)outcome;
    const dress_room_ev_recipe_reveal_t event = {
        bounded(recipe_index, 5), bounded(support_mask, 7), bounded(look_slot, 3),
        bounded(round_index - 1, 7) + 1, safe_outcome,
        bounded(recipes_found, 6), bounded(looks_found, 18)};
    (void)game_event_emit(dress_room_ev_recipe_reveal_type(), &event, sizeof event, _Alignof(dress_room_ev_recipe_reveal_t));
}

void dress_room_events_emit_lookbook_open(int recipes_found, int looks_found) {
    const dress_room_ev_lookbook_open_t event = {bounded(recipes_found, 6), bounded(looks_found, 18)};
    (void)game_event_emit(dress_room_ev_lookbook_open_type(), &event, sizeof event, _Alignof(dress_room_ev_lookbook_open_t));
}

void dress_room_events_emit_collection_mastery(int milestone, int recipes_found) {
    const int safe_milestone = milestone >= 6 ? 6 : (milestone >= 3 ? 3 : 1);
    const dress_room_ev_collection_mastery_t event = {safe_milestone, bounded(recipes_found, 6)};
    (void)game_event_emit(dress_room_ev_collection_mastery_type(), &event, sizeof event, _Alignof(dress_room_ev_collection_mastery_t));
}
