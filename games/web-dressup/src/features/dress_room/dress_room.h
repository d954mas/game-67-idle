#ifndef FEATURES_DRESS_ROOM_H
#define FEATURES_DRESS_ROOM_H
// feature-layer: L2
/* Dress Room freeplay + opt-in Fake Show (single-player, no network). */
#include <stdbool.h>
#include <stdint.h>

typedef enum dress_slot {
    DRESS_SLOT_HAIR = 0,
    DRESS_SLOT_TOP,
    DRESS_SLOT_BOTTOM,
    DRESS_SLOT_SHOES,
    DRESS_SLOT_ACC,
    DRESS_SLOT_COUNT
} dress_slot_t;

typedef enum dress_theme {
    DRESS_THEME_CASUAL = 0,
    DRESS_THEME_STREET,
    DRESS_THEME_GLAM,
    DRESS_THEME_ELEGANT,
    DRESS_THEME_Y2K,
    DRESS_THEME_COUNT
} dress_theme_t;

typedef enum dress_mode {
    DRESS_MODE_FREEPLAY = 0,
    DRESS_MODE_THEME_PICK,
    DRESS_MODE_SHOW_RUNWAY,
    DRESS_MODE_SHOW_PODIUM
} dress_mode_t;

typedef struct dress_item {
    const char *id;
    const char *label;
    dress_slot_t slot;
    uint32_t tint_abgr; /* catalog chrome fallback 0xAABBGGRR */
    uint32_t theme_mask; /* bit i = DRESS_THEME_i */
    const char *atlas_layer; /* region name in dress atlas (layer png) */
    const char *atlas_thumb; /* region name for catalog thumb */
} dress_item_t;

void dress_room_init(void);

/* Pure outfit API (unit-tested). */
void dress_room_reset_outfit(void);
/* seed==0 keeps internal RNG; seed!=0 reseeds then rolls (deterministic). */
void dress_room_randomize_outfit(uint32_t seed);
bool dress_room_equip(int item_index);
int dress_room_equipped(dress_slot_t slot);
void dress_room_set_category(dress_slot_t slot);
dress_slot_t dress_room_category(void);
int dress_room_catalog_count(void);
const dress_item_t *dress_room_catalog_item(int index);
const char *dress_room_slot_label(dress_slot_t slot);

/* Theme + Fake Show (pure scoring is unit-tested). */
void dress_room_set_theme(dress_theme_t theme);
dress_theme_t dress_room_theme(void);
void dress_room_random_theme(uint32_t seed);
const char *dress_room_theme_label(dress_theme_t theme);
uint32_t dress_room_theme_mask(dress_theme_t theme);

/* Score outfit fixture: equipped[slot]=catalog index or -1; returns 1..5 stars. */
int dress_room_score_outfit(const int equipped[DRESS_SLOT_COUNT], dress_theme_t theme, uint32_t seed);
int dress_room_score_current(uint32_t seed);

/* Show state machine (UI-driven). Theme is selected on freeplay; begin_show uses it. */
dress_mode_t dress_room_mode(void);
/* Deprecated path: returns to freeplay (theme chips live on dress room). */
void dress_room_enter_theme_pick(void);
void dress_room_begin_show(void);
void dress_room_show_advance(void); /* runway -> podium, podium -> freeplay */
void dress_room_return_freeplay(void);
float dress_room_show_t(void); /* 0..1 within current show phase for UI */
void dress_room_show_tick(float dt);
int dress_room_player_stars(void);
int dress_room_rival_stars(int rival_index); /* 0..2 */
int dress_room_player_rank(void); /* 1..4 */
/* Rival outfit after begin_show; catalog index or -1. */
int dress_room_rival_equipped(int rival_index, dress_slot_t slot);

/* UI */
struct nt_ui_context;
void dress_room_draw_ui(struct nt_ui_context *ctx, bool interactive);

#endif /* FEATURES_DRESS_ROOM_H */
