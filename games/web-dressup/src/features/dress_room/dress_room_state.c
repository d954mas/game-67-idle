#include "features/dress_room/dress_room.h"

#include <stddef.h>
#include <string.h>

#define TM_CASUAL (1u << DRESS_THEME_CASUAL)
#define TM_STREET (1u << DRESS_THEME_STREET)
#define TM_GLAM (1u << DRESS_THEME_GLAM)
#define TM_ELEGANT (1u << DRESS_THEME_ELEGANT)
#define TM_Y2K (1u << DRESS_THEME_Y2K)
#define TM_ALL (TM_CASUAL | TM_STREET | TM_GLAM | TM_ELEGANT | TM_Y2K)

static const dress_item_t s_catalog[] = {
    {"hair_bob", "Bob", DRESS_SLOT_HAIR, 0xFF5C3A2EU, TM_CASUAL | TM_Y2K | TM_STREET, "hair_bob", "hair_bob_full"},
    {"hair_long", "Long", DRESS_SLOT_HAIR, 0xFF2A1A12U, TM_CASUAL | TM_ELEGANT | TM_GLAM, "hair_long", "hair_long_full"},
    {"hair_pink", "Pink", DRESS_SLOT_HAIR, 0xFFC48CFFU, TM_Y2K | TM_GLAM | TM_STREET, "hair_pink", "hair_pink_full"},
    {"hair_gold", "Gold", DRESS_SLOT_HAIR, 0xFF3AD4F0U, TM_GLAM | TM_ELEGANT | TM_Y2K, "hair_gold", "hair_gold_full"},
    {"top_tee", "Tee", DRESS_SLOT_TOP, 0xFF6EC8FFU, TM_CASUAL | TM_STREET | TM_Y2K, "top_tee", "top_tee_full"},
    {"top_hoodie", "Hoodie", DRESS_SLOT_TOP, 0xFF4A6B3CU, TM_STREET | TM_CASUAL | TM_Y2K, "top_hoodie", "top_hoodie_full"},
    {"top_blazer", "Blazer", DRESS_SLOT_TOP, 0xFF3C2A24U, TM_ELEGANT | TM_GLAM, "top_blazer", "top_blazer_full"},
    {"top_crop", "Crop", DRESS_SLOT_TOP, 0xFF8A5CFFU, TM_Y2K | TM_GLAM | TM_STREET, "top_crop", "top_crop_full"},
    {"bot_jeans", "Jeans", DRESS_SLOT_BOTTOM, 0xFFB06A3CU, TM_CASUAL | TM_STREET | TM_Y2K, "bot_jeans", "bot_jeans_full"},
    {"bot_skirt", "Skirt", DRESS_SLOT_BOTTOM, 0xFFC878A0U, TM_GLAM | TM_Y2K | TM_ELEGANT, "bot_skirt", "bot_skirt_full"},
    {"bot_shorts", "Shorts", DRESS_SLOT_BOTTOM, 0xFF5A7AB0U, TM_CASUAL | TM_STREET | TM_Y2K, "bot_shorts", "bot_shorts_full"},
    {"bot_cargo", "Cargo", DRESS_SLOT_BOTTOM, 0xFF3A5A3AU, TM_STREET | TM_CASUAL, "bot_cargo", "bot_cargo_full"},
    {"shoe_sneak", "Sneaks", DRESS_SLOT_SHOES, 0xFFE8E8E8U, TM_CASUAL | TM_STREET | TM_Y2K, "shoe_sneak", "shoe_sneak_full"},
    {"shoe_boot", "Boots", DRESS_SLOT_SHOES, 0xFF2C1C14U, TM_STREET | TM_ELEGANT | TM_GLAM, "shoe_boot", "shoe_boot_full"},
    {"shoe_heel", "Heels", DRESS_SLOT_SHOES, 0xFF4020C0U, TM_ELEGANT | TM_GLAM, "shoe_heel", "shoe_heel_full"},
    {"shoe_sandal", "Sandals", DRESS_SLOT_SHOES, 0xFF70C8E0U, TM_CASUAL | TM_Y2K | TM_GLAM, "shoe_sandal", "shoe_sandal_full"},
    {"acc_glasses", "Glasses", DRESS_SLOT_ACC, 0xFF202020U, TM_CASUAL | TM_STREET | TM_Y2K | TM_GLAM, "acc_glasses", "acc_glasses_full"},
    {"acc_hat", "Hat", DRESS_SLOT_ACC, 0xFF2040E0U, TM_STREET | TM_CASUAL | TM_Y2K, "acc_hat", "acc_hat_full"},
    {"acc_bag", "Bag", DRESS_SLOT_ACC, 0xFF1858A8U, TM_ELEGANT | TM_GLAM | TM_Y2K, "acc_bag", "acc_bag_full"},
    {"acc_scarf", "Scarf", DRESS_SLOT_ACC, 0xFF28A0FFU, TM_ELEGANT | TM_CASUAL | TM_GLAM, "acc_scarf", "acc_scarf_full"},
};

static const char *const s_slot_labels[DRESS_SLOT_COUNT] = {
    "Hair", "Top", "Bottom", "Shoes", "Acc",
};

static const char *const s_theme_labels[DRESS_THEME_COUNT] = {
    "Casual", "Street", "Glam", "Elegant", "Y2K",
};

static int s_equipped[DRESS_SLOT_COUNT];
static dress_slot_t s_category = DRESS_SLOT_HAIR;
static uint32_t s_rng = 1u;
static dress_theme_t s_theme = DRESS_THEME_CASUAL;
static dress_mode_t s_mode = DRESS_MODE_FREEPLAY;
static float s_show_t = 0.0F;
static int s_player_stars = 3;
static int s_rival_stars[3] = {2, 3, 2};
static int s_player_rank = 1;
static int s_rival_equip[3][DRESS_SLOT_COUNT];

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
    dress_room_reset_outfit();
    s_category = DRESS_SLOT_HAIR;
    s_rng = 0xC0FFEEu;
    s_theme = DRESS_THEME_CASUAL;
    s_mode = DRESS_MODE_FREEPLAY;
    s_show_t = 0.0F;
    s_player_stars = 3;
    s_player_rank = 1;
}

void dress_room_reset_outfit(void) {
    for (int i = 0; i < DRESS_SLOT_COUNT; ++i) {
        s_equipped[i] = -1;
    }
    /* Coherent starter look (first-frame product quality): prefer named ids that
       cover waist/hips when layered. Falls back to first catalog item per slot. */
    static const char *const preferred[DRESS_SLOT_COUNT] = {
        "hair_bob", "top_tee", "bot_jeans", "shoe_sneak", "acc_glasses",
    };
    for (int slot = 0; slot < DRESS_SLOT_COUNT; ++slot) {
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
}

void dress_room_randomize_outfit(uint32_t seed) {
    if (seed != 0u) {
        s_rng = seed;
    }
    fill_random_outfit(s_equipped, &s_rng);
}

bool dress_room_equip(int item_index) {
    if (item_index < 0 || item_index >= (int)(sizeof s_catalog / sizeof s_catalog[0])) {
        return false;
    }
    const dress_slot_t slot = s_catalog[item_index].slot;
    if (s_equipped[slot] == item_index) {
        s_equipped[slot] = -1;
        return true;
    }
    s_equipped[slot] = item_index;
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
