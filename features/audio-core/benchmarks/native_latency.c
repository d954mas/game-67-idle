#include "features/audio/audio.h"

#include "audio_backend.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

static const uint64_t BENCHMARK_ASSET_ID = UINT64_C(1);
static uint8_t *s_wav_bytes;
static uint32_t s_wav_size;

uint32_t audio_core_resource_state(uint64_t asset_id) {
    return asset_id == BENCHMARK_ASSET_ID && s_wav_bytes != NULL && s_wav_size > 0 ? 2u : 0u;
}

bool audio_core_resource_blob_view(uint64_t asset_id, const void **bytes, uint32_t *size) {
    if (bytes == NULL || size == NULL || audio_core_resource_state(asset_id) != 2u) return false;
    *bytes = s_wav_bytes;
    *size = s_wav_size;
    return true;
}

static double now_ms(void) {
#if defined(_WIN32)
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (double)counter.QuadPart * 1000.0 / (double)frequency.QuadPart;
#else
    struct timespec value;
    clock_gettime(CLOCK_MONOTONIC, &value);
    return (double)value.tv_sec * 1000.0 + (double)value.tv_nsec / 1000000.0;
#endif
}

static void sleep_ms(unsigned milliseconds) {
#if defined(_WIN32)
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000u);
#endif
}

static bool read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    long length = ftell(file);
    if (length <= 0 || (uint64_t)length > UINT32_MAX || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    s_wav_bytes = (uint8_t *)malloc((size_t)length);
    if (s_wav_bytes == NULL) {
        fclose(file);
        return false;
    }
    size_t read = fread(s_wav_bytes, 1, (size_t)length, file);
    fclose(file);
    if (read != (size_t)length) {
        free(s_wav_bytes);
        s_wav_bytes = NULL;
        return false;
    }
    s_wav_size = (uint32_t)length;
    return true;
}

int main(int argc, char **argv) {
    bool allow_play = argc == 3 && strcmp(argv[2], "--play") == 0;
    if (argc < 2 || argc > 3 || (argc == 3 && !allow_play)) {
        fprintf(stderr, "usage: audio_benchmark_native <wav> [--play]\n");
        return 2;
    }
    if (!read_file(argv[1])) {
        fprintf(stderr, "failed to read WAV fixture: %s\n", argv[1]);
        return 3;
    }

    double started = now_ms();
    bool available = audio_init();
    double init_ms = now_ms() - started;
    if (!available) {
        printf("{\"device_available\":false,\"played\":false,\"init_ms\":%.6f,"
               "\"load_ms\":null,\"unlock_ms\":null,\"first_play_submit_ms\":null}\n",
               init_ms);
        audio_shutdown();
        free(s_wav_bytes);
        return 0;
    }

    started = now_ms();
    audio_clip_t clip = audio_clip_load((nt_hash64_t){BENCHMARK_ASSET_ID});
    audio_update();
    audio_clip_state_t clip_state = audio_clip_state(clip);
    double load_ms = now_ms() - started;

    double unlock_ms = 0.0;
    double play_ms = 0.0;
    bool played = false;
    audio_voice_t voice = AUDIO_VOICE_INVALID;
    if (allow_play && clip_state == AUDIO_CLIP_STATE_READY) {
        started = now_ms();
        audio_on_user_gesture();
        unlock_ms = now_ms() - started;
        started = now_ms();
        voice = audio_play(clip, AUDIO_BUS_SFX, 0.05f, false);
        play_ms = now_ms() - started;
        played = voice.value != AUDIO_VOICE_INVALID.value;
        if (played) {
            sleep_ms(75);
            audio_voice_stop(voice);
        }
    }

    if (allow_play) {
        printf("{\"device_available\":true,\"played\":%s,\"init_ms\":%.6f,"
               "\"load_ms\":%.6f,\"unlock_ms\":%.6f,\"first_play_submit_ms\":%.6f}\n",
               played ? "true" : "false",
               init_ms,
               load_ms,
               unlock_ms,
               play_ms);
    } else {
        printf("{\"device_available\":true,\"played\":false,\"init_ms\":%.6f,"
               "\"load_ms\":%.6f,\"unlock_ms\":null,\"first_play_submit_ms\":null}\n",
               init_ms,
               load_ms);
    }
    if (clip.value != AUDIO_CLIP_INVALID.value) audio_clip_unload(clip);
    audio_shutdown();
    free(s_wav_bytes);
    return clip_state == AUDIO_CLIP_STATE_READY && (!allow_play || played) ? 0 : 4;
}
