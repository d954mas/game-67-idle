#ifndef GAME_AUDIO_H
#define GAME_AUDIO_H

#include <stdbool.h>

typedef enum GameAudioCue {
    GAME_AUDIO_CUE_UI_CLICK = 0,
    GAME_AUDIO_CUE_DIALOGUE_OPEN,
    GAME_AUDIO_CUE_DIALOGUE_CHOICE,
    GAME_AUDIO_CUE_GEAR_SELECT,
    GAME_AUDIO_CUE_GEAR_EQUIP,
    GAME_AUDIO_CUE_LOCATION_MOVE,
    GAME_AUDIO_CUE_LOCATION_INSPECT,
    GAME_AUDIO_CUE_HEAL,
    GAME_AUDIO_CUE_COMBAT_START,
    GAME_AUDIO_CUE_COMBAT_HIT,
    GAME_AUDIO_CUE_COMBAT_VICTORY,
    GAME_AUDIO_CUE_COMBAT_DEFEAT,
    GAME_AUDIO_CUE_REWARD,
    GAME_AUDIO_CUE_SETTINGS,
    GAME_AUDIO_CUE_COUNT,
} GameAudioCue;

typedef struct GameAudioStatus {
    bool implemented;
    bool initialized;
    bool device_enabled;
    bool music_requested;
    bool music_active;
    bool music_asset_loaded;
    const char *backend;
    const char *asset_root;
    float master_volume;
    float music_volume;
    float sfx_volume;
    int loaded_clip_count;
    int total_play_count;
    int music_play_count;
    int cue_play_count[GAME_AUDIO_CUE_COUNT];
} GameAudioStatus;

void game_audio_init(void);
void game_audio_shutdown(void);
void game_audio_update(void);
void game_audio_set_volume(float master_volume, float music_volume, float sfx_volume);
void game_audio_set_device_enabled(bool enabled);
void game_audio_start_music(void);
void game_audio_stop_music(void);
void game_audio_play(GameAudioCue cue);
GameAudioStatus game_audio_status(void);
const char *game_audio_cue_name(GameAudioCue cue);
const char *game_audio_cue_asset_path(GameAudioCue cue);
const char *game_audio_music_asset_path(void);

#endif
