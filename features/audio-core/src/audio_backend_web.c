#include "audio_backend.h"

#if !defined(__EMSCRIPTEN__)
#error "audio_backend_web.c is only for Emscripten builds"
#endif

/* The streamed-track decoder: miniaudio built without devices, threads or
   engine. It lives in this translation unit so the web build keeps its one
   source file and consumers need no new wiring. */
#include "audio_miniaudio_impl.c"

#include <emscripten.h>
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
extern double audio_web_now(void);
extern int audio_web_stream_buffered_frames(uint32_t voice);
extern int audio_web_stream_push(uint32_t voice, const float *planes, uint32_t frames, uint32_t channels,
    uint32_t rate);
extern void audio_web_stream_end(uint32_t voice);
extern void audio_web_stream_park(uint32_t voice);
extern void audio_web_stream_resume(uint32_t voice);
extern uint32_t audio_web_context_epoch(void);

#define AUDIO_WEB_STREAM_CLIPS 64u
#define AUDIO_WEB_STREAM_VOICES 32u
/* Track frames per scheduled buffer: 64 ms at 32 kHz, so the decode is spread
   thin over frames. */
#define AUDIO_WEB_STREAM_CHUNK_FRAMES 2048u
/* A voice keeps this much scheduled ahead of the clock: a 3 s main-thread
   stall plays through, with a quarter second for the frame before it and the
   one after it. Stop and gain act on the voice's gain node, so a longer
   lookahead adds no control latency. Each gap grows the voice's lookahead by
   GROWTH, up to MAX. */
#define AUDIO_WEB_STREAM_AHEAD_SECONDS 3.25
#define AUDIO_WEB_STREAM_AHEAD_MAX_SECONDS 4.0
#define AUDIO_WEB_STREAM_AHEAD_GROWTH 1.5
/* After a start, a resume or a gap an audible voice fills up to its lookahead
   within this much main-thread time per update for all voices together, one
   chunk per voice in turn, so a load right after it cannot starve it. Once
   full, a voice is fed what played since its last feed (see
   audio_core_stream_pace_frames), however far apart the updates come. */
#define AUDIO_WEB_STREAM_FILL_BUDGET_MS 4.0
/* The chunk after a gap fades in over this long, so the restart does not
   click; the cut at the gap's start happened before it could be known. */
#define AUDIO_WEB_STREAM_GAP_FADE_SECONDS 0.008

typedef struct audio_web_stream_clip_t {
    uint32_t clip;
    void *encoded;
    uint32_t size;
} audio_web_stream_clip_t;

/* The browser gets PCM at the track's own rate and channels and resamples it.
   Each chunk carries one guard frame past its end, the next chunk's first,
   so the browser's interpolation at the join reads the real next sample. */
typedef struct audio_web_stream_voice_t {
    uint32_t voice;
    uint32_t clip;
    ma_decoder decoder;
    float carry[2];
    bool primed;
    bool ended;
    bool parked;
    /* Something was scheduled: an empty queue after that is a gap. */
    bool started;
    /* Fill to the lookahead within the time budget instead of at pace. */
    bool filling;
    double ahead_seconds;
    /* Context time the gain went to zero; negative while audible. */
    double muted_since;
    /* Context time of the last feed; negative before the first. */
    double last_feed;
    /* The context these times belong to: a rebuilt context restarts its clock. */
    uint32_t epoch;
} audio_web_stream_voice_t;

static audio_web_stream_clip_t s_stream_clips[AUDIO_WEB_STREAM_CLIPS];
static audio_web_stream_voice_t *s_stream_voices[AUDIO_WEB_STREAM_VOICES];
static float s_interleaved[(AUDIO_WEB_STREAM_CHUNK_FRAMES + 1u) * 2u];
static float s_planes[(AUDIO_WEB_STREAM_CHUNK_FRAMES + 1u) * 2u];

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
    (void)ma_decoder_uninit(&(*entry)->decoder);
    free(*entry);
    *entry = NULL;
}

static void stream_clip_release(audio_web_stream_clip_t *clip) {
    free(clip->encoded);
    memset(clip, 0, sizeof(*clip));
}

static bool stream_voice_start(uint32_t voice, const audio_web_stream_clip_t *clip, bool loop, float gain) {
    audio_web_stream_voice_t **slot = NULL;
    for (uint32_t i = 0; i < AUDIO_WEB_STREAM_VOICES && slot == NULL; ++i) {
        if (s_stream_voices[i] == NULL) slot = &s_stream_voices[i];
    }
    audio_web_stream_voice_t *stream = slot != NULL ? calloc(1, sizeof(*stream)) : NULL;
    if (stream == NULL) return false;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    if (ma_decoder_init_memory(clip->encoded, clip->size, &config, &stream->decoder) != MA_SUCCESS) {
        free(stream);
        return false;
    }
    /* The loop wraps inside the decoder, so the seam is one more join. */
    (void)ma_data_source_set_looping((ma_data_source *)&stream->decoder, loop ? MA_TRUE : MA_FALSE);
    stream->voice = voice;
    stream->clip = clip->clip;
    stream->muted_since = gain > 0.0f ? -1.0 : audio_web_now();
    stream->ahead_seconds = AUDIO_WEB_STREAM_AHEAD_SECONDS;
    stream->filling = true;
    stream->last_feed = -1.0;
    stream->epoch = audio_web_context_epoch();
    *slot = stream;
    return true;
}

static uint32_t stream_read(audio_web_stream_voice_t *stream, float *out, uint32_t frames) {
    const uint32_t channels = stream->decoder.outputChannels;
    uint32_t total = 0;
    while (total < frames) {
        ma_uint64 got = 0;
        (void)ma_data_source_read_pcm_frames((ma_data_source *)&stream->decoder, out + total * channels,
            frames - total, &got);
        if (got == 0) break;
        total += (uint32_t)got;
    }
    return total;
}

/* Decodes the next chunk and hands it over; false once the stream is done. */
static bool stream_push_chunk(audio_web_stream_voice_t *stream, bool fade_in) {
    const uint32_t channels = stream->decoder.outputChannels;
    if (!stream->primed) {
        if (stream_read(stream, stream->carry, 1) == 0) return false;
        stream->primed = true;
    }
    memcpy(s_interleaved, stream->carry, sizeof(float) * channels);
    const uint32_t got = stream_read(stream, s_interleaved + channels, AUDIO_WEB_STREAM_CHUNK_FRAMES);
    /* A full read ends on the guard, which also opens the next chunk; a short
       one is the end of a stream that does not loop, and repeats its last
       frame as the guard. */
    const bool last = got < AUDIO_WEB_STREAM_CHUNK_FRAMES;
    const uint32_t frames = last ? got + 1u : got;
    if (last) memcpy(s_interleaved + frames * channels, s_interleaved + (frames - 1u) * channels, sizeof(float) * channels);
    memcpy(stream->carry, s_interleaved + frames * channels, sizeof(float) * channels);
    if (fade_in) {
        uint32_t ramp = (uint32_t)(AUDIO_WEB_STREAM_GAP_FADE_SECONDS * (double)stream->decoder.outputSampleRate);
        if (ramp > frames) ramp = frames;
        for (uint32_t i = 0; i < ramp; ++i) {
            for (uint32_t c = 0; c < channels; ++c) s_interleaved[i * channels + c] *= (float)i / (float)ramp;
        }
    }
    for (uint32_t c = 0; c < channels; ++c) {
        for (uint32_t i = 0; i <= frames; ++i) s_planes[c * (frames + 1u) + i] = s_interleaved[i * channels + c];
    }
    const bool pushed = audio_web_stream_push(stream->voice, s_planes, frames, channels,
        stream->decoder.outputSampleRate) != 0;
    return pushed && !last;
}

/* Whether the voice is below its lookahead; notes a gap on the way. `gap` is
   set when the queue ran dry, so the next chunk fades in. */
static bool stream_needs(audio_web_stream_voice_t *stream, bool *gap) {
    const int ahead = audio_web_stream_buffered_frames(stream->voice);
    *gap = false;
    if (ahead < 0) return false;
    if (stream->started && ahead == 0) {
        *gap = true;
        /* One gap grows the lookahead once, however many updates it takes to
           fill again. */
        if (!stream->filling) {
            stream->ahead_seconds *= AUDIO_WEB_STREAM_AHEAD_GROWTH;
            if (stream->ahead_seconds > AUDIO_WEB_STREAM_AHEAD_MAX_SECONDS) {
                stream->ahead_seconds = AUDIO_WEB_STREAM_AHEAD_MAX_SECONDS;
            }
            stream->filling = true;
        }
    }
    if (ahead >= (int)(stream->ahead_seconds * (double)stream->decoder.outputSampleRate)) {
        stream->filling = false;
        return false;
    }
    return true;
}

/* One chunk; false once the voice can take no more this update. A refused
   chunk would leave a hole in the schedule: the voice plays out what it has
   and ends instead. */
static bool stream_feed_one(audio_web_stream_voice_t *stream) {
    bool gap = false;
    if (!stream_needs(stream, &gap)) return false;
    if (!stream_push_chunk(stream, gap)) {
        stream->ended = true;
        audio_web_stream_end(stream->voice);
        return false;
    }
    stream->started = true;
    return true;
}

static bool stream_fills(const audio_web_stream_voice_t *stream) {
    /* A silent voice is about to park: it only keeps pace. */
    return stream->filling && stream->muted_since < 0.0 && !stream->ended && !stream->parked;
}

/* A voice held silent for AUDIO_CORE_STREAM_PARK_SECONDS stops decoding; its
   scheduled chunks are kept, so it resumes on the sample it paused on. */
static void stream_park_check(audio_web_stream_voice_t *stream) {
    if (stream->parked || stream->ended || stream->muted_since < 0.0) return;
    if (audio_web_now() - stream->muted_since < AUDIO_CORE_STREAM_PARK_SECONDS) return;
    audio_web_stream_park(stream->voice);
    stream->parked = true;
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

static void streams_update(void) {
    const double began = emscripten_get_now();
    const double now = audio_web_now();
    const uint32_t epoch = audio_web_context_epoch();
    for (uint32_t i = 0; i < AUDIO_WEB_STREAM_VOICES; ++i) {
        audio_web_stream_voice_t *stream = s_stream_voices[i];
        if (stream == NULL) continue;
        if (!audio_web_voice_active(stream->voice)) {
            stream_voice_release(&s_stream_voices[i]);
            continue;
        }
        if (stream->epoch != epoch) {
            stream->epoch = epoch;
            if (stream->muted_since >= 0.0) stream->muted_since = now;
            stream->last_feed = -1.0;
            stream->filling = true;
        }
        stream_park_check(stream);
        if (stream->ended || stream->parked) continue;
        /* Keep pace: what played since the last feed, with margin. A filling
           voice gets its first chunk here and the rest in turn below. */
        const double elapsed = stream->last_feed < 0.0 || now < stream->last_feed ? 0.0 : now - stream->last_feed;
        const uint32_t allowed = audio_core_stream_pace_frames(elapsed, stream->decoder.outputSampleRate,
            AUDIO_WEB_STREAM_CHUNK_FRAMES);
        stream->last_feed = now;
        for (uint32_t fed = 0; fed < allowed; fed += AUDIO_WEB_STREAM_CHUNK_FRAMES) {
            if (!stream_feed_one(stream) || stream_fills(stream)) break;
        }
    }
    for (bool more = true; more && emscripten_get_now() - began < AUDIO_WEB_STREAM_FILL_BUDGET_MS;) {
        more = false;
        for (uint32_t i = 0; i < AUDIO_WEB_STREAM_VOICES; ++i) {
            audio_web_stream_voice_t *stream = s_stream_voices[i];
            if (stream != NULL && stream_fills(stream) && stream_feed_one(stream)) more = true;
        }
    }
}

void audio_core_backend_update(void) {
    audio_web_update();
    streams_update();
}

/* The web runtime's timer calls this when frames stop coming while the page
   still plays sound (a throttled or offscreen frame), so streams keep their
   lookahead without the game loop. */
void audio_core_web_pump(void);
EMSCRIPTEN_KEEPALIVE void audio_core_web_pump(void) { streams_update(); }

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
    if (!stream_voice_start(voice, entry, loop, gain)) {
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

void audio_core_backend_voice_set_gain(uint32_t voice, float gain) {
    audio_web_voice_set_gain(voice, gain);
    audio_web_stream_voice_t **entry = stream_voice(voice);
    if (entry == NULL) return;
    audio_web_stream_voice_t *stream = *entry;
    if (gain > 0.0f) {
        stream->muted_since = -1.0;
        if (stream->parked) {
            stream->parked = false;
            stream->filling = true;
            audio_web_stream_resume(voice);
        }
    } else if (stream->muted_since < 0.0) {
        stream->muted_since = audio_web_now();
    }
}

void audio_core_backend_voice_set_pitch(uint32_t voice, float pitch) { audio_web_voice_set_pitch(voice, pitch); }

void audio_core_backend_set_mix(float master, float music, float sfx) {
    audio_web_set_mix(master, music, sfx);
}

void audio_core_backend_set_enabled(bool enabled) { audio_web_set_enabled(enabled ? 1 : 0); }

void audio_core_backend_set_paused(bool paused) { audio_web_set_paused(paused ? 1 : 0); }

bool audio_core_backend_user_gesture(void) { return audio_web_user_gesture() != 0; }

bool audio_core_backend_is_unlocked(void) { return audio_web_is_unlocked() != 0; }
