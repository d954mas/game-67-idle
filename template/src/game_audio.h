#ifndef GAME_AUDIO_H
#define GAME_AUDIO_H

#include <stdbool.h>

typedef enum GameAudioCue {
    GAME_AUDIO_CUE_FLASHLIGHT = 0,
    GAME_AUDIO_CUE_FUSE_HUM,
    GAME_AUDIO_CUE_FUSE_PICKUP,
    GAME_AUDIO_CUE_STALKER,
    GAME_AUDIO_CUE_CAUGHT,
    GAME_AUDIO_CUE_ESCAPE,
    GAME_AUDIO_CUE_FOOTSTEP,
    GAME_AUDIO_CUE_SPRINT_STEP,
    GAME_AUDIO_CUE_HEARTBEAT,
    GAME_AUDIO_CUE_COUNT,
} GameAudioCue;

typedef struct GameAudioStatus {
    bool implemented;
    bool initialized;
    bool device_enabled;
    const char *backend;
    int total_play_count;
    int cue_play_count[GAME_AUDIO_CUE_COUNT];
} GameAudioStatus;

void game_audio_init(void);
void game_audio_shutdown(void);
void game_audio_update(void);
void game_audio_set_volume(float master_volume, float sfx_volume);
void game_audio_set_device_enabled(bool enabled);
void game_audio_play(GameAudioCue cue);
GameAudioStatus game_audio_status(void);
const char *game_audio_cue_name(GameAudioCue cue);

#endif
