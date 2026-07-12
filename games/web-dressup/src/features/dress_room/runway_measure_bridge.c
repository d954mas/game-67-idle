#include "features/dress_room/runway_measure_bridge.h"

#include "features/dress_room/dress_room_events.h"
#include "features/platform_sdk/platform_sdk_measure.h"
#include "game_events.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define RUNWAY_MEASURE_PENDING_CAP 16
#define RUNWAY_MEASURE_TOKEN_CAP 33

typedef struct runway_measure_triple {
    char category[RUNWAY_MEASURE_TOKEN_CAP];
    char what[RUNWAY_MEASURE_TOKEN_CAP];
    char action[RUNWAY_MEASURE_TOKEN_CAP];
} runway_measure_triple_t;

static uint8_t s_round_start_mask;
static uint8_t s_round_complete_mask;
static uint8_t s_recipe_mask;
static uint8_t s_mastery_mask;
static runway_measure_triple_t s_pending[RUNWAY_MEASURE_PENDING_CAP];
static int s_pending_count;

static const char *const s_recipe_ids[6] = {
    "moon-moon", "bloom-bloom", "flame-flame",
    "moon-bloom", "moon-flame", "bloom-flame",
};

static void pending_remove_first(void) {
    if (s_pending_count <= 0) return;
    --s_pending_count;
    if (s_pending_count > 0) {
        memmove(&s_pending[0], &s_pending[1],
                (size_t)s_pending_count * sizeof(s_pending[0]));
    }
}

static void submit(const char *category, const char *what, const char *action) {
    const platform_sdk_result_t result = platform_sdk_measure(category, what, action);
    if (result != PLATFORM_SDK_RESULT_NOT_READY ||
        s_pending_count >= RUNWAY_MEASURE_PENDING_CAP) return;
    runway_measure_triple_t *triple = &s_pending[s_pending_count++];
    (void)snprintf(triple->category, sizeof triple->category, "%s", category);
    (void)snprintf(triple->what, sizeof triple->what, "%s", what);
    (void)snprintf(triple->action, sizeof triple->action, "%s", action);
}

static void flush_pending(void) {
    while (s_pending_count > 0) {
        const runway_measure_triple_t *triple = &s_pending[0];
        const platform_sdk_result_t result =
            platform_sdk_measure(triple->category, triple->what, triple->action);
        if (result == PLATFORM_SDK_RESULT_NOT_READY) return;
        pending_remove_first();
    }
}

static void measure_round(int round_index, const char *action, uint8_t *mask) {
    if (round_index < 1 || round_index > 8) return;
    const uint8_t bit = (uint8_t)(1u << (unsigned)(round_index - 1));
    if ((*mask & bit) != 0u) return;
    *mask |= bit;
    char what[3];
    (void)snprintf(what, sizeof what, "%d", round_index);
    submit("round", what, action);
}

static void handle_start(const dress_room_ev_awakening_start_t *event) {
    if (event == NULL || event->look_known != 0u) return;
    measure_round(event->round_index, "start", &s_round_start_mask);
}

static void handle_reveal(const dress_room_ev_recipe_reveal_t *event) {
    if (event == NULL || event->outcome == DRESS_ROOM_REVEAL_REPLAY) return;
    measure_round(event->round_index, "complete", &s_round_complete_mask);
    if (event->outcome != DRESS_ROOM_REVEAL_DISCOVERY ||
        event->recipe_index < 0 || event->recipe_index >= 6) return;
    const uint8_t bit = (uint8_t)(1u << (unsigned)event->recipe_index);
    if ((s_recipe_mask & bit) != 0u) return;
    s_recipe_mask |= bit;
    submit("recipe", s_recipe_ids[event->recipe_index], "discovered");
}

static void handle_mastery(const dress_room_ev_collection_mastery_t *event) {
    if (event == NULL) return;
    int index = -1;
    if (event->milestone == 1) index = 0;
    else if (event->milestone == 3) index = 1;
    else if (event->milestone == 6) index = 2;
    if (index < 0) return;
    const uint8_t bit = (uint8_t)(1u << (unsigned)index);
    if ((s_mastery_mask & bit) != 0u) return;
    s_mastery_mask |= bit;
    char what[2];
    (void)snprintf(what, sizeof what, "%d", event->milestone);
    submit("collection", what, "complete");
}

void runway_measure_bridge_init(void) {
    s_round_start_mask = 0u;
    s_round_complete_mask = 0u;
    s_recipe_mask = 0u;
    s_mastery_mask = 0u;
    s_pending_count = 0;
}

void runway_measure_bridge_record(void) {
    flush_pending();
    int count = 0;
    const game_event_t *events = game_event_log(&count);
    for (int i = 0; i < count; ++i) {
        const game_event_t *event = &events[i];
        if (event->type.value == dress_room_ev_awakening_start_type().value) {
            handle_start((const dress_room_ev_awakening_start_t *)event->payload);
        } else if (event->type.value == dress_room_ev_recipe_reveal_type().value) {
            handle_reveal((const dress_room_ev_recipe_reveal_t *)event->payload);
        } else if (event->type.value == dress_room_ev_collection_mastery_type().value) {
            handle_mastery((const dress_room_ev_collection_mastery_t *)event->payload);
        } else if (event->type.value == dress_room_ev_lookbook_open_type().value) {
            submit("lookbook", "main", "open");
        }
    }
}
