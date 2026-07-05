#include "game_persistence.h"
#include "game_state.h"
#include "game_storage.h"

#include <assert.h>
#include <stdio.h>

static void remove_test_save(void) {
    char path[512];
    char error[128];
    if (game_storage_resolve_key(game_persistence_autosave_key(), GAME_STATE_DOCUMENT, path, (int)sizeof(path),
                                 error, (int)sizeof(error))) {
        (void)remove(path);
    }
}

static void test_clean_state_does_not_create_save(void) {
    char error[256];
    remove_test_save();
    game_state_init();

    assert(!game_state_is_dirty());
    assert(game_persistence_save_autosave_if_dirty(error, (int)sizeof(error)));
    assert(!game_storage_key_exists(game_persistence_autosave_key(), GAME_STATE_DOCUMENT));
}

static void test_autosave_round_trip(void) {
    char error[256];
    remove_test_save();
    game_state_init();

    g_game_state.wallet_gold = 1234;
    g_game_state.hero_xp = 77;
    game_state_mark_dirty();

    assert(game_persistence_save_autosave_if_dirty(error, (int)sizeof(error)));
    assert(!game_state_is_dirty());
    assert(game_storage_key_exists(game_persistence_autosave_key(), GAME_STATE_DOCUMENT));

    game_state_init();
    assert(g_game_state.wallet_gold == GAME_STATE_WALLET_GOLD_DEFAULT);
    assert(g_game_state.hero_xp == GAME_STATE_HERO_XP_DEFAULT);

    assert(game_persistence_load_autosave(false, error, (int)sizeof(error)));
    assert(g_game_state.wallet_gold == 1234);
    assert(g_game_state.hero_xp == 77);
    assert(!game_state_is_dirty());
}

static void test_fresh_state_ignores_saved_data(void) {
    char error[256];

    game_state_init();
    assert(game_persistence_load_autosave(true, error, (int)sizeof(error)));
    assert(g_game_state.wallet_gold == GAME_STATE_WALLET_GOLD_DEFAULT);
    assert(g_game_state.hero_xp == GAME_STATE_HERO_XP_DEFAULT);
    assert(game_storage_key_exists(game_persistence_autosave_key(), GAME_STATE_DOCUMENT));
}

static void test_reset_autosave_starts_new_game(void) {
    char error[256];
    remove_test_save();
    game_state_init();

    g_game_state.wallet_gold = 4321;
    g_game_state.hero_xp = 99;
    game_state_mark_dirty();
    assert(game_persistence_save_autosave_if_dirty(error, (int)sizeof(error)));
    assert(game_storage_key_exists(game_persistence_autosave_key(), GAME_STATE_DOCUMENT));

    assert(game_persistence_reset_autosave(error, (int)sizeof(error)));
    assert(g_game_state.wallet_gold == GAME_STATE_WALLET_GOLD_DEFAULT);
    assert(g_game_state.hero_xp == GAME_STATE_HERO_XP_DEFAULT);
    assert(!game_state_is_dirty());

    g_game_state.wallet_gold = 777;
    g_game_state.hero_xp = 777;
    assert(game_persistence_load_autosave(false, error, (int)sizeof(error)));
    assert(g_game_state.wallet_gold == GAME_STATE_WALLET_GOLD_DEFAULT);
    assert(g_game_state.hero_xp == GAME_STATE_HERO_XP_DEFAULT);
}

int main(void) {
    test_clean_state_does_not_create_save();
    test_autosave_round_trip();
    test_fresh_state_ignores_saved_data();
    test_reset_autosave_starts_new_game();
    remove_test_save();
    return 0;
}
