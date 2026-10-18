#ifndef GAME_AUDIO_H
#define GAME_AUDIO_H

#include <stdbool.h>

typedef enum GameAudioCue {
    GAME_AUDIO_CUE_UI_CLICK = 0,
    GAME_AUDIO_CUE_COUNT,
} GameAudioCue;

typedef enum GameMusicTrack {
    GAME_MUSIC_TRACK_DEMO_JINGLE = 0,
    GAME_MUSIC_TRACK_COUNT,
} GameMusicTrack;

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
    bool music_playing;
    GameAudioLoadState cue_state;
    GameAudioLoadState music_state;
} GameAudioStatus;

bool game_audio_init(void);
void game_audio_shutdown(void);
void game_audio_update(void);
void game_audio_on_user_gesture(void);

bool game_audio_play_cue(GameAudioCue cue);
bool game_audio_play_music(GameMusicTrack track, bool loop);
void game_audio_stop_music(void);

void game_audio_set_enabled(bool enabled);
void game_audio_set_paused(bool paused);
GameAudioStatus game_audio_status(void);

#endif
