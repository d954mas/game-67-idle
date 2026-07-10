#ifndef FEATURES_AUDIO_AUDIO_BACKEND_H
#define FEATURES_AUDIO_AUDIO_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

/* Resource states: 0=missing, 1=not ready, 2=ready BLOB, 3=wrong type.
   Views are borrowed only for the duration of audio_core_backend_decode_begin. */
uint32_t audio_core_resource_state(uint64_t asset_id);
bool audio_core_resource_blob_view(uint64_t asset_id, const void **bytes, uint32_t *size);

/* Decode states: 0=LOADING, 1=READY, 2=FAILED. A successful decode_begin owns
   every byte it needs before returning. Zero backend handles are invalid. */
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

#if defined(AUDIO_MINIAUDIO_TEST_NO_DEVICE)
uint64_t audio_miniaudio_test_allocation_count(void);
#endif

#endif
