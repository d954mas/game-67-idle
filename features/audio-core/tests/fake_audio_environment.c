#include "fake_audio_environment.h"

#include <string.h>

#define FAKE_LIMIT 128u
#define FAKE_BYTES 4u

typedef struct fake_resource_t {
    uint64_t asset_id;
    uint32_t state;
    bool view_succeeds;
    bool used;
    uint8_t bytes[FAKE_BYTES];
} fake_resource_t;

typedef struct fake_decode_t {
    uint32_t state;
    bool used;
    uint8_t copied[FAKE_BYTES];
} fake_decode_t;

typedef struct fake_voice_t {
    bool used;
    bool active;
} fake_voice_t;

static const uint8_t k_original[FAKE_BYTES] = {0x52, 0x49, 0x46, 0x46};
static fake_resource_t s_resources[FAKE_LIMIT];
static fake_decode_t s_decodes[FAKE_LIMIT];
static fake_voice_t s_voices[FAKE_LIMIT];
static uint32_t s_decode_begins;
static uint32_t s_clip_destroys;
static uint32_t s_voice_stops;
static uint32_t s_mix_applies;
static uint32_t s_gestures;
static uint32_t s_shutdowns;
static uint32_t s_plays;
static float s_master;
static float s_music;
static float s_sfx;
static bool s_enabled;
static bool s_paused;
static bool s_available;
static bool s_backend_unlocked;
static bool s_gesture_result;

void fake_audio_reset(void) {
    memset(s_resources, 0, sizeof(s_resources));
    memset(s_decodes, 0, sizeof(s_decodes));
    memset(s_voices, 0, sizeof(s_voices));
    s_decode_begins = 0;
    s_clip_destroys = 0;
    s_voice_stops = 0;
    s_mix_applies = 0;
    s_gestures = 0;
    s_shutdowns = 0;
    s_plays = 0;
    s_master = 1.0f;
    s_music = 1.0f;
    s_sfx = 1.0f;
    s_enabled = true;
    s_paused = false;
    s_available = true;
    s_backend_unlocked = false;
    s_gesture_result = true;
}

static fake_resource_t *add_resource(uint64_t asset_id, uint32_t state, bool view_succeeds) {
    for (uint32_t i = 0; i < FAKE_LIMIT; ++i) {
        if (!s_resources[i].used) {
            s_resources[i].used = true;
            s_resources[i].asset_id = asset_id;
            s_resources[i].state = state;
            s_resources[i].view_succeeds = view_succeeds;
            memcpy(s_resources[i].bytes, k_original, FAKE_BYTES);
            return &s_resources[i];
        }
    }
    return NULL;
}

static fake_resource_t *find_resource(uint64_t asset_id) {
    for (uint32_t i = 0; i < FAKE_LIMIT; ++i) {
        if (s_resources[i].used && s_resources[i].asset_id == asset_id) return &s_resources[i];
    }
    return NULL;
}

void fake_audio_add_ready_blob(uint64_t asset_id) { (void)add_resource(asset_id, 2, true); }
void fake_audio_add_not_ready_blob(uint64_t asset_id) { (void)add_resource(asset_id, 1, false); }
void fake_audio_add_wrong_type_resource(uint64_t asset_id) { (void)add_resource(asset_id, 3, false); }
void fake_audio_add_blob_with_failed_view(uint64_t asset_id) { (void)add_resource(asset_id, 2, false); }

void fake_audio_mutate_and_evict_source(uint64_t asset_id) {
    fake_resource_t *resource = find_resource(asset_id);
    if (resource == NULL) return;
    memset(resource->bytes, 0xEE, sizeof(resource->bytes));
    resource->used = false;
}

bool fake_audio_pending_decode_owns_original_bytes(void) {
    for (uint32_t i = 0; i < FAKE_LIMIT; ++i) {
        if (s_decodes[i].used && s_decodes[i].state == 0) {
            return memcmp(s_decodes[i].copied, k_original, FAKE_BYTES) == 0;
        }
    }
    return false;
}

void fake_audio_complete_next_decode(bool success) {
    for (uint32_t i = 0; i < FAKE_LIMIT; ++i) {
        if (s_decodes[i].used && s_decodes[i].state == 0) {
            s_decodes[i].state = success ? 1u : 2u;
            return;
        }
    }
}

void fake_audio_finish_oldest_voice(void) {
    for (uint32_t i = 0; i < FAKE_LIMIT; ++i) {
        if (s_voices[i].used && s_voices[i].active) {
            s_voices[i].active = false;
            return;
        }
    }
}

void fake_audio_set_backend_available(bool available) { s_available = available; }
void fake_audio_set_backend_unlocked(bool unlocked) { s_backend_unlocked = unlocked; }
void fake_audio_set_gesture_result(bool result) { s_gesture_result = result; }
uint32_t fake_audio_backend_decode_begin_count(void) { return s_decode_begins; }
uint32_t fake_audio_backend_clip_destroy_count(void) { return s_clip_destroys; }
uint32_t fake_audio_backend_voice_stop_count(void) { return s_voice_stops; }
uint32_t fake_audio_backend_mix_apply_count(void) { return s_mix_applies; }
uint32_t fake_audio_backend_gesture_count(void) { return s_gestures; }
uint32_t fake_audio_backend_shutdown_count(void) { return s_shutdowns; }
uint32_t fake_audio_backend_play_count(void) { return s_plays; }
float fake_audio_backend_master(void) { return s_master; }
float fake_audio_backend_music(void) { return s_music; }
float fake_audio_backend_sfx(void) { return s_sfx; }
bool fake_audio_backend_enabled(void) { return s_enabled; }
bool fake_audio_backend_paused(void) { return s_paused; }

uint32_t audio_core_resource_state(uint64_t asset_id) {
    fake_resource_t *resource = find_resource(asset_id);
    return resource == NULL ? 0u : resource->state;
}

bool audio_core_resource_blob_view(uint64_t asset_id, const void **bytes, uint32_t *size) {
    fake_resource_t *resource = find_resource(asset_id);
    if (resource == NULL || resource->state != 2 || !resource->view_succeeds || bytes == NULL || size == NULL) {
        return false;
    }
    *bytes = resource->bytes;
    *size = FAKE_BYTES;
    return true;
}

bool audio_core_backend_init(void) { return s_available; }
void audio_core_backend_shutdown(void) { ++s_shutdowns; }
void audio_core_backend_update(void) {}

uint32_t audio_core_backend_decode_begin(const void *bytes, uint32_t size) {
    if (bytes == NULL || size != FAKE_BYTES) return 0;
    for (uint32_t i = 0; i < FAKE_LIMIT; ++i) {
        if (!s_decodes[i].used) {
            s_decodes[i].used = true;
            s_decodes[i].state = 0;
            memcpy(s_decodes[i].copied, bytes, FAKE_BYTES);
            ++s_decode_begins;
            return i + 1;
        }
    }
    return 0;
}

uint32_t audio_core_backend_decode_state(uint32_t clip) {
    if (clip == 0 || clip > FAKE_LIMIT || !s_decodes[clip - 1].used) return 2;
    return s_decodes[clip - 1].state;
}

void audio_core_backend_clip_destroy(uint32_t clip) {
    if (clip > 0 && clip <= FAKE_LIMIT && s_decodes[clip - 1].used) {
        s_decodes[clip - 1].used = false;
        ++s_clip_destroys;
    }
}

uint32_t audio_core_backend_voice_play(uint32_t clip, uint32_t bus, float gain, bool loop) {
    (void)bus;
    (void)gain;
    (void)loop;
    if (audio_core_backend_decode_state(clip) != 1) return 0;
    for (uint32_t i = 0; i < FAKE_LIMIT; ++i) {
        if (!s_voices[i].used) {
            s_voices[i].used = true;
            s_voices[i].active = true;
            ++s_plays;
            return i + 1;
        }
    }
    return 0;
}

bool audio_core_backend_voice_active(uint32_t voice) {
    return voice > 0 && voice <= FAKE_LIMIT && s_voices[voice - 1].used && s_voices[voice - 1].active;
}

void audio_core_backend_voice_stop(uint32_t voice) {
    if (voice > 0 && voice <= FAKE_LIMIT && s_voices[voice - 1].used) {
        s_voices[voice - 1].used = false;
        s_voices[voice - 1].active = false;
        ++s_voice_stops;
    }
}

void audio_core_backend_set_mix(float master, float music, float sfx) {
    s_master = master;
    s_music = music;
    s_sfx = sfx;
    ++s_mix_applies;
}
void audio_core_backend_set_enabled(bool enabled) { s_enabled = enabled; }
void audio_core_backend_set_paused(bool paused) { s_paused = paused; }
bool audio_core_backend_user_gesture(void) {
    ++s_gestures;
    if (s_gesture_result) s_backend_unlocked = true;
    return s_gesture_result;
}
bool audio_core_backend_is_unlocked(void) { return s_backend_unlocked; }
