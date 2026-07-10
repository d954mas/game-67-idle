#include <stdbool.h>
#include <stdint.h>

#include "unity.h"

#include "audio_backend.h"

static const uint8_t k_wav[] = {
    'R','I','F','F', 44,0,0,0, 'W','A','V','E',
    'f','m','t',' ', 16,0,0,0, 1,0, 2,0,
    0x80,0xBB,0,0, 0,0xEE,2,0, 4,0, 16,0,
    'd','a','t','a', 8,0,0,0,
    0,0, 0,0, 0xE8,3, 0x18,0xFC,
};
static const uint8_t k_invalid[] = {0x00, 0x01, 0x02, 0x03};

void setUp(void) { TEST_ASSERT_TRUE(audio_core_backend_init()); }
void tearDown(void) { audio_core_backend_shutdown(); }

void test_wav_decode_is_synchronous_and_reports_ready(void) {
    uint32_t clip = audio_core_backend_decode_begin(k_wav, (uint32_t)sizeof(k_wav));
    TEST_ASSERT_NOT_EQUAL_UINT32(0, clip);
    TEST_ASSERT_EQUAL_UINT32(1, audio_core_backend_decode_state(clip));
    audio_core_backend_clip_destroy(clip);
    TEST_ASSERT_EQUAL_UINT32(2, audio_core_backend_decode_state(clip));
}

void test_bad_decode_owns_a_failed_slot_until_destroyed(void) {
    uint32_t clip = audio_core_backend_decode_begin(k_invalid, (uint32_t)sizeof(k_invalid));
    TEST_ASSERT_NOT_EQUAL_UINT32(0, clip);
    TEST_ASSERT_EQUAL_UINT32(2, audio_core_backend_decode_state(clip));
    audio_core_backend_clip_destroy(clip);
}

void test_fixed_clip_pool_refuses_the_sixty_fifth_decode(void) {
    uint32_t clips[64];
    for (uint32_t i = 0; i < 64; ++i) {
        clips[i] = audio_core_backend_decode_begin(k_wav, (uint32_t)sizeof(k_wav));
        TEST_ASSERT_NOT_EQUAL_UINT32(0, clips[i]);
    }
    TEST_ASSERT_EQUAL_UINT32(0, audio_core_backend_decode_begin(k_wav, (uint32_t)sizeof(k_wav)));
    for (uint32_t i = 0; i < 64; ++i) audio_core_backend_clip_destroy(clips[i]);
}

void test_no_device_engine_unlocks_and_plays_through_both_groups(void) {
    uint32_t clip = audio_core_backend_decode_begin(k_wav, (uint32_t)sizeof(k_wav));
    TEST_ASSERT_EQUAL_UINT32(1, audio_core_backend_decode_state(clip));
    audio_core_backend_set_mix(0.75f, 0.5f, 0.25f);
    audio_core_backend_set_enabled(true);
    audio_core_backend_set_paused(false);
    TEST_ASSERT_TRUE(audio_core_backend_user_gesture());

    uint32_t music = audio_core_backend_voice_play(clip, 0, 0.8f, true);
    uint32_t sfx = audio_core_backend_voice_play(clip, 1, 0.6f, false);
    TEST_ASSERT_NOT_EQUAL_UINT32(0, music);
    TEST_ASSERT_NOT_EQUAL_UINT32(0, sfx);
    TEST_ASSERT_TRUE(audio_core_backend_voice_active(music));
    TEST_ASSERT_TRUE(audio_core_backend_voice_active(sfx));
    audio_core_backend_voice_stop(music);
    audio_core_backend_voice_stop(sfx);
    TEST_ASSERT_FALSE(audio_core_backend_voice_active(music));
    TEST_ASSERT_FALSE(audio_core_backend_voice_active(sfx));
    audio_core_backend_clip_destroy(clip);
}

void test_fixed_voice_pool_refuses_the_thirty_third_voice(void) {
    uint32_t clip = audio_core_backend_decode_begin(k_wav, (uint32_t)sizeof(k_wav));
    TEST_ASSERT_TRUE(audio_core_backend_user_gesture());
    uint32_t voices[32];
    for (uint32_t i = 0; i < 32; ++i) {
        voices[i] = audio_core_backend_voice_play(clip, 1, 1.0f, true);
        TEST_ASSERT_NOT_EQUAL_UINT32(0, voices[i]);
    }
    TEST_ASSERT_EQUAL_UINT32(0, audio_core_backend_voice_play(clip, 1, 1.0f, true));
    for (uint32_t i = 0; i < 32; ++i) audio_core_backend_voice_stop(voices[i]);
    audio_core_backend_clip_destroy(clip);
}

void test_play_update_and_stop_allocate_nothing_after_decode(void) {
    uint32_t clip = audio_core_backend_decode_begin(k_wav, (uint32_t)sizeof(k_wav));
    TEST_ASSERT_TRUE(audio_core_backend_user_gesture());
    uint64_t before = audio_miniaudio_test_allocation_count();
    uint32_t voice = audio_core_backend_voice_play(clip, 1, 1.0f, false);
    audio_core_backend_update();
    audio_core_backend_voice_stop(voice);
    TEST_ASSERT_EQUAL_UINT64(before, audio_miniaudio_test_allocation_count());
    audio_core_backend_clip_destroy(clip);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_wav_decode_is_synchronous_and_reports_ready);
    RUN_TEST(test_bad_decode_owns_a_failed_slot_until_destroyed);
    RUN_TEST(test_fixed_clip_pool_refuses_the_sixty_fifth_decode);
    RUN_TEST(test_no_device_engine_unlocks_and_plays_through_both_groups);
    RUN_TEST(test_fixed_voice_pool_refuses_the_thirty_third_voice);
    RUN_TEST(test_play_update_and_stop_allocate_nothing_after_decode);
    return UNITY_END();
}
