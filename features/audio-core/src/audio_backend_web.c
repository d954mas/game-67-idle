#include "audio_backend.h"

#if !defined(__EMSCRIPTEN__)
#error "audio_backend_web.c is only for Emscripten builds"
#endif

extern int audio_web_init(void);
extern void audio_web_shutdown(void);
extern void audio_web_update(void);
extern uint32_t audio_web_decode_begin(const void *bytes, uint32_t size);
extern uint32_t audio_web_decode_state(uint32_t clip);
extern void audio_web_clip_destroy(uint32_t clip);
extern uint32_t audio_web_voice_play(uint32_t clip, uint32_t bus, float gain, int loop);
extern int audio_web_voice_active(uint32_t voice);
extern void audio_web_voice_stop(uint32_t voice);
extern void audio_web_set_mix(float master, float music, float sfx);
extern void audio_web_set_enabled(int enabled);
extern void audio_web_set_paused(int paused);
extern int audio_web_user_gesture(void);
extern int audio_web_is_unlocked(void);

bool audio_core_backend_init(void) { return audio_web_init() != 0; }

void audio_core_backend_shutdown(void) { audio_web_shutdown(); }

void audio_core_backend_update(void) { audio_web_update(); }

uint32_t audio_core_backend_decode_begin(const void *bytes, uint32_t size) {
    return audio_web_decode_begin(bytes, size);
}

uint32_t audio_core_backend_decode_state(uint32_t clip) {
    return audio_web_decode_state(clip);
}

void audio_core_backend_clip_destroy(uint32_t clip) { audio_web_clip_destroy(clip); }

uint32_t audio_core_backend_voice_play(uint32_t clip, uint32_t bus, float gain, bool loop) {
    return audio_web_voice_play(clip, bus, gain, loop ? 1 : 0);
}

bool audio_core_backend_voice_active(uint32_t voice) {
    return audio_web_voice_active(voice) != 0;
}

void audio_core_backend_voice_stop(uint32_t voice) { audio_web_voice_stop(voice); }

void audio_core_backend_set_mix(float master, float music, float sfx) {
    audio_web_set_mix(master, music, sfx);
}

void audio_core_backend_set_enabled(bool enabled) { audio_web_set_enabled(enabled ? 1 : 0); }

void audio_core_backend_set_paused(bool paused) { audio_web_set_paused(paused ? 1 : 0); }

bool audio_core_backend_user_gesture(void) { return audio_web_user_gesture() != 0; }

bool audio_core_backend_is_unlocked(void) { return audio_web_is_unlocked() != 0; }
