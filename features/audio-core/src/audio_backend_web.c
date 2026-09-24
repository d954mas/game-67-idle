#include "audio_backend.h"

#if !defined(__EMSCRIPTEN__)
#error "audio_backend_web.c is only for Emscripten builds"
#endif

/* The streamed-track decoder: miniaudio built without devices, threads or
   engine. It lives in this translation unit so the web build keeps its one
   source file and consumers need no new wiring. */
#include "audio_miniaudio_impl.c"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

extern int audio_web_init(void);
extern void audio_web_shutdown(void);
extern void audio_web_update(void);
extern uint32_t audio_web_decode_begin(const void *bytes, uint32_t size);
extern uint32_t audio_web_decode_state(uint32_t clip);
extern void audio_web_clip_destroy(uint32_t clip);
extern uint32_t audio_web_voice_play(uint32_t clip, uint32_t bus, float gain, int loop);
extern int audio_web_voice_active(uint32_t voice);
extern void audio_web_voice_stop(uint32_t voice);
extern void audio_web_voice_set_gain(uint32_t voice, float gain);
extern void audio_web_voice_set_pitch(uint32_t voice, float pitch);
extern void audio_web_set_mix(float master, float music, float sfx);
extern void audio_web_set_enabled(int enabled);
extern void audio_web_set_paused(int paused);
extern int audio_web_user_gesture(void);
extern int audio_web_is_unlocked(void);
extern uint32_t audio_web_stream_open(int ready);
extern uint32_t audio_web_sample_rate(void);
extern int audio_web_stream_buffered_frames(uint32_t voice);
extern int audio_web_stream_push(uint32_t voice, const float *left, const float *right, uint32_t frames);
extern void audio_web_stream_end(uint32_t voice);

#define AUDIO_WEB_STREAM_CLIPS 64u
#define AUDIO_WEB_STREAM_VOICES 32u
/* Output frames per scheduled buffer, at the context rate. */
#define AUDIO_WEB_STREAM_CHUNK_FRAMES 8192u
/* A voice keeps this much scheduled ahead of the clock: the main thread may
   stall this long before the music gaps. */
#define AUDIO_WEB_STREAM_AHEAD_SECONDS 1.0
/* Chunks decoded per voice per update, so a start or a catch-up is spread
   over frames instead of landing in one. */
#define AUDIO_WEB_STREAM_CHUNKS_PER_UPDATE 2u
/* Source frames held between the decoder and the converter; at most stereo. */
#define AUDIO_WEB_STREAM_SOURCE_FRAMES 2048u

typedef struct audio_web_stream_clip_t {
    uint32_t clip;
    void *encoded;
    uint32_t size;
} audio_web_stream_clip_t;

/* The decoder loops at its source rate and the converter after it is never
   reset, so a wrap is sample-exact and the resampler never sees a seam. */
typedef struct audio_web_stream_voice_t {
    uint32_t voice;
    uint32_t clip;
    ma_decoder decoder;
    ma_data_converter converter;
    float source[AUDIO_WEB_STREAM_SOURCE_FRAMES * 2u];
    uint32_t source_offset;
    uint32_t source_frames;
    bool ended;
} audio_web_stream_voice_t;

static audio_web_stream_clip_t s_stream_clips[AUDIO_WEB_STREAM_CLIPS];
static audio_web_stream_voice_t *s_stream_voices[AUDIO_WEB_STREAM_VOICES];
static float s_chunk[AUDIO_WEB_STREAM_CHUNK_FRAMES * 2u];
static float s_left[AUDIO_WEB_STREAM_CHUNK_FRAMES];
static float s_right[AUDIO_WEB_STREAM_CHUNK_FRAMES];

static audio_web_stream_clip_t *stream_clip(uint32_t clip) {
    for (uint32_t i = 0; clip != 0 && i < AUDIO_WEB_STREAM_CLIPS; ++i) {
        if (s_stream_clips[i].clip == clip) return &s_stream_clips[i];
    }
    return NULL;
}

static audio_web_stream_voice_t **stream_voice(uint32_t voice) {
    for (uint32_t i = 0; voice != 0 && i < AUDIO_WEB_STREAM_VOICES; ++i) {
        if (s_stream_voices[i] != NULL && s_stream_voices[i]->voice == voice) return &s_stream_voices[i];
    }
    return NULL;
}

static void stream_voice_release(audio_web_stream_voice_t **entry) {
    audio_web_stream_voice_t *stream = *entry;
    ma_data_converter_uninit(&stream->converter, NULL);
    (void)ma_decoder_uninit(&stream->decoder);
    free(stream);
    *entry = NULL;
}

static void stream_clip_release(audio_web_stream_clip_t *clip) {
    free(clip->encoded);
    memset(clip, 0, sizeof(*clip));
}

static bool stream_voice_start(uint32_t voice, const audio_web_stream_clip_t *clip, bool loop) {
    audio_web_stream_voice_t **slot = NULL;
    for (uint32_t i = 0; i < AUDIO_WEB_STREAM_VOICES && slot == NULL; ++i) {
        if (s_stream_voices[i] == NULL) slot = &s_stream_voices[i];
    }
    const uint32_t rate = audio_web_sample_rate();
    audio_web_stream_voice_t *stream = slot != NULL && rate != 0 ? calloc(1, sizeof(*stream)) : NULL;
    if (stream == NULL) return false;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    if (ma_decoder_init_memory(clip->encoded, clip->size, &config, &stream->decoder) != MA_SUCCESS) {
        free(stream);
        return false;
    }
    ma_data_converter_config convert = ma_data_converter_config_init(ma_format_f32, ma_format_f32,
        stream->decoder.outputChannels, 2, stream->decoder.outputSampleRate, rate);
    if (ma_data_converter_init(&convert, NULL, &stream->converter) != MA_SUCCESS) {
        (void)ma_decoder_uninit(&stream->decoder);
        free(stream);
        return false;
    }
    (void)ma_data_source_set_looping((ma_data_source *)&stream->decoder, loop ? MA_TRUE : MA_FALSE);
    stream->voice = voice;
    stream->clip = clip->clip;
    *slot = stream;
    return true;
}

/* Up to `frames` of context-rate stereo into s_chunk; fewer only at the end
   of a stream that does not loop. */
static uint32_t stream_decode(audio_web_stream_voice_t *stream, uint32_t frames) {
    const uint32_t channels = stream->decoder.outputChannels;
    uint32_t produced = 0;
    while (produced < frames) {
        if (stream->source_frames == 0) {
            ma_uint64 read = 0;
            stream->source_offset = 0;
            while (read < AUDIO_WEB_STREAM_SOURCE_FRAMES) {
                ma_uint64 got = 0;
                (void)ma_data_source_read_pcm_frames((ma_data_source *)&stream->decoder,
                    stream->source + read * channels, AUDIO_WEB_STREAM_SOURCE_FRAMES - read, &got);
                if (got == 0) break;
                read += got;
            }
            if (read == 0) break;
            stream->source_frames = (uint32_t)read;
        }
        ma_uint64 in = stream->source_frames;
        ma_uint64 out = frames - produced;
        if (ma_data_converter_process_pcm_frames(&stream->converter, stream->source + stream->source_offset * channels,
                &in, s_chunk + produced * 2u, &out) != MA_SUCCESS || (in == 0 && out == 0)) {
            break;
        }
        stream->source_offset += (uint32_t)in;
        stream->source_frames -= (uint32_t)in;
        produced += (uint32_t)out;
    }
    return produced;
}

static void stream_feed(audio_web_stream_voice_t *stream, uint32_t rate) {
    const int wanted = (int)(AUDIO_WEB_STREAM_AHEAD_SECONDS * (double)rate);
    int ahead = audio_web_stream_buffered_frames(stream->voice);
    for (uint32_t chunk = 0; chunk < AUDIO_WEB_STREAM_CHUNKS_PER_UPDATE && ahead >= 0 && ahead < wanted; ++chunk) {
        const uint32_t frames = stream_decode(stream, AUDIO_WEB_STREAM_CHUNK_FRAMES);
        for (uint32_t i = 0; i < frames; ++i) {
            s_left[i] = s_chunk[i * 2u];
            s_right[i] = s_chunk[i * 2u + 1u];
        }
        /* A refused chunk would leave a hole in the schedule: the voice plays
           out what it has and ends instead. */
        const bool pushed = frames == 0 || audio_web_stream_push(stream->voice, s_left, s_right, frames) != 0;
        if (!pushed || frames < AUDIO_WEB_STREAM_CHUNK_FRAMES) {
            stream->ended = true;
            audio_web_stream_end(stream->voice);
            return;
        }
        ahead += (int)frames;
    }
}

bool audio_core_backend_init(void) { return audio_web_init() != 0; }

void audio_core_backend_shutdown(void) {
    for (uint32_t i = 0; i < AUDIO_WEB_STREAM_VOICES; ++i) {
        if (s_stream_voices[i] != NULL) stream_voice_release(&s_stream_voices[i]);
    }
    for (uint32_t i = 0; i < AUDIO_WEB_STREAM_CLIPS; ++i) {
        if (s_stream_clips[i].clip != 0) stream_clip_release(&s_stream_clips[i]);
    }
    audio_web_shutdown();
}

void audio_core_backend_update(void) {
    audio_web_update();
    const uint32_t rate = audio_web_sample_rate();
    for (uint32_t i = 0; i < AUDIO_WEB_STREAM_VOICES; ++i) {
        audio_web_stream_voice_t *stream = s_stream_voices[i];
        if (stream == NULL) continue;
        if (!audio_web_voice_active(stream->voice)) {
            stream_voice_release(&s_stream_voices[i]);
        } else if (!stream->ended && rate != 0) {
            stream_feed(stream, rate);
        }
    }
}

uint32_t audio_core_backend_decode_begin(const void *bytes, uint32_t size) {
    return audio_web_decode_begin(bytes, size);
}

uint32_t audio_core_backend_stream_open(const void *bytes, uint32_t size) {
    audio_web_stream_clip_t *entry = NULL;
    for (uint32_t i = 0; i < AUDIO_WEB_STREAM_CLIPS && entry == NULL; ++i) {
        if (s_stream_clips[i].clip == 0) entry = &s_stream_clips[i];
    }
    if (entry == NULL || bytes == NULL || size == 0) return 0;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder probe;
    bool decodable = ma_decoder_init_memory(bytes, size, &config, &probe) == MA_SUCCESS;
    if (decodable) {
        decodable = probe.outputChannels <= 2u;
        (void)ma_decoder_uninit(&probe);
    }
    void *encoded = decodable ? malloc(size) : NULL;
    const uint32_t clip = audio_web_stream_open(encoded != NULL ? 1 : 0);
    if (clip == 0 || encoded == NULL) {
        free(encoded);
        return clip;
    }
    memcpy(encoded, bytes, size);
    *entry = (audio_web_stream_clip_t){.clip = clip, .encoded = encoded, .size = size};
    return clip;
}

uint32_t audio_core_backend_decode_state(uint32_t clip) {
    return audio_web_decode_state(clip);
}

void audio_core_backend_clip_destroy(uint32_t clip) {
    audio_web_stream_clip_t *entry = stream_clip(clip);
    /* A stream voice decodes from its clip's encoded bytes, so none may
       outlive the clip. */
    for (uint32_t i = 0; entry != NULL && i < AUDIO_WEB_STREAM_VOICES; ++i) {
        if (s_stream_voices[i] != NULL && s_stream_voices[i]->clip == clip) {
            audio_web_voice_stop(s_stream_voices[i]->voice);
            stream_voice_release(&s_stream_voices[i]);
        }
    }
    if (entry != NULL) stream_clip_release(entry);
    audio_web_clip_destroy(clip);
}

uint32_t audio_core_backend_voice_play(uint32_t clip, uint32_t bus, float gain, bool loop) {
    const uint32_t voice = audio_web_voice_play(clip, bus, gain, loop ? 1 : 0);
    const audio_web_stream_clip_t *entry = stream_clip(clip);
    if (voice == 0 || entry == NULL) return voice;
    if (!stream_voice_start(voice, entry, loop)) {
        audio_web_voice_stop(voice);
        return 0;
    }
    return voice;
}

bool audio_core_backend_voice_active(uint32_t voice) {
    return audio_web_voice_active(voice) != 0;
}

void audio_core_backend_voice_stop(uint32_t voice) {
    audio_web_voice_stop(voice);
    audio_web_stream_voice_t **entry = stream_voice(voice);
    if (entry != NULL) stream_voice_release(entry);
}

void audio_core_backend_voice_set_gain(uint32_t voice, float gain) { audio_web_voice_set_gain(voice, gain); }

void audio_core_backend_voice_set_pitch(uint32_t voice, float pitch) { audio_web_voice_set_pitch(voice, pitch); }

void audio_core_backend_set_mix(float master, float music, float sfx) {
    audio_web_set_mix(master, music, sfx);
}

void audio_core_backend_set_enabled(bool enabled) { audio_web_set_enabled(enabled ? 1 : 0); }

void audio_core_backend_set_paused(bool paused) { audio_web_set_paused(paused ? 1 : 0); }

bool audio_core_backend_user_gesture(void) { return audio_web_user_gesture() != 0; }

bool audio_core_backend_is_unlocked(void) { return audio_web_is_unlocked() != 0; }
