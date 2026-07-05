#include "game_audio.h"

#include <assert.h>
#include <math.h>
#include <string.h>

static void test_cue_names_are_game_specific(void) {
    assert(strcmp(game_audio_cue_name(GAME_AUDIO_CUE_UI_CLICK), "ui_click") == 0);
    assert(strcmp(game_audio_cue_name(GAME_AUDIO_CUE_DIALOGUE_OPEN), "dialogue_open") == 0);
    assert(strcmp(game_audio_cue_name(GAME_AUDIO_CUE_GEAR_EQUIP), "gear_equip") == 0);
    assert(strcmp(game_audio_cue_name(GAME_AUDIO_CUE_COMBAT_HIT), "combat_hit") == 0);
    assert(strcmp(game_audio_cue_name(GAME_AUDIO_CUE_REWARD), "reward") == 0);
    assert(strcmp(game_audio_cue_name(GAME_AUDIO_CUE_LOCATION_MOVE), "location_move") == 0);
}

static void test_cue_assets_are_game_specific(void) {
    assert(strcmp(game_audio_cue_asset_path(GAME_AUDIO_CUE_UI_CLICK), "sfx/ui_click.mp3") == 0);
    assert(strcmp(game_audio_cue_asset_path(GAME_AUDIO_CUE_DIALOGUE_OPEN), "sfx/dialogue_open.mp3") == 0);
    assert(strcmp(game_audio_cue_asset_path(GAME_AUDIO_CUE_GEAR_EQUIP), "sfx/gear_equip.mp3") == 0);
    assert(strcmp(game_audio_cue_asset_path(GAME_AUDIO_CUE_COMBAT_HIT), "sfx/combat_hit.mp3") == 0);
    assert(strcmp(game_audio_cue_asset_path(GAME_AUDIO_CUE_REWARD), "sfx/reward.mp3") == 0);
    assert(strcmp(game_audio_music_asset_path(), "music/dark_forest_theme.mp3") == 0);
}

static void test_audio_assets_load_from_game_folder(void) {
    game_audio_init();
    game_audio_set_device_enabled(false);

    const GameAudioStatus status = game_audio_status();
    assert(status.initialized);
    assert(status.asset_root != NULL);
    assert(status.loaded_clip_count == GAME_AUDIO_CUE_COUNT);
    assert(status.music_asset_loaded);

    game_audio_shutdown();
}

static void test_sfx_counts_even_when_device_disabled(void) {
    game_audio_init();
    game_audio_set_device_enabled(false);

    game_audio_play(GAME_AUDIO_CUE_UI_CLICK);
    game_audio_play(GAME_AUDIO_CUE_COMBAT_HIT);
    game_audio_play(GAME_AUDIO_CUE_COMBAT_HIT);

    const GameAudioStatus status = game_audio_status();
    assert(status.initialized);
    assert(!status.device_enabled);
    assert(status.total_play_count == 3);
    assert(status.cue_play_count[GAME_AUDIO_CUE_UI_CLICK] == 1);
    assert(status.cue_play_count[GAME_AUDIO_CUE_COMBAT_HIT] == 2);

    game_audio_shutdown();
}

static void test_music_lifecycle_and_volume_clamping(void) {
    game_audio_init();
    game_audio_set_device_enabled(false);
    game_audio_set_volume(1.5F, -0.25F, 0.35F);
    game_audio_start_music();

    GameAudioStatus status = game_audio_status();
    assert(status.music_requested);
    assert(status.music_play_count == 1);
    assert(fabsf(status.master_volume - 1.0F) < 0.001F);
    assert(fabsf(status.music_volume - 0.0F) < 0.001F);
    assert(fabsf(status.sfx_volume - 0.35F) < 0.001F);

    game_audio_set_volume(0.5F, 0.8F, 1.25F);
    game_audio_update();
    status = game_audio_status();
    assert(status.music_requested);
    assert(fabsf(status.master_volume - 0.5F) < 0.001F);
    assert(fabsf(status.music_volume - 0.8F) < 0.001F);
    assert(fabsf(status.sfx_volume - 1.0F) < 0.001F);

    game_audio_stop_music();
    status = game_audio_status();
    assert(!status.music_requested);

    game_audio_shutdown();
}

int main(void) {
    test_cue_names_are_game_specific();
    test_cue_assets_are_game_specific();
    test_audio_assets_load_from_game_folder();
    test_sfx_counts_even_when_device_disabled();
    test_music_lifecycle_and_volume_clamping();
    return 0;
}
