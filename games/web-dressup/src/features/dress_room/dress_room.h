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

typedef enum dress_essence {
    DRESS_ESSENCE_NONE = 0,
    DRESS_ESSENCE_MOON,
    DRESS_ESSENCE_BLOOM,
    DRESS_ESSENCE_FLAME,
    DRESS_ESSENCE_COUNT
} dress_essence_t;

typedef struct dress_essence_meta {
    const char *id;
    const char *label;
    uint32_t color_abgr;
} dress_essence_meta_t;

typedef struct dress_awakening_recipe {
    const char *id;
    const char *label;
    dress_essence_t first;
    dress_essence_t second;
} dress_awakening_recipe_t;

typedef enum dress_awakening_phase {
    DRESS_AWAKENING_IDLE = 0,
    DRESS_AWAKENING_INTRO,
    DRESS_AWAKENING_CHARGE,
    DRESS_AWAKENING_FLASH,
    DRESS_AWAKENING_REVEAL,
    DRESS_AWAKENING_VICTORY,
    DRESS_AWAKENING_RECIPE_CARD
} dress_awakening_phase_t;

typedef struct dress_item {
    const char *id;
    const char *label;
    dress_slot_t slot;
    uint32_t tint_abgr; /* catalog chrome fallback 0xAABBGGRR */
    uint32_t theme_mask; /* bit i = DRESS_THEME_i */
    const char *atlas_layer; /* region name in dress atlas (layer png) */
    const char *atlas_thumb; /* region name for catalog thumb */
    dress_essence_t essence; /* focus ingredient; NONE for supporting pieces */
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

/* Runway Awakening domain. TOP is the primary focus and ACC the secondary. */
const dress_essence_meta_t *dress_room_essence_meta(dress_essence_t essence);
const char *dress_room_essence_label(dress_essence_t essence);
int dress_room_recipe_count(void);
const dress_awakening_recipe_t *dress_room_recipe_at(int index);
/* Recipe pairs are unordered: Moon+Bloom and Bloom+Moon return the same row. */
const dress_awakening_recipe_t *dress_room_recipe_for(dress_essence_t first,
                                                       dress_essence_t second);
dress_essence_t dress_room_primary_essence(void);
dress_essence_t dress_room_secondary_essence(void);
bool dress_room_focus_complete(void);
/* A focus pair opens a deliberate three-step styling pass: Hair -> Bottom ->
 * Shoes. Selecting the equipped support item confirms it; changing an earlier
 * decision invalidates the later decisions. */
bool dress_room_support_confirmed(dress_slot_t slot);
int dress_room_support_confirmed_count(void); /* 0..3 */
bool dress_room_support_complete(void);
uint8_t dress_room_support_mask(void); /* Hair=1, Bottom=2, Shoes=4 */
bool dress_room_main_covers_bottom(void);
bool dress_room_begin_awakening(void);
dress_awakening_phase_t dress_room_awakening_phase(void);
const dress_awakening_recipe_t *dress_room_awakening_recipe(void);
float dress_room_awakening_phase_duration(void);
float dress_room_awakening_phase_t(void);
void dress_room_awakening_tick(float dt);
bool dress_room_awakening_won(void); /* accepted awakenings always win */
bool dress_room_awakening_is_new(void);
bool dress_room_awakening_is_new_remix(void);
bool dress_room_awakening_lookbook_full(void);
int dress_room_awakening_saved_slot(void); /* 0..2 saved/existing, -1 when unsaved */
bool dress_room_skip_replay(void);
void dress_room_restyle(void);       /* return to styling, preserve outfit */
int dress_room_discovered_count(void); /* 0..6 recipe-card collection progress */
/* Persistent recipe progress yields deterministic unlocks at 1, 3, and 6.
 * milestone returns the highest reached target (0 before the first unlock).
 * next_target returns 1, 3, or 6; 0 means every collection unlock is reached. */
int dress_room_collection_milestone(void);
int dress_room_collection_next_target(void);
const dress_awakening_recipe_t *dress_room_next_undiscovered_recipe(void);
bool dress_room_current_recipe_is_discovered(void);
/* Legacy 3-signature compatibility surface for the current Phase-1 UI only.
 * saved_looks is the authored outfit truth for the Phase-2 Lookbook cutover. */
bool dress_room_current_look_is_recorded(void);
int dress_room_lookbook_count(void); /* compatibility count; not exact authorship */
/* UI boundary notification; emits a bounded, privacy-safe collection snapshot. */
void dress_room_lookbook_opened(void);
bool dress_room_lookbook_has(int recipe_index, int signature); /* compatibility-only */
/* Exact authored looks. Three stable slots per recipe, eighteen globally. */
int dress_room_saved_look_count(void);
int dress_room_saved_look_count_for_recipe(int recipe_index);
int dress_room_current_saved_slot(void); /* -1 when the exact five-item outfit is novel */
bool dress_room_saved_look_indices(int recipe_index, int saved_slot,
                                   int out_indices[DRESS_SLOT_COUNT]);
bool dress_room_equip_saved_look(int recipe_index, int saved_slot);
bool dress_room_save_current_look(void); /* first free slot for the current recipe */
bool dress_room_replace_saved_look(int recipe_index, int saved_slot);
bool dress_room_recipe_is_discovered(int recipe_index);
bool dress_room_prepare_recipe(int recipe_index);
/* Equip the next unknown Main + Accent pair, then open support styling. */
bool dress_room_prepare_next_undiscovered(void);
int dress_room_style_signature(void); /* 0 crown, 1 trail, 2 spark */
const char *dress_room_style_signature_label(void);

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
