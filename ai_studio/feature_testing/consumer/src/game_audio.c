#include "game_audio.h"

#include <generated/game_assets.h>

#include "features/audio/audio.h"
#include "features/platform_sdk/platform_sdk.h"
#include "features/settings/settings.h"
#include "log/nt_log.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"

typedef struct game_audio_asset_t {
    nt_hash64_t id;
    nt_resource_t resource;
    audio_clip_t clip;
    GameAudioLoadState state;
    bool load_attempted;
    uint8_t resource_state;
    const char *name;
} game_audio_asset_t;

typedef struct game_audio_cue_desc_t {
    game_audio_asset_t *asset;
    float gain;
} game_audio_cue_desc_t;

static game_audio_asset_t s_ui_click;
static game_audio_asset_t s_demo_jingle;
static audio_voice_t s_music_voice;
static bool s_initialized;
static bool s_mix_applied;
static float s_master;
static float s_music;
static float s_sfx;
static platform_sdk_listener_id_t s_pause_listener;
static platform_sdk_listener_id_t s_resume_listener;
static const game_audio_cue_desc_t s_cues[GAME_AUDIO_CUE_COUNT] = {
    [GAME_AUDIO_CUE_UI_CLICK] = {&s_ui_click, 0.80f},
};

static bool clip_is_valid(audio_clip_t clip) {
    return clip.value != AUDIO_CLIP_INVALID.value;
}

static bool voice_is_valid(audio_voice_t voice) {
    return voice.value != AUDIO_VOICE_INVALID.value;
}

static void on_platform_pause(void *userdata) {
    (void)userdata;
    game_audio_set_paused(true);
}

static void on_platform_resume(void *userdata) {
    (void)userdata;
    game_audio_set_paused(false);
}

static void asset_init(game_audio_asset_t *asset, nt_hash64_t id, const char *name) {
    *asset = (game_audio_asset_t){
        .id = id,
        .resource = nt_resource_request(id, NT_ASSET_BLOB),
        .clip = AUDIO_CLIP_INVALID,
        .state = GAME_AUDIO_LOAD_WAITING,
        .load_attempted = false,
        .resource_state = UINT8_MAX,
        .name = name,
    };
}

static void asset_update(game_audio_asset_t *asset, bool backend_available) {
    const uint8_t resource_state = nt_resource_get_state(asset->resource);
    if (resource_state != asset->resource_state) {
        asset->resource_state = resource_state;
        nt_log_info("[audio] resource %s state=%u", asset->name, (unsigned)resource_state);
    }
    if (!asset->load_attempted && backend_available && nt_resource_is_ready(asset->resource)) {
        asset->load_attempted = true;
        asset->clip = audio_clip_load(asset->id);
        asset->state = clip_is_valid(asset->clip) ? GAME_AUDIO_LOAD_LOADING : GAME_AUDIO_LOAD_FAILED;
        if (!clip_is_valid(asset->clip)) nt_log_error("[audio] failed to start decode: %s", asset->name);
    }

    if (!clip_is_valid(asset->clip)) return;
    switch (audio_clip_state(asset->clip)) {
    case AUDIO_CLIP_STATE_LOADING:
        asset->state = GAME_AUDIO_LOAD_LOADING;
        break;
    case AUDIO_CLIP_STATE_READY:
        if (asset->state != GAME_AUDIO_LOAD_READY) nt_log_info("[audio] decoded: %s", asset->name);
        asset->state = GAME_AUDIO_LOAD_READY;
        break;
    case AUDIO_CLIP_STATE_FAILED:
    case AUDIO_CLIP_STATE_INVALID:
    default:
        asset->state = GAME_AUDIO_LOAD_FAILED;
        break;
    }
}

static void update_mix(void) {
    const float master = settings_master();
    const float music = settings_music();
    const float sfx = settings_sfx();
    if (s_mix_applied && master == s_master && music == s_music && sfx == s_sfx) return;
    s_master = master;
    s_music = music;
    s_sfx = sfx;
    s_mix_applied = true;
    audio_set_mix(master, music, sfx);
}

bool game_audio_init(void) {
    if (s_initialized) game_audio_shutdown();

    s_music_voice = AUDIO_VOICE_INVALID;
    s_mix_applied = false;
    asset_init(&s_ui_click, ASSET_BLOB_AUDIO_SFX_UI_CLICK, "audio/sfx/ui_click");
    asset_init(&s_demo_jingle, ASSET_BLOB_AUDIO_MUSIC_DEMO_JINGLE, "audio/music/demo_jingle");
    const bool available = audio_init();
    s_pause_listener = platform_sdk_on_pause(on_platform_pause, NULL);
    s_resume_listener = platform_sdk_on_resume(on_platform_resume, NULL);
    s_initialized = true;
    return available;
}

void game_audio_shutdown(void) {
    if (!s_initialized) return;
    if (s_pause_listener != 0U) {
        platform_sdk_remove_listener(s_pause_listener);
        s_pause_listener = 0U;
    }
    if (s_resume_listener != 0U) {
        platform_sdk_remove_listener(s_resume_listener);
        s_resume_listener = 0U;
    }
    game_audio_stop_music();
    audio_shutdown();
    s_ui_click = (game_audio_asset_t){0};
    s_demo_jingle = (game_audio_asset_t){0};
    s_music_voice = AUDIO_VOICE_INVALID;
    s_initialized = false;
    s_mix_applied = false;
}

void game_audio_update(void) {
    if (!s_initialized) return;
    audio_update();
    update_mix();
    const bool available = audio_status().available;
    asset_update(&s_ui_click, available);
    asset_update(&s_demo_jingle, available);
    if (voice_is_valid(s_music_voice) && !audio_voice_is_playing(s_music_voice)) {
        s_music_voice = AUDIO_VOICE_INVALID;
    }
}

void game_audio_on_user_gesture(void) {
    if (s_initialized) audio_on_user_gesture();
}

bool game_audio_play_cue(GameAudioCue cue) {
    if (!s_initialized || cue < 0 || cue >= GAME_AUDIO_CUE_COUNT) return false;
    const game_audio_cue_desc_t desc = s_cues[cue];
    if (desc.asset == NULL || desc.asset->state != GAME_AUDIO_LOAD_READY) return false;
    return voice_is_valid(audio_play(desc.asset->clip, AUDIO_BUS_SFX, desc.gain, false));
}

bool game_audio_play_music(GameMusicTrack track, bool loop) {
    if (!s_initialized || track < 0 || track >= GAME_MUSIC_TRACK_COUNT) return false;
    game_audio_asset_t *asset = NULL;
    switch (track) {
    case GAME_MUSIC_TRACK_DEMO_JINGLE:
        asset = &s_demo_jingle;
        break;
    case GAME_MUSIC_TRACK_COUNT:
    default:
        return false;
    }
    if (asset->state != GAME_AUDIO_LOAD_READY) return false;

    const audio_voice_t replacement = audio_play(asset->clip, AUDIO_BUS_MUSIC, 1.0f, loop);
    if (!voice_is_valid(replacement)) return false;
    if (voice_is_valid(s_music_voice)) audio_voice_stop(s_music_voice);
    s_music_voice = replacement;
    return true;
}

void game_audio_stop_music(void) {
    if (!voice_is_valid(s_music_voice)) return;
    audio_voice_stop(s_music_voice);
    s_music_voice = AUDIO_VOICE_INVALID;
}

void game_audio_set_enabled(bool enabled) {
    if (s_initialized) audio_set_enabled(enabled);
}

void game_audio_set_paused(bool paused) {
    if (s_initialized) audio_set_paused(paused);
}

GameAudioStatus game_audio_status(void) {
    const audio_status_t core = audio_status();
    return (GameAudioStatus){
        .initialized = s_initialized,
        .available = core.available,
        .unlocked = core.unlocked,
        .enabled = core.enabled,
        .paused = core.paused,
        .music_playing = voice_is_valid(s_music_voice) && audio_voice_is_playing(s_music_voice),
        .cue_state = s_ui_click.state,
        .music_state = s_demo_jingle.state,
    };
}
