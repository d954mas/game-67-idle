#ifndef GAME_AUDIO_H
#define GAME_AUDIO_H

#include <stdbool.h>

/* Procedural (synthesized) SFX cues. The engine has no sample/WAV loader for
 * game SFX — sounds are short PCM tones generated in-memory and played via the
 * platform device (winmm waveOut on Windows). Identity = CALM / ASMR: every cue
 * uses soft sine partials with smooth attack/release envelopes (no harsh
 * transients), balanced low volume, and gentle pitch.
 *
 * The first four cues are the original generic seed cues (kept for
 * compatibility); the CORRAL_* cues are tuned per-event for Critter Corral. */
typedef enum GameAudioCue {
    GAME_AUDIO_CUE_CLICK = 0,
    GAME_AUDIO_CUE_SUCCESS,
    GAME_AUDIO_CUE_NOTIFY,
    GAME_AUDIO_CUE_ERROR,
    GAME_AUDIO_CUE_CORRAL_POP,     /* critter captured — soft satisfying "plip" */
    GAME_AUDIO_CUE_CORRAL_CHAIN,   /* chain step — pitched up as the chain grows */
    GAME_AUDIO_CUE_CORRAL_BONK,    /* wrong-color bounce — gentle soft "bonk" */
    GAME_AUDIO_CUE_CORRAL_CHIME,   /* upgrade pick — pleasant two-note chime */
    GAME_AUDIO_CUE_CORRAL_WAVE,    /* wave cleared — short happy ascending flourish */
    GAME_AUDIO_CUE_CORRAL_WIN,     /* win milestone — bigger arpeggio flourish */
    GAME_AUDIO_CUE_CORRAL_START,   /* title -> play — soft welcoming swell */
    GAME_AUDIO_CUE_COUNT,
} GameAudioCue;

typedef struct GameAudioStatus {
    bool implemented;
    bool initialized;
    bool device_enabled;
    const char *backend;
    int total_play_count;
    int cue_play_count[GAME_AUDIO_CUE_COUNT];
    /* Last cue actually requested + its pitch offset in semitones — DevAPI
     * surfaces these so an automated playtest can PROVE audio fired for an
     * event (the test can't hear the speaker). */
    int last_cue;       /* GameAudioCue of the most recent play, -1 if none */
    float last_semitones;
} GameAudioStatus;

void game_audio_init(void);
void game_audio_shutdown(void);
void game_audio_update(void);
void game_audio_set_volume(float master_volume, float sfx_volume);
void game_audio_set_device_enabled(bool enabled);
void game_audio_play(GameAudioCue cue);
/* Play a cue transposed by `semitones` (can be negative). Used for per-capture
 * pitch variation and the rising chain. semitones==0 == game_audio_play(). */
void game_audio_play_pitched(GameAudioCue cue, float semitones);
GameAudioStatus game_audio_status(void);
const char *game_audio_cue_name(GameAudioCue cue);

#endif
