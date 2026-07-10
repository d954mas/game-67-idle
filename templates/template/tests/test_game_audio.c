#include <stdbool.h>
#include <stdint.h>

#include "unity.h"

#include "features/audio/audio.h"
#include "game_audio.h"
#include "generated/game_assets.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"

typedef struct play_call_t {
    audio_clip_t clip;
    audio_bus_t bus;
    float gain;
    bool loop;
} play_call_t;

static bool s_backend_available;
static bool s_resource_ready[2];
static nt_hash64_t s_requested_ids[2];
static uint8_t s_requested_types[2];
static unsigned s_request_count;
static unsigned s_audio_init_count;
static unsigned s_audio_update_count;
static unsigned s_audio_shutdown_count;
static unsigned s_clip_load_count[2];
static audio_clip_state_t s_clip_state[2];
static play_call_t s_play_calls[8];
static unsigned s_play_count;
static audio_voice_t s_stopped_voices[8];
static unsigned s_stop_count;
static float s_master;
static float s_music;
static float s_sfx;
static unsigned s_mix_count;
static bool s_enabled;
static bool s_paused;
static unsigned s_gesture_count;

static bool same_hash(nt_hash64_t lhs, nt_hash64_t rhs) {
    return lhs.value == rhs.value;
}

static unsigned asset_index(nt_hash64_t id) {
    if (same_hash(id, ASSET_BLOB_AUDIO_SFX_UI_CLICK)) return 0;
    if (same_hash(id, ASSET_BLOB_AUDIO_MUSIC_DEMO_JINGLE)) return 1;
    return 2;
}

void setUp(void) {
    s_backend_available = true;
    s_resource_ready[0] = false;
    s_resource_ready[1] = false;
    s_request_count = 0;
    s_audio_init_count = 0;
    s_audio_update_count = 0;
    s_audio_shutdown_count = 0;
    s_clip_load_count[0] = 0;
    s_clip_load_count[1] = 0;
    s_clip_state[0] = AUDIO_CLIP_STATE_LOADING;
    s_clip_state[1] = AUDIO_CLIP_STATE_LOADING;
    s_play_count = 0;
    s_stop_count = 0;
    s_master = 0.80f;
    s_music = 0.70f;
    s_sfx = 0.60f;
    s_mix_count = 0;
    s_enabled = true;
    s_paused = false;
    s_gesture_count = 0;
}

void tearDown(void) {
    game_audio_shutdown();
}

nt_resource_t nt_resource_request(nt_hash64_t resource_id, uint8_t asset_type) {
    const unsigned index = asset_index(resource_id);
    TEST_ASSERT_LESS_THAN_UINT32(2, index);
    s_requested_ids[s_request_count] = resource_id;
    s_requested_types[s_request_count] = asset_type;
    ++s_request_count;
    return (nt_resource_t){index + 1};
}

bool nt_resource_is_ready(nt_resource_t handle) {
    return handle.id >= 1 && handle.id <= 2 && s_resource_ready[handle.id - 1];
}

bool audio_init(void) {
    ++s_audio_init_count;
    return s_backend_available;
}

void audio_shutdown(void) {
    ++s_audio_shutdown_count;
}

void audio_update(void) {
    ++s_audio_update_count;
}

audio_clip_t audio_clip_load(nt_hash64_t ready_blob_id) {
    const unsigned index = asset_index(ready_blob_id);
    TEST_ASSERT_LESS_THAN_UINT32(2, index);
    ++s_clip_load_count[index];
    return (audio_clip_t){index + 1};
}

audio_clip_state_t audio_clip_state(audio_clip_t clip) {
    if (clip.value < 1 || clip.value > 2) return AUDIO_CLIP_STATE_INVALID;
    return s_clip_state[clip.value - 1];
}

void audio_clip_unload(audio_clip_t clip) {
    (void)clip;
}

audio_voice_t audio_play(audio_clip_t clip, audio_bus_t bus, float gain, bool loop) {
    s_play_calls[s_play_count] = (play_call_t){clip, bus, gain, loop};
    ++s_play_count;
    return (audio_voice_t){100 + s_play_count};
}

void audio_voice_stop(audio_voice_t voice) {
    s_stopped_voices[s_stop_count++] = voice;
}

bool audio_voice_is_playing(audio_voice_t voice) {
    (void)voice;
    return false;
}

void audio_set_mix(float master, float music, float sfx) {
    s_master = master;
    s_music = music;
    s_sfx = sfx;
    ++s_mix_count;
}

void audio_set_enabled(bool enabled) {
    s_enabled = enabled;
}

void audio_set_paused(bool paused) {
    s_paused = paused;
}

void audio_on_user_gesture(void) {
    ++s_gesture_count;
}

audio_status_t audio_status(void) {
    return (audio_status_t){
        .available = s_backend_available,
        .unlocked = true,
        .enabled = s_enabled,
        .paused = s_paused,
    };
}

float settings_master(void) { return s_master; }
float settings_music(void) { return s_music; }
float settings_sfx(void) { return s_sfx; }

static void make_catalog_ready(void) {
    s_resource_ready[0] = true;
    s_resource_ready[1] = true;
    game_audio_update();
    s_clip_state[0] = AUDIO_CLIP_STATE_READY;
    s_clip_state[1] = AUDIO_CLIP_STATE_READY;
    game_audio_update();
}

void test_init_requests_both_blob_resources_and_waits_for_pack_readiness(void) {
    TEST_ASSERT_TRUE(game_audio_init());

    TEST_ASSERT_EQUAL_UINT32(1, s_audio_init_count);
    TEST_ASSERT_EQUAL_UINT32(2, s_request_count);
    TEST_ASSERT_TRUE(same_hash(ASSET_BLOB_AUDIO_SFX_UI_CLICK, s_requested_ids[0]));
    TEST_ASSERT_TRUE(same_hash(ASSET_BLOB_AUDIO_MUSIC_DEMO_JINGLE, s_requested_ids[1]));
    TEST_ASSERT_EQUAL_UINT8(NT_ASSET_BLOB, s_requested_types[0]);
    TEST_ASSERT_EQUAL_UINT8(NT_ASSET_BLOB, s_requested_types[1]);

    game_audio_update();
    TEST_ASSERT_EQUAL_UINT32(0, s_clip_load_count[0]);
    TEST_ASSERT_EQUAL_UINT32(0, s_clip_load_count[1]);

    s_resource_ready[0] = true;
    game_audio_update();
    game_audio_update();
    TEST_ASSERT_EQUAL_UINT32(1, s_clip_load_count[0]);
    TEST_ASSERT_EQUAL_UINT32(0, s_clip_load_count[1]);

    s_resource_ready[1] = true;
    game_audio_update();
    game_audio_update();
    TEST_ASSERT_EQUAL_UINT32(1, s_clip_load_count[0]);
    TEST_ASSERT_EQUAL_UINT32(1, s_clip_load_count[1]);
}

void test_update_applies_settings_only_when_values_change_and_polls_clip_state(void) {
    TEST_ASSERT_TRUE(game_audio_init());
    s_resource_ready[0] = true;
    game_audio_update();

    TEST_ASSERT_EQUAL_UINT32(1, s_mix_count);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.80f, s_master);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.70f, s_music);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.60f, s_sfx);
    TEST_ASSERT_EQUAL(GAME_AUDIO_LOAD_LOADING, game_audio_status().cue_state);

    game_audio_update();
    TEST_ASSERT_EQUAL_UINT32(1, s_mix_count);
    s_music = 0.25f;
    game_audio_update();
    TEST_ASSERT_EQUAL_UINT32(2, s_mix_count);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.25f, s_music);

    s_clip_state[0] = AUDIO_CLIP_STATE_READY;
    game_audio_update();
    TEST_ASSERT_EQUAL(GAME_AUDIO_LOAD_READY, game_audio_status().cue_state);
}

void test_play_cue_uses_the_catalog_sfx_bus_and_gain(void) {
    TEST_ASSERT_TRUE(game_audio_init());
    make_catalog_ready();

    TEST_ASSERT_TRUE(game_audio_play_cue(GAME_AUDIO_CUE_UI_CLICK));
    TEST_ASSERT_EQUAL_UINT32(1, s_play_count);
    TEST_ASSERT_EQUAL(AUDIO_BUS_SFX, s_play_calls[0].bus);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.80f, s_play_calls[0].gain);
    TEST_ASSERT_FALSE(s_play_calls[0].loop);
    TEST_ASSERT_FALSE(game_audio_play_cue(GAME_AUDIO_CUE_COUNT));
    TEST_ASSERT_EQUAL_UINT32(1, s_play_count);
}

void test_music_play_replaces_the_previous_voice_and_stop_is_idempotent(void) {
    TEST_ASSERT_TRUE(game_audio_init());
    make_catalog_ready();

    TEST_ASSERT_TRUE(game_audio_play_music(GAME_MUSIC_TRACK_DEMO_JINGLE, true));
    TEST_ASSERT_EQUAL(AUDIO_BUS_MUSIC, s_play_calls[0].bus);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, s_play_calls[0].gain);
    TEST_ASSERT_TRUE(s_play_calls[0].loop);
    TEST_ASSERT_EQUAL_UINT32(0, s_stop_count);

    TEST_ASSERT_TRUE(game_audio_play_music(GAME_MUSIC_TRACK_DEMO_JINGLE, false));
    TEST_ASSERT_FALSE(s_play_calls[1].loop);
    TEST_ASSERT_EQUAL_UINT32(1, s_stop_count);
    TEST_ASSERT_EQUAL_UINT32(101, s_stopped_voices[0].value);

    game_audio_stop_music();
    game_audio_stop_music();
    TEST_ASSERT_EQUAL_UINT32(2, s_stop_count);
    TEST_ASSERT_EQUAL_UINT32(102, s_stopped_voices[1].value);
}

void test_controls_are_forwarded_and_unavailable_backend_is_reported(void) {
    s_backend_available = false;
    TEST_ASSERT_FALSE(game_audio_init());
    TEST_ASSERT_EQUAL_UINT32(2, s_request_count);
    TEST_ASSERT_TRUE(game_audio_status().initialized);
    TEST_ASSERT_FALSE(game_audio_status().available);
    TEST_ASSERT_FALSE(game_audio_play_cue(GAME_AUDIO_CUE_UI_CLICK));
    TEST_ASSERT_FALSE(game_audio_play_music(GAME_MUSIC_TRACK_DEMO_JINGLE, true));
    TEST_ASSERT_EQUAL_UINT32(0, s_play_count);

    game_audio_set_enabled(false);
    game_audio_set_paused(true);
    game_audio_on_user_gesture();
    TEST_ASSERT_FALSE(s_enabled);
    TEST_ASSERT_TRUE(s_paused);
    TEST_ASSERT_EQUAL_UINT32(1, s_gesture_count);
    TEST_ASSERT_FALSE(game_audio_status().enabled);
    TEST_ASSERT_TRUE(game_audio_status().paused);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_requests_both_blob_resources_and_waits_for_pack_readiness);
    RUN_TEST(test_update_applies_settings_only_when_values_change_and_polls_clip_state);
    RUN_TEST(test_play_cue_uses_the_catalog_sfx_bus_and_gain);
    RUN_TEST(test_music_play_replaces_the_previous_voice_and_stop_is_idempotent);
    RUN_TEST(test_controls_are_forwarded_and_unavailable_backend_is_reported);
    return UNITY_END();
}
