#ifndef FEATURES_GAME_RECORDING_H
#define FEATURES_GAME_RECORDING_H

#ifdef NT_DEVAPI_GROUP_RECORDER
void game_recording_register(void);
void game_recording_frame(double now, float dt);
void game_recording_shutdown(void);
#else
static inline void game_recording_register(void) {}
static inline void game_recording_frame(double now, float dt) { (void)now; (void)dt; }
static inline void game_recording_shutdown(void) {}
#endif

#endif
