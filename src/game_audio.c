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
#define GAME_AUDIO_MAX_SAMPLES 6615
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
    case GAME_AUDIO_CUE_FLASHLIGHT:
        return "flashlight";
    case GAME_AUDIO_CUE_FUSE_HUM:
        return "fuse_hum";
    case GAME_AUDIO_CUE_FUSE_PICKUP:
        return "fuse_pickup";
    case GAME_AUDIO_CUE_STALKER:
        return "stalker";
    case GAME_AUDIO_CUE_CAUGHT:
        return "caught";
    case GAME_AUDIO_CUE_ESCAPE:
        return "escape";
    case GAME_AUDIO_CUE_FOOTSTEP:
        return "footstep";
    case GAME_AUDIO_CUE_SPRINT_STEP:
        return "sprint_step";
    case GAME_AUDIO_CUE_HEARTBEAT:
        return "heartbeat";
    case GAME_AUDIO_CUE_COUNT:
    default:
        return "unknown";
    }
}

void game_audio_init(void) {
    memset(s_voices, 0, sizeof(s_voices));
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
static int cue_sample_count(GameAudioCue cue) {
    switch (cue) {
    case GAME_AUDIO_CUE_FLASHLIGHT:
        return (int)(0.08F * (float)GAME_AUDIO_SAMPLE_RATE);
    case GAME_AUDIO_CUE_FUSE_HUM:
        return (int)(0.28F * (float)GAME_AUDIO_SAMPLE_RATE);
    case GAME_AUDIO_CUE_FUSE_PICKUP:
        return (int)(0.26F * (float)GAME_AUDIO_SAMPLE_RATE);
    case GAME_AUDIO_CUE_STALKER:
        return (int)(0.24F * (float)GAME_AUDIO_SAMPLE_RATE);
    case GAME_AUDIO_CUE_CAUGHT:
        return (int)(0.30F * (float)GAME_AUDIO_SAMPLE_RATE);
    case GAME_AUDIO_CUE_ESCAPE:
        return (int)(0.28F * (float)GAME_AUDIO_SAMPLE_RATE);
    case GAME_AUDIO_CUE_FOOTSTEP:
        return (int)(0.09F * (float)GAME_AUDIO_SAMPLE_RATE);
    case GAME_AUDIO_CUE_SPRINT_STEP:
        return (int)(0.075F * (float)GAME_AUDIO_SAMPLE_RATE);
    case GAME_AUDIO_CUE_HEARTBEAT:
        return (int)(0.18F * (float)GAME_AUDIO_SAMPLE_RATE);
    case GAME_AUDIO_CUE_COUNT:
    default:
        return (int)(0.10F * (float)GAME_AUDIO_SAMPLE_RATE);
    }
}

static float sine(float phase) {
    return sinf(phase * 6.28318530718F);
}

static void fill_cue_samples(GameAudioCue cue, int16_t *out, int sample_count, float volume) {
    for (int i = 0; i < sample_count; ++i) {
        const float t = (float)i / (float)GAME_AUDIO_SAMPLE_RATE;
        const float u = (float)i / (float)(sample_count > 1 ? sample_count - 1 : 1);
        const float attack = fminf(1.0F, u * 18.0F);
        const float release = fminf(1.0F, (1.0F - u) * 5.0F);
        const float env = attack * release;
        float sample = 0.0F;
        switch (cue) {
        case GAME_AUDIO_CUE_FLASHLIGHT: {
            const float f = (u < 0.45F) ? 1500.0F : 420.0F;
            sample = 0.62F * sine(f * t) + 0.28F * sine((f * 0.5F) * t);
            break;
        }
        case GAME_AUDIO_CUE_FUSE_HUM: {
            const float wobble = 1.0F + 0.09F * sine(7.0F * t);
            const float buzz = (sine(180.0F * wobble * t) > 0.0F) ? 1.0F : -1.0F;
            sample = 0.42F * sine(92.0F * wobble * t) + 0.18F * buzz;
            break;
        }
        case GAME_AUDIO_CUE_FUSE_PICKUP: {
            const float f = 260.0F + 680.0F * u;
            sample = 0.66F * sine(f * t) + 0.32F * sine((f * 1.51F) * t);
            break;
        }
        case GAME_AUDIO_CUE_STALKER: {
            const float tremolo = 0.35F + 0.65F * fabsf(sine(11.0F * t));
            sample = tremolo * (0.58F * sine(58.0F * t) + 0.24F * sine(116.0F * t));
            break;
        }
        case GAME_AUDIO_CUE_CAUGHT: {
            const float f = 92.0F - 34.0F * u;
            const float grit = (sine(410.0F * t) > 0.0F) ? 0.18F : -0.18F;
            sample = 0.72F * sine(f * t) + grit;
            break;
        }
        case GAME_AUDIO_CUE_ESCAPE: {
            const float f = (u < 0.45F) ? 392.0F : 784.0F;
            sample = 0.54F * sine(f * t) + 0.24F * sine((f + 220.0F * u) * t);
            break;
        }
        case GAME_AUDIO_CUE_FOOTSTEP: {
            const float strike = expf(-u * 11.5F);
            const float grit = (sine(920.0F * t + 3.0F * sine(37.0F * t)) > 0.0F) ? 0.10F : -0.10F;
            sample = strike * (0.82F * sine(66.0F * t) + 0.24F * sine(132.0F * t) + grit);
            break;
        }
        case GAME_AUDIO_CUE_SPRINT_STEP: {
            const float strike = expf(-u * 14.0F);
            const float slap = expf(-u * 28.0F) * ((sine(1500.0F * t) > 0.0F) ? 0.20F : -0.20F);
            sample = strike * (0.94F * sine(88.0F * t) + 0.30F * sine(176.0F * t)) + slap;
            break;
        }
        case GAME_AUDIO_CUE_HEARTBEAT: {
            const float pulse_a = expf(-fabsf(u - 0.14F) * 34.0F);
            const float pulse_b = expf(-fabsf(u - 0.42F) * 42.0F) * 0.68F;
            const float pulse = pulse_a + pulse_b;
            sample = pulse * (0.78F * sine(54.0F * t) + 0.20F * sine(108.0F * t));
            break;
        }
        case GAME_AUDIO_CUE_COUNT:
        default:
            sample = 0.0F;
            break;
        }
        const float scaled = sample * env * volume * 0.42F;
        out[i] = (int16_t)(scaled * 32767.0F);
    }
}
#endif

void game_audio_play(GameAudioCue cue) {
    if (cue < 0 || cue >= GAME_AUDIO_CUE_COUNT) {
        return;
    }
    s_total_play_count++;
    s_cue_play_count[cue]++;

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
    fill_cue_samples(cue, voice->samples, sample_count, volume);
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
#endif
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
    return status;
}
