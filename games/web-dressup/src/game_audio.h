#ifndef GAME_AUDIO_H
#define GAME_AUDIO_H

#include <stdbool.h>

typedef enum GameAudioCue {
    GAME_AUDIO_CUE_UI_CLICK = 0,
    GAME_AUDIO_CUE_AWAKENING_CHARGE,
    GAME_AUDIO_CUE_AWAKENING_FLASH,
    GAME_AUDIO_CUE_AWAKENING_REVEAL,
    GAME_AUDIO_CUE_COUNT,
} GameAudioCue;

typedef enum GameAudioLoadState {
    GAME_AUDIO_LOAD_WAITING = 0,
    GAME_AUDIO_LOAD_LOADING,
    GAME_AUDIO_LOAD_READY,
    GAME_AUDIO_LOAD_FAILED,
} GameAudioLoadState;

typedef struct GameAudioStatus {
    bool initialized;
    bool available;
    bool unlocked;
    bool enabled;
    bool paused;
    GameAudioLoadState cue_state[GAME_AUDIO_CUE_COUNT];
} GameAudioStatus;

bool game_audio_init(void);
void game_audio_shutdown(void);
void game_audio_update(void);
void game_audio_on_user_gesture(void);
bool game_audio_play_cue(GameAudioCue cue);
void game_audio_set_enabled(bool enabled);
void game_audio_set_paused(bool paused);
GameAudioStatus game_audio_status(void);
const char *game_audio_cue_name(GameAudioCue cue);

#endif
