#include "features/audio/audio.h"

#include "audio_backend.h"
#include "core/nt_assert.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define AUDIO_HANDLE_INDEX_BITS 8u
#define AUDIO_HANDLE_INDEX_MASK UINT32_C(0xFF)
#define AUDIO_HANDLE_GENERATION_MASK UINT32_C(0x00FFFFFF)

typedef struct audio_clip_slot_t {
    uint32_t generation;
    uint32_t backend;
    audio_clip_state_t state;
    bool occupied;
} audio_clip_slot_t;

typedef struct audio_voice_slot_t {
    uint32_t generation;
    uint32_t backend;
    uint32_t clip_index;
    uint32_t clip_generation;
    uint64_t serial;
    bool occupied;
} audio_voice_slot_t;

static audio_clip_slot_t s_clips[AUDIO_MAX_CLIPS];
static audio_voice_slot_t s_voices[AUDIO_MAX_VOICES];
static audio_status_t s_status;
static float s_master;
static float s_music;
static float s_sfx;
static uint64_t s_voice_serial;
static bool s_initialized;

static uint32_t next_generation(uint32_t generation) {
    generation = (generation + 1u) & AUDIO_HANDLE_GENERATION_MASK;
    return generation == 0 ? 1u : generation;
}

static uint32_t handle_pack(uint32_t index, uint32_t generation) {
    return (generation << AUDIO_HANDLE_INDEX_BITS) | (index + 1u);
}

static bool handle_unpack(uint32_t value, uint32_t limit, uint32_t *index, uint32_t *generation) {
    uint32_t encoded_index = value & AUDIO_HANDLE_INDEX_MASK;
    uint32_t decoded_generation = value >> AUDIO_HANDLE_INDEX_BITS;
    if (encoded_index == 0 || encoded_index > limit || decoded_generation == 0) return false;
    *index = encoded_index - 1u;
    *generation = decoded_generation;
    return true;
}

static audio_clip_slot_t *clip_slot(audio_clip_t clip, uint32_t *index_out) {
    uint32_t index;
    uint32_t generation;
    if (!handle_unpack(clip.value, AUDIO_MAX_CLIPS, &index, &generation)) return NULL;
    audio_clip_slot_t *slot = &s_clips[index];
    if (!slot->occupied || slot->generation != generation) return NULL;
    if (index_out != NULL) *index_out = index;
    return slot;
}

static audio_voice_slot_t *voice_slot(audio_voice_t voice, uint32_t *index_out) {
    uint32_t index;
    uint32_t generation;
    if (!handle_unpack(voice.value, AUDIO_MAX_VOICES, &index, &generation)) return NULL;
    audio_voice_slot_t *slot = &s_voices[index];
    if (!slot->occupied || slot->generation != generation) return NULL;
    if (index_out != NULL) *index_out = index;
    return slot;
}

static float finite_gain(float value) {
    if (!isfinite(value)) return 0.0f;
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static void release_voice(uint32_t index) {
    audio_voice_slot_t *slot = &s_voices[index];
    if (!slot->occupied) return;
    if (slot->backend != 0) audio_core_backend_voice_stop(slot->backend);
    slot->occupied = false;
    slot->backend = 0;
    slot->generation = next_generation(slot->generation);
}

bool audio_init(void) {
    if (s_initialized) audio_shutdown();
    memset(s_clips, 0, sizeof(s_clips));
    memset(s_voices, 0, sizeof(s_voices));
    for (uint32_t i = 0; i < AUDIO_MAX_CLIPS; ++i) s_clips[i].generation = 1;
    for (uint32_t i = 0; i < AUDIO_MAX_VOICES; ++i) s_voices[i].generation = 1;
    s_master = 1.0f;
    s_music = 1.0f;
    s_sfx = 1.0f;
    s_voice_serial = 0;
    s_status.available = audio_core_backend_init();
    s_status.unlocked = false;
    s_status.enabled = true;
    s_status.paused = false;
    s_initialized = true;
    if (s_status.available) {
        audio_core_backend_set_mix(s_master, s_music, s_sfx);
        audio_core_backend_set_enabled(true);
        audio_core_backend_set_paused(false);
    }
    return s_status.available;
}

void audio_shutdown(void) {
    if (!s_initialized) return;
    for (uint32_t i = 0; i < AUDIO_MAX_VOICES; ++i) release_voice(i);
    for (uint32_t i = 0; i < AUDIO_MAX_CLIPS; ++i) {
        audio_clip_slot_t *slot = &s_clips[i];
        if (!slot->occupied) continue;
        if (slot->backend != 0) audio_core_backend_clip_destroy(slot->backend);
        slot->occupied = false;
        slot->backend = 0;
        slot->state = AUDIO_CLIP_STATE_INVALID;
        slot->generation = next_generation(slot->generation);
    }
    if (s_status.available) audio_core_backend_shutdown();
    s_status = (audio_status_t){0};
    s_initialized = false;
}

void audio_update(void) {
    if (!s_initialized || !s_status.available) return;
    audio_core_backend_update();
    for (uint32_t i = 0; i < AUDIO_MAX_CLIPS; ++i) {
        audio_clip_slot_t *slot = &s_clips[i];
        if (!slot->occupied || slot->state != AUDIO_CLIP_STATE_LOADING) continue;
        uint32_t state = audio_core_backend_decode_state(slot->backend);
        if (state == 1) slot->state = AUDIO_CLIP_STATE_READY;
        else if (state == 2) slot->state = AUDIO_CLIP_STATE_FAILED;
    }
    for (uint32_t i = 0; i < AUDIO_MAX_VOICES; ++i) {
        audio_voice_slot_t *slot = &s_voices[i];
        if (slot->occupied && !audio_core_backend_voice_active(slot->backend)) release_voice(i);
    }
}

audio_clip_t audio_clip_load(nt_hash64_t ready_blob_id) {
    if (!s_initialized || !s_status.available || ready_blob_id.value == 0) return AUDIO_CLIP_INVALID;
    if (audio_core_resource_state(ready_blob_id.value) != 2) return AUDIO_CLIP_INVALID;
    const void *bytes = NULL;
    uint32_t size = 0;
    if (!audio_core_resource_blob_view(ready_blob_id.value, &bytes, &size) || bytes == NULL || size == 0) {
        return AUDIO_CLIP_INVALID;
    }
    uint32_t index = AUDIO_MAX_CLIPS;
    for (uint32_t i = 0; i < AUDIO_MAX_CLIPS; ++i) {
        if (!s_clips[i].occupied) { index = i; break; }
    }
    if (index == AUDIO_MAX_CLIPS) return AUDIO_CLIP_INVALID;
    uint32_t backend = audio_core_backend_decode_begin(bytes, size);
    if (backend == 0) return AUDIO_CLIP_INVALID;
    audio_clip_slot_t *slot = &s_clips[index];
    slot->occupied = true;
    slot->backend = backend;
    slot->state = AUDIO_CLIP_STATE_LOADING;
    return (audio_clip_t){handle_pack(index, slot->generation)};
}

audio_clip_state_t audio_clip_state(audio_clip_t clip) {
    audio_clip_slot_t *slot = clip_slot(clip, NULL);
    return slot == NULL ? AUDIO_CLIP_STATE_INVALID : slot->state;
}

void audio_clip_unload(audio_clip_t clip) {
    uint32_t index;
    audio_clip_slot_t *slot = clip_slot(clip, &index);
    if (slot == NULL) return;
    for (uint32_t i = 0; i < AUDIO_MAX_VOICES; ++i) {
        audio_voice_slot_t *voice = &s_voices[i];
        NT_ASSERT(!voice->occupied || voice->clip_index != index || voice->clip_generation != slot->generation);
    }
    if (slot->backend != 0) audio_core_backend_clip_destroy(slot->backend);
    slot->occupied = false;
    slot->backend = 0;
    slot->state = AUDIO_CLIP_STATE_INVALID;
    slot->generation = next_generation(slot->generation);
}

audio_voice_t audio_play(audio_clip_t clip, audio_bus_t bus, float gain, bool loop) {
    if (!s_initialized || !s_status.available || !s_status.unlocked || !s_status.enabled || s_status.paused) {
        return AUDIO_VOICE_INVALID;
    }
    if (bus != AUDIO_BUS_MUSIC && bus != AUDIO_BUS_SFX) return AUDIO_VOICE_INVALID;
    uint32_t clip_index;
    audio_clip_slot_t *clip_entry = clip_slot(clip, &clip_index);
    if (clip_entry == NULL || clip_entry->state != AUDIO_CLIP_STATE_READY || !isfinite(gain)) {
        return AUDIO_VOICE_INVALID;
    }
    uint32_t voice_index = AUDIO_MAX_VOICES;
    for (uint32_t i = 0; i < AUDIO_MAX_VOICES; ++i) {
        if (!s_voices[i].occupied) { voice_index = i; break; }
    }
    if (voice_index == AUDIO_MAX_VOICES) {
        uint64_t oldest = UINT64_MAX;
        for (uint32_t i = 0; i < AUDIO_MAX_VOICES; ++i) {
            if (s_voices[i].serial < oldest) {
                oldest = s_voices[i].serial;
                voice_index = i;
            }
        }
        release_voice(voice_index);
    }
    uint32_t backend = audio_core_backend_voice_play(clip_entry->backend, (uint32_t)bus, finite_gain(gain), loop);
    if (backend == 0) return AUDIO_VOICE_INVALID;
    audio_voice_slot_t *voice = &s_voices[voice_index];
    voice->occupied = true;
    voice->backend = backend;
    voice->clip_index = clip_index;
    voice->clip_generation = clip_entry->generation;
    voice->serial = ++s_voice_serial;
    return (audio_voice_t){handle_pack(voice_index, voice->generation)};
}

void audio_voice_stop(audio_voice_t voice) {
    uint32_t index;
    if (voice_slot(voice, &index) != NULL) release_voice(index);
}

bool audio_voice_is_playing(audio_voice_t voice) {
    audio_voice_slot_t *slot = voice_slot(voice, NULL);
    return slot != NULL && audio_core_backend_voice_active(slot->backend);
}

void audio_set_mix(float master, float music, float sfx) {
    s_master = finite_gain(master);
    s_music = finite_gain(music);
    s_sfx = finite_gain(sfx);
    if (s_initialized && s_status.available) audio_core_backend_set_mix(s_master, s_music, s_sfx);
}

void audio_set_enabled(bool enabled) {
    s_status.enabled = enabled;
    if (s_initialized && s_status.available) audio_core_backend_set_enabled(enabled);
}

void audio_set_paused(bool paused) {
    s_status.paused = paused;
    if (s_initialized && s_status.available) audio_core_backend_set_paused(paused);
}

void audio_on_user_gesture(void) {
    if (s_initialized && s_status.available && audio_core_backend_user_gesture()) s_status.unlocked = true;
}

audio_status_t audio_status(void) { return s_status; }
