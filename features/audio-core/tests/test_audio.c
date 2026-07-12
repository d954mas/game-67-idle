#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "features/audio/audio.h"
#include "fake_audio_environment.h"

_Static_assert(AUDIO_MAX_CLIPS == 64, "T0393 v1 clip budget changed");
_Static_assert(AUDIO_MAX_VOICES == 32, "T0393 v1 voice budget changed");

static bool s_shutdown_in_test;
static const uint64_t ASSET_READY = UINT64_C(0x1001);

static nt_hash64_t asset(uint64_t value) {
    nt_hash64_t id = {value};
    return id;
}

static bool clip_equal(audio_clip_t lhs, audio_clip_t rhs) {
    return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

static bool voice_equal(audio_voice_t lhs, audio_voice_t rhs) {
    return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

static audio_clip_t make_ready_clip(uint64_t id) {
    fake_audio_add_ready_blob(id);
    audio_clip_t clip = audio_clip_load(asset(id));
    TEST_ASSERT_FALSE(clip_equal(AUDIO_CLIP_INVALID, clip));
    TEST_ASSERT_EQUAL(AUDIO_CLIP_STATE_LOADING, audio_clip_state(clip));
    fake_audio_complete_next_decode(true);
    audio_update();
    TEST_ASSERT_EQUAL(AUDIO_CLIP_STATE_READY, audio_clip_state(clip));
    return clip;
}

static void unlock_audio(void) {
    fake_audio_set_gesture_result(true);
    audio_on_user_gesture();
    TEST_ASSERT_TRUE(audio_status().unlocked);
}

void setUp(void) {
    fake_audio_reset();
    s_shutdown_in_test = false;
    TEST_ASSERT_TRUE(audio_init());
}

void tearDown(void) {
    if (!s_shutdown_in_test) audio_shutdown();
}

void test_clip_load_requires_an_immediately_viewable_ready_blob(void) {
    fake_audio_add_not_ready_blob(UINT64_C(0x1101));
    fake_audio_add_wrong_type_resource(UINT64_C(0x1102));
    fake_audio_add_blob_with_failed_view(UINT64_C(0x1103));

    TEST_ASSERT_TRUE(clip_equal(AUDIO_CLIP_INVALID, audio_clip_load(asset(UINT64_C(0x1100)))));
    TEST_ASSERT_TRUE(clip_equal(AUDIO_CLIP_INVALID, audio_clip_load(asset(UINT64_C(0x1101)))));
    TEST_ASSERT_TRUE(clip_equal(AUDIO_CLIP_INVALID, audio_clip_load(asset(UINT64_C(0x1102)))));
    TEST_ASSERT_TRUE(clip_equal(AUDIO_CLIP_INVALID, audio_clip_load(asset(UINT64_C(0x1103)))));
    TEST_ASSERT_EQUAL_UINT32(0, fake_audio_backend_decode_begin_count());
}

void test_backend_decode_is_async_and_can_fail(void) {
    fake_audio_add_ready_blob(ASSET_READY);
    audio_clip_t clip = audio_clip_load(asset(ASSET_READY));
    TEST_ASSERT_EQUAL(AUDIO_CLIP_STATE_LOADING, audio_clip_state(clip));
    TEST_ASSERT_EQUAL_UINT32(1, fake_audio_backend_decode_begin_count());
    fake_audio_complete_next_decode(false);
    audio_update();
    TEST_ASSERT_EQUAL(AUDIO_CLIP_STATE_FAILED, audio_clip_state(clip));
}

void test_decode_copies_borrowed_blob_bytes_before_load_returns(void) {
    fake_audio_add_ready_blob(ASSET_READY);
    audio_clip_t clip = audio_clip_load(asset(ASSET_READY));
    TEST_ASSERT_EQUAL(AUDIO_CLIP_STATE_LOADING, audio_clip_state(clip));
    TEST_ASSERT_TRUE(fake_audio_pending_decode_owns_original_bytes());

    fake_audio_mutate_and_evict_source(ASSET_READY);
    TEST_ASSERT_TRUE(fake_audio_pending_decode_owns_original_bytes());
    fake_audio_complete_next_decode(true);
    audio_update();
    TEST_ASSERT_EQUAL(AUDIO_CLIP_STATE_READY, audio_clip_state(clip));
}

void test_clip_handles_are_generation_safe_and_capacity_is_fixed(void) {
    audio_clip_t clips[AUDIO_MAX_CLIPS];
    for (uint32_t i = 0; i < AUDIO_MAX_CLIPS; ++i) {
        uint64_t id = UINT64_C(0x2000) + i;
        fake_audio_add_ready_blob(id);
        clips[i] = audio_clip_load(asset(id));
        TEST_ASSERT_FALSE(clip_equal(AUDIO_CLIP_INVALID, clips[i]));
    }
    fake_audio_add_ready_blob(UINT64_C(0x3000));
    TEST_ASSERT_TRUE(clip_equal(AUDIO_CLIP_INVALID, audio_clip_load(asset(UINT64_C(0x3000)))));

    audio_clip_t stale = clips[0];
    audio_clip_unload(stale);
    fake_audio_add_ready_blob(UINT64_C(0x3001));
    audio_clip_t replacement = audio_clip_load(asset(UINT64_C(0x3001)));
    TEST_ASSERT_FALSE(clip_equal(AUDIO_CLIP_INVALID, replacement));
    TEST_ASSERT_FALSE(clip_equal(stale, replacement));
    TEST_ASSERT_EQUAL(AUDIO_CLIP_STATE_INVALID, audio_clip_state(stale));
}

void test_mix_is_clamped_and_applied_atomically(void) {
    uint32_t before = fake_audio_backend_mix_apply_count();
    audio_set_mix(2.0f, -1.0f, 0.25f);
    TEST_ASSERT_EQUAL_UINT32(before + 1, fake_audio_backend_mix_apply_count());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, fake_audio_backend_master());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, fake_audio_backend_music());
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.25f, fake_audio_backend_sfx());
}

void test_play_requires_available_unlocked_enabled_and_not_paused(void) {
    audio_clip_t clip = make_ready_clip(ASSET_READY);
    TEST_ASSERT_TRUE(audio_status().available);
    TEST_ASSERT_FALSE(audio_status().unlocked);
    TEST_ASSERT_TRUE(voice_equal(AUDIO_VOICE_INVALID, audio_play(clip, AUDIO_BUS_SFX, 1.0f, false)));

    fake_audio_set_gesture_result(false);
    audio_on_user_gesture();
    TEST_ASSERT_FALSE(audio_status().unlocked);
    TEST_ASSERT_TRUE(voice_equal(AUDIO_VOICE_INVALID, audio_play(clip, AUDIO_BUS_SFX, 1.0f, false)));

    unlock_audio();
    audio_set_enabled(false);
    TEST_ASSERT_FALSE(audio_status().enabled);
    TEST_ASSERT_TRUE(voice_equal(AUDIO_VOICE_INVALID, audio_play(clip, AUDIO_BUS_SFX, 1.0f, false)));

    audio_set_enabled(true);
    audio_set_paused(true);
    TEST_ASSERT_TRUE(audio_status().paused);
    TEST_ASSERT_TRUE(voice_equal(AUDIO_VOICE_INVALID, audio_play(clip, AUDIO_BUS_MUSIC, 0.5f, true)));
    TEST_ASSERT_EQUAL_UINT32(0, fake_audio_backend_play_count());
}

void test_unavailable_backend_is_reported_and_refuses_load_and_play(void) {
    audio_shutdown();
    fake_audio_reset();
    fake_audio_set_backend_available(false);
    TEST_ASSERT_FALSE(audio_init());
    fake_audio_add_ready_blob(ASSET_READY);
    audio_clip_t clip = audio_clip_load(asset(ASSET_READY));
    TEST_ASSERT_FALSE(audio_status().available);
    TEST_ASSERT_TRUE(clip_equal(AUDIO_CLIP_INVALID, clip));
    TEST_ASSERT_TRUE(voice_equal(AUDIO_VOICE_INVALID, audio_play(clip, AUDIO_BUS_SFX, 1.0f, false)));
}

void test_user_gesture_is_forwarded_and_status_records_success_only(void) {
    fake_audio_set_gesture_result(false);
    audio_on_user_gesture();
    TEST_ASSERT_FALSE(audio_status().unlocked);
    fake_audio_set_gesture_result(true);
    audio_on_user_gesture();
    TEST_ASSERT_TRUE(audio_status().unlocked);
    fake_audio_set_backend_unlocked(false);
    fake_audio_set_gesture_unlocks_immediately(false);
    audio_on_user_gesture();
    TEST_ASSERT_FALSE(audio_status().unlocked);
    TEST_ASSERT_EQUAL_UINT32(3, fake_audio_backend_gesture_count());
}

void test_update_reconciles_an_async_backend_unlock_rejection(void) {
    unlock_audio();
    fake_audio_set_backend_unlocked(false);
    audio_update();
    TEST_ASSERT_FALSE(audio_status().unlocked);
}

void test_finished_voice_is_cleaned_and_reused_with_new_generation(void) {
    audio_clip_t clip = make_ready_clip(ASSET_READY);
    unlock_audio();
    audio_voice_t first = audio_play(clip, AUDIO_BUS_SFX, 1.0f, false);
    TEST_ASSERT_TRUE(audio_voice_is_playing(first));
    fake_audio_finish_oldest_voice();
    audio_update();
    TEST_ASSERT_FALSE(audio_voice_is_playing(first));

    audio_voice_t replacement = audio_play(clip, AUDIO_BUS_SFX, 1.0f, false);
    TEST_ASSERT_TRUE(audio_voice_is_playing(replacement));
    TEST_ASSERT_FALSE(voice_equal(first, replacement));
    audio_voice_stop(first);
    TEST_ASSERT_TRUE(audio_voice_is_playing(replacement));
}

void test_full_voice_pool_evicts_oldest_voice_without_growing(void) {
    audio_clip_t clip = make_ready_clip(ASSET_READY);
    unlock_audio();
    audio_voice_t voices[AUDIO_MAX_VOICES];
    for (uint32_t i = 0; i < AUDIO_MAX_VOICES; ++i) {
        voices[i] = audio_play(clip, AUDIO_BUS_SFX, 1.0f, false);
        TEST_ASSERT_TRUE(audio_voice_is_playing(voices[i]));
    }
    audio_voice_t newest = audio_play(clip, AUDIO_BUS_SFX, 1.0f, false);
    TEST_ASSERT_TRUE(audio_voice_is_playing(newest));
    TEST_ASSERT_FALSE(audio_voice_is_playing(voices[0]));
    TEST_ASSERT_EQUAL_UINT32(1, fake_audio_backend_voice_stop_count());
    for (uint32_t i = 1; i < AUDIO_MAX_VOICES; ++i) {
        TEST_ASSERT_TRUE(audio_voice_is_playing(voices[i]));
    }
}

void test_loading_failed_and_stale_clips_refuse_play(void) {
    unlock_audio();
    fake_audio_add_ready_blob(ASSET_READY);
    audio_clip_t loading = audio_clip_load(asset(ASSET_READY));
    TEST_ASSERT_TRUE(voice_equal(AUDIO_VOICE_INVALID, audio_play(loading, AUDIO_BUS_SFX, 1.0f, false)));
    fake_audio_complete_next_decode(false);
    audio_update();
    TEST_ASSERT_TRUE(voice_equal(AUDIO_VOICE_INVALID, audio_play(loading, AUDIO_BUS_SFX, 1.0f, false)));
    audio_clip_unload(loading);
    TEST_ASSERT_TRUE(voice_equal(AUDIO_VOICE_INVALID, audio_play(loading, AUDIO_BUS_SFX, 1.0f, false)));
    TEST_ASSERT_EQUAL_UINT32(0, fake_audio_backend_play_count());
}

void test_shutdown_stops_voices_and_destroys_all_backend_clips(void) {
    audio_clip_t ready = make_ready_clip(ASSET_READY);
    unlock_audio();
    fake_audio_add_ready_blob(UINT64_C(0x1002));
    audio_clip_t loading = audio_clip_load(asset(UINT64_C(0x1002)));
    audio_voice_t voice = audio_play(ready, AUDIO_BUS_MUSIC, 1.0f, true);
    TEST_ASSERT_TRUE(audio_voice_is_playing(voice));

    audio_shutdown();
    s_shutdown_in_test = true;
    TEST_ASSERT_EQUAL_UINT32(1, fake_audio_backend_shutdown_count());
    TEST_ASSERT_EQUAL_UINT32(1, fake_audio_backend_voice_stop_count());
    TEST_ASSERT_EQUAL_UINT32(2, fake_audio_backend_clip_destroy_count());
    TEST_ASSERT_EQUAL(AUDIO_CLIP_STATE_INVALID, audio_clip_state(ready));
    TEST_ASSERT_EQUAL(AUDIO_CLIP_STATE_INVALID, audio_clip_state(loading));
    TEST_ASSERT_FALSE(audio_voice_is_playing(voice));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_clip_load_requires_an_immediately_viewable_ready_blob);
    RUN_TEST(test_backend_decode_is_async_and_can_fail);
    RUN_TEST(test_decode_copies_borrowed_blob_bytes_before_load_returns);
    RUN_TEST(test_clip_handles_are_generation_safe_and_capacity_is_fixed);
    RUN_TEST(test_mix_is_clamped_and_applied_atomically);
    RUN_TEST(test_play_requires_available_unlocked_enabled_and_not_paused);
    RUN_TEST(test_unavailable_backend_is_reported_and_refuses_load_and_play);
    RUN_TEST(test_user_gesture_is_forwarded_and_status_records_success_only);
    RUN_TEST(test_update_reconciles_an_async_backend_unlock_rejection);
    RUN_TEST(test_finished_voice_is_cleaned_and_reused_with_new_generation);
    RUN_TEST(test_full_voice_pool_evicts_oldest_voice_without_growing);
    RUN_TEST(test_loading_failed_and_stale_clips_refuse_play);
    RUN_TEST(test_shutdown_stops_voices_and_destroys_all_backend_clips);
    return UNITY_END();
}
