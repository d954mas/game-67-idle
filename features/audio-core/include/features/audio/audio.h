#ifndef FEATURES_AUDIO_AUDIO_H
#define FEATURES_AUDIO_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

#include "hash/nt_hash.h"

#define AUDIO_MAX_CLIPS 64
#define AUDIO_MAX_VOICES 32

typedef struct audio_clip_t { uint32_t value; } audio_clip_t;
typedef struct audio_voice_t { uint32_t value; } audio_voice_t;

#define AUDIO_CLIP_INVALID ((audio_clip_t){0})
#define AUDIO_VOICE_INVALID ((audio_voice_t){0})

typedef enum audio_clip_state_t {
    AUDIO_CLIP_STATE_INVALID = 0,
    AUDIO_CLIP_STATE_LOADING,
    AUDIO_CLIP_STATE_READY,
    AUDIO_CLIP_STATE_FAILED,
} audio_clip_state_t;

typedef enum audio_bus_t {
    AUDIO_BUS_MUSIC = 0,
    AUDIO_BUS_SFX,
} audio_bus_t;

typedef struct audio_status_t {
    bool available;
    bool unlocked;
    bool enabled;
    bool paused;
} audio_status_t;

bool audio_init(void);
void audio_shutdown(void);
void audio_update(void);

audio_clip_t audio_clip_load(nt_hash64_t ready_blob_id);
audio_clip_state_t audio_clip_state(audio_clip_t clip);
void audio_clip_unload(audio_clip_t clip);

audio_voice_t audio_play(audio_clip_t clip, audio_bus_t bus, float gain, bool loop);
void audio_voice_stop(audio_voice_t voice);
bool audio_voice_is_playing(audio_voice_t voice);

void audio_set_mix(float master, float music, float sfx);
void audio_set_enabled(bool enabled);
void audio_set_paused(bool paused);
void audio_on_user_gesture(void);
audio_status_t audio_status(void);

#endif
