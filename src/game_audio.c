#include "game_audio.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#if defined(_WIN32) && !defined(NT_PLATFORM_WEB)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

#define GAME_AUDIO_SAMPLE_RATE 22050
/* Longest cue is the WIN flourish (~0.9s). 22050 * 1.0s == 22050; round up. */
#define GAME_AUDIO_MAX_SAMPLES 24000
#define GAME_AUDIO_VOICE_COUNT 8

typedef struct AudioVoice {
#if defined(_WIN32) && !defined(NT_PLATFORM_WEB)
    HWAVEOUT wave;
    WAVEHDR header;
#endif
    int16_t samples[GAME_AUDIO_MAX_SAMPLES];
    bool active;
} AudioVoice;

static AudioVoice s_voices[GAME_AUDIO_VOICE_COUNT];
static float s_master_volume = 0.75F;
static float s_sfx_volume = 0.80F;
static bool s_initialized;
static bool s_device_enabled = true;
static int s_total_play_count;
static int s_cue_play_count[GAME_AUDIO_CUE_COUNT];
static int s_last_cue = -1;
static float s_last_semitones;

static float clamp01(float value) {
    if (value < 0.0F) {
        return 0.0F;
    }
    if (value > 1.0F) {
        return 1.0F;
    }
    return value;
}

const char *game_audio_cue_name(GameAudioCue cue) {
    switch (cue) {
    case GAME_AUDIO_CUE_CLICK:
        return "click";
    case GAME_AUDIO_CUE_SUCCESS:
        return "success";
    case GAME_AUDIO_CUE_NOTIFY:
        return "notify";
    case GAME_AUDIO_CUE_ERROR:
        return "error";
    case GAME_AUDIO_CUE_CORRAL_POP:
        return "corral_pop";
    case GAME_AUDIO_CUE_CORRAL_CHAIN:
        return "corral_chain";
    case GAME_AUDIO_CUE_CORRAL_BONK:
        return "corral_bonk";
    case GAME_AUDIO_CUE_CORRAL_CHIME:
        return "corral_chime";
    case GAME_AUDIO_CUE_CORRAL_WAVE:
        return "corral_wave";
    case GAME_AUDIO_CUE_CORRAL_WIN:
        return "corral_win";
    case GAME_AUDIO_CUE_CORRAL_START:
        return "corral_start";
    case GAME_AUDIO_CUE_COUNT:
    default:
        return "unknown";
    }
}

void game_audio_init(void) {
    memset(s_voices, 0, sizeof(s_voices));
    s_last_cue = -1;
    s_last_semitones = 0.0F;
    s_initialized = true;
}

#if defined(_WIN32) && !defined(NT_PLATFORM_WEB)
static void cleanup_voice(AudioVoice *voice) {
    if (!voice->active) {
        return;
    }
    if ((voice->header.dwFlags & WHDR_PREPARED) != 0U) {
        waveOutUnprepareHeader(voice->wave, &voice->header, sizeof(voice->header));
    }
    if (voice->wave) {
        waveOutClose(voice->wave);
    }
    memset(&voice->header, 0, sizeof(voice->header));
    voice->wave = NULL;
    voice->active = false;
}
#else
static void cleanup_voice(AudioVoice *voice) {
    voice->active = false;
}
#endif

void game_audio_shutdown(void) {
    for (int i = 0; i < GAME_AUDIO_VOICE_COUNT; ++i) {
        cleanup_voice(&s_voices[i]);
    }
    s_initialized = false;
}

void game_audio_update(void) {
#if defined(_WIN32) && !defined(NT_PLATFORM_WEB)
    for (int i = 0; i < GAME_AUDIO_VOICE_COUNT; ++i) {
        if (s_voices[i].active && (s_voices[i].header.dwFlags & WHDR_DONE) != 0U) {
            cleanup_voice(&s_voices[i]);
        }
    }
#endif
}

void game_audio_set_volume(float master_volume, float sfx_volume) {
    s_master_volume = clamp01(master_volume);
    s_sfx_volume = clamp01(sfx_volume);
}

void game_audio_set_device_enabled(bool enabled) {
    s_device_enabled = enabled;
}

#if defined(_WIN32) && !defined(NT_PLATFORM_WEB)
static float cue_duration_seconds(GameAudioCue cue) {
    switch (cue) {
    case GAME_AUDIO_CUE_CLICK:
        return 0.12F;
    case GAME_AUDIO_CUE_SUCCESS:
        return 0.22F;
    case GAME_AUDIO_CUE_NOTIFY:
        return 0.26F;
    case GAME_AUDIO_CUE_ERROR:
        return 0.11F;
    case GAME_AUDIO_CUE_CORRAL_POP:
        return 0.16F; /* short, soft plip */
    case GAME_AUDIO_CUE_CORRAL_CHAIN:
        return 0.15F; /* short so a fast chain stays crisp, not muddy */
    case GAME_AUDIO_CUE_CORRAL_BONK:
        return 0.13F; /* gentle low thud, brief */
    case GAME_AUDIO_CUE_CORRAL_CHIME:
        return 0.34F; /* pleasant lingering two-note */
    case GAME_AUDIO_CUE_CORRAL_WAVE:
        return 0.55F; /* short happy flourish */
    case GAME_AUDIO_CUE_CORRAL_WIN:
        return 0.90F; /* bigger flourish */
    case GAME_AUDIO_CUE_CORRAL_START:
        return 0.40F; /* welcoming swell */
    case GAME_AUDIO_CUE_COUNT:
    default:
        return 0.10F;
    }
}

static int cue_sample_count(GameAudioCue cue) {
    int n = (int)(cue_duration_seconds(cue) * (float)GAME_AUDIO_SAMPLE_RATE);
    if (n > GAME_AUDIO_MAX_SAMPLES) {
        n = GAME_AUDIO_MAX_SAMPLES;
    }
    if (n < 1) {
        n = 1;
    }
    return n;
}

static float sine(float phase) {
    return sinf(phase * 6.28318530718F);
}

/* Smooth bell envelope (raised-cosine attack + exponential-ish release): no
 * harsh clicks at onset or tail — the core of the calm/ASMR feel. u in [0,1]. */
static float soft_env(float u) {
    /* gentle attack over the first ~12% */
    float attack = clamp01(u / 0.12F);
    attack = 0.5F - 0.5F * cosf(attack * 3.14159265F); /* raised cosine */
    /* smooth release over the back ~70% */
    float rel_u = (u - 0.30F) / 0.70F;
    float release = (rel_u <= 0.0F) ? 1.0F : (1.0F - clamp01(rel_u));
    release = release * release; /* curved tail */
    return attack * release;
}

/* Semitone offset -> frequency ratio (2^(n/12)). */
static float semitone_ratio(float semitones) {
    return powf(2.0F, semitones / 12.0F);
}

static void fill_cue_samples(GameAudioCue cue, int16_t *out, int sample_count,
                             float volume, float pitch_ratio) {
    for (int i = 0; i < sample_count; ++i) {
        const float t = (float)i / (float)GAME_AUDIO_SAMPLE_RATE;
        const float u = (float)i / (float)(sample_count > 1 ? sample_count - 1 : 1);
        float sample = 0.0F;
        float env = soft_env(u);
        switch (cue) {
        case GAME_AUDIO_CUE_CLICK: {
            const float f = (440.0F + 520.0F * u) * pitch_ratio;
            sample = 0.78F * sine(f * t) + 0.20F * sine(f * 2.0F * t);
            break;
        }
        case GAME_AUDIO_CUE_SUCCESS: {
            const float f = ((u < 0.50F) ? 620.0F : 930.0F) * pitch_ratio;
            sample = 0.62F * sine(f * t) + 0.28F * sine((f + 180.0F * u) * t);
            break;
        }
        case GAME_AUDIO_CUE_NOTIFY: {
            const float f =
                ((u < 0.33F) ? 523.25F : ((u < 0.66F) ? 659.25F : 783.99F)) *
                pitch_ratio;
            sample = 0.64F * sine(f * t) + 0.24F * sine(f * 2.0F * t);
            break;
        }
        case GAME_AUDIO_CUE_ERROR: {
            const float f = ((u < 0.50F) ? 180.0F : 150.0F) * pitch_ratio;
            sample = 0.58F * sine(f * t) + 0.14F * sine(90.0F * t);
            break;
        }
        case GAME_AUDIO_CUE_CORRAL_POP: {
            /* a soft "plip": a single mellow sine that gently rises a little,
             * with a faint octave shimmer for a rounded, water-droplet feel. */
            const float f = (560.0F + 120.0F * u) * pitch_ratio;
            sample = 0.70F * sine(f * t) + 0.12F * sine(f * 2.0F * t);
            break;
        }
        case GAME_AUDIO_CUE_CORRAL_CHAIN: {
            /* like the pop but a touch brighter; the RISING pitch across a chain
             * comes from the caller's semitone offset (pitch_ratio). */
            const float f = (620.0F + 100.0F * u) * pitch_ratio;
            sample = 0.66F * sine(f * t) + 0.16F * sine(f * 1.5F * t);
            break;
        }
        case GAME_AUDIO_CUE_CORRAL_BONK: {
            /* gentle, non-punishing: a soft low rounded thud that sags slightly
             * in pitch (a "boop"), low partial content so it never stings. */
            const float f = (250.0F - 40.0F * u) * pitch_ratio;
            sample = 0.55F * sine(f * t) + 0.18F * sine(f * 0.5F * t);
            break;
        }
        case GAME_AUDIO_CUE_CORRAL_CHIME: {
            /* pleasant reward: two soft notes (a perfect fifth up) ringing
             * together, like a gentle glass chime. */
            const float f1 = 659.25F * pitch_ratio;             /* E5 */
            const float f2 = 987.77F * pitch_ratio;             /* B5 */
            float n2 = (u < 0.18F) ? (u / 0.18F) : 1.0F;        /* 2nd note in */
            sample = 0.50F * sine(f1 * t) + 0.42F * n2 * sine(f2 * t) +
                     0.08F * sine(f1 * 2.0F * t);
            break;
        }
        case GAME_AUDIO_CUE_CORRAL_WAVE: {
            /* short happy flourish: a soft 3-note ascending major arpeggio
             * (C-E-G), each note rounded so it reads as a little cheer. */
            const float notes[3] = {523.25F, 659.25F, 783.99F}; /* C5 E5 G5 */
            int step = (int)(u * 3.0F);
            if (step > 2) {
                step = 2;
            }
            const float f = notes[step] * pitch_ratio;
            /* per-note micro-envelope so each step has its own gentle swell. */
            float su = u * 3.0F - (float)step;
            float ne = 0.5F - 0.5F * cosf(clamp01(su / 0.5F) * 3.14159265F);
            sample = (0.55F * sine(f * t) + 0.16F * sine(f * 2.0F * t)) * ne;
            /* fold the per-note swell into a softer overall tail. */
            env = clamp01(env * 1.15F);
            break;
        }
        case GAME_AUDIO_CUE_CORRAL_WIN: {
            /* bigger flourish: a 4-note ascending arpeggio up to the octave
             * (C-E-G-C), warmer (extra soft octave under) — a real "you did it!"
             * but still gentle, no brass-y harshness. */
            const float notes[4] = {523.25F, 659.25F, 783.99F, 1046.50F};
            int step = (int)(u * 4.0F);
            if (step > 3) {
                step = 3;
            }
            const float f = notes[step] * pitch_ratio;
            float su = u * 4.0F - (float)step;
            float ne = 0.5F - 0.5F * cosf(clamp01(su / 0.5F) * 3.14159265F);
            sample = (0.50F * sine(f * t) + 0.18F * sine(f * 2.0F * t) +
                      0.10F * sine(f * 0.5F * t)) *
                     ne;
            env = clamp01(env * 1.15F);
            break;
        }
        case GAME_AUDIO_CUE_CORRAL_START: {
            /* a soft welcoming swell: a low-to-mid rising sine pad that fades in
             * and out — warm, inviting, calm. */
            const float f = (392.0F + 130.0F * u) * pitch_ratio; /* G4 -> ~B4 */
            sample = 0.60F * sine(f * t) + 0.20F * sine(f * 1.5F * t);
            /* extra-soft, symmetric swell for a pad-like onset. */
            env = 0.5F - 0.5F * cosf(u * 6.28318530718F);
            break;
        }
        case GAME_AUDIO_CUE_COUNT:
        default:
            sample = 0.0F;
            break;
        }
        const float scaled = sample * env * volume * 0.42F;
        out[i] = (int16_t)(clamp01((scaled + 1.0F) * 0.5F) * 65535.0F - 32768.0F);
    }
}
#endif

void game_audio_play_pitched(GameAudioCue cue, float semitones) {
    if (cue < 0 || cue >= GAME_AUDIO_CUE_COUNT) {
        return;
    }
    s_total_play_count++;
    s_cue_play_count[cue]++;
    s_last_cue = (int)cue;
    s_last_semitones = semitones;

#if defined(_WIN32) && !defined(NT_PLATFORM_WEB)
    if (!s_initialized) {
        return;
    }
    if (!s_device_enabled) {
        return;
    }
    const float volume = clamp01(s_master_volume * s_sfx_volume);
    if (volume <= 0.001F) {
        return;
    }
    game_audio_update();
    AudioVoice *voice = NULL;
    for (int i = 0; i < GAME_AUDIO_VOICE_COUNT; ++i) {
        if (!s_voices[i].active) {
            voice = &s_voices[i];
            break;
        }
    }
    if (!voice) {
        voice = &s_voices[0];
        cleanup_voice(voice);
    }

    WAVEFORMATEX format;
    memset(&format, 0, sizeof(format));
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = GAME_AUDIO_SAMPLE_RATE;
    format.wBitsPerSample = 16;
    format.nBlockAlign = (WORD)(format.nChannels * format.wBitsPerSample / 8);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    if (waveOutOpen(&voice->wave, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        voice->wave = NULL;
        return;
    }

    const int sample_count = cue_sample_count(cue);
    fill_cue_samples(cue, voice->samples, sample_count, volume,
                     semitone_ratio(semitones));
    memset(&voice->header, 0, sizeof(voice->header));
    voice->header.lpData = (LPSTR)voice->samples;
    voice->header.dwBufferLength = (DWORD)(sample_count * (int)sizeof(int16_t));
    if (waveOutPrepareHeader(voice->wave, &voice->header, sizeof(voice->header)) != MMSYSERR_NOERROR) {
        waveOutClose(voice->wave);
        voice->wave = NULL;
        return;
    }
    voice->active = true;
    if (waveOutWrite(voice->wave, &voice->header, sizeof(voice->header)) != MMSYSERR_NOERROR) {
        cleanup_voice(voice);
    }
#else
    (void)semitones;
#endif
}

void game_audio_play(GameAudioCue cue) {
    game_audio_play_pitched(cue, 0.0F);
}

GameAudioStatus game_audio_status(void) {
    GameAudioStatus status;
    memset(&status, 0, sizeof(status));
#if defined(_WIN32) && !defined(NT_PLATFORM_WEB)
    status.implemented = true;
    status.backend = "winmm-waveout-generated-pcm";
#else
    status.implemented = false;
    status.backend = "noop";
#endif
    status.initialized = s_initialized;
    status.device_enabled = s_device_enabled;
    status.total_play_count = s_total_play_count;
    for (int i = 0; i < GAME_AUDIO_CUE_COUNT; ++i) {
        status.cue_play_count[i] = s_cue_play_count[i];
    }
    status.last_cue = s_last_cue;
    status.last_semitones = s_last_semitones;
    return status;
}
