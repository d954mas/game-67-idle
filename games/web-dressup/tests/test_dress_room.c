#include "unity.h"

#include "features/dress_room/dress_room.h"

void setUp(void) { dress_room_init(); }
void tearDown(void) {}

void test_reset_equips_one_per_slot(void) {
    dress_room_reset_outfit();
    for (int s = 0; s < DRESS_SLOT_COUNT; ++s) {
        const int idx = dress_room_equipped((dress_slot_t)s);
        TEST_ASSERT_TRUE(idx >= 0);
        const dress_item_t *it = dress_room_catalog_item(idx);
        TEST_ASSERT_NOT_NULL(it);
        TEST_ASSERT_EQUAL_INT(s, (int)it->slot);
        TEST_ASSERT_NOT_NULL(it->atlas_layer);
    }
}

void test_equip_toggle_clears_slot(void) {
    dress_room_reset_outfit();
    const int idx = dress_room_equipped(DRESS_SLOT_TOP);
    TEST_ASSERT_TRUE(idx >= 0);
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

void test_catalog_has_four_per_slot(void) {
    int counts[DRESS_SLOT_COUNT] = {0};
    for (int i = 0; i < dress_room_catalog_count(); ++i) {
        const dress_item_t *it = dress_room_catalog_item(i);
        TEST_ASSERT_NOT_NULL(it);
        counts[it->slot] += 1;
    }
    for (int s = 0; s < DRESS_SLOT_COUNT; ++s) {
        TEST_ASSERT_EQUAL_INT(4, counts[s]);
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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_reset_equips_one_per_slot);
    RUN_TEST(test_equip_toggle_clears_slot);
    RUN_TEST(test_randomize_fills_all_slots);
    RUN_TEST(test_category_roundtrip);
    RUN_TEST(test_catalog_has_four_per_slot);
    RUN_TEST(test_equip_oob_rejected);
    RUN_TEST(test_randomize_seed_deterministic);
    RUN_TEST(test_score_outfit_deterministic);
    RUN_TEST(test_score_theme_fit_matters);
    RUN_TEST(test_show_returns_to_freeplay);
    RUN_TEST(test_show_tick_advances_to_podium);
    RUN_TEST(test_show_again_from_podium_restarts_show);
    RUN_TEST(test_enter_theme_pick_returns_freeplay_from_show);
    RUN_TEST(test_rival_outfits_filled_after_begin_show);
    return UNITY_END();
}
