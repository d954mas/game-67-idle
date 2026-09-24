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

/* A decoded clip owns its PCM; a streamed one owns only its encoded bytes,
   which each of its voices decodes on the audio thread as it plays. */
typedef struct audio_native_clip_t {
    void *pcm;
    ma_uint64 frames;
    uint64_t pcm_bytes;
    void *encoded;
    uint32_t encoded_bytes;
    ma_uint32 state;
    ma_bool32 used;
} audio_native_clip_t;

/* `sound` plays the pooled buffer of a decoded clip; a streamed voice builds
   `stream` over its own `decoder` at play and tears both down at stop. */
typedef struct audio_native_voice_t {
    ma_audio_buffer_ref buffer;
    ma_sound sound;
    ma_decoder decoder;
    ma_sound stream;
    ma_bool32 buffer_initialized;
    ma_bool32 sound_initialized;
    ma_bool32 streaming;
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

static void stream_teardown(audio_native_voice_t *voice) {
    if (!voice->streaming) return;
    /* Uninit detaches the sound from the graph first, so the audio thread is
       done with the decoder before it goes. */
    ma_sound_uninit(&voice->stream);
    (void)ma_decoder_uninit(&voice->decoder);
    voice->streaming = MA_FALSE;
}

static ma_sound *voice_sound(audio_native_voice_t *voice) {
    return voice->streaming ? &voice->stream : &voice->sound;
}

static void voice_uninit(audio_native_voice_t *voice) {
    stream_teardown(voice);
    if (voice->sound_initialized) ma_sound_uninit(&voice->sound);
    if (voice->buffer_initialized) ma_audio_buffer_ref_uninit(&voice->buffer);
    memset(voice, 0, sizeof(*voice));
}

static void voice_stop(audio_native_voice_t *voice) {
    if (!voice->used) return;
    if (voice->streaming) {
        stream_teardown(voice);
        voice->used = MA_FALSE;
        return;
    }
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
    if (clip->encoded != NULL) audio_free(clip->encoded, NULL);
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
                MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT | MA_SOUND_FLAG_NO_SPATIALIZATION,
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

static ma_uint32 clip_claim(void) {
    for (ma_uint32 i = 0; i < AUDIO_NATIVE_CLIPS; ++i) {
        if (!s_clips[i].used) {
            s_clips[i].used = MA_TRUE;
            s_clips[i].state = 2;
            return i;
        }
    }
    return AUDIO_NATIVE_CLIPS;
}

/* A stream decodes at its source rate and channel count; the sound's own
   converter takes it to the engine's, so a loop wraps before that converter
   and the resampler never sees the seam. */
static ma_decoder_config stream_decoder_config(void) {
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    config.allocationCallbacks.onMalloc = audio_malloc;
    config.allocationCallbacks.onRealloc = audio_realloc;
    config.allocationCallbacks.onFree = audio_free;
    return config;
}

uint32_t audio_core_backend_stream_open(const void *bytes, uint32_t size) {
    if (!s_available || bytes == NULL || size == 0) return 0;
    const ma_uint32 index = clip_claim();
    if (index == AUDIO_NATIVE_CLIPS) return 0;
    audio_native_clip_t *clip = &s_clips[index];
    ma_decoder_config config = stream_decoder_config();
    ma_decoder probe;
    if (ma_decoder_init_memory(bytes, size, &config, &probe) != MA_SUCCESS) return index + 1u;
    (void)ma_decoder_uninit(&probe);
    clip->encoded = audio_malloc(size, NULL);
    if (clip->encoded == NULL) return index + 1u;
    memcpy(clip->encoded, bytes, size);
    clip->encoded_bytes = size;
    clip->state = 1;
    return index + 1u;
}

uint32_t audio_core_backend_decode_begin(const void *bytes, uint32_t size) {
    if (!s_available || bytes == NULL || size == 0) return 0;
    const ma_uint32 index = clip_claim();
    if (index == AUDIO_NATIVE_CLIPS) return 0;
    audio_native_clip_t *clip = &s_clips[index];
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

static uint32_t stream_play(audio_native_voice_t *voice, ma_uint32 index, const audio_native_clip_t *clip,
        ma_sound_group *group, float gain, bool loop) {
    ma_decoder_config config = stream_decoder_config();
    if (ma_decoder_init_memory(clip->encoded, clip->encoded_bytes, &config, &voice->decoder) != MA_SUCCESS) return 0;
    if (ma_sound_init_from_data_source(&s_engine, (ma_data_source *)&voice->decoder,
            MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT | MA_SOUND_FLAG_NO_SPATIALIZATION, NULL, &voice->stream) != MA_SUCCESS) {
        (void)ma_decoder_uninit(&voice->decoder);
        return 0;
    }
    voice->streaming = MA_TRUE;
    voice->used = MA_TRUE;
    ma_sound_set_volume(&voice->stream, gain);
    ma_sound_set_looping(&voice->stream, loop ? MA_TRUE : MA_FALSE);
    if (ma_node_attach_output_bus((ma_node *)&voice->stream, 0, (ma_node *)group, 0) != MA_SUCCESS ||
            ma_sound_start(&voice->stream) != MA_SUCCESS) {
        voice_stop(voice);
        return 0;
    }
    return index + 1u;
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
    ma_sound_group *group = bus == 0 ? &s_music_group : &s_sfx_group;
    if (clip_slot->encoded != NULL) return stream_play(voice, index, clip_slot, group, gain, loop);
    if (ma_audio_buffer_ref_set_data(&voice->buffer, clip_slot->pcm, clip_slot->frames) != MA_SUCCESS) {
        return 0;
    }
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
    return ma_sound_is_playing(voice_sound(&s_voices[voice - 1u])) == MA_TRUE;
}

void audio_core_backend_voice_set_gain(uint32_t voice, float gain) {
    if (voice == 0 || voice > AUDIO_NATIVE_VOICES || !s_voices[voice - 1u].used) return;
    ma_sound_set_volume(voice_sound(&s_voices[voice - 1u]), gain);
}

void audio_core_backend_voice_set_pitch(uint32_t voice, float pitch) {
    if (voice == 0 || voice > AUDIO_NATIVE_VOICES || !s_voices[voice - 1u].used) return;
    ma_sound_set_pitch(voice_sound(&s_voices[voice - 1u]), pitch);
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
uint64_t audio_miniaudio_test_clip_frames(uint32_t clip) {
    if (clip == 0 || clip > AUDIO_NATIVE_CLIPS || !s_clips[clip - 1u].used) return 0;
    return s_clips[clip - 1u].frames;
}
uint64_t audio_miniaudio_test_render(uint64_t frames) {
    float block[512 * AUDIO_NATIVE_CHANNELS];
    uint64_t rendered = 0;
    while (rendered < frames) {
        const ma_uint64 step = frames - rendered < 512 ? frames - rendered : 512;
        ma_uint64 read = 0;
        if (ma_engine_read_pcm_frames(&s_engine, block, step, &read) != MA_SUCCESS || read == 0) break;
        rendered += read;
    }
    return rendered;
}
uint64_t audio_miniaudio_test_stream_read(uint32_t voice, float *frames_out, uint64_t frames) {
    if (voice == 0 || voice > AUDIO_NATIVE_VOICES || !s_voices[voice - 1u].streaming) return 0;
    ma_decoder *decoder = &s_voices[voice - 1u].decoder;
    uint64_t total = 0;
    while (total < frames) {
        ma_uint64 read = 0;
        if (ma_data_source_read_pcm_frames((ma_data_source *)decoder,
                frames_out + total * decoder->outputChannels, frames - total, &read) != MA_SUCCESS || read == 0) {
            break;
        }
        total += read;
    }
    return total;
}
const float *audio_miniaudio_test_clip_pcm(uint32_t clip) {
    if (clip == 0 || clip > AUDIO_NATIVE_CLIPS || !s_clips[clip - 1u].used) return NULL;
    return (const float *)s_clips[clip - 1u].pcm;
}
#endif
