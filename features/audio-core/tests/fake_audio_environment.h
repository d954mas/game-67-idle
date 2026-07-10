#ifndef FAKE_AUDIO_ENVIRONMENT_H
#define FAKE_AUDIO_ENVIRONMENT_H

#include <stdbool.h>
#include <stdint.h>

void fake_audio_reset(void);
void fake_audio_add_ready_blob(uint64_t asset_id);
void fake_audio_add_not_ready_blob(uint64_t asset_id);
void fake_audio_add_wrong_type_resource(uint64_t asset_id);
void fake_audio_add_blob_with_failed_view(uint64_t asset_id);
void fake_audio_mutate_and_evict_source(uint64_t asset_id);
bool fake_audio_pending_decode_owns_original_bytes(void);
void fake_audio_complete_next_decode(bool success);
void fake_audio_finish_oldest_voice(void);
void fake_audio_set_backend_available(bool available);
void fake_audio_set_backend_unlocked(bool unlocked);
void fake_audio_set_gesture_result(bool result);

uint32_t fake_audio_backend_decode_begin_count(void);
uint32_t fake_audio_backend_clip_destroy_count(void);
uint32_t fake_audio_backend_voice_stop_count(void);
uint32_t fake_audio_backend_mix_apply_count(void);
uint32_t fake_audio_backend_gesture_count(void);
uint32_t fake_audio_backend_shutdown_count(void);
uint32_t fake_audio_backend_play_count(void);
float fake_audio_backend_master(void);
float fake_audio_backend_music(void);
float fake_audio_backend_sfx(void);
bool fake_audio_backend_enabled(void);
bool fake_audio_backend_paused(void);

/* Private test seam. Resource states: 0=missing, 1=not ready, 2=ready BLOB,
   3=ready resource of the wrong type. Returned views are borrowed. */
uint32_t audio_core_resource_state(uint64_t asset_id);
bool audio_core_resource_blob_view(uint64_t asset_id, const void **bytes, uint32_t *size);

/* Backend decode states: 0=LOADING, 1=READY, 2=FAILED. Decode begin must copy
   the borrowed resource bytes before returning. Zero backend handles are invalid. */
bool audio_core_backend_init(void);
void audio_core_backend_shutdown(void);
void audio_core_backend_update(void);
uint32_t audio_core_backend_decode_begin(const void *bytes, uint32_t size);
uint32_t audio_core_backend_decode_state(uint32_t clip);
void audio_core_backend_clip_destroy(uint32_t clip);
uint32_t audio_core_backend_voice_play(uint32_t clip, uint32_t bus, float gain, bool loop);
bool audio_core_backend_voice_active(uint32_t voice);
void audio_core_backend_voice_stop(uint32_t voice);
void audio_core_backend_set_mix(float master, float music, float sfx);
void audio_core_backend_set_enabled(bool enabled);
void audio_core_backend_set_paused(bool paused);
bool audio_core_backend_user_gesture(void);
bool audio_core_backend_is_unlocked(void);

#endif
