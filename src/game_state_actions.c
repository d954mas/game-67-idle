#include "game_state_actions.h"

#include <stddef.h>
#include <string.h>

static const Game67VariantDef k_variants[GAME_67_VARIANT_COUNT] = {
    {"tiny_67", "Tiny 67", 1, 1, 0},
    {"berry_67", "Berry 67", 2, 2, 5},
    {"banana_67", "Banana 67", 3, 4, 10},
    {"smoothie_67", "Smoothie 67", 4, 8, 15},
    {"cool_67", "Cool 67", 5, 16, 25},
    {"portal_67", "Portal 67", 6, 32, 40},
    {"mystery_67", "Mystery 67", 7, 48, 67},
    {"jelly_67", "Jelly 67", 8, 72, 90},
    {"lemon_67", "Lemon 67", 9, 105, 120},
    {"watermelon_67", "Watermelon 67", 10, 150, 150},
    {"bubblegum_67", "Bubblegum 67", 11, 210, 190},
    {"sticker_67", "Sticker 67", 12, 290, 240},
    {"party_67", "Party 67", 13, 390, 300},
    {"arcade_67", "Arcade 67", 14, 520, 380},
    {"cloud_67", "Cloud 67", 15, 680, 480},
    {"crown_67", "Crown 67", 16, 880, 600},
    {"rocket_67", "Rocket 67", 17, 1120, 760},
    {"rainbow_67", "Rainbow 67", 18, 1400, 950},
    {"neon_67", "Neon 67", 19, 1800, 1200},
    {"gummy_67", "Gummy 67", 20, 2300, 1500},
    {"pixel_67", "Pixel 67", 21, 3000, 1900},
    {"lava_67", "Lava 67", 22, 3900, 2400},
    {"donut_67", "Donut 67", 23, 5100, 3000},
    {"slime_67", "Slime 67", 24, 6600, 3800},
    {"disco_67", "Disco 67", 25, 8600, 4800},
    {"dragon_67", "Dragon 67", 26, 11200, 6000},
    {"ninja_67", "Ninja 67", 27, 14500, 7600},
    {"galaxy_67", "Galaxy 67", 28, 19000, 9500},
    {"golden_67", "Golden 67", 29, 25000, 12000},
    {"cosmic_67", "Cosmic 67", 30, 33000, 15000},
};

static const char *k_goal_labels[GAME_67_VARIANT_COUNT] = {
    "NEXT TINY",
    "NEXT BERRY",
    "NEXT BANANA",
    "NEXT SMOOTHIE",
    "NEXT COOL",
    "NEXT PORTAL",
    "NEXT MYSTERY",
    "NEXT JELLY",
    "NEXT LEMON",
    "NEXT MELON",
    "NEXT BUBBLE",
    "NEXT STICKER",
    "NEXT PARTY",
    "NEXT ARCADE",
    "NEXT CLOUD",
    "NEXT CROWN",
    "NEXT ROCKET",
    "NEXT RAINBOW",
    "NEXT NEON",
    "NEXT GUMMY",
    "NEXT PIXEL",
    "NEXT LAVA",
    "NEXT DONUT",
    "NEXT SLIME",
    "NEXT DISCO",
    "NEXT DRAGON",
    "NEXT NINJA",
    "NEXT GALAXY",
    "NEXT GOLDEN",
    "NEXT COSMIC",
};

const Game67VariantDef *game_67_variants(void) {
    return k_variants;
}

static int *variant_count_mut(GameState *state, int index) {
    switch (index) {
    case 0:
        return &state->count_tiny_67;
    case 1:
        return &state->count_berry_67;
    case 2:
        return &state->count_banana_67;
    case 3:
        return &state->count_smoothie_67;
    case 4:
        return &state->count_cool_67;
    case 5:
        return &state->count_portal_67;
    case 6:
        return &state->count_mystery_67;
    case 7:
        return &state->count_jelly_67;
    case 8:
        return &state->count_lemon_67;
    case 9:
        return &state->count_watermelon_67;
    case 10:
        return &state->count_bubblegum_67;
    case 11:
        return &state->count_sticker_67;
    case 12:
        return &state->count_party_67;
    case 13:
        return &state->count_arcade_67;
    case 14:
        return &state->count_cloud_67;
    case 15:
        return &state->count_crown_67;
    case 16:
        return &state->count_rocket_67;
    case 17:
        return &state->count_rainbow_67;
    case 18:
        return &state->count_neon_67;
    case 19:
        return &state->count_gummy_67;
    case 20:
        return &state->count_pixel_67;
    case 21:
        return &state->count_lava_67;
    case 22:
        return &state->count_donut_67;
    case 23:
        return &state->count_slime_67;
    case 24:
        return &state->count_disco_67;
    case 25:
        return &state->count_dragon_67;
    case 26:
        return &state->count_ninja_67;
    case 27:
        return &state->count_galaxy_67;
    case 28:
        return &state->count_golden_67;
    case 29:
        return &state->count_cosmic_67;
    default:
        return NULL;
    }
}

int game_67_variant_count(const GameState *state, int index) {
    switch (index) {
    case 0:
        return state->count_tiny_67;
    case 1:
        return state->count_berry_67;
    case 2:
        return state->count_banana_67;
    case 3:
        return state->count_smoothie_67;
    case 4:
        return state->count_cool_67;
    case 5:
        return state->count_portal_67;
    case 6:
        return state->count_mystery_67;
    case 7:
        return state->count_jelly_67;
    case 8:
        return state->count_lemon_67;
    case 9:
        return state->count_watermelon_67;
    case 10:
        return state->count_bubblegum_67;
    case 11:
        return state->count_sticker_67;
    case 12:
        return state->count_party_67;
    case 13:
        return state->count_arcade_67;
    case 14:
        return state->count_cloud_67;
    case 15:
        return state->count_crown_67;
    case 16:
        return state->count_rocket_67;
    case 17:
        return state->count_rainbow_67;
    case 18:
        return state->count_neon_67;
    case 19:
        return state->count_gummy_67;
    case 20:
        return state->count_pixel_67;
    case 21:
        return state->count_lava_67;
    case 22:
        return state->count_donut_67;
    case 23:
        return state->count_slime_67;
    case 24:
        return state->count_disco_67;
    case 25:
        return state->count_dragon_67;
    case 26:
        return state->count_ninja_67;
    case 27:
        return state->count_galaxy_67;
    case 28:
        return state->count_golden_67;
    case 29:
        return state->count_cosmic_67;
    default:
        return 0;
    }
}

static void discover_if_needed(GameState *state, int index) {
    const int order = k_variants[index].order;
    if (state->highest_variant_order < order) {
        state->highest_variant_order = order;
        if (state->collection_discovered_count < order) {
            state->collection_discovered_count = order;
        }
        if (k_variants[index].discovery_bonus > 0) {
            state->wallet_soft += k_variants[index].discovery_bonus;
        }
    }
}

int game_67_total_on_board(const GameState *state) {
    int total = 0;
    for (int i = 0; i < GAME_67_VARIANT_COUNT; i++) {
        total += game_67_variant_count(state, i);
    }
    return total;
}

int game_67_passive_income_per_tick(const GameState *state) {
    int income = 0;
    for (int i = 0; i < GAME_67_VARIANT_COUNT; i++) {
        income += game_67_variant_count(state, i) * k_variants[i].passive_coins_per_tick;
    }
    return income;
}

bool game_67_can_spawn(const GameState *state) {
    return game_67_total_on_board(state) < GAME_67_BOARD_SLOTS;
}

bool game_67_can_merge(const GameState *state) {
    for (int i = 0; i < GAME_67_VARIANT_COUNT - 1; i++) {
        if (game_67_variant_count(state, i) >= 2) {
            return true;
        }
    }
    return false;
}

bool game_67_can_recycle_lowest(const GameState *state) {
    return game_67_total_on_board(state) >= GAME_67_BOARD_SLOTS && !game_67_can_merge(state);
}

bool game_67_can_buy_faster_spawn(const GameState *state) {
    return !state->faster_spawn_bought && state->collection_discovered_count >= 2 && state->wallet_soft >= GAME_67_FASTER_SPAWN_COST;
}

static int better_crate_next_level(const GameState *state) {
    if (state->better_crate_level < 0) {
        return 0;
    }
    return state->better_crate_level + 1;
}

int game_67_better_crate_next_cost(const GameState *state) {
    const int next_level = better_crate_next_level(state);
    if (next_level <= 0 || next_level > GAME_67_BETTER_CRATE_MAX_LEVEL) {
        return 0;
    }
    const int passive = k_variants[next_level].passive_coins_per_tick;
    if (next_level <= 3) {
        return passive * 216 + 270;
    }
    if (next_level <= 10) {
        return passive * 378 + next_level * 216;
    }
    if (next_level <= 20) {
        return passive * 560 + next_level * 700;
    }
    return passive * 800 + next_level * 1500;
}

bool game_67_can_buy_better_crate(const GameState *state) {
    if (!state->faster_spawn_bought) {
        return false;
    }
    const int next_level = better_crate_next_level(state);
    if (next_level <= 0 || next_level > GAME_67_BETTER_CRATE_MAX_LEVEL) {
        return false;
    }
    if (state->collection_discovered_count <= next_level) {
        return false;
    }
    return state->wallet_soft >= game_67_better_crate_next_cost(state);
}

int game_67_faster_spawn_cost_remaining(const GameState *state) {
    if (state->faster_spawn_bought) {
        return 0;
    }
    const int remaining = GAME_67_FASTER_SPAWN_COST - state->wallet_soft;
    return remaining > 0 ? remaining : 0;
}

int game_67_better_crate_cost_remaining(const GameState *state) {
    const int cost = game_67_better_crate_next_cost(state);
    if (cost <= 0) {
        return 0;
    }
    const int remaining = cost - state->wallet_soft;
    return remaining > 0 ? remaining : 0;
}

int game_67_spawn_variant_index(const GameState *state) {
    int index = state->better_crate_level;
    if (index < 0) {
        index = 0;
    }
    if (index > GAME_67_BETTER_CRATE_MAX_LEVEL) {
        index = GAME_67_BETTER_CRATE_MAX_LEVEL;
    }
    const int discovered_cap = state->collection_discovered_count - 1;
    if (index > discovered_cap) {
        index = discovered_cap;
    }
    return index > 0 ? index : 0;
}

const char *game_67_faster_spawn_state(const GameState *state) {
    if (state->faster_spawn_bought) {
        return "bought";
    }
    if (state->collection_discovered_count < 2) {
        return "locked";
    }
    if (game_67_can_buy_faster_spawn(state)) {
        return "ready";
    }
    return "saving";
}

const char *game_67_better_crate_state(const GameState *state) {
    if (!state->faster_spawn_bought) {
        return "locked";
    }
    const int next_level = better_crate_next_level(state);
    if (next_level > GAME_67_BETTER_CRATE_MAX_LEVEL) {
        return "max";
    }
    if (state->collection_discovered_count <= next_level) {
        return "locked";
    }
    if (game_67_can_buy_better_crate(state)) {
        return "ready";
    }
    return "saving";
}

const char *game_67_ftue_step(const GameState *state) {
    const int total = game_67_total_on_board(state);
    if (state->tutorial_done) {
        if (game_67_can_buy_better_crate(state)) {
            return "buy_crate";
        }
        return game_67_can_buy_faster_spawn(state) ? "buy_upgrade" : "keep_merging";
    }
    if (game_67_can_merge(state)) {
        return "merge_pair";
    }
    if (total == 0) {
        return "spawn_first";
    }
    if (total == 1) {
        return "spawn_second";
    }
    return "make_pair";
}

const char *game_67_ftue_prompt(const GameState *state) {
    const char *step = game_67_ftue_step(state);
    if (strcmp(step, "spawn_first") == 0) {
        return "TAP BOX";
    }
    if (strcmp(step, "spawn_second") == 0) {
        return "TAP BOX AGAIN";
    }
    if (strcmp(step, "merge_pair") == 0) {
        return "TAP MATCHING 67S";
    }
    if (strcmp(step, "make_pair") == 0) {
        return "MAKE A PAIR";
    }
    if (strcmp(step, "buy_upgrade") == 0) {
        return "BUY SPEED";
    }
    if (strcmp(step, "buy_crate") == 0) {
        return "UPGRADE BOX";
    }
    return "KEEP MERGING";
}

const char *game_67_next_goal(const GameState *state) {
    if (state->collection_discovered_count >= 0 && state->collection_discovered_count < GAME_67_VARIANT_COUNT) {
        return k_goal_labels[state->collection_discovered_count];
    }
    return "WORLD COMPLETE";
}

void game_67_reset_playtest(GameState *state) {
    game_state_init_defaults(state);
    state->wallet_soft = 0;
    state->wallet_hard = 0;
    state->tutorial_done = false;
    state->collection_discovered_count = 1;
    state->highest_variant_order = 1;
    state->faster_spawn_bought = false;
    state->better_crate_level = 0;
    state->passive_accum_frames = 0;
    state->count_tiny_67 = 0;
    state->count_berry_67 = 0;
    state->count_banana_67 = 0;
    state->count_smoothie_67 = 0;
    state->count_cool_67 = 0;
    state->count_portal_67 = 0;
    state->count_mystery_67 = 0;
    state->count_jelly_67 = 0;
    state->count_lemon_67 = 0;
    state->count_watermelon_67 = 0;
    state->count_bubblegum_67 = 0;
    state->count_sticker_67 = 0;
    state->count_party_67 = 0;
    state->count_arcade_67 = 0;
    state->count_cloud_67 = 0;
    state->count_crown_67 = 0;
    state->count_rocket_67 = 0;
    state->count_rainbow_67 = 0;
    state->count_neon_67 = 0;
    state->count_gummy_67 = 0;
    state->count_pixel_67 = 0;
    state->count_lava_67 = 0;
    state->count_donut_67 = 0;
    state->count_slime_67 = 0;
    state->count_disco_67 = 0;
    state->count_dragon_67 = 0;
    state->count_ninja_67 = 0;
    state->count_galaxy_67 = 0;
    state->count_golden_67 = 0;
    state->count_cosmic_67 = 0;
    game_state_mark_dirty();
}

bool game_67_spawn(GameState *state) {
    if (!game_67_can_spawn(state)) {
        return false;
    }
    int *spawn_count = variant_count_mut(state, game_67_spawn_variant_index(state));
    if (!spawn_count) {
        return false;
    }
    *spawn_count += 1;
    if (state->collection_discovered_count < 1) {
        state->collection_discovered_count = 1;
    }
    if (state->highest_variant_order < 1) {
        state->highest_variant_order = 1;
    }
    game_state_mark_dirty();
    return true;
}

bool game_67_recycle_lowest(GameState *state) {
    if (!game_67_can_recycle_lowest(state)) {
        return false;
    }
    for (int i = 0; i < GAME_67_VARIANT_COUNT; i++) {
        int *count = variant_count_mut(state, i);
        if (count && *count > 0) {
            *count -= 1;
            state->wallet_soft += k_variants[i].passive_coins_per_tick * 2;
            game_state_mark_dirty();
            return true;
        }
    }
    return false;
}

bool game_67_merge_lowest(GameState *state) {
    for (int i = 0; i < GAME_67_VARIANT_COUNT - 1; i++) {
        if (game_67_merge_variant(state, i)) {
            return true;
        }
    }
    return false;
}

bool game_67_merge_variant(GameState *state, int index) {
    if (index < 0 || index >= GAME_67_VARIANT_COUNT - 1) {
        return false;
    }
    int *from = variant_count_mut(state, index);
    int *to = variant_count_mut(state, index + 1);
    if (!from || !to || *from < 2) {
        return false;
    }
    *from -= 2;
    *to += 1;
    state->wallet_soft += 1;
    discover_if_needed(state, index + 1);
    if (index == 0) {
        state->tutorial_done = true;
    }
    game_state_mark_dirty();
    return true;
}

bool game_67_buy_faster_spawn(GameState *state) {
    if (!game_67_can_buy_faster_spawn(state)) {
        return false;
    }
    state->wallet_soft -= GAME_67_FASTER_SPAWN_COST;
    state->faster_spawn_bought = true;
    game_state_mark_dirty();
    return true;
}

bool game_67_buy_better_crate(GameState *state) {
    if (!game_67_can_buy_better_crate(state)) {
        return false;
    }
    state->wallet_soft -= game_67_better_crate_next_cost(state);
    state->better_crate_level += 1;
    game_state_mark_dirty();
    return true;
}

bool game_67_tick_passive(GameState *state, int frames) {
    if (frames <= 0) {
        return false;
    }
    state->passive_accum_frames += frames;
    bool changed = false;
    while (state->passive_accum_frames >= GAME_67_PASSIVE_INTERVAL_FRAMES) {
        state->passive_accum_frames -= GAME_67_PASSIVE_INTERVAL_FRAMES;
        const int income = game_67_passive_income_per_tick(state);
        if (income > 0) {
            state->wallet_soft += income;
            changed = true;
        }
    }
    if (changed) {
        game_state_mark_dirty();
    }
    return changed;
}
