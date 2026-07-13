#include "unity.h"

#include "game_audio.h"
#include <generated/game_assets.h>
#include "features/audio/audio.h"
#include "features/platform_sdk/platform_sdk.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"

#include <string.h>

typedef struct play_call_t {
    audio_clip_t clip;
    audio_bus_t bus;
    float gain;
    bool loop;
} play_call_t;

static nt_hash64_t s_requested[2];
static unsigned s_request_count;
static bool s_ready[2];
static audio_clip_state_t s_clip_state[2];
static unsigned s_load_count[2];
static play_call_t s_plays[8];
static unsigned s_play_count;
static float s_master = 0.8f;
static float s_music = 0.7f;
static float s_sfx = 0.6f;
static float s_applied_master;
static float s_applied_music;
static float s_applied_sfx;
static unsigned s_mix_count;
static unsigned s_gesture_count;
static unsigned s_shutdown_count;
static unsigned s_clip_unload_count;
static bool s_enabled = true;
static bool s_paused;
static platform_sdk_lifecycle_callback_t s_pause_callback;
static platform_sdk_lifecycle_callback_t s_resume_callback;
static void *s_pause_userdata;
static void *s_resume_userdata;

static unsigned asset_index(nt_hash64_t id) {
    if (id.value == ASSET_BLOB_AUDIO_SFX_UI_CLICK.value) return 0U;
    if (id.value == ASSET_BLOB_AUDIO_SFX_AWAKENING_JINGLE.value) return 1U;
    return 99U;
}

void setUp(void) {
    game_audio_shutdown();
    memset(s_requested, 0, sizeof s_requested);
    memset(s_ready, 0, sizeof s_ready);
    memset(s_load_count, 0, sizeof s_load_count);
    memset(s_plays, 0, sizeof s_plays);
    s_request_count = 0U;
    s_play_count = 0U;
    s_clip_state[0] = AUDIO_CLIP_STATE_LOADING;
    s_clip_state[1] = AUDIO_CLIP_STATE_LOADING;
    s_master = 0.8f;
    s_music = 0.7f;
    s_sfx = 0.6f;
    s_mix_count = 0U;
    s_gesture_count = 0U;
    s_shutdown_count = 0U;
    s_clip_unload_count = 0U;
    s_enabled = true;
    s_paused = false;
    s_pause_callback = NULL;
    s_resume_callback = NULL;
}

void tearDown(void) { game_audio_shutdown(); }

nt_resource_t nt_resource_request(nt_hash64_t id, uint8_t type) {
    TEST_ASSERT_EQUAL_UINT8(NT_ASSET_BLOB, type);
    const unsigned index = asset_index(id);
    TEST_ASSERT_LESS_THAN_UINT32(2U, index);
    s_requested[s_request_count++] = id;
    return (nt_resource_t){index + 1U};
}

bool nt_resource_is_ready(nt_resource_t resource) {
    return resource.id > 0U && resource.id <= 2U && s_ready[resource.id - 1U];
}

uint8_t nt_resource_get_state(nt_resource_t resource) {
    return nt_resource_is_ready(resource) ? 2U : 1U;
}

bool audio_init(void) { return true; }
void audio_shutdown(void) { ++s_shutdown_count; }
void audio_update(void) {}

audio_clip_t audio_clip_load(nt_hash64_t id) {
    const unsigned index = asset_index(id);
    TEST_ASSERT_LESS_THAN_UINT32(2U, index);
    ++s_load_count[index];
    return (audio_clip_t){index + 1U};
}

audio_clip_state_t audio_clip_state(audio_clip_t clip) {
    return clip.value > 0U && clip.value <= 2U ? s_clip_state[clip.value - 1U] : AUDIO_CLIP_STATE_INVALID;
}

void audio_clip_unload(audio_clip_t clip) {
    (void)clip;
    ++s_clip_unload_count;
}

audio_voice_t audio_play(audio_clip_t clip, audio_bus_t bus, float gain, bool loop) {
    s_plays[s_play_count] = (play_call_t){clip, bus, gain, loop};
    ++s_play_count;
    return (audio_voice_t){s_play_count};
}

void audio_voice_stop(audio_voice_t voice) { (void)voice; }
bool audio_voice_is_playing(audio_voice_t voice) { return voice.value != 0U; }

void audio_set_mix(float master, float music, float sfx) {
    s_applied_master = master;
    s_applied_music = music;
    s_applied_sfx = sfx;
    ++s_mix_count;
}

void audio_set_enabled(bool enabled) { s_enabled = enabled; }
void audio_set_paused(bool paused) { s_paused = paused; }
void audio_on_user_gesture(void) { ++s_gesture_count; }
audio_status_t audio_status(void) {
    return (audio_status_t){.available = true, .unlocked = true, .enabled = s_enabled, .paused = s_paused};
}

platform_sdk_listener_id_t platform_sdk_on_pause(platform_sdk_lifecycle_callback_t callback, void *userdata) {
    s_pause_callback = callback;
    s_pause_userdata = userdata;
    return 11U;
}

platform_sdk_listener_id_t platform_sdk_on_resume(platform_sdk_lifecycle_callback_t callback, void *userdata) {
    s_resume_callback = callback;
    s_resume_userdata = userdata;
    return 12U;
}

void platform_sdk_remove_listener(platform_sdk_listener_id_t listener_id) { (void)listener_id; }

float settings_master(void) { return s_master; }
float settings_music(void) { return s_music; }
float settings_sfx(void) { return s_sfx; }

static void make_ready(void) {
    s_ready[0] = true;
    s_ready[1] = true;
    game_audio_update();
    s_clip_state[0] = AUDIO_CLIP_STATE_READY;
    s_clip_state[1] = AUDIO_CLIP_STATE_READY;
    game_audio_update();
}

void test_catalog_requests_two_codec_neutral_blobs_and_exposes_four_runway_cues(void) {
    TEST_ASSERT_TRUE(game_audio_init());
    TEST_ASSERT_EQUAL_UINT32(2U, s_request_count);
    TEST_ASSERT_EQUAL_UINT64(ASSET_BLOB_AUDIO_SFX_UI_CLICK.value, s_requested[0].value);
    TEST_ASSERT_EQUAL_UINT64(ASSET_BLOB_AUDIO_SFX_AWAKENING_JINGLE.value, s_requested[1].value);
    make_ready();
    TEST_ASSERT_EQUAL_STRING("ui_click", game_audio_cue_name(GAME_AUDIO_CUE_UI_CLICK));
    TEST_ASSERT_EQUAL_STRING("awakening_charge", game_audio_cue_name(GAME_AUDIO_CUE_AWAKENING_CHARGE));
    TEST_ASSERT_EQUAL_STRING("awakening_flash", game_audio_cue_name(GAME_AUDIO_CUE_AWAKENING_FLASH));
    TEST_ASSERT_EQUAL_STRING("awakening_reveal", game_audio_cue_name(GAME_AUDIO_CUE_AWAKENING_REVEAL));
    for (int cue = 0; cue < GAME_AUDIO_CUE_COUNT; ++cue) {
        TEST_ASSERT_TRUE(game_audio_play_cue((GameAudioCue)cue));
        TEST_ASSERT_EQUAL(AUDIO_BUS_SFX, s_plays[cue].bus);
        TEST_ASSERT_FALSE(s_plays[cue].loop);
    }
    TEST_ASSERT_EQUAL_UINT32(1U, s_plays[GAME_AUDIO_CUE_UI_CLICK].clip.value);
    TEST_ASSERT_EQUAL_UINT32(2U, s_plays[GAME_AUDIO_CUE_AWAKENING_CHARGE].clip.value);
    TEST_ASSERT_EQUAL_UINT32(1U, s_plays[GAME_AUDIO_CUE_AWAKENING_FLASH].clip.value);
    TEST_ASSERT_EQUAL_UINT32(2U, s_plays[GAME_AUDIO_CUE_AWAKENING_REVEAL].clip.value);
}

void test_settings_pause_resume_gesture_and_enable_are_composed(void) {
    TEST_ASSERT_TRUE(game_audio_init());
    game_audio_update();
    TEST_ASSERT_EQUAL_UINT32(1U, s_mix_count);
    TEST_ASSERT_TRUE(s_applied_master == 0.8f);
    TEST_ASSERT_TRUE(s_applied_music == 0.7f);
    TEST_ASSERT_TRUE(s_applied_sfx == 0.6f);
    s_sfx = 0.25f;
    game_audio_update();
    TEST_ASSERT_EQUAL_UINT32(2U, s_mix_count);
    TEST_ASSERT_TRUE(s_applied_sfx == 0.25f);
    TEST_ASSERT_NOT_NULL(s_pause_callback);
    TEST_ASSERT_NOT_NULL(s_resume_callback);
    s_pause_callback(s_pause_userdata);
    TEST_ASSERT_TRUE(game_audio_status().paused);
    s_resume_callback(s_resume_userdata);
    TEST_ASSERT_FALSE(game_audio_status().paused);
    game_audio_on_user_gesture();
    TEST_ASSERT_EQUAL_UINT32(1U, s_gesture_count);
    game_audio_set_enabled(false);
    TEST_ASSERT_FALSE(game_audio_status().enabled);
}

void test_shutdown_delegates_active_voice_and_clip_teardown_to_audio_core(void) {
    TEST_ASSERT_TRUE(game_audio_init());
    make_ready();
    TEST_ASSERT_TRUE(game_audio_play_cue(GAME_AUDIO_CUE_AWAKENING_REVEAL));

    game_audio_shutdown();

    TEST_ASSERT_EQUAL_UINT32(1U, s_shutdown_count);
    TEST_ASSERT_EQUAL_UINT32(0U, s_clip_unload_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_catalog_requests_two_codec_neutral_blobs_and_exposes_four_runway_cues);
    RUN_TEST(test_settings_pause_resume_gesture_and_enable_are_composed);
    RUN_TEST(test_shutdown_delegates_active_voice_and_clip_teardown_to_audio_core);
    return UNITY_END();
}
