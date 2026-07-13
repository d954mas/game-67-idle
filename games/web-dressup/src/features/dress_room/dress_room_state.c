#include "features/dress_room/dress_room.h"
#include "features/dress_room/dress_room_events.h"
#include "game_save.h"
#include "game_state.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define TM_CASUAL (1u << DRESS_THEME_CASUAL)
#define TM_STREET (1u << DRESS_THEME_STREET)
#define TM_GLAM (1u << DRESS_THEME_GLAM)
#define TM_ELEGANT (1u << DRESS_THEME_ELEGANT)
#define TM_Y2K (1u << DRESS_THEME_Y2K)
#define TM_ALL (TM_CASUAL | TM_STREET | TM_GLAM | TM_ELEGANT | TM_Y2K)

static const dress_item_t s_catalog[] = {
    {"hair_bob", "Bob", DRESS_SLOT_HAIR, 0xFF5C3A2EU, TM_CASUAL | TM_Y2K | TM_STREET, "hair_bob", "hair_bob", DRESS_ESSENCE_NONE},
    {"hair_long", "Long", DRESS_SLOT_HAIR, 0xFF2A1A12U, TM_CASUAL | TM_ELEGANT | TM_GLAM, "hair_long", "hair_long", DRESS_ESSENCE_NONE},
    {"hair_pink", "Pink", DRESS_SLOT_HAIR, 0xFFC48CFFU, TM_Y2K | TM_GLAM | TM_STREET, "hair_pink", "hair_pink", DRESS_ESSENCE_NONE},
    {"hair_gold", "Gold", DRESS_SLOT_HAIR, 0xFF3AD4F0U, TM_GLAM | TM_ELEGANT | TM_Y2K, "hair_gold", "hair_gold", DRESS_ESSENCE_NONE},
    {"top_tee", "Crescent", DRESS_SLOT_TOP, 0xFF6EC8FFU, TM_CASUAL | TM_STREET | TM_Y2K, "top_tee", "top_tee", DRESS_ESSENCE_MOON},
    {"top_hoodie", "Verdant", DRESS_SLOT_TOP, 0xFF4A6B3CU, TM_STREET | TM_CASUAL | TM_Y2K, "top_hoodie", "top_hoodie", DRESS_ESSENCE_BLOOM},
    {"top_blazer", "Solar Fang", DRESS_SLOT_TOP, 0xFF3C2A24U, TM_ELEGANT | TM_GLAM, "top_blazer", "top_blazer", DRESS_ESSENCE_FLAME},
    {"top_crop", "Crop", DRESS_SLOT_TOP, 0xFF8A5CFFU, TM_Y2K | TM_GLAM | TM_STREET, "top_crop", "top_crop", DRESS_ESSENCE_MOON},
    {"bot_jeans", "Celestial Tide", DRESS_SLOT_BOTTOM, 0xFFB06A3CU, TM_CASUAL | TM_STREET | TM_Y2K, "bot_jeans", "bot_jeans", DRESS_ESSENCE_NONE},
    {"bot_skirt", "Verdant Petal", DRESS_SLOT_BOTTOM, 0xFFC878A0U, TM_GLAM | TM_Y2K | TM_ELEGANT, "bot_skirt", "bot_skirt", DRESS_ESSENCE_NONE},
    {"bot_shorts", "Astral Comet", DRESS_SLOT_BOTTOM, 0xFF5A7AB0U, TM_CASUAL | TM_STREET | TM_Y2K, "bot_shorts", "bot_shorts", DRESS_ESSENCE_NONE},
    {"bot_cargo", "Solar Cargo", DRESS_SLOT_BOTTOM, 0xFF3A5A3AU, TM_STREET | TM_CASUAL, "bot_cargo", "bot_cargo", DRESS_ESSENCE_NONE},
    {"bot_moonveil", "Moonveil", DRESS_SLOT_BOTTOM, 0xFFB67CFFU, TM_ELEGANT | TM_GLAM | TM_Y2K, "bot_moonveil", "bot_moonveil", DRESS_ESSENCE_NONE},
    {"bot_phoenix", "Phoenix Regalia", DRESS_SLOT_BOTTOM, 0xFF355CE8U, TM_GLAM | TM_ELEGANT | TM_STREET, "bot_phoenix", "bot_phoenix", DRESS_ESSENCE_NONE},
    {"shoe_sneak", "Lunar High-Tops", DRESS_SLOT_SHOES, 0xFFE8E8E8U, TM_CASUAL | TM_STREET | TM_Y2K, "shoe_sneak", "shoe_sneak", DRESS_ESSENCE_NONE},
    {"shoe_boot", "Verdant Boots", DRESS_SLOT_SHOES, 0xFF2C1C14U, TM_STREET | TM_ELEGANT | TM_GLAM, "shoe_boot", "shoe_boot", DRESS_ESSENCE_NONE},
    {"shoe_heel", "Solar Heels", DRESS_SLOT_SHOES, 0xFF4020C0U, TM_ELEGANT | TM_GLAM, "shoe_heel", "shoe_heel", DRESS_ESSENCE_NONE},
    {"shoe_sandal", "Crystal Sandals", DRESS_SLOT_SHOES, 0xFF70C8E0U, TM_CASUAL | TM_Y2K | TM_GLAM, "shoe_sandal", "shoe_sandal", DRESS_ESSENCE_NONE},
    {"shoe_eclipse", "Eclipse Platforms", DRESS_SLOT_SHOES, 0xFFB354E8U, TM_STREET | TM_GLAM | TM_Y2K, "shoe_eclipse", "shoe_eclipse", DRESS_ESSENCE_NONE},
    {"shoe_phoenix", "Phoenix Boots", DRESS_SLOT_SHOES, 0xFF365AF4U, TM_STREET | TM_ELEGANT | TM_GLAM, "shoe_phoenix", "shoe_phoenix", DRESS_ESSENCE_NONE},
    {"acc_glasses", "Astral Diadem", DRESS_SLOT_ACC, 0xFF202020U, TM_CASUAL | TM_STREET | TM_Y2K | TM_GLAM, "acc_glasses", "acc_glasses", DRESS_ESSENCE_MOON},
    {"acc_hat", "Bloom Crown", DRESS_SLOT_ACC, 0xFF2040E0U, TM_STREET | TM_CASUAL | TM_Y2K, "acc_hat", "acc_hat", DRESS_ESSENCE_BLOOM},
    {"acc_bag", "Solar Sigil", DRESS_SLOT_ACC, 0xFF1858A8U, TM_ELEGANT | TM_GLAM | TM_Y2K, "acc_bag", "acc_bag", DRESS_ESSENCE_FLAME},
    {"acc_scarf", "Ember Mantle", DRESS_SLOT_ACC, 0xFF2858FFU, TM_ELEGANT | TM_STREET | TM_GLAM, "acc_scarf", "acc_scarf", DRESS_ESSENCE_FLAME},
    {"acc_moon", "Orbit Veil", DRESS_SLOT_ACC, 0xFFE0B6FFU, TM_ELEGANT | TM_GLAM | TM_Y2K, "acc_moon", "acc_moon", DRESS_ESSENCE_MOON},
    {"acc_bloom", "Verdant Garland", DRESS_SLOT_ACC, 0xFF78DCA0U, TM_ELEGANT | TM_GLAM | TM_CASUAL, "acc_bloom", "acc_bloom", DRESS_ESSENCE_BLOOM},
};

static const dress_essence_meta_t s_essences[DRESS_ESSENCE_COUNT] = {
    {"none", "None", 0xFF808080U},
    {"moon", "Moon", 0xFFFFD7A8U},
    {"bloom", "Bloom", 0xFF8FE09BU},
    {"flame", "Flame", 0xFF526CFFU},
};

static const dress_awakening_recipe_t s_recipes[] = {
    {"moon-moon", "Lunar Oracle", DRESS_ESSENCE_MOON, DRESS_ESSENCE_MOON},
    {"bloom-bloom", "Garden Empress", DRESS_ESSENCE_BLOOM, DRESS_ESSENCE_BLOOM},
    {"flame-flame", "Solar Guardian", DRESS_ESSENCE_FLAME, DRESS_ESSENCE_FLAME},
    {"moon-bloom", "Dreamgarden Fae", DRESS_ESSENCE_MOON, DRESS_ESSENCE_BLOOM},
    {"moon-flame", "Eclipse Guardian", DRESS_ESSENCE_MOON, DRESS_ESSENCE_FLAME},
    {"bloom-flame", "Phoenix Rose", DRESS_ESSENCE_BLOOM, DRESS_ESSENCE_FLAME},
};

static const char *const s_slot_labels[DRESS_SLOT_COUNT] = {
    "Hair", "Top", "Bottom", "Shoes", "Acc",
};

static const char *const s_theme_labels[DRESS_THEME_COUNT] = {
    "Casual", "Street", "Glam", "Elegant", "Y2K",
};

static int s_equipped[DRESS_SLOT_COUNT];
static dress_slot_t s_category = DRESS_SLOT_TOP;
static uint32_t s_rng = 1u;
static dress_theme_t s_theme = DRESS_THEME_CASUAL;
static dress_mode_t s_mode = DRESS_MODE_FREEPLAY;
static float s_show_t = 0.0F;
static int s_player_stars = 3;
static int s_rival_stars[3] = {2, 3, 2};
static int s_player_rank = 1;
static int s_rival_equip[3][DRESS_SLOT_COUNT];
static dress_awakening_phase_t s_awakening_phase = DRESS_AWAKENING_IDLE;
static float s_awakening_elapsed = 0.0F;
static const dress_awakening_recipe_t *s_awakening_recipe = NULL;
static bool s_awakening_won = false;
static bool s_awakening_was_discovered = false;
static bool s_awakening_exact_look_was_saved = false;
static bool s_awakening_lookbook_full = false;
static int s_awakening_saved_slot = -1;
static bool s_awakening_progress_committed = false;
static int s_awakening_round_index = 1;
static bool s_persistence_ready = false;
static uint8_t s_support_mask = 0u;
static bool current_outfit_is_complete(void);

#define SUPPORT_HAIR_BIT 0x01u
#define SUPPORT_BOTTOM_BIT 0x02u
#define SUPPORT_SHOES_BIT 0x04u
#define SUPPORT_ALL_BITS (SUPPORT_HAIR_BIT | SUPPORT_BOTTOM_BIT | SUPPORT_SHOES_BIT)

static bool copy_state_id(char dst[GAME_STATE_STRING_MAX], const char *src) {
    const size_t len = src ? strlen(src) : 0u;
    if (!src || len >= GAME_STATE_STRING_MAX || strcmp(dst, src) == 0) {
        return false;
    }
    memcpy(dst, src, len + 1u);
    return true;
}

static const char *equipped_id(dress_slot_t slot) {
    const int index = s_equipped[slot];
    if (index < 0 || index >= (int)(sizeof s_catalog / sizeof s_catalog[0])) {
        return "none";
    }
    return s_catalog[index].id;
}

static void persist_outfit(bool count_as_first_equip) {
    if (!s_persistence_ready) {
        return;
    }
    char *const state_ids[DRESS_SLOT_COUNT] = {
        game_state.outfit_hair_id,
        game_state.outfit_main_id,
        game_state.outfit_bottom_id,
        game_state.outfit_shoes_id,
        game_state.outfit_accent_id,
    };
    bool changed = false;
    for (int slot = 0; slot < DRESS_SLOT_COUNT; ++slot) {
        changed = copy_state_id(state_ids[slot], equipped_id((dress_slot_t)slot)) || changed;
    }
    if (count_as_first_equip && !game_state.first_equip_done) {
        game_state.first_equip_done = true;
        changed = true;
    }
    if (changed) {
        game_save_mark_dirty();
    }
}

static int catalog_index_for_saved_id(const char *id, dress_slot_t slot) {
    if (!id || strcmp(id, "none") == 0) {
        return -1;
    }
    for (int i = 0; i < (int)(sizeof s_catalog / sizeof s_catalog[0]); ++i) {
        if (s_catalog[i].slot == slot && strcmp(s_catalog[i].id, id) == 0) {
            return i;
        }
    }
    return -2; /* invalid or wrong-slot id: retain the safe starter for this slot */
}

static void hydrate_outfit(void) {
    const char *const state_ids[DRESS_SLOT_COUNT] = {
        game_state.outfit_hair_id,
        game_state.outfit_main_id,
        game_state.outfit_bottom_id,
        game_state.outfit_shoes_id,
        game_state.outfit_accent_id,
    };
    bool has_saved_outfit = game_state.first_equip_done;
    for (int slot = 0; slot < DRESS_SLOT_COUNT && !has_saved_outfit; ++slot) {
        has_saved_outfit = strcmp(state_ids[slot], "none") != 0;
    }
    if (!has_saved_outfit) {
        return; /* pristine fragment: preserve the authored starter silhouette */
    }
    for (int slot = 0; slot < DRESS_SLOT_COUNT; ++slot) {
        const int index = catalog_index_for_saved_id(state_ids[slot], (dress_slot_t)slot);
        if (index >= -1) {
            s_equipped[slot] = index;
        }
    }
}

static int recipe_index_of(const dress_awakening_recipe_t *recipe) {
    for (int i = 0; i < (int)(sizeof s_recipes / sizeof s_recipes[0]); ++i) {
        if (&s_recipes[i] == recipe) {
            return i;
        }
    }
    return -1;
}

static int lookbook_bit_index(int recipe_index, int signature) {
    if (recipe_index < 0 || recipe_index >= 6 || signature < 0 || signature >= 3) {
        return -1;
    }
    return recipe_index * 3 + signature;
}

static void commit_awakening_progress(void) {
    if (s_awakening_progress_committed || !s_awakening_recipe) {
        return;
    }
    const int recipe_index = recipe_index_of(s_awakening_recipe);
    if (recipe_index < 0) {
        return;
    }
    const int old_milestone = dress_room_collection_milestone();
    const int recipe_bit = 1 << recipe_index;
    const int look_index = lookbook_bit_index(recipe_index, dress_room_style_signature());
    const int look_bit = look_index >= 0 ? 1 << look_index : 0;
    const bool saved_new_look = !s_awakening_exact_look_was_saved &&
                                !s_awakening_lookbook_full &&
                                dress_room_save_current_look();
    if (saved_new_look) {
        s_awakening_saved_slot = dress_room_current_saved_slot();
    }
    const int first_bit = 1 << ((int)s_awakening_recipe->first - 1);
    const int second_bit = 1 << ((int)s_awakening_recipe->second - 1);
    game_state.recipe_mask |= recipe_bit;
    game_state.lookbook_mask |= look_bit;
    game_state.essence_mask |= first_bit | second_bit;
    if (saved_new_look && game_state.rounds_completed < GAME_STATE_ROUNDS_COMPLETED_MAX) {
        ++game_state.rounds_completed;
    }
    const dress_room_reveal_outcome_t outcome = !s_awakening_was_discovered
        ? DRESS_ROOM_REVEAL_DISCOVERY
        : (!s_awakening_exact_look_was_saved ? DRESS_ROOM_REVEAL_REMIX
                                             : DRESS_ROOM_REVEAL_REPLAY);
    const int recipes_found = dress_room_discovered_count();
    const int looks_found = dress_room_saved_look_count();
    dress_room_events_emit_recipe_reveal(recipe_index, dress_room_support_mask(),
                                         s_awakening_saved_slot >= 0 ? s_awakening_saved_slot : 3,
                                         s_awakening_round_index, outcome,
                                         recipes_found, looks_found);
    const int new_milestone = dress_room_collection_milestone();
    if (outcome == DRESS_ROOM_REVEAL_DISCOVERY && new_milestone > old_milestone) {
        dress_room_events_emit_collection_mastery(new_milestone, recipes_found);
    }
    s_awakening_progress_committed = true;
    game_save_mark_dirty();
}

static float awakening_duration(dress_awakening_phase_t phase) {
    switch (phase) {
        case DRESS_AWAKENING_INTRO: return 0.65F;
        case DRESS_AWAKENING_CHARGE: return 0.85F;
        case DRESS_AWAKENING_FLASH: return 0.18F;
        case DRESS_AWAKENING_REVEAL: return 0.85F;
        case DRESS_AWAKENING_VICTORY: return 0.85F;
        case DRESS_AWAKENING_IDLE:
        case DRESS_AWAKENING_RECIPE_CARD:
        default: return 0.0F;
    }
}

static dress_essence_t equipped_essence(dress_slot_t slot) {
    const int index = dress_room_equipped(slot);
    const dress_item_t *item = dress_room_catalog_item(index);
    return item ? item->essence : DRESS_ESSENCE_NONE;
}

static uint32_t rng_next(void) {
    uint32_t x = s_rng ? s_rng : 1u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rng = x ? x : 1u;
    return s_rng;
}

static void fill_random_outfit(int out_equipped[DRESS_SLOT_COUNT], uint32_t *rng) {
    for (int slot = 0; slot < DRESS_SLOT_COUNT; ++slot) {
        int options[16];
        int n = 0;
        for (int i = 0; i < (int)(sizeof s_catalog / sizeof s_catalog[0]); ++i) {
            if ((int)s_catalog[i].slot == slot && n < 16) {
                options[n++] = i;
            }
        }
        if (n > 0) {
            uint32_t x = *rng ? *rng : 1u;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            *rng = x ? x : 1u;
            out_equipped[slot] = options[(int)(*rng % (uint32_t)n)];
        } else {
            out_equipped[slot] = -1;
        }
    }
}

void dress_room_init(void) {
    s_persistence_ready = false;
    dress_room_reset_outfit();
    hydrate_outfit();
    s_category = DRESS_SLOT_TOP;
    s_rng = 0xC0FFEEu;
    s_theme = DRESS_THEME_CASUAL;
    s_mode = DRESS_MODE_FREEPLAY;
    s_show_t = 0.0F;
    s_player_stars = 3;
    s_player_rank = 1;
    s_awakening_phase = DRESS_AWAKENING_IDLE;
    s_awakening_elapsed = 0.0F;
    s_awakening_recipe = NULL;
    s_awakening_won = false;
    s_awakening_was_discovered = false;
    s_awakening_exact_look_was_saved = false;
    s_awakening_lookbook_full = false;
    s_awakening_saved_slot = -1;
    s_awakening_progress_committed = false;
    s_awakening_round_index = 1;
    s_support_mask = 0u;
    s_persistence_ready = true;
}

void dress_room_reset_outfit(void) {
    s_support_mask = 0u;
    for (int i = 0; i < DRESS_SLOT_COUNT; ++i) {
        s_equipped[i] = -1;
    }
    /* Coherent starter look (first-frame product quality): prefer named ids that
       cover waist/hips when layered. Falls back to first catalog item per slot. */
    static const char *const preferred[DRESS_SLOT_COUNT] = {
        "hair_bob", NULL, "bot_jeans", "shoe_sneak", NULL,
    };
    for (int slot = 0; slot < DRESS_SLOT_COUNT; ++slot) {
        /* The two magical ingredients belong to the player.  Starting with
           them pre-equipped erases causality and makes AWAKEN feel random. */
        if (slot == DRESS_SLOT_TOP || slot == DRESS_SLOT_ACC) {
            s_equipped[slot] = -1;
            continue;
        }
        int found = -1;
        for (int i = 0; i < (int)(sizeof s_catalog / sizeof s_catalog[0]); ++i) {
            if ((int)s_catalog[i].slot != slot) {
                continue;
            }
            if (found < 0) {
                found = i; /* first-in-slot fallback */
            }
            if (preferred[slot] && s_catalog[i].id &&
                strcmp(s_catalog[i].id, preferred[slot]) == 0) {
                found = i;
                break;
            }
        }
        s_equipped[slot] = found;
    }
    persist_outfit(false);
}

void dress_room_randomize_outfit(uint32_t seed) {
    if (seed != 0u) {
        s_rng = seed;
    }
    fill_random_outfit(s_equipped, &s_rng);
    s_support_mask = 0u;
    persist_outfit(true);
}

bool dress_room_equip(int item_index) {
    if (item_index < 0 || item_index >= (int)(sizeof s_catalog / sizeof s_catalog[0])) {
        return false;
    }
    const dress_slot_t slot = s_catalog[item_index].slot;
    if (slot == DRESS_SLOT_TOP || slot == DRESS_SLOT_ACC) {
        s_support_mask = 0u;
    }
    if (slot == DRESS_SLOT_HAIR && dress_room_focus_complete()) {
        s_equipped[slot] = item_index;
        s_support_mask = SUPPORT_HAIR_BIT;
        s_category = DRESS_SLOT_BOTTOM;
        persist_outfit(true);
        return true;
    }
    if (slot == DRESS_SLOT_BOTTOM && dress_room_focus_complete()) {
        s_equipped[slot] = item_index;
        s_support_mask &= SUPPORT_HAIR_BIT;
        if ((s_support_mask & SUPPORT_HAIR_BIT) != 0u) {
            s_support_mask |= SUPPORT_BOTTOM_BIT;
            s_category = DRESS_SLOT_SHOES;
        } else {
            s_category = DRESS_SLOT_HAIR;
        }
        persist_outfit(true);
        return true;
    }
    if (slot == DRESS_SLOT_SHOES && dress_room_focus_complete()) {
        s_equipped[slot] = item_index;
        if ((s_support_mask & (SUPPORT_HAIR_BIT | SUPPORT_BOTTOM_BIT)) ==
            (SUPPORT_HAIR_BIT | SUPPORT_BOTTOM_BIT)) {
            s_support_mask |= SUPPORT_SHOES_BIT;
        } else if ((s_support_mask & SUPPORT_HAIR_BIT) != 0u) {
            s_category = DRESS_SLOT_BOTTOM;
        } else {
            s_category = DRESS_SLOT_HAIR;
        }
        persist_outfit(true);
        return true;
    }
    if (s_equipped[slot] == item_index) {
        s_equipped[slot] = -1;
        persist_outfit(true);
        return true;
    }
    s_equipped[slot] = item_index;
    if (slot == DRESS_SLOT_TOP) {
        s_category = DRESS_SLOT_ACC;
    } else if (slot == DRESS_SLOT_ACC && dress_room_focus_complete()) {
        s_category = DRESS_SLOT_HAIR;
    }
    persist_outfit(true);
    return true;
}

int dress_room_equipped(dress_slot_t slot) {
    if (slot < 0 || slot >= DRESS_SLOT_COUNT) {
        return -1;
    }
    return s_equipped[slot];
}

void dress_room_set_category(dress_slot_t slot) {
    if (slot >= 0 && slot < DRESS_SLOT_COUNT) {
        s_category = slot;
    }
}

dress_slot_t dress_room_category(void) { return s_category; }

int dress_room_catalog_count(void) { return (int)(sizeof s_catalog / sizeof s_catalog[0]); }

const dress_item_t *dress_room_catalog_item(int index) {
    if (index < 0 || index >= dress_room_catalog_count()) {
        return NULL;
    }
    return &s_catalog[index];
}

const char *dress_room_slot_label(dress_slot_t slot) {
    if (slot < 0 || slot >= DRESS_SLOT_COUNT) {
        return "?";
    }
    return s_slot_labels[slot];
}

const dress_essence_meta_t *dress_room_essence_meta(dress_essence_t essence) {
    if (essence < DRESS_ESSENCE_NONE || essence >= DRESS_ESSENCE_COUNT) {
        return NULL;
    }
    return &s_essences[essence];
}

const char *dress_room_essence_label(dress_essence_t essence) {
    const dress_essence_meta_t *meta = dress_room_essence_meta(essence);
    return meta ? meta->label : "?";
}

int dress_room_recipe_count(void) {
    return (int)(sizeof s_recipes / sizeof s_recipes[0]);
}

const dress_awakening_recipe_t *dress_room_recipe_at(int index) {
    if (index < 0 || index >= dress_room_recipe_count()) {
        return NULL;
    }
    return &s_recipes[index];
}

const dress_awakening_recipe_t *dress_room_recipe_for(dress_essence_t first,
                                                       dress_essence_t second) {
    if (first <= DRESS_ESSENCE_NONE || first >= DRESS_ESSENCE_COUNT ||
        second <= DRESS_ESSENCE_NONE || second >= DRESS_ESSENCE_COUNT) {
        return NULL;
    }
    for (int i = 0; i < dress_room_recipe_count(); ++i) {
        const dress_awakening_recipe_t *recipe = &s_recipes[i];
        if ((recipe->first == first && recipe->second == second) ||
            (recipe->first == second && recipe->second == first)) {
            return recipe;
        }
    }
    return NULL;
}

dress_essence_t dress_room_primary_essence(void) {
    return equipped_essence(DRESS_SLOT_TOP);
}

dress_essence_t dress_room_secondary_essence(void) {
    return equipped_essence(DRESS_SLOT_ACC);
}

bool dress_room_focus_complete(void) {
    return dress_room_recipe_for(dress_room_primary_essence(),
                                 dress_room_secondary_essence()) != NULL;
}

bool dress_room_support_confirmed(dress_slot_t slot) {
    uint8_t bit = 0u;
    if (slot == DRESS_SLOT_HAIR) bit = SUPPORT_HAIR_BIT;
    else if (slot == DRESS_SLOT_BOTTOM) bit = SUPPORT_BOTTOM_BIT;
    else if (slot == DRESS_SLOT_SHOES) bit = SUPPORT_SHOES_BIT;
    return bit != 0u && (s_support_mask & bit) != 0u;
}

int dress_room_support_confirmed_count(void) {
    int count = 0;
    count += (s_support_mask & SUPPORT_HAIR_BIT) != 0u;
    count += (s_support_mask & SUPPORT_BOTTOM_BIT) != 0u;
    count += (s_support_mask & SUPPORT_SHOES_BIT) != 0u;
    return count;
}

bool dress_room_support_complete(void) {
    return (s_support_mask & SUPPORT_ALL_BITS) == SUPPORT_ALL_BITS;
}

uint8_t dress_room_support_mask(void) { return s_support_mask & SUPPORT_ALL_BITS; }

bool dress_room_main_covers_bottom(void) {
    const dress_item_t *main = dress_room_catalog_item(dress_room_equipped(DRESS_SLOT_TOP));
    return main != NULL && main->id != NULL && strcmp(main->id, "top_tee") == 0;
}

bool dress_room_begin_awakening(void) {
    const dress_awakening_recipe_t *recipe =
        dress_room_recipe_for(dress_room_primary_essence(), dress_room_secondary_essence());
    if (!recipe || !dress_room_support_complete() || !current_outfit_is_complete()) {
        return false;
    }
    s_awakening_recipe = recipe;
    const int recipe_index = recipe_index_of(recipe);
    const int recipe_bit = recipe_index >= 0 ? (1 << recipe_index) : 0;
    s_awakening_was_discovered = recipe_bit != 0 &&
                                 (game_state.recipe_mask & recipe_bit) != 0;
    s_awakening_saved_slot = dress_room_current_saved_slot();
    s_awakening_exact_look_was_saved = s_awakening_saved_slot >= 0;
    if (!s_awakening_exact_look_was_saved) {
        int free_slot = -1;
        int ignored[DRESS_SLOT_COUNT];
        for (int slot = 0; slot < 3; ++slot) {
            if (!dress_room_saved_look_indices(recipe_index, slot, ignored)) {
                free_slot = slot;
                break;
            }
        }
        s_awakening_saved_slot = free_slot;
        s_awakening_lookbook_full = free_slot < 0;
    }
    s_awakening_phase = DRESS_AWAKENING_INTRO;
    s_awakening_elapsed = 0.0F;
    s_awakening_won = true;
    s_awakening_progress_committed = false;
    s_awakening_round_index = game_state.rounds_completed < 8
        ? game_state.rounds_completed + 1 : 8;
    s_mode = DRESS_MODE_SHOW_RUNWAY;
    s_show_t = 0.0F;
    dress_room_events_emit_awakening_start(recipe_index, dress_room_support_mask(),
                                           s_awakening_saved_slot >= 0 ? s_awakening_saved_slot : 3,
                                           s_awakening_round_index,
                                           s_awakening_was_discovered,
                                           s_awakening_exact_look_was_saved);
    return true;
}

dress_awakening_phase_t dress_room_awakening_phase(void) {
    return s_awakening_phase;
}

const dress_awakening_recipe_t *dress_room_awakening_recipe(void) {
    return s_awakening_recipe;
}

float dress_room_awakening_phase_duration(void) {
    return awakening_duration(s_awakening_phase);
}

float dress_room_awakening_phase_t(void) {
    if (s_awakening_phase == DRESS_AWAKENING_RECIPE_CARD) {
        return 1.0F;
    }
    const float duration = awakening_duration(s_awakening_phase);
    if (duration <= 0.0F) {
        return 0.0F;
    }
    const float t = s_awakening_elapsed / duration;
    return t < 0.0F ? 0.0F : (t > 1.0F ? 1.0F : t);
}

void dress_room_awakening_tick(float dt) {
    if (dt <= 0.0F || s_awakening_phase == DRESS_AWAKENING_IDLE ||
        s_awakening_phase == DRESS_AWAKENING_RECIPE_CARD) {
        return;
    }
    s_awakening_elapsed += dt;
    while (s_awakening_phase >= DRESS_AWAKENING_INTRO &&
           s_awakening_phase < DRESS_AWAKENING_RECIPE_CARD) {
        const float duration = awakening_duration(s_awakening_phase);
        if (s_awakening_elapsed < duration) {
            break;
        }
        s_awakening_elapsed -= duration;
        s_awakening_phase = (dress_awakening_phase_t)(s_awakening_phase + 1);
    }
    if (s_awakening_phase == DRESS_AWAKENING_RECIPE_CARD) {
        s_awakening_elapsed = 0.0F;
        s_mode = DRESS_MODE_SHOW_PODIUM;
        s_show_t = 0.0F;
        commit_awakening_progress();
    }
}

bool dress_room_awakening_won(void) {
    return s_awakening_won;
}

bool dress_room_awakening_is_new(void) {
    return s_awakening_recipe != NULL && !s_awakening_was_discovered;
}

bool dress_room_awakening_is_new_remix(void) {
    return s_awakening_recipe != NULL && s_awakening_was_discovered &&
           !s_awakening_exact_look_was_saved;
}

bool dress_room_awakening_lookbook_full(void) {
    return s_awakening_recipe != NULL && s_awakening_lookbook_full;
}

int dress_room_awakening_saved_slot(void) { return s_awakening_saved_slot; }

bool dress_room_skip_replay(void) {
    if (!s_awakening_recipe || !s_awakening_exact_look_was_saved ||
        s_awakening_phase == DRESS_AWAKENING_IDLE ||
        s_awakening_phase == DRESS_AWAKENING_RECIPE_CARD) {
        return false;
    }
    s_awakening_phase = DRESS_AWAKENING_RECIPE_CARD;
    s_awakening_elapsed = 0.0F;
    s_mode = DRESS_MODE_SHOW_PODIUM;
    s_show_t = 0.0F;
    commit_awakening_progress();
    return true;
}

void dress_room_restyle(void) {
    s_awakening_phase = DRESS_AWAKENING_IDLE;
    s_awakening_elapsed = 0.0F;
    s_awakening_recipe = NULL;
    s_awakening_won = false;
    s_awakening_was_discovered = false;
    s_awakening_exact_look_was_saved = false;
    s_awakening_lookbook_full = false;
    s_awakening_saved_slot = -1;
    s_equipped[DRESS_SLOT_TOP] = -1;
    s_equipped[DRESS_SLOT_ACC] = -1;
    s_support_mask = 0u;
    s_category = DRESS_SLOT_TOP;
    persist_outfit(true);
    dress_room_return_freeplay();
}

int dress_room_discovered_count(void) {
    uint32_t bits = (uint32_t)game_state.recipe_mask & 0x3Fu;
    int count = 0;
    while (bits != 0u) {
        count += (int)(bits & 1u);
        bits >>= 1u;
    }
    return count;
}

int dress_room_collection_milestone(void) {
    const int found = dress_room_discovered_count();
    if (found >= 6) {
        return 6;
    }
    if (found >= 3) {
        return 3;
    }
    if (found >= 1) {
        return 1;
    }
    return 0;
}

int dress_room_collection_next_target(void) {
    const int milestone = dress_room_collection_milestone();
    if (milestone < 1) {
        return 1;
    }
    if (milestone < 3) {
        return 3;
    }
    if (milestone < 6) {
        return 6;
    }
    return 0;
}

const dress_awakening_recipe_t *dress_room_next_undiscovered_recipe(void) {
    for (int i = 0; i < dress_room_recipe_count(); ++i) {
        if ((game_state.recipe_mask & (1 << i)) == 0) {
            return &s_recipes[i];
        }
    }
    return NULL;
}

bool dress_room_current_recipe_is_discovered(void) {
    const dress_awakening_recipe_t *recipe =
        dress_room_recipe_for(dress_room_primary_essence(), dress_room_secondary_essence());
    const int index = recipe_index_of(recipe);
    return index >= 0 && (game_state.recipe_mask & (1 << index)) != 0;
}

bool dress_room_current_look_is_recorded(void) {
    return dress_room_current_saved_slot() >= 0;
}

int dress_room_lookbook_count(void) {
    return dress_room_saved_look_count();
}

void dress_room_lookbook_opened(void) {
    dress_room_events_emit_lookbook_open(dress_room_discovered_count(),
                                         dress_room_saved_look_count());
}

bool dress_room_lookbook_has(int recipe_index, int signature) {
    const int look_index = lookbook_bit_index(recipe_index, signature);
    return look_index >= 0 && (game_state.lookbook_mask & (1 << look_index)) != 0;
}

static bool saved_look_key(char out[GAME_STATE_STRING_MAX], int recipe_index, int saved_slot) {
    if (!out || recipe_index < 0 || recipe_index >= dress_room_recipe_count() ||
        saved_slot < 0 || saved_slot >= 3) {
        return false;
    }
    const int written = snprintf(out, GAME_STATE_STRING_MAX, "%s/%d",
                                 s_recipes[recipe_index].id, saved_slot);
    return written > 0 && written < GAME_STATE_STRING_MAX;
}

static GameSavedLook *saved_look_entry(int recipe_index, int saved_slot) {
    char key[GAME_STATE_STRING_MAX];
    if (!saved_look_key(key, recipe_index, saved_slot)) {
        return NULL;
    }
    for (int i = 0; i < GAME_STATE_MAX_SAVED_LOOKS; ++i) {
        if (game_state.saved_looks[i].used &&
            strcmp(game_state.saved_looks[i].key, key) == 0) {
            return &game_state.saved_looks[i];
        }
    }
    return NULL;
}

static int saved_item_index(const char *id, dress_slot_t slot) {
    const int index = catalog_index_for_saved_id(id, slot);
    return index >= 0 ? index : -1;
}

bool dress_room_saved_look_indices(int recipe_index, int saved_slot,
                                   int out_indices[DRESS_SLOT_COUNT]) {
    if (!out_indices) {
        return false;
    }
    const GameSavedLook *look = saved_look_entry(recipe_index, saved_slot);
    if (!look) {
        return false;
    }
    const char *const ids[DRESS_SLOT_COUNT] = {
        look->hair_id, look->main_id, look->bottom_id, look->shoes_id, look->accent_id,
    };
    int resolved[DRESS_SLOT_COUNT];
    for (int slot = 0; slot < DRESS_SLOT_COUNT; ++slot) {
        resolved[slot] = saved_item_index(ids[slot], (dress_slot_t)slot);
        if (resolved[slot] < 0) {
            return false;
        }
    }
    const dress_awakening_recipe_t *stored_recipe =
        dress_room_recipe_for(s_catalog[resolved[DRESS_SLOT_TOP]].essence,
                              s_catalog[resolved[DRESS_SLOT_ACC]].essence);
    if (recipe_index_of(stored_recipe) != recipe_index) {
        return false;
    }
    memcpy(out_indices, resolved, sizeof resolved);
    return true;
}

int dress_room_saved_look_count(void) {
    int count = 0;
    int ignored[DRESS_SLOT_COUNT];
    for (int recipe = 0; recipe < dress_room_recipe_count(); ++recipe) {
        for (int slot = 0; slot < 3; ++slot) {
            if (dress_room_saved_look_indices(recipe, slot, ignored)) {
                ++count;
            }
        }
    }
    return count;
}

int dress_room_saved_look_count_for_recipe(int recipe_index) {
    if (recipe_index < 0 || recipe_index >= dress_room_recipe_count()) {
        return 0;
    }
    int count = 0;
    int ignored[DRESS_SLOT_COUNT];
    for (int slot = 0; slot < 3; ++slot) {
        if (dress_room_saved_look_indices(recipe_index, slot, ignored)) {
            ++count;
        }
    }
    return count;
}

static int current_recipe_index(void) {
    return recipe_index_of(dress_room_recipe_for(dress_room_primary_essence(),
                                                  dress_room_secondary_essence()));
}

static bool current_outfit_is_complete(void) {
    for (int slot = 0; slot < DRESS_SLOT_COUNT; ++slot) {
        if (dress_room_equipped((dress_slot_t)slot) < 0) {
            return false;
        }
    }
    return current_recipe_index() >= 0;
}

int dress_room_current_saved_slot(void) {
    const int recipe_index = current_recipe_index();
    if (recipe_index < 0 || !current_outfit_is_complete()) {
        return -1;
    }
    for (int saved_slot = 0; saved_slot < 3; ++saved_slot) {
        int indices[DRESS_SLOT_COUNT];
        if (!dress_room_saved_look_indices(recipe_index, saved_slot, indices)) {
            continue;
        }
        bool same = true;
        for (int slot = 0; slot < DRESS_SLOT_COUNT; ++slot) {
            same = same && indices[slot] == s_equipped[slot];
        }
        if (same) {
            return saved_slot;
        }
    }
    return -1;
}

static void write_current_saved_look(GameSavedLook *look, const char *key) {
    memset(look, 0, sizeof *look);
    look->used = true;
    (void)copy_state_id(look->key, key);
    char *const ids[DRESS_SLOT_COUNT] = {
        look->hair_id, look->main_id, look->bottom_id, look->shoes_id, look->accent_id,
    };
    for (int slot = 0; slot < DRESS_SLOT_COUNT; ++slot) {
        (void)copy_state_id(ids[slot], equipped_id((dress_slot_t)slot));
    }
}

bool dress_room_save_current_look(void) {
    const int recipe_index = current_recipe_index();
    if (recipe_index < 0 || !current_outfit_is_complete() ||
        dress_room_current_saved_slot() >= 0) {
        return false;
    }
    int saved_slot = -1;
    for (int slot = 0; slot < 3; ++slot) {
        if (!saved_look_entry(recipe_index, slot)) {
            saved_slot = slot;
            break;
        }
    }
    if (saved_slot < 0) {
        return false;
    }
    GameSavedLook *free_entry = NULL;
    for (int i = 0; i < GAME_STATE_MAX_SAVED_LOOKS; ++i) {
        if (!game_state.saved_looks[i].used) {
            free_entry = &game_state.saved_looks[i];
            break;
        }
    }
    char key[GAME_STATE_STRING_MAX];
    if (!free_entry || !saved_look_key(key, recipe_index, saved_slot)) {
        return false;
    }
    write_current_saved_look(free_entry, key);
    if (!s_awakening_progress_committed &&
        s_awakening_phase != DRESS_AWAKENING_RECIPE_CARD) {
        game_save_mark_dirty();
    }
    return true;
}

bool dress_room_replace_saved_look(int recipe_index, int saved_slot) {
    GameSavedLook *look = saved_look_entry(recipe_index, saved_slot);
    if (!look || recipe_index != current_recipe_index() || !current_outfit_is_complete()) {
        return false;
    }
    char key[GAME_STATE_STRING_MAX];
    if (!saved_look_key(key, recipe_index, saved_slot)) {
        return false;
    }
    write_current_saved_look(look, key);
    game_save_mark_dirty();
    return true;
}

bool dress_room_equip_saved_look(int recipe_index, int saved_slot) {
    int indices[DRESS_SLOT_COUNT];
    if (!dress_room_saved_look_indices(recipe_index, saved_slot, indices)) {
        return false;
    }
    memcpy(s_equipped, indices, sizeof indices);
    s_support_mask = SUPPORT_ALL_BITS;
    s_category = DRESS_SLOT_SHOES;
    persist_outfit(true);
    return true;
}

bool dress_room_recipe_is_discovered(int recipe_index) {
    return recipe_index >= 0 && recipe_index < dress_room_recipe_count() &&
           (game_state.recipe_mask & (1 << recipe_index)) != 0;
}

static int first_focus_item(dress_slot_t slot, dress_essence_t essence) {
    for (int i = 0; i < dress_room_catalog_count(); ++i) {
        if (s_catalog[i].slot == slot && s_catalog[i].essence == essence) {
            return i;
        }
    }
    return -1;
}

bool dress_room_prepare_recipe(int recipe_index) {
    const dress_awakening_recipe_t *recipe = dress_room_recipe_at(recipe_index);
    if (!recipe) {
        return false;
    }
    const int main = first_focus_item(DRESS_SLOT_TOP, recipe->first);
    const int accent = first_focus_item(DRESS_SLOT_ACC, recipe->second);
    if (main < 0 || accent < 0) {
        return false;
    }
    s_equipped[DRESS_SLOT_TOP] = main;
    s_equipped[DRESS_SLOT_ACC] = accent;
    s_support_mask = 0u;
    s_category = DRESS_SLOT_HAIR;
    persist_outfit(true);
    return true;
}

bool dress_room_prepare_next_undiscovered(void) {
    const dress_awakening_recipe_t *next = dress_room_next_undiscovered_recipe();
    return next ? dress_room_prepare_recipe(recipe_index_of(next)) : false;
}

static int slot_ordinal(dress_slot_t slot) {
    const int equipped = dress_room_equipped(slot);
    int ordinal = 0;
    for (int i = 0; i < dress_room_catalog_count(); ++i) {
        if (s_catalog[i].slot != slot) {
            continue;
        }
        if (i == equipped) {
            return ordinal;
        }
        ++ordinal;
    }
    return 0;
}

int dress_room_style_signature(void) {
    return (slot_ordinal(DRESS_SLOT_HAIR) + slot_ordinal(DRESS_SLOT_BOTTOM) +
            slot_ordinal(DRESS_SLOT_SHOES)) % 3;
}

const char *dress_room_style_signature_label(void) {
    static const char *const labels[] = {"Crown", "Trail", "Spark"};
    return labels[dress_room_style_signature()];
}

void dress_room_set_theme(dress_theme_t theme) {
    if (theme >= 0 && theme < DRESS_THEME_COUNT) {
        s_theme = theme;
    }
}

dress_theme_t dress_room_theme(void) { return s_theme; }

void dress_room_random_theme(uint32_t seed) {
    if (seed != 0u) {
        s_rng = seed;
    }
    s_theme = (dress_theme_t)(rng_next() % (uint32_t)DRESS_THEME_COUNT);
}

const char *dress_room_theme_label(dress_theme_t theme) {
    if (theme < 0 || theme >= DRESS_THEME_COUNT) {
        return "?";
    }
    return s_theme_labels[theme];
}

uint32_t dress_room_theme_mask(dress_theme_t theme) {
    if (theme < 0 || theme >= DRESS_THEME_COUNT) {
        return 0u;
    }
    return 1u << (unsigned)theme;
}

int dress_room_score_outfit(const int equipped[DRESS_SLOT_COUNT], dress_theme_t theme, uint32_t seed) {
    if (!equipped) {
        return 1;
    }
    const uint32_t bit = dress_room_theme_mask(theme);
    int filled = 0;
    int matched = 0;
    for (int s = 0; s < DRESS_SLOT_COUNT; ++s) {
        const int idx = equipped[s];
        if (idx < 0 || idx >= dress_room_catalog_count()) {
            continue;
        }
        filled += 1;
        if ((s_catalog[idx].theme_mask & bit) != 0u) {
            matched += 1;
        }
    }
    /* Deterministic mix: completeness + theme fit + small seeded spice in [0,1). */
    float score = 1.0F;
    score += (float)filled * 0.45F; /* 0..2.25 */
    score += (float)matched * 0.55F; /* 0..2.75 */
    uint32_t x = seed ? seed : 1u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    const float spice = (float)(x % 1000u) / 1000.0F; /* 0..0.999 */
    score += spice * 0.6F;
    if (score < 1.0F) {
        score = 1.0F;
    }
    if (score > 5.0F) {
        score = 5.0F;
    }
    int stars = (int)(score + 0.5F);
    if (stars < 1) {
        stars = 1;
    }
    if (stars > 5) {
        stars = 5;
    }
    return stars;
}

int dress_room_score_current(uint32_t seed) { return dress_room_score_outfit(s_equipped, s_theme, seed); }

dress_mode_t dress_room_mode(void) { return s_mode; }

void dress_room_enter_theme_pick(void) {
    /* Theme is chosen on freeplay; this no longer opens a separate screen.
       From show modes, return to freeplay so the player can change theme chips. */
    if (s_mode == DRESS_MODE_SHOW_PODIUM || s_mode == DRESS_MODE_SHOW_RUNWAY ||
        s_mode == DRESS_MODE_THEME_PICK) {
        s_mode = DRESS_MODE_FREEPLAY;
        s_show_t = 0.0F;
    }
}

void dress_room_begin_show(void) {
    /* Build 3 rival outfits + scores; player score from current outfit. */
    uint32_t r = s_rng ^ 0xA5A5A5A5u;
    for (int i = 0; i < 3; ++i) {
        fill_random_outfit(s_rival_equip[i], &r);
        s_rival_stars[i] = dress_room_score_outfit(s_rival_equip[i], s_theme, r ^ (0x1111u * (uint32_t)(i + 1)));
    }
    s_player_stars = dress_room_score_current(r ^ 0xBEEFu);
    /* Rank: count how many rivals beat or tie player. */
    int better = 0;
    for (int i = 0; i < 3; ++i) {
        if (s_rival_stars[i] > s_player_stars) {
            better += 1;
        } else if (s_rival_stars[i] == s_player_stars && i < 1) {
            better += 1; /* slight tie-break: first rival wins ties for drama */
        }
    }
    s_player_rank = better + 1;
    s_mode = DRESS_MODE_SHOW_RUNWAY;
    s_show_t = 0.0F;
    s_rng = r;
}

void dress_room_show_advance(void) {
    if (s_mode == DRESS_MODE_SHOW_RUNWAY) {
        s_mode = DRESS_MODE_SHOW_PODIUM;
        s_show_t = 0.0F;
    } else if (s_mode == DRESS_MODE_SHOW_PODIUM || s_mode == DRESS_MODE_THEME_PICK) {
        dress_room_return_freeplay();
    }
}

void dress_room_return_freeplay(void) {
    s_mode = DRESS_MODE_FREEPLAY;
    s_show_t = 0.0F;
}

float dress_room_show_t(void) { return s_show_t; }

void dress_room_show_tick(float dt) {
    if (s_mode != DRESS_MODE_SHOW_RUNWAY) {
        return;
    }
    s_show_t += dt / 2.4F;
    if (s_show_t >= 1.0F) {
        s_show_t = 1.0F;
        s_mode = DRESS_MODE_SHOW_PODIUM;
        s_show_t = 0.0F;
    }
}

int dress_room_player_stars(void) { return s_player_stars; }

int dress_room_rival_stars(int rival_index) {
    if (rival_index < 0 || rival_index > 2) {
        return 0;
    }
    return s_rival_stars[rival_index];
}

int dress_room_player_rank(void) { return s_player_rank; }

int dress_room_rival_equipped(int rival_index, dress_slot_t slot) {
    if (rival_index < 0 || rival_index > 2 || slot < 0 || slot >= DRESS_SLOT_COUNT) {
        return -1;
    }
    if (s_mode != DRESS_MODE_SHOW_RUNWAY && s_mode != DRESS_MODE_SHOW_PODIUM) {
        return -1;
    }
    return s_rival_equip[rival_index][slot];
}
