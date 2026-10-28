#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"
#include "cJSON.h"
#include "devapi/nt_devapi.h"
#include "features/audio/audio.h"
#include "features/audio/capture.h"
#include "features/recording/recording.h"
#include "recorder/nt_recorder.h"
#include "time/nt_time.h"

static struct {
    audio_status_t audio;
    bool begin_ok, end_ok, encoder_ok, capture_active, encoder_active, capture_failed;
    bool pcm_changed, frame_before_audio;
    uint32_t begin_calls, end_calls, start_calls, stop_calls, frame_calls;
    uint32_t pending, pushed, encoded_frames, fps;
    double now, frame_time, frame_dt, first_origin;
} s_fake;
static const nt_devapi_command *s_commands;
static size_t s_command_count;
static void *s_command_user;
static cJSON *s_result;
static nt_devapi_error s_error;

void nt_devapi_register(const nt_devapi_command *commands, size_t count, void *user) {
    s_commands = commands;
    s_command_count = count;
    s_command_user = user;
}

audio_status_t audio_status(void) { return s_fake.audio; }
double nt_time_now(void) { return s_fake.now; }

bool audio_capture_begin(void) {
    ++s_fake.begin_calls;
    s_fake.capture_active = s_fake.begin_ok;
    return s_fake.begin_ok;
}

bool audio_capture_end(void) {
    ++s_fake.end_calls;
    if (s_fake.end_ok) s_fake.capture_active = false;
    return s_fake.end_ok;
}

uint32_t audio_capture_read(float *stereo, uint32_t capacity_frames) {
    const uint32_t count = s_fake.pending < capacity_frames ? s_fake.pending : capacity_frames;
    for (uint32_t i = 0; i < count; ++i) {
        stereo[i * 2U] = 0.25F;
        stereo[i * 2U + 1U] = -0.5F;
    }
    s_fake.pending -= count;
    return count;
}

bool audio_capture_failed(void) { return s_fake.capture_failed; }

bool nt_recorder_start(const char *name, uint32_t fps) {
    (void)name;
    ++s_fake.start_calls;
    s_fake.fps = fps;
    s_fake.encoder_active = s_fake.encoder_ok;
    return s_fake.encoder_ok;
}

bool nt_recorder_active(void) { return s_fake.encoder_active; }

void nt_recorder_push_audio(const float *stereo, uint32_t frames) {
    for (uint32_t i = 0; i < frames; ++i) {
        if (stereo[i * 2U] != 0.25F || stereo[i * 2U + 1U] != -0.5F) s_fake.pcm_changed = true;
    }
    if (s_fake.encoder_active) s_fake.pushed += frames;
}

void nt_recorder_frame(double now, double dt) {
    if (!s_fake.encoder_active) return;
    if (s_fake.frame_calls == 0) {
        s_fake.first_origin = now - (s_fake.pushed > 0 ? (double)s_fake.pushed / 48000.0 : dt);
        s_fake.frame_before_audio = s_fake.capture_active && s_fake.pushed == 0;
    }
    ++s_fake.frame_calls;
    s_fake.frame_time = now;
    s_fake.frame_dt = dt;
}

uint32_t nt_recorder_stop(void) {
    ++s_fake.stop_calls;
    const uint32_t frames = s_fake.encoder_active ? s_fake.encoded_frames : 0;
    s_fake.encoder_active = false;
    return frames;
}

static bool invoke(const char *method, const cJSON *params) {
    cJSON_Delete(s_result);
    s_result = cJSON_CreateObject();
    s_error = (nt_devapi_error){0};
    for (size_t i = 0; i < s_command_count; ++i) {
        if (strcmp(s_commands[i].method, method) == 0) {
            return s_commands[i].fn(params, s_result, &s_error, s_command_user);
        }
    }
    TEST_FAIL_MESSAGE("recording command was not registered");
    return false;
}

static bool start_json(const char *json) {
    cJSON *params = cJSON_Parse(json);
    TEST_ASSERT_NOT_NULL(params);
    const bool started = invoke("game.record.start", params);
    cJSON_Delete(params);
    return started;
}

static void start_take(void) {
    TEST_ASSERT_TRUE(start_json("{\"name\":\"take.mp4\",\"fps\":60}"));
}

static bool stop_take(void) { return invoke("game.record.stop", NULL); }

static double number(const char *name) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(s_result, name);
    TEST_ASSERT_TRUE(cJSON_IsNumber(value));
    return value->valuedouble;
}

static bool valid(void) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(s_result, "valid");
    TEST_ASSERT_TRUE(cJSON_IsBool(value));
    return cJSON_IsTrue(value);
}

static void present_half_second(void) {
    s_fake.pending = 24000;
    s_fake.now += 0.5;
    s_fake.encoded_frames = 30;
    game_recording_frame(s_fake.now - 0.1, 0.1F);
}

void setUp(void) {
    s_fake.end_ok = true;
    game_recording_shutdown();
    memset(&s_fake, 0, sizeof s_fake);
    s_fake.audio = (audio_status_t){.available = true, .unlocked = true, .enabled = true};
    s_fake.begin_ok = true;
    s_fake.end_ok = true;
    s_fake.encoder_ok = true;
    s_fake.now = 100.0;
    game_recording_register();
}

void tearDown(void) {
    s_fake.end_ok = true;
    game_recording_shutdown();
    cJSON_Delete(s_result);
    s_result = NULL;
}

static void test_rejects_bad_start_parameters_before_touching_devices(void) {
    static const char *cases[] = {
        "{}", "{\"name\":\"\",\"fps\":60}", "{\"name\":\"take\"}",
        "{\"name\":\"take\",\"fps\":0}", "{\"name\":\"take\",\"fps\":121}",
        "{\"name\":\"take\",\"fps\":59.5}", "{\"name\":\"take\",\"fps\":\"60\"}"
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) TEST_ASSERT_FALSE(start_json(cases[i]));
    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "name", "take");
    cJSON_AddNumberToObject(params, "fps", (double)NAN);
    TEST_ASSERT_FALSE(invoke("game.record.start", params));
    cJSON_Delete(params);
    TEST_ASSERT_EQUAL_UINT32(0, s_fake.start_calls);
    TEST_ASSERT_EQUAL_UINT32(0, s_fake.begin_calls);
}

static void test_requires_available_unlocked_audio(void) {
    s_fake.audio.available = false;
    TEST_ASSERT_FALSE(start_json("{\"name\":\"take\",\"fps\":60}"));
    s_fake.audio.available = true;
    s_fake.audio.unlocked = false;
    TEST_ASSERT_FALSE(start_json("{\"name\":\"take\",\"fps\":60}"));
    TEST_ASSERT_EQUAL_UINT32(0, s_fake.start_calls);
    TEST_ASSERT_EQUAL_UINT32(0, s_fake.begin_calls);
}

static void test_encoder_start_failure_does_not_start_audio(void) {
    s_fake.encoder_ok = false;
    TEST_ASSERT_FALSE(start_json("{\"name\":\"take\",\"fps\":60}"));
    TEST_ASSERT_EQUAL_UINT32(0, s_fake.begin_calls);
    TEST_ASSERT_FALSE(s_fake.capture_active);
}

static void test_capture_start_failure_closes_encoder(void) {
    s_fake.begin_ok = false;
    TEST_ASSERT_FALSE(start_json("{\"name\":\"take\",\"fps\":60}"));
    TEST_ASSERT_EQUAL_UINT32(1, s_fake.stop_calls);
    TEST_ASSERT_FALSE(s_fake.encoder_active);
}

static void test_duplicate_start_preserves_active_take(void) {
    start_take();
    TEST_ASSERT_FALSE(start_json("{\"name\":\"second\",\"fps\":30}"));
    TEST_ASSERT_EQUAL_UINT32(1, s_fake.start_calls);
    TEST_ASSERT_EQUAL_UINT32(1, s_fake.begin_calls);
    TEST_ASSERT_TRUE(s_fake.encoder_active);
    TEST_ASSERT_TRUE(s_fake.capture_active);
}

static void test_waits_for_pcm_and_uses_presentation_clock_after_slow_first_frame(void) {
    start_take();
    game_recording_frame(99.9, 1.0F);
    s_fake.now = 100.01;
    game_recording_frame(100.005, 0.005F);
    TEST_ASSERT_EQUAL_UINT32(0, s_fake.frame_calls);
    s_fake.pending = 14400;
    s_fake.now = 100.3;
    game_recording_frame(100.02, 0.01F);
    TEST_ASSERT_EQUAL_UINT32(1, s_fake.frame_calls);
    TEST_ASSERT_FALSE(s_fake.frame_before_audio);
    TEST_ASSERT_FALSE(s_fake.pcm_changed);
    TEST_ASSERT_EQUAL_UINT32(14400, s_fake.pushed);
    TEST_ASSERT_TRUE(fabs(s_fake.frame_time - 100.3) < 0.00001);
    TEST_ASSERT_TRUE(fabs(s_fake.first_origin - 100.0) < 0.00001);
    s_fake.pending = 960;
    s_fake.now = 100.32;
    game_recording_frame(100.301, 0.001F);
    TEST_ASSERT_TRUE(fabs(s_fake.frame_dt - 0.02) < 0.00001);
    s_fake.encoded_frames = 19;
    TEST_ASSERT_TRUE(stop_take());
    TEST_ASSERT_TRUE(valid());
    TEST_ASSERT_TRUE(number("max_drift_ms") < 0.001);
}

static void test_stop_drains_final_pcm_and_reports_tail(void) {
    start_take();
    present_half_second();
    s_fake.pending = 480;
    TEST_ASSERT_TRUE(stop_take());
    TEST_ASSERT_TRUE(valid());
    TEST_ASSERT_EQUAL_UINT32(24480, s_fake.pushed);
    TEST_ASSERT_EQUAL_UINT32(24480, (uint32_t)number("audio_frames"));
    TEST_ASSERT_TRUE(fabs(number("tail_delta_ms") - 10.0) < 0.001);
    TEST_ASSERT_FALSE(s_fake.capture_active);
    TEST_ASSERT_FALSE(s_fake.encoder_active);
}

static void test_drift_ends_capture_and_retains_invalid_result(void) {
    start_take();
    present_half_second();
    s_fake.now += 0.3;
    game_recording_frame(s_fake.now - 0.01, 0.01F);
    TEST_ASSERT_FALSE(s_fake.capture_active);
    TEST_ASSERT_FALSE(s_fake.encoder_active);
    TEST_ASSERT_TRUE(stop_take());
    TEST_ASSERT_FALSE(valid());
    TEST_ASSERT_TRUE(number("max_drift_ms") > 100.0);
}

static void test_overflow_ends_capture_and_retains_invalid_result(void) {
    start_take();
    present_half_second();
    s_fake.capture_failed = true;
    s_fake.now += 0.02;
    game_recording_frame(s_fake.now, 0.02F);
    TEST_ASSERT_TRUE(stop_take());
    TEST_ASSERT_FALSE(valid());
    TEST_ASSERT_FALSE(s_fake.capture_active);
}

static void test_failed_audio_end_is_invalid_and_retry_quiesces_it(void) {
    start_take();
    present_half_second();
    s_fake.end_ok = false;
    TEST_ASSERT_TRUE(stop_take());
    TEST_ASSERT_FALSE(valid());
    TEST_ASSERT_TRUE(s_fake.capture_active);
    TEST_ASSERT_TRUE(s_fake.encoder_active);
    TEST_ASSERT_EQUAL_UINT32(0, s_fake.stop_calls);
    s_fake.end_ok = true;
    TEST_ASSERT_TRUE(stop_take());
    TEST_ASSERT_FALSE(valid());
    TEST_ASSERT_EQUAL_UINT32(2, s_fake.end_calls);
    TEST_ASSERT_FALSE(s_fake.capture_active);
    TEST_ASSERT_FALSE(s_fake.encoder_active);
}

static void test_encoder_auto_end_is_reported_as_invalid(void) {
    start_take();
    present_half_second();
    s_fake.encoder_active = false;
    s_fake.now += 0.02;
    game_recording_frame(s_fake.now, 0.02F);
    TEST_ASSERT_TRUE(stop_take());
    TEST_ASSERT_FALSE(valid());
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)number("frames"));
    TEST_ASSERT_EQUAL_UINT32(24000, (uint32_t)number("audio_frames"));
}

static void test_repeated_stop_preserves_result(void) {
    start_take();
    present_half_second();
    TEST_ASSERT_TRUE(stop_take());
    TEST_ASSERT_TRUE(valid());
    const uint32_t stops = s_fake.stop_calls;
    TEST_ASSERT_TRUE(stop_take());
    TEST_ASSERT_TRUE(valid());
    TEST_ASSERT_EQUAL_UINT32(stops, s_fake.stop_calls);
    TEST_ASSERT_EQUAL_UINT32(30, (uint32_t)number("frames"));
    TEST_ASSERT_EQUAL_UINT32(24000, (uint32_t)number("audio_frames"));
}

static void test_shutdown_finalizes_later_raw_recording_without_losing_take_result(void) {
    start_take();
    present_half_second();
    TEST_ASSERT_TRUE(stop_take());
    TEST_ASSERT_TRUE(nt_recorder_start("raw.mp4", 30));
    s_fake.encoded_frames = 7;
    game_recording_frame(101.0, 0.25F);
    TEST_ASSERT_TRUE(fabs(s_fake.frame_time - 101.0) < 0.00001);
    TEST_ASSERT_TRUE(fabs(s_fake.frame_dt - 0.25) < 0.00001);
    game_recording_shutdown();
    TEST_ASSERT_FALSE(s_fake.encoder_active);
    TEST_ASSERT_TRUE(stop_take());
    TEST_ASSERT_TRUE(valid());
    TEST_ASSERT_EQUAL_UINT32(30, (uint32_t)number("frames"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_rejects_bad_start_parameters_before_touching_devices);
    RUN_TEST(test_requires_available_unlocked_audio);
    RUN_TEST(test_encoder_start_failure_does_not_start_audio);
    RUN_TEST(test_capture_start_failure_closes_encoder);
    RUN_TEST(test_duplicate_start_preserves_active_take);
    RUN_TEST(test_waits_for_pcm_and_uses_presentation_clock_after_slow_first_frame);
    RUN_TEST(test_stop_drains_final_pcm_and_reports_tail);
    RUN_TEST(test_drift_ends_capture_and_retains_invalid_result);
    RUN_TEST(test_overflow_ends_capture_and_retains_invalid_result);
    RUN_TEST(test_failed_audio_end_is_invalid_and_retry_quiesces_it);
    RUN_TEST(test_encoder_auto_end_is_reported_as_invalid);
    RUN_TEST(test_repeated_stop_preserves_result);
    RUN_TEST(test_shutdown_finalizes_later_raw_recording_without_losing_take_result);
    return UNITY_END();
}
