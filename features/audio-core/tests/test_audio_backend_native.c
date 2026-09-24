#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

#ifndef AUDIO_TEST_MP3_PATH
#error "AUDIO_TEST_MP3_PATH must name the committed MP3 fixture"
#endif
#ifndef AUDIO_TEST_CUE_WAV_PATH
#error "AUDIO_TEST_CUE_WAV_PATH must name the mastered cue the shipped MP3 is encoded from"
#endif
#ifndef AUDIO_TEST_CUE_MP3_PATH
#error "AUDIO_TEST_CUE_MP3_PATH must name the shipped MP3 of that same cue"
#endif

/* The backend's fixed decode target. */
#define AUDIO_TEST_CHANNELS 2u
#define AUDIO_TEST_RATE 48000u
#define AUDIO_TEST_MS(ms) ((uint64_t)(AUDIO_TEST_RATE * (ms) / 1000u))

static uint8_t *read_fixture(const char *path, uint32_t *size) {
    FILE *file = fopen(path, "rb");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
        if (file != NULL) fclose(file);
        return NULL;
    }
    const long length = ftell(file);
    if (length <= 0 || (uint64_t)length > UINT32_MAX || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    uint8_t *bytes = (uint8_t *)malloc((size_t)length);
    if (bytes == NULL || fread(bytes, 1, (size_t)length, file) != (size_t)length) {
        free(bytes);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *size = (uint32_t)length;
    return bytes;
}

void setUp(void) { TEST_ASSERT_TRUE(audio_core_backend_init()); }
void tearDown(void) { audio_core_backend_shutdown(); }

void test_wav_decode_is_synchronous_and_reports_ready(void) {
    uint32_t clip = audio_core_backend_decode_begin(k_wav, (uint32_t)sizeof(k_wav));
    TEST_ASSERT_NOT_EQUAL_UINT32(0, clip);
    TEST_ASSERT_EQUAL_UINT32(1, audio_core_backend_decode_state(clip));
    audio_core_backend_clip_destroy(clip);
    TEST_ASSERT_EQUAL_UINT32(2, audio_core_backend_decode_state(clip));
}

void test_committed_mp3_decodes_with_the_native_backend(void) {
    uint32_t size = 0;
    uint8_t *bytes = read_fixture(AUDIO_TEST_MP3_PATH, &size);
    TEST_ASSERT_NOT_NULL(bytes);
    uint32_t clip = audio_core_backend_decode_begin(bytes, size);
    free(bytes);
    TEST_ASSERT_NOT_EQUAL_UINT32(0, clip);
    TEST_ASSERT_EQUAL_UINT32(1, audio_core_backend_decode_state(clip));
    audio_core_backend_clip_destroy(clip);
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

void test_default_decoded_pcm_budgets_are_fixed(void) {
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(128) * UINT64_C(1024) * UINT64_C(1024),
        audio_miniaudio_test_per_clip_limit());
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(256) * UINT64_C(1024) * UINT64_C(1024),
        audio_miniaudio_test_total_limit());
}

void test_per_clip_budget_rejects_before_pcm_allocation(void) {
    audio_miniaudio_test_set_decoded_limits(15, 256);
    uint64_t allocations_before = audio_miniaudio_test_allocation_count();
    uint32_t clip = audio_core_backend_decode_begin(k_wav, (uint32_t)sizeof(k_wav));
    TEST_ASSERT_NOT_EQUAL_UINT32(0, clip);
    TEST_ASSERT_EQUAL_UINT32(2, audio_core_backend_decode_state(clip));
    TEST_ASSERT_EQUAL_UINT64(allocations_before, audio_miniaudio_test_allocation_count());
    TEST_ASSERT_EQUAL_UINT64(0, audio_miniaudio_test_decoded_bytes());
    audio_core_backend_clip_destroy(clip);
}

void test_total_budget_is_released_when_a_clip_is_destroyed(void) {
    audio_miniaudio_test_set_decoded_limits(16, 16);
    uint32_t first = audio_core_backend_decode_begin(k_wav, (uint32_t)sizeof(k_wav));
    TEST_ASSERT_EQUAL_UINT32(1, audio_core_backend_decode_state(first));
    TEST_ASSERT_EQUAL_UINT64(16, audio_miniaudio_test_decoded_bytes());

    uint32_t rejected = audio_core_backend_decode_begin(k_wav, (uint32_t)sizeof(k_wav));
    TEST_ASSERT_NOT_EQUAL_UINT32(0, rejected);
    TEST_ASSERT_EQUAL_UINT32(2, audio_core_backend_decode_state(rejected));
    TEST_ASSERT_EQUAL_UINT64(16, audio_miniaudio_test_decoded_bytes());

    audio_core_backend_clip_destroy(first);
    TEST_ASSERT_EQUAL_UINT64(0, audio_miniaudio_test_decoded_bytes());
    uint32_t replacement = audio_core_backend_decode_begin(k_wav, (uint32_t)sizeof(k_wav));
    TEST_ASSERT_EQUAL_UINT32(1, audio_core_backend_decode_state(replacement));
    TEST_ASSERT_EQUAL_UINT64(16, audio_miniaudio_test_decoded_bytes());
    audio_core_backend_clip_destroy(rejected);
    audio_core_backend_clip_destroy(replacement);
}

void test_shutdown_releases_the_total_decoded_budget(void) {
    audio_miniaudio_test_set_decoded_limits(16, 16);
    uint32_t clip = audio_core_backend_decode_begin(k_wav, (uint32_t)sizeof(k_wav));
    TEST_ASSERT_EQUAL_UINT32(1, audio_core_backend_decode_state(clip));
    TEST_ASSERT_EQUAL_UINT64(16, audio_miniaudio_test_decoded_bytes());
    audio_core_backend_shutdown();
    TEST_ASSERT_EQUAL_UINT64(0, audio_miniaudio_test_decoded_bytes());
    TEST_ASSERT_TRUE(audio_core_backend_init());
}

void test_pcm_size_calculation_rejects_uint64_overflow(void) {
    uint64_t bytes = UINT64_MAX;
    TEST_ASSERT_FALSE(audio_miniaudio_test_pcm_size(UINT64_MAX, &bytes));
    TEST_ASSERT_EQUAL_UINT64(0, bytes);
}

typedef struct decoded_cue_t {
    uint32_t clip;
    uint64_t frames;
    const float *samples;
} decoded_cue_t;

static decoded_cue_t decode_cue(const char *path) {
    uint32_t size = 0;
    uint8_t *bytes = read_fixture(path, &size);
    TEST_ASSERT_NOT_NULL(bytes);
    decoded_cue_t cue = {0};
    cue.clip = audio_core_backend_decode_begin(bytes, size);
    free(bytes);
    TEST_ASSERT_EQUAL_UINT32(1, audio_core_backend_decode_state(cue.clip));
    cue.frames = audio_miniaudio_test_clip_frames(cue.clip);
    cue.samples = audio_miniaudio_test_clip_pcm(cue.clip);
    TEST_ASSERT_NOT_NULL(cue.samples);
    TEST_ASSERT_TRUE(cue.frames > 0);
    return cue;
}

static float cue_peak(const decoded_cue_t *cue) {
    float peak = 0.0f;
    for (uint64_t i = 0; i < cue->frames * AUDIO_TEST_CHANNELS; ++i) {
        const float magnitude = fabsf(cue->samples[i]);
        if (magnitude > peak) peak = magnitude;
    }
    return peak;
}

static uint64_t cue_onset_frame(const decoded_cue_t *cue) {
    const float threshold = cue_peak(cue) * 0.15f;
    for (uint64_t frame = 0; frame < cue->frames; ++frame) {
        if (fabsf(cue->samples[frame * AUDIO_TEST_CHANNELS]) > threshold) return frame;
    }
    return cue->frames;
}

static double cue_rms(const decoded_cue_t *cue, uint64_t frames) {
    if (frames > cue->frames) frames = cue->frames;
    double sum = 0.0;
    for (uint64_t frame = 0; frame < frames; ++frame) {
        const double value = (double)cue->samples[frame * AUDIO_TEST_CHANNELS];
        sum += value * value;
    }
    return sqrt(sum / (double)(frames == 0 ? 1 : frames));
}

/* An MP3 declares its encoder delay in its own Xing/LAME header, and the
   decoder trims it from there. A cue encoded without that header decodes with
   the encoder's silence in front of it and fires a frame late, so these two
   compare the shipped cue against the PCM master it was encoded from. */
void test_shipped_cue_starts_where_its_master_starts(void) {
    decoded_cue_t master = decode_cue(AUDIO_TEST_CUE_WAV_PATH);
    decoded_cue_t shipped = decode_cue(AUDIO_TEST_CUE_MP3_PATH);
    const uint64_t master_onset = cue_onset_frame(&master);
    const uint64_t shipped_onset = cue_onset_frame(&shipped);
    const uint64_t drift = master_onset > shipped_onset
        ? master_onset - shipped_onset
        : shipped_onset - master_onset;
    TEST_ASSERT_TRUE_MESSAGE(drift <= AUDIO_TEST_MS(1),
        "the shipped cue does not start on the master's sample");
    const uint64_t length_drift = master.frames > shipped.frames
        ? master.frames - shipped.frames
        : shipped.frames - master.frames;
    TEST_ASSERT_TRUE(length_drift <= AUDIO_TEST_MS(1));
    audio_core_backend_clip_destroy(master.clip);
    audio_core_backend_clip_destroy(shipped.clip);
}

void test_shipped_cue_attack_window_carries_the_transient(void) {
    decoded_cue_t master = decode_cue(AUDIO_TEST_CUE_WAV_PATH);
    decoded_cue_t shipped = decode_cue(AUDIO_TEST_CUE_MP3_PATH);
    const double master_attack = cue_rms(&master, AUDIO_TEST_MS(35));
    const double shipped_attack = cue_rms(&shipped, AUDIO_TEST_MS(35));
    TEST_ASSERT_TRUE(master_attack > 0.0);
    TEST_ASSERT_TRUE_MESSAGE(shipped_attack >= master_attack * 0.8,
        "the attack window is encoder padding, not the cue");
    audio_core_backend_clip_destroy(master.clip);
    audio_core_backend_clip_destroy(shipped.clip);
}

static uint64_t frames_until_silent(uint32_t voice) {
    uint64_t rendered = 0;
    while (audio_core_backend_voice_active(voice) && rendered < AUDIO_TEST_RATE * 10u) {
        rendered += audio_miniaudio_test_render(256);
    }
    return rendered;
}

void test_pitch_reaches_a_pooled_voice(void) {
    decoded_cue_t cue = decode_cue(AUDIO_TEST_CUE_WAV_PATH);
    TEST_ASSERT_TRUE(audio_core_backend_user_gesture());
    const uint64_t plain = frames_until_silent(audio_core_backend_voice_play(cue.clip, 1, 1.0f, false));
    const uint32_t fast = audio_core_backend_voice_play(cue.clip, 1, 1.0f, false);
    audio_core_backend_voice_set_pitch(fast, 2.0f);
    const uint64_t pitched = frames_until_silent(fast);
    TEST_ASSERT_TRUE(plain >= cue.frames);
    TEST_ASSERT_TRUE_MESSAGE(pitched * 10u < plain * 6u, "an octave up did not shorten the voice");
    audio_core_backend_clip_destroy(cue.clip);
}

void test_a_replay_on_a_pooled_voice_starts_at_its_own_pitch(void) {
    decoded_cue_t cue = decode_cue(AUDIO_TEST_CUE_WAV_PATH);
    TEST_ASSERT_TRUE(audio_core_backend_user_gesture());
    const uint64_t plain = frames_until_silent(audio_core_backend_voice_play(cue.clip, 1, 1.0f, false));
    const uint32_t slow = audio_core_backend_voice_play(cue.clip, 1, 1.0f, false);
    audio_core_backend_voice_set_pitch(slow, 0.5f);
    audio_core_backend_voice_stop(slow);
    const uint32_t replay = audio_core_backend_voice_play(cue.clip, 1, 1.0f, false);
    TEST_ASSERT_EQUAL_UINT32(slow, replay);
    const uint64_t replayed = frames_until_silent(replay);
    const uint64_t drift = replayed > plain ? replayed - plain : plain - replayed;
    TEST_ASSERT_TRUE_MESSAGE(drift <= 512u, "the replay kept the previous play's pitch");
    audio_core_backend_clip_destroy(cue.clip);
}

void test_destroying_a_clip_stops_its_voices(void) {
    decoded_cue_t cue = decode_cue(AUDIO_TEST_CUE_WAV_PATH);
    uint32_t size = 0;
    uint8_t *bytes = read_fixture(AUDIO_TEST_CUE_MP3_PATH, &size);
    TEST_ASSERT_NOT_NULL(bytes);
    const uint32_t stream = audio_core_backend_stream_open(bytes, size);
    free(bytes);
    TEST_ASSERT_TRUE(audio_core_backend_user_gesture());
    const uint32_t pooled = audio_core_backend_voice_play(cue.clip, 1, 1.0f, true);
    const uint32_t streamed = audio_core_backend_voice_play(stream, 0, 1.0f, true);
    audio_core_backend_clip_destroy(stream);
    TEST_ASSERT_FALSE(audio_core_backend_voice_active(streamed));
    TEST_ASSERT_TRUE(audio_core_backend_voice_active(pooled));
    audio_core_backend_clip_destroy(cue.clip);
    TEST_ASSERT_FALSE(audio_core_backend_voice_active(pooled));
    TEST_ASSERT_EQUAL_UINT64(2048u, audio_miniaudio_test_render(2048u));
}

static uint32_t open_stream(const char *path) {
    uint32_t size = 0;
    uint8_t *bytes = read_fixture(path, &size);
    TEST_ASSERT_NOT_NULL(bytes);
    const uint32_t clip = audio_core_backend_stream_open(bytes, size);
    free(bytes);
    return clip;
}

void test_stream_opens_ready_without_decoding_pcm(void) {
    const uint32_t clip = open_stream(AUDIO_TEST_CUE_MP3_PATH);
    TEST_ASSERT_NOT_EQUAL_UINT32(0, clip);
    TEST_ASSERT_EQUAL_UINT32(1, audio_core_backend_decode_state(clip));
    TEST_ASSERT_EQUAL_UINT64(0, audio_miniaudio_test_decoded_bytes());
    audio_core_backend_clip_destroy(clip);
    TEST_ASSERT_EQUAL_UINT32(2, audio_core_backend_decode_state(clip));

    const uint32_t bad = audio_core_backend_stream_open(k_invalid, (uint32_t)sizeof(k_invalid));
    TEST_ASSERT_NOT_EQUAL_UINT32(0, bad);
    TEST_ASSERT_EQUAL_UINT32(2, audio_core_backend_decode_state(bad));
    audio_core_backend_clip_destroy(bad);
}

void test_stream_voice_plays_through_the_engine_and_ends(void) {
    const uint32_t clip = open_stream(AUDIO_TEST_CUE_MP3_PATH);
    TEST_ASSERT_TRUE(audio_core_backend_user_gesture());
    const uint32_t voice = audio_core_backend_voice_play(clip, 0, 1.0f, false);
    TEST_ASSERT_NOT_EQUAL_UINT32(0, voice);
    TEST_ASSERT_TRUE(audio_core_backend_voice_active(voice));
    const uint64_t played = frames_until_silent(voice);
    TEST_ASSERT_FALSE(audio_core_backend_voice_active(voice));
    TEST_ASSERT_TRUE(played > 0 && played < AUDIO_TEST_RATE * 10u);
    audio_core_backend_voice_stop(voice);

    const uint32_t looping = audio_core_backend_voice_play(clip, 0, 1.0f, true);
    TEST_ASSERT_EQUAL_UINT64(played * 3u, audio_miniaudio_test_render(played * 3u));
    TEST_ASSERT_TRUE(audio_core_backend_voice_active(looping));
    audio_core_backend_voice_stop(looping);
    TEST_ASSERT_FALSE(audio_core_backend_voice_active(looping));
    audio_core_backend_clip_destroy(clip);
}

void test_a_stream_held_silent_parks_and_resumes_where_it_paused(void) {
    const uint32_t clip = open_stream(AUDIO_TEST_CUE_MP3_PATH);
    TEST_ASSERT_TRUE(audio_core_backend_user_gesture());
    const uint32_t voice = audio_core_backend_voice_play(clip, 0, 1.0f, true);
    audio_miniaudio_test_render(AUDIO_TEST_MS(100));
    audio_core_backend_voice_set_gain(voice, 0.0f);
    audio_miniaudio_test_render(AUDIO_TEST_MS(1000));
    audio_core_backend_update();
    TEST_ASSERT_FALSE_MESSAGE(audio_miniaudio_test_stream_parked(voice), "parked before its silence ran out");
    audio_miniaudio_test_render(AUDIO_TEST_MS(1100));
    audio_core_backend_update();
    TEST_ASSERT_TRUE(audio_miniaudio_test_stream_parked(voice));
    TEST_ASSERT_TRUE(audio_core_backend_voice_active(voice));
    const uint64_t paused_at = audio_miniaudio_test_stream_cursor(voice);
    audio_miniaudio_test_render(AUDIO_TEST_MS(3000));
    audio_core_backend_update();
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(paused_at, audio_miniaudio_test_stream_cursor(voice), "a parked stream decoded");

    audio_core_backend_voice_set_gain(voice, 0.5f);
    TEST_ASSERT_FALSE(audio_miniaudio_test_stream_parked(voice));
    audio_miniaudio_test_render(AUDIO_TEST_MS(10));
    const uint64_t resumed = audio_miniaudio_test_stream_cursor(voice);
    TEST_ASSERT_TRUE_MESSAGE(resumed != paused_at && resumed - paused_at < 44100u / 10u,
        "the stream did not go on from where it paused");
    audio_core_backend_voice_stop(voice);
    audio_core_backend_clip_destroy(clip);
}

/* Fed only at pace from a full queue, a stream must never run dry, from a
   60 fps loop down to the web runtime's 150 ms timer and a 2 fps crawl. */
void test_stream_pace_supplies_at_least_real_time(void) {
    const uint32_t rate = 32000u;
    const uint32_t chunk = 2048u;
    const double target = 3.25 * (double)rate;
    const double intervals[] = {1.0 / 60.0, 0.15, 0.5};
    for (size_t k = 0; k < sizeof intervals / sizeof intervals[0]; ++k) {
        double queue = target;
        double lowest = queue;
        for (double t = 0.0; t < 60.0; t += intervals[k]) {
            queue -= intervals[k] * (double)rate;
            if (queue < lowest) lowest = queue;
            const uint32_t allowed = audio_core_stream_pace_frames(intervals[k], rate, chunk);
            for (uint32_t fed = 0; fed < allowed && queue < target; fed += chunk) queue += (double)chunk;
        }
        TEST_ASSERT_TRUE_MESSAGE(lowest > target - (intervals[k] * (double)rate + (double)chunk),
            "the queue drains at this update interval");
    }
    TEST_ASSERT_TRUE(audio_core_stream_pace_frames(0.0, rate, chunk) >= chunk);
    TEST_ASSERT_TRUE(audio_core_stream_pace_frames(-1.0, rate, chunk) >= chunk);
}

/* A loop is seamless when each pass repeats the first sample for sample and
   is as long as the master: the encoder's delay and padding are both gone. */
void test_stream_loop_repeats_the_master_length_exactly(void) {
    decoded_cue_t master = decode_cue(AUDIO_TEST_CUE_WAV_PATH);
    const uint32_t clip = open_stream(AUDIO_TEST_CUE_MP3_PATH);
    TEST_ASSERT_TRUE(audio_core_backend_user_gesture());
    const uint32_t voice = audio_core_backend_voice_play(clip, 0, 0.0f, true);
    audio_core_backend_set_paused(true);

    const uint64_t master_frames = master.frames * 44100u / AUDIO_TEST_RATE;
    const uint64_t frames = master_frames * 2u + 64u;
    float *pcm = (float *)malloc(sizeof(float) * (size_t)frames);
    TEST_ASSERT_NOT_NULL(pcm);
    TEST_ASSERT_EQUAL_UINT64(frames, audio_miniaudio_test_stream_read(voice, pcm, frames));
    uint64_t period = 0;
    for (uint64_t candidate = master_frames - 64u; candidate <= master_frames + 64u && period == 0; ++candidate) {
        bool repeats = true;
        for (uint64_t i = 0; i + candidate < frames && repeats; ++i) repeats = pcm[i] == pcm[i + candidate];
        if (repeats) period = candidate;
    }
    free(pcm);
    TEST_ASSERT_TRUE_MESSAGE(period != 0, "the loop does not repeat its first pass");
    const uint64_t drift = period > master_frames ? period - master_frames : master_frames - period;
    TEST_ASSERT_TRUE_MESSAGE(drift <= 44u, "the loop is longer or shorter than its master");
    audio_core_backend_voice_stop(voice);
    audio_core_backend_clip_destroy(clip);
    audio_core_backend_clip_destroy(master.clip);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_wav_decode_is_synchronous_and_reports_ready);
    RUN_TEST(test_committed_mp3_decodes_with_the_native_backend);
    RUN_TEST(test_bad_decode_owns_a_failed_slot_until_destroyed);
    RUN_TEST(test_fixed_clip_pool_refuses_the_sixty_fifth_decode);
    RUN_TEST(test_no_device_engine_unlocks_and_plays_through_both_groups);
    RUN_TEST(test_fixed_voice_pool_refuses_the_thirty_third_voice);
    RUN_TEST(test_play_update_and_stop_allocate_nothing_after_decode);
    RUN_TEST(test_default_decoded_pcm_budgets_are_fixed);
    RUN_TEST(test_per_clip_budget_rejects_before_pcm_allocation);
    RUN_TEST(test_total_budget_is_released_when_a_clip_is_destroyed);
    RUN_TEST(test_shutdown_releases_the_total_decoded_budget);
    RUN_TEST(test_pcm_size_calculation_rejects_uint64_overflow);
    RUN_TEST(test_shipped_cue_starts_where_its_master_starts);
    RUN_TEST(test_shipped_cue_attack_window_carries_the_transient);
    RUN_TEST(test_pitch_reaches_a_pooled_voice);
    RUN_TEST(test_a_replay_on_a_pooled_voice_starts_at_its_own_pitch);
    RUN_TEST(test_destroying_a_clip_stops_its_voices);
    RUN_TEST(test_stream_opens_ready_without_decoding_pcm);
    RUN_TEST(test_stream_voice_plays_through_the_engine_and_ends);
    RUN_TEST(test_stream_loop_repeats_the_master_length_exactly);
    RUN_TEST(test_stream_pace_supplies_at_least_real_time);
    RUN_TEST(test_a_stream_held_silent_parks_and_resumes_where_it_paused);
    return UNITY_END();
}
