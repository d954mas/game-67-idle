#include "audio_backend.h"
#include "audio_miniaudio_config.h"

#include "../vendor/miniaudio/miniaudio.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_NATIVE_CLIPS 64u
#define AUDIO_NATIVE_VOICES 32u
#define AUDIO_NATIVE_CHANNELS 2u
#define AUDIO_NATIVE_SAMPLE_RATE 48000u
#define AUDIO_NATIVE_MAX_DECODED_BYTES_PER_CLIP (UINT64_C(128) * UINT64_C(1024) * UINT64_C(1024))
#define AUDIO_NATIVE_MAX_DECODED_BYTES_TOTAL (UINT64_C(256) * UINT64_C(1024) * UINT64_C(1024))

typedef struct audio_native_clip_t {
    void *pcm;
    ma_uint64 frames;
    uint64_t pcm_bytes;
    ma_uint32 state;
    ma_bool32 used;
} audio_native_clip_t;

typedef struct audio_native_voice_t {
    ma_audio_buffer_ref buffer;
    ma_sound sound;
    ma_bool32 buffer_initialized;
    ma_bool32 sound_initialized;
    ma_bool32 used;
} audio_native_voice_t;

static ma_context s_context;
static ma_engine s_engine;
static ma_sound_group s_music_group;
static ma_sound_group s_sfx_group;
static audio_native_clip_t s_clips[AUDIO_NATIVE_CLIPS];
static audio_native_voice_t s_voices[AUDIO_NATIVE_VOICES];
static ma_bool32 s_context_initialized;
static ma_bool32 s_engine_initialized;
static ma_bool32 s_music_initialized;
static ma_bool32 s_sfx_initialized;
static ma_bool32 s_available;
static ma_bool32 s_unlocked;
static ma_bool32 s_enabled;
static ma_bool32 s_paused;
static ma_bool32 s_started;
static uint64_t s_allocation_count;
static uint64_t s_decoded_pcm_bytes;
static uint64_t s_decoded_pcm_per_clip_limit;
static uint64_t s_decoded_pcm_total_limit;

static void *audio_malloc(size_t size, void *user_data) {
    (void)user_data;
    ++s_allocation_count;
    return malloc(size);
}

static void *audio_realloc(void *memory, size_t size, void *user_data) {
    (void)user_data;
    ++s_allocation_count;
    return realloc(memory, size);
}

static void audio_free(void *memory, void *user_data) {
    (void)user_data;
    free(memory);
}

static void voice_uninit(audio_native_voice_t *voice) {
    if (voice->sound_initialized) ma_sound_uninit(&voice->sound);
    if (voice->buffer_initialized) ma_audio_buffer_ref_uninit(&voice->buffer);
    memset(voice, 0, sizeof(*voice));
}

static void voice_stop(audio_native_voice_t *voice) {
    if (!voice->used) return;
    (void)ma_sound_stop(&voice->sound);
    (void)ma_node_detach_output_bus((ma_node *)&voice->sound, 0);
    (void)ma_audio_buffer_ref_set_data(&voice->buffer, NULL, 0);
    voice->used = MA_FALSE;
}

static void clip_destroy(audio_native_clip_t *clip) {
    if (clip->pcm != NULL) {
        if (s_decoded_pcm_bytes < clip->pcm_bytes) abort();
        s_decoded_pcm_bytes -= clip->pcm_bytes;
        audio_free(clip->pcm, NULL);
    }
    memset(clip, 0, sizeof(*clip));
}

static bool decoded_pcm_size(uint64_t frames, uint64_t *bytes) {
    const uint64_t bytes_per_frame =
        (uint64_t)AUDIO_NATIVE_CHANNELS * (uint64_t)sizeof(float);
    if (bytes == NULL) return false;
    *bytes = 0;
    if (frames == 0 || frames > UINT64_MAX / bytes_per_frame) return false;
    uint64_t decoded_bytes = frames * bytes_per_frame;
    if (decoded_bytes > SIZE_MAX) return false;
    *bytes = decoded_bytes;
    return true;
}

static bool decoded_pcm_budget_allows(uint64_t decoded_bytes) {
    if (decoded_bytes > s_decoded_pcm_per_clip_limit ||
            decoded_bytes > s_decoded_pcm_total_limit) {
        return false;
    }
    return s_decoded_pcm_bytes <= s_decoded_pcm_total_limit - decoded_bytes;
}

static void apply_run_state(void) {
    ma_bool32 should_run = s_available && s_unlocked && s_enabled && !s_paused;
#if defined(AUDIO_MINIAUDIO_TEST_NO_DEVICE)
    s_started = should_run;
#else
    if (should_run && !s_started) {
        if (ma_engine_start(&s_engine) == MA_SUCCESS) s_started = MA_TRUE;
    } else if (!should_run && s_started) {
        (void)ma_engine_stop(&s_engine);
        s_started = MA_FALSE;
    }
#endif
}

static void backend_reset(void) {
    memset(s_clips, 0, sizeof(s_clips));
    memset(s_voices, 0, sizeof(s_voices));
    s_context_initialized = MA_FALSE;
    s_engine_initialized = MA_FALSE;
    s_music_initialized = MA_FALSE;
    s_sfx_initialized = MA_FALSE;
    s_available = MA_FALSE;
    s_unlocked = MA_FALSE;
    s_enabled = MA_TRUE;
    s_paused = MA_FALSE;
    s_started = MA_FALSE;
    s_allocation_count = 0;
    s_decoded_pcm_bytes = 0;
    s_decoded_pcm_per_clip_limit = AUDIO_NATIVE_MAX_DECODED_BYTES_PER_CLIP;
    s_decoded_pcm_total_limit = AUDIO_NATIVE_MAX_DECODED_BYTES_TOTAL;
}

static void backend_cleanup(void) {
    for (ma_uint32 i = 0; i < AUDIO_NATIVE_VOICES; ++i) voice_uninit(&s_voices[i]);
    for (ma_uint32 i = 0; i < AUDIO_NATIVE_CLIPS; ++i) clip_destroy(&s_clips[i]);
    if (s_started && s_engine_initialized) (void)ma_engine_stop(&s_engine);
    if (s_sfx_initialized) ma_sound_group_uninit(&s_sfx_group);
    if (s_music_initialized) ma_sound_group_uninit(&s_music_group);
    if (s_engine_initialized) ma_engine_uninit(&s_engine);
    if (s_context_initialized) (void)ma_context_uninit(&s_context);
    backend_reset();
}

static ma_bool32 engine_and_groups_init(ma_context *context, ma_bool32 no_device) {
    ma_engine_config config = ma_engine_config_init();
    config.pContext = context;
    config.noAutoStart = MA_TRUE;
    config.noDevice = no_device;
    config.channels = AUDIO_NATIVE_CHANNELS;
    config.sampleRate = AUDIO_NATIVE_SAMPLE_RATE;
    config.allocationCallbacks.onMalloc = audio_malloc;
    config.allocationCallbacks.onRealloc = audio_realloc;
    config.allocationCallbacks.onFree = audio_free;
    if (ma_engine_init(&config, &s_engine) != MA_SUCCESS) return MA_FALSE;
    s_engine_initialized = MA_TRUE;
    if (ma_sound_group_init(&s_engine, 0, NULL, &s_music_group) != MA_SUCCESS) return MA_FALSE;
    s_music_initialized = MA_TRUE;
    if (ma_sound_group_init(&s_engine, 0, NULL, &s_sfx_group) != MA_SUCCESS) return MA_FALSE;
    s_sfx_initialized = MA_TRUE;
    for (ma_uint32 i = 0; i < AUDIO_NATIVE_VOICES; ++i) {
        audio_native_voice_t *voice = &s_voices[i];
        if (ma_audio_buffer_ref_init(
                ma_format_f32,
                AUDIO_NATIVE_CHANNELS,
                NULL,
                0,
                &voice->buffer) != MA_SUCCESS) {
            return MA_FALSE;
        }
        voice->buffer_initialized = MA_TRUE;
        if (ma_sound_init_from_data_source(
                &s_engine,
                (ma_data_source *)&voice->buffer,
                MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT | MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_NO_SPATIALIZATION,
                NULL,
                &voice->sound) != MA_SUCCESS) {
            return MA_FALSE;
        }
        voice->sound_initialized = MA_TRUE;
    }
    return MA_TRUE;
}

bool audio_core_backend_init(void) {
    backend_cleanup();
#if defined(AUDIO_MINIAUDIO_TEST_NO_DEVICE)
    if (!engine_and_groups_init(NULL, MA_TRUE)) {
        backend_cleanup();
        return false;
    }
    s_available = MA_TRUE;
    return true;
#else
#if defined(_WIN32)
    static const ma_backend backends[] = {ma_backend_wasapi, ma_backend_null};
#elif defined(__linux__)
    static const ma_backend backends[] = {ma_backend_alsa, ma_backend_pulseaudio, ma_backend_null};
#endif
    for (size_t i = 0; i < sizeof(backends) / sizeof(backends[0]); ++i) {
        ma_backend backend = backends[i];
        if (ma_context_init(&backend, 1, NULL, &s_context) != MA_SUCCESS) continue;
        s_context_initialized = MA_TRUE;
        if (backend == ma_backend_null || s_context.backend == ma_backend_null) {
            backend_cleanup();
            return false;
        }
        if (engine_and_groups_init(&s_context, MA_FALSE)) {
            s_available = MA_TRUE;
            return true;
        }
        backend_cleanup();
    }
    backend_cleanup();
    return false;
#endif
}

void audio_core_backend_shutdown(void) { backend_cleanup(); }
void audio_core_backend_update(void) {}

uint32_t audio_core_backend_decode_begin(const void *bytes, uint32_t size) {
    if (!s_available || bytes == NULL || size == 0) return 0;
    ma_uint32 index = AUDIO_NATIVE_CLIPS;
    for (ma_uint32 i = 0; i < AUDIO_NATIVE_CLIPS; ++i) {
        if (!s_clips[i].used) { index = i; break; }
    }
    if (index == AUDIO_NATIVE_CLIPS) return 0;
    audio_native_clip_t *clip = &s_clips[index];
    clip->used = MA_TRUE;
    clip->state = 2;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, AUDIO_NATIVE_CHANNELS, AUDIO_NATIVE_SAMPLE_RATE);
    ma_decoder decoder;
    if (ma_decoder_init_memory(bytes, size, &config, &decoder) == MA_SUCCESS) {
        ma_uint64 frames = 0;
        uint64_t pcm_bytes = 0;
        if (ma_decoder_get_length_in_pcm_frames(&decoder, &frames) == MA_SUCCESS &&
                decoded_pcm_size(frames, &pcm_bytes) &&
                decoded_pcm_budget_allows(pcm_bytes)) {
            void *pcm = audio_malloc((size_t)pcm_bytes, NULL);
            if (pcm != NULL) {
                ma_uint64 frames_read = 0;
                ma_result read_result = ma_decoder_read_pcm_frames(&decoder, pcm, frames, &frames_read);
                if (read_result == MA_SUCCESS && frames_read == frames) {
                    clip->pcm = pcm;
                    clip->frames = frames;
                    clip->pcm_bytes = pcm_bytes;
                    clip->state = 1;
                    s_decoded_pcm_bytes += pcm_bytes;
                } else {
                    audio_free(pcm, NULL);
                }
            }
        }
        (void)ma_decoder_uninit(&decoder);
    }
    return index + 1u;
}

uint32_t audio_core_backend_decode_state(uint32_t clip) {
    if (clip == 0 || clip > AUDIO_NATIVE_CLIPS || !s_clips[clip - 1u].used) return 2;
    return s_clips[clip - 1u].state;
}

void audio_core_backend_clip_destroy(uint32_t clip) {
    if (clip == 0 || clip > AUDIO_NATIVE_CLIPS || !s_clips[clip - 1u].used) return;
    clip_destroy(&s_clips[clip - 1u]);
}

uint32_t audio_core_backend_voice_play(uint32_t clip, uint32_t bus, float gain, bool loop) {
    if (!s_available || clip == 0 || clip > AUDIO_NATIVE_CLIPS || s_clips[clip - 1u].state != 1) return 0;
    if (bus > 1) return 0;
    ma_uint32 index = AUDIO_NATIVE_VOICES;
    for (ma_uint32 i = 0; i < AUDIO_NATIVE_VOICES; ++i) {
        if (!s_voices[i].used) { index = i; break; }
    }
    if (index == AUDIO_NATIVE_VOICES) return 0;
    audio_native_clip_t *clip_slot = &s_clips[clip - 1u];
    audio_native_voice_t *voice = &s_voices[index];
    if (ma_audio_buffer_ref_set_data(&voice->buffer, clip_slot->pcm, clip_slot->frames) != MA_SUCCESS) {
        return 0;
    }
    ma_sound_group *group = bus == 0 ? &s_music_group : &s_sfx_group;
    if (ma_sound_seek_to_pcm_frame(&voice->sound, 0) != MA_SUCCESS ||
            ma_node_attach_output_bus((ma_node *)&voice->sound, 0, (ma_node *)group, 0) != MA_SUCCESS) {
        (void)ma_audio_buffer_ref_set_data(&voice->buffer, NULL, 0);
        return 0;
    }
    voice->used = MA_TRUE;
    ma_sound_set_volume(&voice->sound, gain);
    ma_sound_set_looping(&voice->sound, loop ? MA_TRUE : MA_FALSE);
    if (ma_sound_start(&voice->sound) != MA_SUCCESS) {
        voice_stop(voice);
        return 0;
    }
    return index + 1u;
}

bool audio_core_backend_voice_active(uint32_t voice) {
    if (voice == 0 || voice > AUDIO_NATIVE_VOICES || !s_voices[voice - 1u].used) return false;
    return ma_sound_is_playing(&s_voices[voice - 1u].sound) == MA_TRUE;
}

void audio_core_backend_voice_stop(uint32_t voice) {
    if (voice == 0 || voice > AUDIO_NATIVE_VOICES || !s_voices[voice - 1u].used) return;
    voice_stop(&s_voices[voice - 1u]);
}

void audio_core_backend_set_mix(float master, float music, float sfx) {
    if (!s_available) return;
    (void)ma_engine_set_volume(&s_engine, master);
    ma_sound_group_set_volume(&s_music_group, music);
    ma_sound_group_set_volume(&s_sfx_group, sfx);
}

void audio_core_backend_set_enabled(bool enabled) { s_enabled = enabled ? MA_TRUE : MA_FALSE; apply_run_state(); }
void audio_core_backend_set_paused(bool paused) { s_paused = paused ? MA_TRUE : MA_FALSE; apply_run_state(); }

bool audio_core_backend_user_gesture(void) {
    if (!s_available) return false;
    s_unlocked = MA_TRUE;
    apply_run_state();
    return s_started == MA_TRUE;
}

bool audio_core_backend_is_unlocked(void) {
    return s_unlocked == MA_TRUE && s_started == MA_TRUE;
}

#if defined(AUDIO_MINIAUDIO_TEST_NO_DEVICE)
uint64_t audio_miniaudio_test_allocation_count(void) { return s_allocation_count; }
uint64_t audio_miniaudio_test_per_clip_limit(void) { return s_decoded_pcm_per_clip_limit; }
uint64_t audio_miniaudio_test_total_limit(void) { return s_decoded_pcm_total_limit; }
void audio_miniaudio_test_set_decoded_limits(uint64_t per_clip_bytes, uint64_t total_bytes) {
    if (s_decoded_pcm_bytes != 0) abort();
    s_decoded_pcm_per_clip_limit = per_clip_bytes;
    s_decoded_pcm_total_limit = total_bytes;
}
uint64_t audio_miniaudio_test_decoded_bytes(void) { return s_decoded_pcm_bytes; }
bool audio_miniaudio_test_pcm_size(uint64_t frames, uint64_t *bytes) {
    return decoded_pcm_size(frames, bytes);
}
#endif
