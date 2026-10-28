#include "features/recording/recording.h"

#ifdef NT_DEVAPI_GROUP_RECORDER
#include <math.h>
#include "cJSON.h"
#include "devapi/nt_devapi.h"
#include "features/audio/audio.h"
#include "features/audio/capture.h"
#include "recorder/nt_recorder.h"
#include "time/nt_time.h"

static bool s_owned;
static bool s_has_take;
static bool s_failed;
static bool s_first;
static double s_origin;
static double s_presented;
static double s_audio_origin;
static double s_max_drift;
static uint32_t s_fps;
static uint64_t s_audio_frames;
static uint32_t s_video_frames;
static float s_pcm[4096 * 2];

static void drain_audio(void) {
    uint32_t frames;
    while ((frames = audio_capture_read(s_pcm, 4096)) != 0) {
        nt_recorder_push_audio(s_pcm, frames);
        s_audio_frames += frames;
    }
    s_failed = s_failed || audio_capture_failed();
}

void game_recording_shutdown(void) {
    if (!s_owned) {
        if (nt_recorder_active()) (void)nt_recorder_stop();
        return;
    }
    if (s_owned) {
        if (!audio_capture_end()) { s_failed = true; return; }
        if (nt_recorder_active()) drain_audio();
        else s_failed = true;
        s_owned = false;
    }
    s_video_frames = nt_recorder_stop();
}

void game_recording_frame(double now, float dt) {
    if (s_owned) {
        if (!nt_recorder_active()) { game_recording_shutdown(); return; }
        // Encoder initialization blocks the command's frame; its earlier timestamp is stale.
        if (now <= s_origin) return;
        drain_audio();
        if (s_failed) { game_recording_shutdown(); return; }
        // The device produces through presentation, not the earlier game-frame timestamp.
        now = nt_time_now();
        if (s_first) {
            if (s_audio_frames == 0) return;
            dt = (float)(now - s_origin);
            s_audio_origin = now - (double)s_audio_frames / 48000.0;
            s_first = false;
        } else {
            dt = (float)(now - s_presented);
        }
        s_presented = now;
        const double drift = fabs((double)s_audio_frames / 48000.0 - (now - s_audio_origin));
        if (drift > s_max_drift) s_max_drift = drift;
        if (drift > 0.1) { s_failed = true; game_recording_shutdown(); return; }
    }
    nt_recorder_frame(now, (double)dt);
}

static bool start(const cJSON *params, cJSON *result, nt_devapi_error *error, void *user) {
    (void)result;
    (void)user;
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(params, "name");
    const cJSON *fps = cJSON_GetObjectItemCaseSensitive(params, "fps");
    if (!cJSON_IsString(name) || name->valuestring[0] == '\0' || !cJSON_IsNumber(fps) ||
        !isfinite(fps->valuedouble) || fps->valuedouble < 1 || fps->valuedouble > 120 || floor(fps->valuedouble) != fps->valuedouble) {
        return nt_devapi_fail(error, "name and integer fps in [1,120] are required");
    }
    if (s_owned || nt_recorder_active()) return nt_devapi_fail(error, "a recording is already active");
    const audio_status_t audio = audio_status();
    if (!audio.available || !audio.unlocked) return nt_devapi_fail(error, "audio device must be available and unlocked by player input");
    if (!nt_recorder_start(name->valuestring, (uint32_t)fps->valuedouble)) return nt_devapi_fail(error, "video encoder refused to start");
    if (!audio_capture_begin()) {
        (void)nt_recorder_stop();
        return nt_devapi_fail(error, "audio capture refused to start");
    }
    s_owned = true;
    s_has_take = true;
    s_failed = false;
    s_first = true;
    s_origin = nt_time_now();
    s_presented = s_origin;
    s_max_drift = 0;
    s_fps = (uint32_t)fps->valuedouble;
    s_audio_frames = 0;
    s_video_frames = 0;
    return true;
}

static bool stop(const cJSON *params, cJSON *result, nt_devapi_error *error, void *user) {
    (void)params;
    (void)user;
    if (!s_has_take) return nt_devapi_fail(error, "no audiovisual recording has started");
    game_recording_shutdown();
    cJSON_AddNumberToObject(result, "frames", s_video_frames);
    cJSON_AddNumberToObject(result, "audio_frames", (double)s_audio_frames);
    cJSON_AddNumberToObject(result, "max_drift_ms", s_max_drift * 1000.0);
    cJSON_AddNumberToObject(result, "tail_delta_ms", ((double)s_audio_frames / 48000.0 - (double)s_video_frames / s_fps) * 1000.0);
    cJSON_AddBoolToObject(result, "valid", !s_owned && !s_failed && s_video_frames > 0 && s_audio_frames > 0);
    return true;
}

void game_recording_register(void) {
    static const nt_devapi_command commands[] = {
        {.method = "game.record.start", .summary = "Record live framebuffer and the actual stereo mix; requires unlocked audio.",
         .params_shape = "{name:string,fps:int}", .result_shape = "{}", .fn = start},
        {.method = "game.record.stop", .summary = "Drain audio and finalize the MP4. Invalid takes must not be published.",
         .params_shape = "{}", .result_shape = "{frames:int,audio_frames:int,max_drift_ms:number,tail_delta_ms:number,valid:bool}", .fn = stop},
    };
    nt_devapi_register(commands, 2, NULL);
}
#endif
