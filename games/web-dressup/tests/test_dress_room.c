#include "unity.h"

#include "features/dress_room/dress_room.h"
#include "features/dress_room/dress_room_events.h"
#include "game_events.h"
#include "game_state.h"

#include <string.h>

static int s_dirty_calls;
void game_save_mark_dirty(void) { ++s_dirty_calls; }

void setUp(void) {
    game_events_init();
    game_state_init_defaults(&game_state);
    s_dirty_calls = 0;
    dress_room_init();
}
void tearDown(void) { game_events_shutdown(); }

static const game_event_t *last_event_of_type(nt_hash64_t type) {
    int count = 0;
    const game_event_t *events = game_event_log(&count);
    for (int i = count - 1; i >= 0; --i) {
        if (events[i].type.value == type.value) return &events[i];
    }
    return NULL;
}

static int event_count_of_type(nt_hash64_t type) {
    int count = 0;
    int matches = 0;
    const game_event_t *events = game_event_log(&count);
    for (int i = 0; i < count; ++i) {
        if (events[i].type.value == type.value) ++matches;
    }
    return matches;
}

static int catalog_index_for_id(const char *id) {
    for (int i = 0; i < dress_room_catalog_count(); ++i) {
        const dress_item_t *item = dress_room_catalog_item(i);
        if (item && item->id && strcmp(item->id, id) == 0) {
            return i;
        }
    }
    return -1;
}

static void equip_id(const char *id) {
    const int index = catalog_index_for_id(id);
    TEST_ASSERT_TRUE(index >= 0);
    TEST_ASSERT_TRUE(dress_room_equip(index));
}

static void equip_moon_moon_focus(void) {
    equip_id("top_tee");
    equip_id("acc_glasses");
}

static void equip_complete_moon_moon(void) {
    equip_moon_moon_focus();
    if (dress_room_equipped(DRESS_SLOT_HAIR) < 0) equip_id("hair_bob");
    if (dress_room_equipped(DRESS_SLOT_BOTTOM) < 0) equip_id("bot_jeans");
    if (dress_room_equipped(DRESS_SLOT_SHOES) < 0) equip_id("shoe_sneak");
}

static void confirm_supports(void) {
    TEST_ASSERT_TRUE(dress_room_equip(dress_room_equipped(DRESS_SLOT_HAIR)));
    TEST_ASSERT_TRUE(dress_room_equip(dress_room_equipped(DRESS_SLOT_BOTTOM)));
    TEST_ASSERT_TRUE(dress_room_equip(dress_room_equipped(DRESS_SLOT_SHOES)));
    TEST_ASSERT_TRUE(dress_room_support_complete());
}

static const char *equipped_catalog_id(dress_slot_t slot) {
    const dress_item_t *item = dress_room_catalog_item(dress_room_equipped(slot));
    return item ? item->id : "none";
}

void test_reset_keeps_focus_slots_empty_for_player_authorship(void) {
    dress_room_reset_outfit();
    TEST_ASSERT_TRUE(dress_room_equipped(DRESS_SLOT_HAIR) >= 0);
    TEST_ASSERT_EQUAL_INT(-1, dress_room_equipped(DRESS_SLOT_TOP));
    TEST_ASSERT_TRUE(dress_room_equipped(DRESS_SLOT_BOTTOM) >= 0);
    TEST_ASSERT_TRUE(dress_room_equipped(DRESS_SLOT_SHOES) >= 0);
    TEST_ASSERT_EQUAL_INT(-1, dress_room_equipped(DRESS_SLOT_ACC));
    TEST_ASSERT_FALSE(dress_room_focus_complete());
}

void test_equip_toggle_clears_slot(void) {
    dress_room_reset_outfit();
    const int idx = catalog_index_for_id("top_tee");
    TEST_ASSERT_TRUE(idx >= 0);
    TEST_ASSERT_TRUE(dress_room_equip(idx));
    TEST_ASSERT_EQUAL_INT(idx, dress_room_equipped(DRESS_SLOT_TOP));
    TEST_ASSERT_TRUE(dress_room_equip(idx));
    TEST_ASSERT_EQUAL_INT(-1, dress_room_equipped(DRESS_SLOT_TOP));
    TEST_ASSERT_TRUE(dress_room_equip(idx));
    TEST_ASSERT_EQUAL_INT(idx, dress_room_equipped(DRESS_SLOT_TOP));
}

void test_randomize_fills_all_slots(void) {
    dress_room_randomize_outfit(42u);
    for (int s = 0; s < DRESS_SLOT_COUNT; ++s) {
        TEST_ASSERT_TRUE(dress_room_equipped((dress_slot_t)s) >= 0);
    }
}

void test_category_roundtrip(void) {
    dress_room_set_category(DRESS_SLOT_SHOES);
    TEST_ASSERT_EQUAL_INT(DRESS_SLOT_SHOES, (int)dress_room_category());
}

void test_fresh_round_starts_on_main_and_main_equip_opens_accent(void) {
    TEST_ASSERT_EQUAL_INT(DRESS_SLOT_TOP, (int)dress_room_category());
    equip_id("top_tee");
    TEST_ASSERT_EQUAL_INT(DRESS_SLOT_ACC, (int)dress_room_category());
}

void test_focus_pair_requires_hair_bottom_shoes_confirmation_in_order(void) {
    equip_id("top_tee");
    equip_id("acc_hat");
    TEST_ASSERT_EQUAL_INT(DRESS_SLOT_HAIR, dress_room_category());
    TEST_ASSERT_FALSE(dress_room_support_complete());
    TEST_ASSERT_FALSE(dress_room_begin_awakening());

    const int hair = dress_room_equipped(DRESS_SLOT_HAIR);
    TEST_ASSERT_TRUE(dress_room_equip(hair)); /* confirm, do not toggle off */
    TEST_ASSERT_EQUAL_INT(hair, dress_room_equipped(DRESS_SLOT_HAIR));
    TEST_ASSERT_TRUE(dress_room_support_confirmed(DRESS_SLOT_HAIR));
    TEST_ASSERT_EQUAL_INT(DRESS_SLOT_BOTTOM, dress_room_category());

    const int bottom = dress_room_equipped(DRESS_SLOT_BOTTOM);
    TEST_ASSERT_TRUE(dress_room_equip(bottom));
    TEST_ASSERT_TRUE(dress_room_support_confirmed(DRESS_SLOT_BOTTOM));
    TEST_ASSERT_EQUAL_INT(DRESS_SLOT_SHOES, dress_room_category());

    const int shoes = dress_room_equipped(DRESS_SLOT_SHOES);
    TEST_ASSERT_TRUE(dress_room_equip(shoes));
    TEST_ASSERT_EQUAL_INT(shoes, dress_room_equipped(DRESS_SLOT_SHOES));
    TEST_ASSERT_EQUAL_INT(3, dress_room_support_confirmed_count());
    TEST_ASSERT_EQUAL_HEX8(0x07, dress_room_support_mask());
    TEST_ASSERT_TRUE(dress_room_begin_awakening());
}

void test_changing_earlier_support_or_focus_resets_later_confirmations(void) {
    equip_id("top_tee");
    equip_id("acc_hat");
    confirm_supports();

    equip_id("hair_long");
    TEST_ASSERT_TRUE(dress_room_support_confirmed(DRESS_SLOT_HAIR));
    TEST_ASSERT_FALSE(dress_room_support_confirmed(DRESS_SLOT_BOTTOM));
    TEST_ASSERT_FALSE(dress_room_support_confirmed(DRESS_SLOT_SHOES));
    TEST_ASSERT_EQUAL_INT(DRESS_SLOT_BOTTOM, dress_room_category());

    equip_id("top_blazer");
    TEST_ASSERT_EQUAL_INT(0, dress_room_support_confirmed_count());
    TEST_ASSERT_EQUAL_INT(DRESS_SLOT_ACC, dress_room_category());
}

void test_crescent_main_suppresses_support_bottom(void) {
    TEST_ASSERT_FALSE(dress_room_main_covers_bottom());
    equip_id("top_tee");
    TEST_ASSERT_TRUE(dress_room_main_covers_bottom());
    equip_id("top_tee");
    TEST_ASSERT_FALSE(dress_room_main_covers_bottom());
}

void test_catalog_has_expected_production_counts_per_slot(void) {
    int counts[DRESS_SLOT_COUNT] = {0};
    for (int i = 0; i < dress_room_catalog_count(); ++i) {
        const dress_item_t *it = dress_room_catalog_item(i);
        TEST_ASSERT_NOT_NULL(it);
        counts[it->slot] += 1;
    }
    const int expected[DRESS_SLOT_COUNT] = {4, 4, 6, 6, 6};
    for (int s = 0; s < DRESS_SLOT_COUNT; ++s) {
        TEST_ASSERT_EQUAL_INT(expected[s], counts[s]);
    }
}

void test_equip_oob_rejected(void) {
    dress_room_reset_outfit();
    const int top = dress_room_equipped(DRESS_SLOT_TOP);
    TEST_ASSERT_FALSE(dress_room_equip(-1));
    TEST_ASSERT_FALSE(dress_room_equip(999));
    TEST_ASSERT_EQUAL_INT(top, dress_room_equipped(DRESS_SLOT_TOP));
}

void test_randomize_seed_deterministic(void) {
    dress_room_randomize_outfit(42u);
    int a[DRESS_SLOT_COUNT];
    for (int s = 0; s < DRESS_SLOT_COUNT; ++s) {
        a[s] = dress_room_equipped((dress_slot_t)s);
    }
    dress_room_init();
    dress_room_randomize_outfit(42u);
    for (int s = 0; s < DRESS_SLOT_COUNT; ++s) {
        TEST_ASSERT_EQUAL_INT(a[s], dress_room_equipped((dress_slot_t)s));
    }
}

void test_score_outfit_deterministic(void) {
    int equipped[DRESS_SLOT_COUNT];
    dress_room_reset_outfit();
    for (int s = 0; s < DRESS_SLOT_COUNT; ++s) {
        equipped[s] = dress_room_equipped((dress_slot_t)s);
    }
    const int a = dress_room_score_outfit(equipped, DRESS_THEME_CASUAL, 12345u);
    const int b = dress_room_score_outfit(equipped, DRESS_THEME_CASUAL, 12345u);
    TEST_ASSERT_EQUAL_INT(a, b);
    TEST_ASSERT_TRUE(a >= 1 && a <= 5);
}

void test_score_theme_fit_matters(void) {
    /* All empty outfit scores low. */
    int empty[DRESS_SLOT_COUNT] = {-1, -1, -1, -1, -1};
    const int low = dress_room_score_outfit(empty, DRESS_THEME_GLAM, 1u);
    /* Full starter outfit scores higher with same seed family. */
    dress_room_reset_outfit();
    int full[DRESS_SLOT_COUNT];
    for (int s = 0; s < DRESS_SLOT_COUNT; ++s) {
        full[s] = dress_room_equipped((dress_slot_t)s);
    }
    const int high = dress_room_score_outfit(full, DRESS_THEME_CASUAL, 1u);
    TEST_ASSERT_TRUE(high >= low);
    TEST_ASSERT_TRUE(low >= 1 && low <= 5);
    TEST_ASSERT_TRUE(high >= 1 && high <= 5);
}

void test_show_returns_to_freeplay(void) {
    /* Theme is set on freeplay; Show starts immediately (no theme-pick screen). */
    dress_room_set_theme(DRESS_THEME_STREET);
    dress_room_begin_show();
    TEST_ASSERT_EQUAL_INT(DRESS_MODE_SHOW_RUNWAY, (int)dress_room_mode());
    TEST_ASSERT_TRUE(dress_room_player_stars() >= 1 && dress_room_player_stars() <= 5);
    dress_room_show_advance();
    TEST_ASSERT_EQUAL_INT(DRESS_MODE_SHOW_PODIUM, (int)dress_room_mode());
    dress_room_return_freeplay();
    TEST_ASSERT_EQUAL_INT(DRESS_MODE_FREEPLAY, (int)dress_room_mode());
    TEST_ASSERT_EQUAL_INT(DRESS_THEME_STREET, (int)dress_room_theme());
}

void test_show_tick_advances_to_podium(void) {
    dress_room_begin_show();
    for (int i = 0; i < 40; ++i) {
        dress_room_show_tick(0.1F);
    }
    TEST_ASSERT_EQUAL_INT(DRESS_MODE_SHOW_PODIUM, (int)dress_room_mode());
}

void test_show_again_from_podium_restarts_show(void) {
    dress_room_set_theme(DRESS_THEME_GLAM);
    dress_room_begin_show();
    dress_room_show_advance();
    TEST_ASSERT_EQUAL_INT(DRESS_MODE_SHOW_PODIUM, (int)dress_room_mode());
    /* UI "Show again" — restart runway with current theme (not a dead control). */
    dress_room_begin_show();
    TEST_ASSERT_EQUAL_INT(DRESS_MODE_SHOW_RUNWAY, (int)dress_room_mode());
    TEST_ASSERT_EQUAL_INT(DRESS_THEME_GLAM, (int)dress_room_theme());
}

void test_enter_theme_pick_returns_freeplay_from_show(void) {
    dress_room_begin_show();
    dress_room_show_advance();
    dress_room_enter_theme_pick();
    TEST_ASSERT_EQUAL_INT(DRESS_MODE_FREEPLAY, (int)dress_room_mode());
}

void test_rival_outfits_filled_after_begin_show(void) {
    TEST_ASSERT_EQUAL_INT(-1, dress_room_rival_equipped(0, DRESS_SLOT_HAIR));
    dress_room_begin_show();
    for (int r = 0; r < 3; ++r) {
        for (int s = 0; s < DRESS_SLOT_COUNT; ++s) {
            const int idx = dress_room_rival_equipped(r, (dress_slot_t)s);
            TEST_ASSERT_TRUE(idx >= 0);
            const dress_item_t *it = dress_room_catalog_item(idx);
            TEST_ASSERT_NOT_NULL(it);
            TEST_ASSERT_EQUAL_INT(s, (int)it->slot);
            TEST_ASSERT_NOT_NULL(it->atlas_layer);
        }
        TEST_ASSERT_TRUE(dress_room_rival_stars(r) >= 1 && dress_room_rival_stars(r) <= 5);
    }
    /* Rivals differ from a pure empty state and stay readable on podium. */
    dress_room_show_advance();
    TEST_ASSERT_EQUAL_INT(DRESS_MODE_SHOW_PODIUM, (int)dress_room_mode());
    TEST_ASSERT_TRUE(dress_room_rival_equipped(1, DRESS_SLOT_TOP) >= 0);
    dress_room_return_freeplay();
    TEST_ASSERT_EQUAL_INT(-1, dress_room_rival_equipped(0, DRESS_SLOT_HAIR));
}

void test_essence_metadata_is_stable_and_human_readable(void) {
    TEST_ASSERT_EQUAL_STRING("None", dress_room_essence_label(DRESS_ESSENCE_NONE));
    TEST_ASSERT_EQUAL_STRING("Moon", dress_room_essence_label(DRESS_ESSENCE_MOON));
    TEST_ASSERT_EQUAL_STRING("Bloom", dress_room_essence_label(DRESS_ESSENCE_BLOOM));
    TEST_ASSERT_EQUAL_STRING("Flame", dress_room_essence_label(DRESS_ESSENCE_FLAME));
    TEST_ASSERT_EQUAL_STRING("?", dress_room_essence_label((dress_essence_t)99));

    const dress_item_t *moon_main = dress_room_catalog_item(catalog_index_for_id("top_tee"));
    const dress_item_t *bloom_accent = dress_room_catalog_item(catalog_index_for_id("acc_hat"));
    TEST_ASSERT_NOT_NULL(moon_main);
    TEST_ASSERT_NOT_NULL(bloom_accent);
    TEST_ASSERT_EQUAL_INT(DRESS_ESSENCE_MOON, moon_main->essence);
    TEST_ASSERT_EQUAL_INT(DRESS_ESSENCE_BLOOM, bloom_accent->essence);
}

void test_six_recipes_cover_every_unordered_pair(void) {
    TEST_ASSERT_EQUAL_INT(6, dress_room_recipe_count());
    const dress_awakening_recipe_t *moon_bloom =
        dress_room_recipe_for(DRESS_ESSENCE_MOON, DRESS_ESSENCE_BLOOM);
    const dress_awakening_recipe_t *bloom_moon =
        dress_room_recipe_for(DRESS_ESSENCE_BLOOM, DRESS_ESSENCE_MOON);
    TEST_ASSERT_NOT_NULL(moon_bloom);
    TEST_ASSERT_EQUAL_PTR(moon_bloom, bloom_moon);
    TEST_ASSERT_EQUAL_STRING("moon-bloom", moon_bloom->id);
    TEST_ASSERT_EQUAL_STRING("Dreamgarden Fae", moon_bloom->label);

    const dress_awakening_recipe_t *expected[6] = {
        dress_room_recipe_for(DRESS_ESSENCE_MOON, DRESS_ESSENCE_MOON),
        dress_room_recipe_for(DRESS_ESSENCE_BLOOM, DRESS_ESSENCE_BLOOM),
        dress_room_recipe_for(DRESS_ESSENCE_FLAME, DRESS_ESSENCE_FLAME),
        moon_bloom,
        dress_room_recipe_for(DRESS_ESSENCE_MOON, DRESS_ESSENCE_FLAME),
        dress_room_recipe_for(DRESS_ESSENCE_BLOOM, DRESS_ESSENCE_FLAME),
    };
    for (int i = 0; i < 6; ++i) {
        TEST_ASSERT_NOT_NULL(expected[i]);
        TEST_ASSERT_NOT_NULL(expected[i]->id);
        TEST_ASSERT_NOT_NULL(expected[i]->label);
        TEST_ASSERT_EQUAL_PTR(expected[i], dress_room_recipe_at(i));
    }
    TEST_ASSERT_NULL(dress_room_recipe_for(DRESS_ESSENCE_NONE, DRESS_ESSENCE_MOON));
    TEST_ASSERT_NULL(dress_room_recipe_at(-1));
    TEST_ASSERT_NULL(dress_room_recipe_at(6));
}

void test_awakening_requires_both_focus_essences(void) {
    TEST_ASSERT_FALSE(dress_room_focus_complete());
    equip_id("top_tee");
    TEST_ASSERT_EQUAL_INT(DRESS_ESSENCE_MOON, dress_room_primary_essence());
    TEST_ASSERT_EQUAL_INT(DRESS_ESSENCE_NONE, dress_room_secondary_essence());
    TEST_ASSERT_FALSE(dress_room_begin_awakening());

    equip_id("acc_glasses");
    TEST_ASSERT_TRUE(dress_room_focus_complete());
    TEST_ASSERT_EQUAL_INT(DRESS_ESSENCE_MOON, dress_room_secondary_essence());

    TEST_ASSERT_TRUE(dress_room_equip(dress_room_equipped(DRESS_SLOT_ACC)));
    TEST_ASSERT_FALSE(dress_room_focus_complete());
    TEST_ASSERT_EQUAL_INT(DRESS_ESSENCE_NONE, dress_room_secondary_essence());
    TEST_ASSERT_FALSE(dress_room_begin_awakening());
    TEST_ASSERT_EQUAL_INT(DRESS_AWAKENING_IDLE, dress_room_awakening_phase());
}

void test_begin_awakening_snapshots_deterministic_recipe_and_always_wins(void) {
    equip_id("top_tee");
    equip_id("acc_hat");
    confirm_supports();
    TEST_ASSERT_TRUE(dress_room_begin_awakening());
    TEST_ASSERT_EQUAL_INT(DRESS_AWAKENING_INTRO, dress_room_awakening_phase());
    TEST_ASSERT_EQUAL_STRING("moon-bloom", dress_room_awakening_recipe()->id);
    TEST_ASSERT_TRUE(dress_room_awakening_won());

    /* Changing the outfit after begin cannot mutate the in-flight result. */
    TEST_ASSERT_TRUE(dress_room_equip(catalog_index_for_id("top_blazer")));
    TEST_ASSERT_EQUAL_STRING("moon-bloom", dress_room_awakening_recipe()->id);
}

void test_awakening_timeline_advances_through_every_phase(void) {
    equip_moon_moon_focus();
    confirm_supports();
    TEST_ASSERT_TRUE(dress_room_begin_awakening());
    TEST_ASSERT_EQUAL_INT(DRESS_AWAKENING_INTRO, dress_room_awakening_phase());
    dress_room_awakening_tick(dress_room_awakening_phase_duration());
    TEST_ASSERT_EQUAL_INT(DRESS_AWAKENING_CHARGE, dress_room_awakening_phase());
    dress_room_awakening_tick(dress_room_awakening_phase_duration());
    TEST_ASSERT_EQUAL_INT(DRESS_AWAKENING_FLASH, dress_room_awakening_phase());
    dress_room_awakening_tick(dress_room_awakening_phase_duration());
    TEST_ASSERT_EQUAL_INT(DRESS_AWAKENING_REVEAL, dress_room_awakening_phase());
    dress_room_awakening_tick(dress_room_awakening_phase_duration());
    TEST_ASSERT_EQUAL_INT(DRESS_AWAKENING_VICTORY, dress_room_awakening_phase());
    dress_room_awakening_tick(dress_room_awakening_phase_duration());
    TEST_ASSERT_EQUAL_INT(DRESS_AWAKENING_RECIPE_CARD, dress_room_awakening_phase());
    TEST_ASSERT_TRUE(dress_room_awakening_phase_t() >= 0.9999F);

    dress_room_awakening_tick(100.0F);
    TEST_ASSERT_EQUAL_INT(DRESS_AWAKENING_RECIPE_CARD, dress_room_awakening_phase());
}

void test_large_tick_is_deterministic_and_restyle_starts_a_fresh_focus_pair(void) {
    equip_moon_moon_focus();
    confirm_supports();
    TEST_ASSERT_TRUE(dress_room_begin_awakening());
    dress_room_awakening_tick(100.0F);
    TEST_ASSERT_EQUAL_INT(DRESS_AWAKENING_RECIPE_CARD, dress_room_awakening_phase());

    dress_room_restyle();
    TEST_ASSERT_EQUAL_INT(DRESS_AWAKENING_IDLE, dress_room_awakening_phase());
    TEST_ASSERT_NULL(dress_room_awakening_recipe());
    TEST_ASSERT_FALSE(dress_room_awakening_won());
    TEST_ASSERT_EQUAL_INT(DRESS_MODE_FREEPLAY, dress_room_mode());
    TEST_ASSERT_EQUAL_INT(-1, dress_room_equipped(DRESS_SLOT_TOP));
    TEST_ASSERT_EQUAL_INT(-1, dress_room_equipped(DRESS_SLOT_ACC));
    TEST_ASSERT_EQUAL_INT(DRESS_SLOT_TOP, (int)dress_room_category());
}

void test_init_hydrates_valid_saved_outfit_ids_without_dirtying(void) {
    strcpy(game_state.outfit_hair_id, "hair_pink");
    strcpy(game_state.outfit_main_id, "top_blazer");
    strcpy(game_state.outfit_bottom_id, "bot_skirt");
    strcpy(game_state.outfit_shoes_id, "shoe_heel");
    strcpy(game_state.outfit_accent_id, "acc_hat");
    s_dirty_calls = 0;

    dress_room_init();

    TEST_ASSERT_EQUAL_STRING("hair_pink", dress_room_catalog_item(dress_room_equipped(DRESS_SLOT_HAIR))->id);
    TEST_ASSERT_EQUAL_STRING("top_blazer", dress_room_catalog_item(dress_room_equipped(DRESS_SLOT_TOP))->id);
    TEST_ASSERT_EQUAL_STRING("bot_skirt", dress_room_catalog_item(dress_room_equipped(DRESS_SLOT_BOTTOM))->id);
    TEST_ASSERT_EQUAL_STRING("shoe_heel", dress_room_catalog_item(dress_room_equipped(DRESS_SLOT_SHOES))->id);
    TEST_ASSERT_EQUAL_STRING("acc_hat", dress_room_catalog_item(dress_room_equipped(DRESS_SLOT_ACC))->id);
    TEST_ASSERT_EQUAL_INT(0, s_dirty_calls);
}

void test_init_rejects_unknown_or_wrong_slot_ids_with_safe_slot_fallback(void) {
    strcpy(game_state.outfit_hair_id, "top_tee"); /* exists, wrong slot */
    strcpy(game_state.outfit_main_id, "missing_main");
    strcpy(game_state.outfit_bottom_id, "bot_cargo");
    strcpy(game_state.outfit_shoes_id, "none");
    strcpy(game_state.outfit_accent_id, "none");

    dress_room_init();

    TEST_ASSERT_EQUAL_STRING("hair_bob", dress_room_catalog_item(dress_room_equipped(DRESS_SLOT_HAIR))->id);
    TEST_ASSERT_EQUAL_INT(-1, dress_room_equipped(DRESS_SLOT_TOP));
    TEST_ASSERT_EQUAL_STRING("bot_cargo", dress_room_catalog_item(dress_room_equipped(DRESS_SLOT_BOTTOM))->id);
    TEST_ASSERT_EQUAL_INT(-1, dress_room_equipped(DRESS_SLOT_SHOES));
    TEST_ASSERT_EQUAL_INT(-1, dress_room_equipped(DRESS_SLOT_ACC));
    TEST_ASSERT_EQUAL_INT(0, s_dirty_calls);
}

void test_valid_equip_persists_stable_id_and_first_equip_then_marks_dirty(void) {
    TEST_ASSERT_FALSE(game_state.first_equip_done);
    TEST_ASSERT_TRUE(dress_room_equip(catalog_index_for_id("top_crop")));
    TEST_ASSERT_EQUAL_STRING("top_crop", game_state.outfit_main_id);
    TEST_ASSERT_TRUE(game_state.first_equip_done);
    TEST_ASSERT_EQUAL_INT(1, s_dirty_calls);

    TEST_ASSERT_FALSE(dress_room_equip(999));
    TEST_ASSERT_EQUAL_INT(1, s_dirty_calls);

    TEST_ASSERT_TRUE(dress_room_equip(catalog_index_for_id("top_crop")));
    TEST_ASSERT_EQUAL_STRING("none", game_state.outfit_main_id);
    TEST_ASSERT_EQUAL_INT(2, s_dirty_calls);
}

void test_recipe_card_commits_discovery_and_round_exactly_once(void) {
    equip_id("top_tee");
    equip_id("acc_hat");
    confirm_supports();
    TEST_ASSERT_TRUE(dress_room_begin_awakening());
    const int dirty_before_card = s_dirty_calls;
    TEST_ASSERT_EQUAL_INT(0, game_state.recipe_mask);
    TEST_ASSERT_EQUAL_INT(0, game_state.rounds_completed);

    dress_room_awakening_tick(100.0F);

    TEST_ASSERT_EQUAL_INT(DRESS_AWAKENING_RECIPE_CARD, dress_room_awakening_phase());
    TEST_ASSERT_EQUAL_INT(1 << 3, game_state.recipe_mask); /* recipe index: moon-bloom */
    TEST_ASSERT_EQUAL_INT((1 << 0) | (1 << 1), game_state.essence_mask);
    TEST_ASSERT_EQUAL_INT(1, game_state.rounds_completed);
    TEST_ASSERT_EQUAL_INT(dirty_before_card + 1, s_dirty_calls);

    dress_room_awakening_tick(100.0F);
    TEST_ASSERT_EQUAL_INT(1 << 3, game_state.recipe_mask);
    TEST_ASSERT_EQUAL_INT(1, game_state.rounds_completed);
    TEST_ASSERT_EQUAL_INT(dirty_before_card + 1, s_dirty_calls);
}

void test_discovered_count_tracks_unique_recipe_bits_only(void) {
    game_state.recipe_mask = 0u;
    TEST_ASSERT_EQUAL_INT(0, dress_room_discovered_count());
    game_state.recipe_mask = (1u << 0) | (1u << 3) | (1u << 5) | (1u << 9);
    TEST_ASSERT_EQUAL_INT(3, dress_room_discovered_count());
}

void test_collection_milestones_are_derived_from_persistent_recipe_progress(void) {
    game_state.recipe_mask = 0u;
    TEST_ASSERT_EQUAL_INT(0, dress_room_collection_milestone());
    TEST_ASSERT_EQUAL_INT(1, dress_room_collection_next_target());

    game_state.recipe_mask = 1u << 4;
    TEST_ASSERT_EQUAL_INT(1, dress_room_collection_milestone());
    TEST_ASSERT_EQUAL_INT(3, dress_room_collection_next_target());

    game_state.recipe_mask = (1u << 0) | (1u << 2) | (1u << 5);
    TEST_ASSERT_EQUAL_INT(3, dress_room_collection_milestone());
    TEST_ASSERT_EQUAL_INT(6, dress_room_collection_next_target());

    game_state.recipe_mask = 0x3Fu;
    TEST_ASSERT_EQUAL_INT(6, dress_room_collection_milestone());
    TEST_ASSERT_EQUAL_INT(0, dress_room_collection_next_target());
}

void test_collection_milestones_ignore_non_recipe_bits(void) {
    game_state.recipe_mask = (1u << 0) | (1u << 8) | (1u << 12);
    TEST_ASSERT_EQUAL_INT(1, dress_room_collection_milestone());
    TEST_ASSERT_EQUAL_INT(3, dress_room_collection_next_target());
}

void test_repeat_is_known_and_can_skip_directly_to_card(void) {
    equip_id("top_tee");
    equip_id("acc_hat");
    confirm_supports();
    TEST_ASSERT_TRUE(dress_room_begin_awakening());
    TEST_ASSERT_TRUE(dress_room_awakening_is_new());
    TEST_ASSERT_FALSE(dress_room_awakening_is_new_remix());
    TEST_ASSERT_FALSE(dress_room_skip_replay());
    dress_room_awakening_tick(100.0F);
    TEST_ASSERT_EQUAL_INT(1, dress_room_lookbook_count());
    dress_room_restyle();

    equip_id("top_tee");
    equip_id("acc_hat");
    confirm_supports();
    TEST_ASSERT_TRUE(dress_room_begin_awakening());
    TEST_ASSERT_FALSE(dress_room_awakening_is_new());
    TEST_ASSERT_FALSE(dress_room_awakening_is_new_remix());
    TEST_ASSERT_TRUE(dress_room_skip_replay());
    TEST_ASSERT_EQUAL_INT(DRESS_AWAKENING_RECIPE_CARD, dress_room_awakening_phase());
    TEST_ASSERT_EQUAL_INT(1, game_state.rounds_completed);
    TEST_ASSERT_EQUAL_INT(1 << 3, game_state.recipe_mask);
}

void test_new_support_signature_records_a_remix_but_exact_duplicate_does_not(void) {
    equip_id("top_tee");
    equip_id("acc_hat");
    confirm_supports();
    TEST_ASSERT_TRUE(dress_room_begin_awakening());
    dress_room_awakening_tick(100.0F);
    TEST_ASSERT_EQUAL_INT(1, game_state.rounds_completed);
    TEST_ASSERT_EQUAL_INT(1, dress_room_lookbook_count());
    dress_room_restyle();

    equip_id("hair_long");
    equip_id("top_tee");
    equip_id("acc_hat");
    confirm_supports();
    TEST_ASSERT_FALSE(dress_room_current_look_is_recorded());
    TEST_ASSERT_TRUE(dress_room_begin_awakening());
    TEST_ASSERT_FALSE(dress_room_awakening_is_new());
    TEST_ASSERT_TRUE(dress_room_awakening_is_new_remix());
    TEST_ASSERT_FALSE(dress_room_skip_replay());
    dress_room_awakening_tick(100.0F);
    TEST_ASSERT_EQUAL_INT(2, game_state.rounds_completed);
    TEST_ASSERT_EQUAL_INT(2, dress_room_lookbook_count());
    TEST_ASSERT_TRUE(dress_room_current_look_is_recorded());
}

void test_next_undiscovered_recipe_and_style_signature_are_player_driven(void) {
    game_state.recipe_mask = (1 << 0) | (1 << 1);
    TEST_ASSERT_EQUAL_STRING("flame-flame", dress_room_next_undiscovered_recipe()->id);
    const int starter_signature = dress_room_style_signature();
    TEST_ASSERT_EQUAL_STRING("Crown", dress_room_style_signature_label());
    equip_id("hair_long");
    TEST_ASSERT_TRUE(dress_room_style_signature() != starter_signature);
    TEST_ASSERT_EQUAL_STRING("Trail", dress_room_style_signature_label());
    game_state.recipe_mask = 0x3F;
    TEST_ASSERT_NULL(dress_room_next_undiscovered_recipe());
}

void test_known_current_recipe_can_prepare_the_next_unknown_pair(void) {
    game_state.recipe_mask = 1 << 0; /* Moon + Moon is already known. */
    equip_id("top_tee");
    equip_id("acc_glasses");
    TEST_ASSERT_TRUE(dress_room_current_recipe_is_discovered());

    TEST_ASSERT_TRUE(dress_room_prepare_next_undiscovered());
    TEST_ASSERT_EQUAL_INT(DRESS_ESSENCE_BLOOM, dress_room_primary_essence());
    TEST_ASSERT_EQUAL_INT(DRESS_ESSENCE_BLOOM, dress_room_secondary_essence());
    TEST_ASSERT_FALSE(dress_room_current_recipe_is_discovered());
    TEST_ASSERT_EQUAL_INT(DRESS_SLOT_HAIR, dress_room_category());

    game_state.recipe_mask = 0x3F;
    TEST_ASSERT_FALSE(dress_room_prepare_next_undiscovered());
}

void test_lookbook_can_route_to_any_recipe_and_reports_saved_signatures(void) {
    game_state.recipe_mask = 1 << 3;
    game_state.lookbook_mask = (1 << (3 * 3)) | (1 << (3 * 3 + 2));
    TEST_ASSERT_TRUE(dress_room_recipe_is_discovered(3));
    TEST_ASSERT_FALSE(dress_room_recipe_is_discovered(5));
    TEST_ASSERT_TRUE(dress_room_lookbook_has(3, 0));
    TEST_ASSERT_FALSE(dress_room_lookbook_has(3, 1));
    TEST_ASSERT_TRUE(dress_room_lookbook_has(3, 2));
    TEST_ASSERT_FALSE(dress_room_lookbook_has(-1, 0));

    TEST_ASSERT_TRUE(dress_room_prepare_recipe(5));
    TEST_ASSERT_EQUAL_INT(DRESS_ESSENCE_BLOOM, dress_room_primary_essence());
    TEST_ASSERT_EQUAL_INT(DRESS_ESSENCE_FLAME, dress_room_secondary_essence());
    TEST_ASSERT_EQUAL_INT(DRESS_SLOT_HAIR, dress_room_category());
    TEST_ASSERT_FALSE(dress_room_prepare_recipe(6));
}

void test_telemetry_discovery_emits_one_start_reveal_and_first_mastery(void) {
    equip_moon_moon_focus();
    confirm_supports();
    game_event_frame_reset();

    TEST_ASSERT_TRUE(dress_room_begin_awakening());
    const game_event_t *start_event = last_event_of_type(dress_room_ev_awakening_start_type());
    TEST_ASSERT_NOT_NULL(start_event);
    const dress_room_ev_awakening_start_t *start = start_event->payload;
    TEST_ASSERT_EQUAL_INT(0, start->recipe_index);
    TEST_ASSERT_EQUAL_INT(7, start->support_mask);
    TEST_ASSERT_EQUAL_INT(0, start->look_slot);
    TEST_ASSERT_FALSE(start->recipe_known);
    TEST_ASSERT_FALSE(start->look_known);

    dress_room_awakening_tick(100.0F);
    const game_event_t *reveal_event = last_event_of_type(dress_room_ev_recipe_reveal_type());
    TEST_ASSERT_NOT_NULL(reveal_event);
    const dress_room_ev_recipe_reveal_t *reveal = reveal_event->payload;
    TEST_ASSERT_EQUAL_INT(DRESS_ROOM_REVEAL_DISCOVERY, reveal->outcome);
    TEST_ASSERT_EQUAL_INT(1, reveal->recipes_found);
    TEST_ASSERT_EQUAL_INT(1, reveal->looks_found);
    TEST_ASSERT_EQUAL_INT(1, event_count_of_type(dress_room_ev_recipe_reveal_type()));

    const game_event_t *mastery_event = last_event_of_type(dress_room_ev_collection_mastery_type());
    TEST_ASSERT_NOT_NULL(mastery_event);
    const dress_room_ev_collection_mastery_t *mastery = mastery_event->payload;
    TEST_ASSERT_EQUAL_INT(1, mastery->milestone);
    TEST_ASSERT_EQUAL_INT(1, mastery->recipes_found);
}

void test_telemetry_remix_is_not_discovery_or_mastery(void) {
    equip_moon_moon_focus();
    confirm_supports();
    game_state.recipe_mask = 1 << 0;
    game_event_frame_reset();

    TEST_ASSERT_TRUE(dress_room_begin_awakening());
    dress_room_awakening_tick(100.0F);

    const dress_room_ev_recipe_reveal_t *reveal =
        last_event_of_type(dress_room_ev_recipe_reveal_type())->payload;
    TEST_ASSERT_EQUAL_INT(DRESS_ROOM_REVEAL_REMIX, reveal->outcome);
    TEST_ASSERT_EQUAL_INT(1, reveal->recipes_found);
    TEST_ASSERT_EQUAL_INT(1, reveal->looks_found);
    TEST_ASSERT_EQUAL_INT(0, event_count_of_type(dress_room_ev_collection_mastery_type()));
}

void test_telemetry_exact_replay_is_idempotent_and_reports_replay(void) {
    equip_moon_moon_focus();
    confirm_supports();
    const int look_bit = 1 << dress_room_style_signature();
    game_state.recipe_mask = 1 << 0;
    game_state.lookbook_mask = look_bit;
    TEST_ASSERT_TRUE(dress_room_save_current_look());
    game_state.rounds_completed = 4;
    game_event_frame_reset();

    TEST_ASSERT_TRUE(dress_room_begin_awakening());
    TEST_ASSERT_TRUE(dress_room_skip_replay());

    const dress_room_ev_recipe_reveal_t *reveal =
        last_event_of_type(dress_room_ev_recipe_reveal_type())->payload;
    TEST_ASSERT_EQUAL_INT(DRESS_ROOM_REVEAL_REPLAY, reveal->outcome);
    TEST_ASSERT_EQUAL_INT(1, reveal->recipes_found);
    TEST_ASSERT_EQUAL_INT(1, reveal->looks_found);
    TEST_ASSERT_EQUAL_INT(4, game_state.rounds_completed);
    TEST_ASSERT_EQUAL_INT(1, event_count_of_type(dress_room_ev_recipe_reveal_type()));
    TEST_ASSERT_EQUAL_INT(0, event_count_of_type(dress_room_ev_collection_mastery_type()));
}

void test_telemetry_lookbook_open_uses_only_bounded_collection_counts(void) {
    game_state.recipe_mask = (1 << 0) | (1 << 3);
    game_state.lookbook_mask = (1 << 0) | (1 << 9) | (1 << 11);
    game_event_frame_reset();

    dress_room_lookbook_opened();

    const dress_room_ev_lookbook_open_t *opened =
        last_event_of_type(dress_room_ev_lookbook_open_type())->payload;
    TEST_ASSERT_EQUAL_INT(2, opened->recipes_found);
    TEST_ASSERT_EQUAL_INT(0, opened->looks_found); /* legacy mask is not authorship */
    TEST_ASSERT_EQUAL_INT(1, event_count_of_type(dress_room_ev_lookbook_open_type()));
}

void test_telemetry_contract_clamps_every_public_numeric_dimension(void) {
    TEST_ASSERT_EQUAL_INT(4, dress_room_ev_desc_count);
    TEST_ASSERT_EQUAL_STRING("runway.awakening_start", dress_room_ev_descs[0]->name);
    TEST_ASSERT_EQUAL_STRING("runway.recipe_reveal", dress_room_ev_descs[1]->name);

    dress_room_events_emit_awakening_start(-7, 99, 99, 99, true, false);
    const dress_room_ev_awakening_start_t *start =
        last_event_of_type(dress_room_ev_awakening_start_type())->payload;
    TEST_ASSERT_EQUAL_INT(0, start->recipe_index);
    TEST_ASSERT_EQUAL_INT(7, start->support_mask);
    TEST_ASSERT_EQUAL_INT(3, start->look_slot);
    TEST_ASSERT_EQUAL_INT(8, start->round_index);

    dress_room_events_emit_recipe_reveal(99, -4, -3, -3,
                                         (dress_room_reveal_outcome_t)99, 99, -1);
    const dress_room_ev_recipe_reveal_t *reveal =
        last_event_of_type(dress_room_ev_recipe_reveal_type())->payload;
    TEST_ASSERT_EQUAL_INT(5, reveal->recipe_index);
    TEST_ASSERT_EQUAL_INT(0, reveal->support_mask);
    TEST_ASSERT_EQUAL_INT(0, reveal->look_slot);
    TEST_ASSERT_EQUAL_INT(1, reveal->round_index);
    TEST_ASSERT_EQUAL_INT(DRESS_ROOM_REVEAL_REPLAY, reveal->outcome);
    TEST_ASSERT_EQUAL_INT(6, reveal->recipes_found);
    TEST_ASSERT_EQUAL_INT(0, reveal->looks_found);
}

void test_saved_look_exact_duplicate_and_one_item_change_is_novel(void) {
    equip_id("top_tee");
    equip_id("acc_hat");
    TEST_ASSERT_EQUAL_INT(-1, dress_room_current_saved_slot());
    TEST_ASSERT_TRUE(dress_room_save_current_look());
    TEST_ASSERT_EQUAL_INT(0, dress_room_current_saved_slot());
    TEST_ASSERT_EQUAL_INT(1, dress_room_saved_look_count());
    TEST_ASSERT_EQUAL_INT(1, dress_room_saved_look_count_for_recipe(3));
    TEST_ASSERT_FALSE(dress_room_save_current_look()); /* exact duplicate */

    const char *const alternatives[DRESS_SLOT_COUNT] = {
        "hair_long", "top_crop", "bot_skirt", "shoe_boot", "acc_scarf",
    };
    for (int changed_slot = 0; changed_slot < DRESS_SLOT_COUNT; ++changed_slot) {
        equip_id(alternatives[changed_slot]);
        TEST_ASSERT_EQUAL_INT(-1, dress_room_current_saved_slot());
        TEST_ASSERT_TRUE(dress_room_equip_saved_look(3, 0));
        TEST_ASSERT_EQUAL_INT(0, dress_room_current_saved_slot());
    }
}

void test_same_legacy_signature_can_hold_distinct_exact_outfits(void) {
    equip_complete_moon_moon();
    const int old_signature = dress_room_style_signature();
    TEST_ASSERT_TRUE(dress_room_save_current_look());

    equip_id("hair_long");
    equip_id("bot_phoenix"); /* +1 hair, +5 bottom: same modulo-3 signature. */
    TEST_ASSERT_EQUAL_INT(old_signature, dress_room_style_signature());
    TEST_ASSERT_EQUAL_INT(-1, dress_room_current_saved_slot());
    TEST_ASSERT_TRUE(dress_room_save_current_look());
    TEST_ASSERT_EQUAL_INT(2, dress_room_saved_look_count());
}

void test_replace_changes_only_the_selected_saved_slot(void) {
    equip_complete_moon_moon();
    TEST_ASSERT_TRUE(dress_room_save_current_look());
    equip_id("hair_long");
    TEST_ASSERT_TRUE(dress_room_save_current_look());

    equip_id("hair_pink");
    TEST_ASSERT_TRUE(dress_room_replace_saved_look(0, 0));
    TEST_ASSERT_TRUE(dress_room_equip_saved_look(0, 1));
    TEST_ASSERT_EQUAL_STRING("hair_long", equipped_catalog_id(DRESS_SLOT_HAIR));
    TEST_ASSERT_TRUE(dress_room_equip_saved_look(0, 0));
    TEST_ASSERT_EQUAL_STRING("hair_pink", equipped_catalog_id(DRESS_SLOT_HAIR));
}

void test_saved_look_indices_equip_and_invalid_ids_are_rejected(void) {
    equip_complete_moon_moon();
    TEST_ASSERT_TRUE(dress_room_save_current_look());
    int indices[DRESS_SLOT_COUNT] = {-1, -1, -1, -1, -1};
    TEST_ASSERT_TRUE(dress_room_saved_look_indices(0, 0, indices));
    for (int slot = 0; slot < DRESS_SLOT_COUNT; ++slot) {
        TEST_ASSERT_TRUE(indices[slot] >= 0);
    }

    strcpy(game_state.saved_looks[0].hair_id, "top_tee"); /* existing, wrong slot */
    TEST_ASSERT_FALSE(dress_room_saved_look_indices(0, 0, indices));
    TEST_ASSERT_FALSE(dress_room_equip_saved_look(0, 0));
    TEST_ASSERT_FALSE(dress_room_equip_saved_look(-1, 0));
    TEST_ASSERT_FALSE(dress_room_equip_saved_look(0, 3));

    game_state.saved_looks[1].used = true;
    strcpy(game_state.saved_looks[1].key, "unknown-recipe/0");
    strcpy(game_state.saved_looks[1].hair_id, "hair_bob");
    strcpy(game_state.saved_looks[1].main_id, "top_tee");
    strcpy(game_state.saved_looks[1].bottom_id, "bot_jeans");
    strcpy(game_state.saved_looks[1].shoes_id, "shoe_sneak");
    strcpy(game_state.saved_looks[1].accent_id, "acc_glasses");
    TEST_ASSERT_EQUAL_INT(0, dress_room_saved_look_count());
}

void test_saved_look_global_capacity_is_eighteen(void) {
    const char *const tops[] = {"top_tee", "top_hoodie", "top_blazer", "top_tee", "top_tee", "top_hoodie"};
    const char *const accs[] = {"acc_glasses", "acc_hat", "acc_bag", "acc_hat", "acc_bag", "acc_bag"};
    for (int recipe = 0; recipe < 6; ++recipe) {
        for (int slot = 0; slot < 3; ++slot) {
            dress_room_reset_outfit();
            equip_id(tops[recipe]);
            equip_id(accs[recipe]);
            if (slot == 1) equip_id("hair_long");
            if (slot == 2) equip_id("hair_pink");
            TEST_ASSERT_TRUE(dress_room_save_current_look());
        }
        TEST_ASSERT_EQUAL_INT(3, dress_room_saved_look_count_for_recipe(recipe));
    }
    TEST_ASSERT_EQUAL_INT(18, dress_room_saved_look_count());
    dress_room_reset_outfit();
    equip_complete_moon_moon();
    equip_id("hair_gold");
    TEST_ASSERT_FALSE(dress_room_save_current_look());
}

void test_fourth_novel_recipe_look_wins_but_stays_unsaved_when_three_slots_are_full(void) {
    equip_complete_moon_moon();
    TEST_ASSERT_TRUE(dress_room_save_current_look());
    equip_id("hair_long");
    TEST_ASSERT_TRUE(dress_room_save_current_look());
    equip_id("hair_pink");
    TEST_ASSERT_TRUE(dress_room_save_current_look());
    TEST_ASSERT_EQUAL_INT(3, dress_room_saved_look_count_for_recipe(0));

    equip_id("hair_gold");
    confirm_supports();
    TEST_ASSERT_TRUE(dress_room_begin_awakening());
    TEST_ASSERT_TRUE(dress_room_awakening_won());
    TEST_ASSERT_TRUE(dress_room_awakening_lookbook_full());
    TEST_ASSERT_EQUAL_INT(-1, dress_room_awakening_saved_slot());
    dress_room_awakening_tick(100.0F);

    TEST_ASSERT_EQUAL_INT(3, dress_room_saved_look_count_for_recipe(0));
    TEST_ASSERT_EQUAL_INT(0, game_state.rounds_completed);
    TEST_ASSERT_EQUAL_STRING("hair_gold", equipped_catalog_id(DRESS_SLOT_HAIR));
}

void test_saved_look_survives_state_reload_and_equips_exact_five_ids(void) {
    equip_id("hair_pink");
    equip_id("top_tee");
    equip_id("bot_skirt");
    equip_id("shoe_heel");
    equip_id("acc_hat");
    TEST_ASSERT_TRUE(dress_room_save_current_look());
    cJSON *saved = game_state_to_json(&game_state);
    TEST_ASSERT_NOT_NULL(saved);

    game_state_init_defaults(&game_state);
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_state_from_json(&game_state, saved, err, (int)sizeof err));
    dress_room_init();
    dress_room_reset_outfit();
    TEST_ASSERT_TRUE(dress_room_equip_saved_look(3, 0));
    TEST_ASSERT_EQUAL_STRING("hair_pink", equipped_catalog_id(DRESS_SLOT_HAIR));
    TEST_ASSERT_EQUAL_STRING("top_tee", equipped_catalog_id(DRESS_SLOT_TOP));
    TEST_ASSERT_EQUAL_STRING("bot_skirt", equipped_catalog_id(DRESS_SLOT_BOTTOM));
    TEST_ASSERT_EQUAL_STRING("shoe_heel", equipped_catalog_id(DRESS_SLOT_SHOES));
    TEST_ASSERT_EQUAL_STRING("acc_hat", equipped_catalog_id(DRESS_SLOT_ACC));
    cJSON_Delete(saved);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_reset_keeps_focus_slots_empty_for_player_authorship);
    RUN_TEST(test_equip_toggle_clears_slot);
    RUN_TEST(test_randomize_fills_all_slots);
    RUN_TEST(test_category_roundtrip);
    RUN_TEST(test_fresh_round_starts_on_main_and_main_equip_opens_accent);
    RUN_TEST(test_focus_pair_requires_hair_bottom_shoes_confirmation_in_order);
    RUN_TEST(test_changing_earlier_support_or_focus_resets_later_confirmations);
    RUN_TEST(test_crescent_main_suppresses_support_bottom);
    RUN_TEST(test_catalog_has_expected_production_counts_per_slot);
    RUN_TEST(test_equip_oob_rejected);
    RUN_TEST(test_randomize_seed_deterministic);
    RUN_TEST(test_score_outfit_deterministic);
    RUN_TEST(test_score_theme_fit_matters);
    RUN_TEST(test_show_returns_to_freeplay);
    RUN_TEST(test_show_tick_advances_to_podium);
    RUN_TEST(test_show_again_from_podium_restarts_show);
    RUN_TEST(test_enter_theme_pick_returns_freeplay_from_show);
    RUN_TEST(test_rival_outfits_filled_after_begin_show);
    RUN_TEST(test_essence_metadata_is_stable_and_human_readable);
    RUN_TEST(test_six_recipes_cover_every_unordered_pair);
    RUN_TEST(test_awakening_requires_both_focus_essences);
    RUN_TEST(test_begin_awakening_snapshots_deterministic_recipe_and_always_wins);
    RUN_TEST(test_awakening_timeline_advances_through_every_phase);
    RUN_TEST(test_large_tick_is_deterministic_and_restyle_starts_a_fresh_focus_pair);
    RUN_TEST(test_init_hydrates_valid_saved_outfit_ids_without_dirtying);
    RUN_TEST(test_init_rejects_unknown_or_wrong_slot_ids_with_safe_slot_fallback);
    RUN_TEST(test_valid_equip_persists_stable_id_and_first_equip_then_marks_dirty);
    RUN_TEST(test_recipe_card_commits_discovery_and_round_exactly_once);
    RUN_TEST(test_discovered_count_tracks_unique_recipe_bits_only);
    RUN_TEST(test_collection_milestones_are_derived_from_persistent_recipe_progress);
    RUN_TEST(test_collection_milestones_ignore_non_recipe_bits);
    RUN_TEST(test_repeat_is_known_and_can_skip_directly_to_card);
    RUN_TEST(test_new_support_signature_records_a_remix_but_exact_duplicate_does_not);
    RUN_TEST(test_next_undiscovered_recipe_and_style_signature_are_player_driven);
    RUN_TEST(test_known_current_recipe_can_prepare_the_next_unknown_pair);
    RUN_TEST(test_lookbook_can_route_to_any_recipe_and_reports_saved_signatures);
    RUN_TEST(test_telemetry_discovery_emits_one_start_reveal_and_first_mastery);
    RUN_TEST(test_telemetry_remix_is_not_discovery_or_mastery);
    RUN_TEST(test_telemetry_exact_replay_is_idempotent_and_reports_replay);
    RUN_TEST(test_telemetry_lookbook_open_uses_only_bounded_collection_counts);
    RUN_TEST(test_telemetry_contract_clamps_every_public_numeric_dimension);
    RUN_TEST(test_saved_look_exact_duplicate_and_one_item_change_is_novel);
    RUN_TEST(test_same_legacy_signature_can_hold_distinct_exact_outfits);
    RUN_TEST(test_replace_changes_only_the_selected_saved_slot);
    RUN_TEST(test_saved_look_indices_equip_and_invalid_ids_are_rejected);
    RUN_TEST(test_saved_look_global_capacity_is_eighteen);
    RUN_TEST(test_fourth_novel_recipe_look_wins_but_stays_unsaved_when_three_slots_are_full);
    RUN_TEST(test_saved_look_survives_state_reload_and_equips_exact_five_ids);
    return UNITY_END();
}
