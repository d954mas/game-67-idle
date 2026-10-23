#include "game_audio.h"

#include <generated/game.h>
#include <generated/music.h>

#include "game_asset_paths.h"

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
    bool streamed;
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
static bool s_music_load_allowed;
static bool s_music_pack_mounted;
static bool s_music_wanted;
static nt_hash32_t s_music_pack;
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
    if (asset->resource.id == NT_RESOURCE_INVALID.id) return;
    const uint8_t resource_state = nt_resource_get_state(asset->resource);
    if (resource_state != asset->resource_state) {
        asset->resource_state = resource_state;
        nt_log_info("[audio] resource %s state=%u", asset->name, (unsigned)resource_state);
    }
    if (!asset->load_attempted && backend_available && nt_resource_is_ready(asset->resource)) {
        asset->load_attempted = true;
        asset->clip = asset->streamed ? audio_clip_stream(asset->id) : audio_clip_load(asset->id);
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

static void music_update(bool wanted) {
    const bool newly_wanted = wanted && !s_music_wanted;
    if (s_music_pack_mounted && newly_wanted && nt_resource_pack_state(s_music_pack) == NT_PACK_STATE_FAILED) {
        nt_resource_unmount(s_music_pack);
        s_music_pack_mounted = false;
        s_demo_jingle.state = GAME_AUDIO_LOAD_WAITING;
    }
    s_music_wanted = wanted;
    if (!s_music_load_allowed || !wanted) return;
    if (!s_music_pack_mounted) {
        if (s_demo_jingle.state == GAME_AUDIO_LOAD_FAILED && !newly_wanted) return;
        char path[GAME_ASSET_PATH_MAX];
        if (!game_asset_paths_resolve("assets/music.ntpack", path, sizeof path) ||
            nt_resource_mount(s_music_pack, 100) != NT_OK) {
            s_demo_jingle.state = GAME_AUDIO_LOAD_FAILED;
            return;
        }
        s_music_pack_mounted = true;
        if (nt_resource_load_auto(s_music_pack, path) != NT_OK) {
            s_demo_jingle.state = GAME_AUDIO_LOAD_FAILED;
            return;
        }
        s_demo_jingle.state = GAME_AUDIO_LOAD_LOADING;
    }
    const nt_pack_state_t state = nt_resource_pack_state(s_music_pack);
    if (state == NT_PACK_STATE_FAILED) {
        s_demo_jingle.state = GAME_AUDIO_LOAD_FAILED;
    } else if (state == NT_PACK_STATE_READY && s_demo_jingle.resource.id == NT_RESOURCE_INVALID.id) {
        s_demo_jingle.resource = nt_resource_request(s_demo_jingle.id, NT_ASSET_BLOB);
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
    s_music_load_allowed = false;
    s_music_pack_mounted = false;
    s_music_wanted = false;
    s_music_pack = nt_hash32_str("music");
    asset_init(&s_ui_click, ASSET_BLOB_AUDIO_SFX_UI_CLICK, "audio/sfx/ui_click");
    s_demo_jingle = (game_audio_asset_t){
        .id = ASSET_BLOB_AUDIO_MUSIC_DEMO_JINGLE,
        .resource = NT_RESOURCE_INVALID,
        .clip = AUDIO_CLIP_INVALID,
        .state = GAME_AUDIO_LOAD_WAITING,
        .streamed = true,
        .resource_state = UINT8_MAX,
        .name = "audio/music/demo_jingle",
    };
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
    if (s_music_pack_mounted) nt_resource_unmount(s_music_pack);
    s_ui_click = (game_audio_asset_t){0};
    s_demo_jingle = (game_audio_asset_t){0};
    s_music_voice = AUDIO_VOICE_INVALID;
    s_initialized = false;
    s_music_load_allowed = false;
    s_music_pack_mounted = false;
    s_mix_applied = false;
}

void game_audio_update(void) {
    if (!s_initialized) return;
    audio_update();
    update_mix();
    const audio_status_t current = audio_status();
    music_update(current.available && current.enabled && settings_master() > 0.0F && settings_music() > 0.0F);
    asset_update(&s_ui_click, current.available);
    asset_update(&s_demo_jingle, current.available);
    if (voice_is_valid(s_music_voice) && !audio_voice_is_playing(s_music_voice)) {
        s_music_voice = AUDIO_VOICE_INVALID;
    }
}

void game_audio_on_game_ready(void) {
    if (s_initialized) s_music_load_allowed = true;
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
