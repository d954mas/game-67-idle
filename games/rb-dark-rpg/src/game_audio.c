#include "game_audio.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && !defined(NT_PLATFORM_WEB)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#ifndef GAME_AUDIO_ASSET_DIR
#define GAME_AUDIO_ASSET_DIR "assets/audio"
#endif

#define GAME_AUDIO_PATH_MAX 512
#define GAME_AUDIO_VOICE_COUNT 10
#define GAME_AUDIO_MUSIC_VOICE 0
#define GAME_AUDIO_FIRST_SFX_VOICE 1
#define GAME_AUDIO_DEFAULT_SFX_TTL_MS 1400U

typedef struct AudioAsset {
    char path[GAME_AUDIO_PATH_MAX];
    bool loaded;
} AudioAsset;

typedef struct AudioVoice {
#if defined(_WIN32) && !defined(NT_PLATFORM_WEB)
    char alias[32];
    DWORD started_ms;
    DWORD ttl_ms;
#endif
    bool active;
    bool music;
} AudioVoice;

static AudioVoice s_voices[GAME_AUDIO_VOICE_COUNT];
static AudioAsset s_cue_assets[GAME_AUDIO_CUE_COUNT];
static AudioAsset s_music_asset;
static float s_master_volume = 0.80F;
static float s_music_volume = 0.45F;
static float s_sfx_volume = 0.90F;
static bool s_initialized;
static bool s_device_enabled = true;
static bool s_music_requested;
static int s_loaded_clip_count;
static int s_total_play_count;
static int s_music_play_count;
static int s_cue_play_count[GAME_AUDIO_CUE_COUNT];

static float clamp01(float value) {
    if (value < 0.0F) {
        return 0.0F;
    }
    if (value > 1.0F) {
        return 1.0F;
    }
    return value;
}

static void copy_string(char *out, size_t out_size, const char *value) {
    if (!out || out_size == 0U) {
        return;
    }
    const int written = snprintf(out, out_size, "%s", value ? value : "");
    if (written < 0 || (size_t)written >= out_size) {
        out[out_size - 1U] = '\0';
    }
}

const char *game_audio_cue_name(GameAudioCue cue) {
    switch (cue) {
    case GAME_AUDIO_CUE_UI_CLICK:
        return "ui_click";
    case GAME_AUDIO_CUE_DIALOGUE_OPEN:
        return "dialogue_open";
    case GAME_AUDIO_CUE_DIALOGUE_CHOICE:
        return "dialogue_choice";
    case GAME_AUDIO_CUE_GEAR_SELECT:
        return "gear_select";
    case GAME_AUDIO_CUE_GEAR_EQUIP:
        return "gear_equip";
    case GAME_AUDIO_CUE_LOCATION_MOVE:
        return "location_move";
    case GAME_AUDIO_CUE_LOCATION_INSPECT:
        return "location_inspect";
    case GAME_AUDIO_CUE_HEAL:
        return "heal";
    case GAME_AUDIO_CUE_COMBAT_START:
        return "combat_start";
    case GAME_AUDIO_CUE_COMBAT_HIT:
        return "combat_hit";
    case GAME_AUDIO_CUE_COMBAT_VICTORY:
        return "combat_victory";
    case GAME_AUDIO_CUE_COMBAT_DEFEAT:
        return "combat_defeat";
    case GAME_AUDIO_CUE_REWARD:
        return "reward";
    case GAME_AUDIO_CUE_SETTINGS:
        return "settings";
    case GAME_AUDIO_CUE_COUNT:
    default:
        return "unknown";
    }
}

const char *game_audio_cue_asset_path(GameAudioCue cue) {
    switch (cue) {
    case GAME_AUDIO_CUE_UI_CLICK:
        return "sfx/ui_click.mp3";
    case GAME_AUDIO_CUE_DIALOGUE_OPEN:
        return "sfx/dialogue_open.mp3";
    case GAME_AUDIO_CUE_DIALOGUE_CHOICE:
        return "sfx/dialogue_choice.mp3";
    case GAME_AUDIO_CUE_GEAR_SELECT:
        return "sfx/gear_select.mp3";
    case GAME_AUDIO_CUE_GEAR_EQUIP:
        return "sfx/gear_equip.mp3";
    case GAME_AUDIO_CUE_LOCATION_MOVE:
        return "sfx/location_move.mp3";
    case GAME_AUDIO_CUE_LOCATION_INSPECT:
        return "sfx/location_inspect.mp3";
    case GAME_AUDIO_CUE_HEAL:
        return "sfx/heal.mp3";
    case GAME_AUDIO_CUE_COMBAT_START:
        return "sfx/combat_start.mp3";
    case GAME_AUDIO_CUE_COMBAT_HIT:
        return "sfx/combat_hit.mp3";
    case GAME_AUDIO_CUE_COMBAT_VICTORY:
        return "sfx/combat_victory.mp3";
    case GAME_AUDIO_CUE_COMBAT_DEFEAT:
        return "sfx/combat_defeat.mp3";
    case GAME_AUDIO_CUE_REWARD:
        return "sfx/reward.mp3";
    case GAME_AUDIO_CUE_SETTINGS:
        return "sfx/settings.mp3";
    case GAME_AUDIO_CUE_COUNT:
    default:
        return NULL;
    }
}

const char *game_audio_music_asset_path(void) {
    return "music/dark_forest_theme.mp3";
}

static bool build_audio_path(char *out, size_t out_size, const char *relative_path) {
    if (!relative_path || !out || out_size == 0U) {
        return false;
    }
    const char *root = GAME_AUDIO_ASSET_DIR;
    const size_t root_len = strlen(root);
    const bool needs_separator = root_len > 0U && root[root_len - 1U] != '/' && root[root_len - 1U] != '\\';
    const int written = snprintf(out, out_size, "%s%s%s", root, needs_separator ? "/" : "", relative_path);
    return written > 0 && (size_t)written < out_size;
}

static bool file_exists(const char *path) {
    FILE *f = NULL;
#if defined(_WIN32)
    if (fopen_s(&f, path, "rb") != 0) {
        f = NULL;
    }
#else
    f = fopen(path, "rb");
#endif
    if (!f) {
        return false;
    }
    fclose(f);
    return true;
}

static void clear_asset(AudioAsset *asset) {
    asset->path[0] = '\0';
    asset->loaded = false;
}

static bool load_asset(AudioAsset *asset, const char *relative_path) {
    clear_asset(asset);
    char path[GAME_AUDIO_PATH_MAX];
    if (!build_audio_path(path, sizeof(path), relative_path) || !file_exists(path)) {
        return false;
    }
    copy_string(asset->path, sizeof(asset->path), path);
    asset->loaded = true;
    return true;
}

static void unload_audio_assets(void) {
    for (int i = 0; i < GAME_AUDIO_CUE_COUNT; ++i) {
        clear_asset(&s_cue_assets[i]);
    }
    clear_asset(&s_music_asset);
    s_loaded_clip_count = 0;
}

static void load_audio_assets(void) {
    unload_audio_assets();

    for (int i = 0; i < GAME_AUDIO_CUE_COUNT; ++i) {
        const char *relative_path = game_audio_cue_asset_path((GameAudioCue)i);
        if (relative_path && load_asset(&s_cue_assets[i], relative_path)) {
            s_loaded_clip_count++;
        }
    }

    const char *music_path = game_audio_music_asset_path();
    if (music_path) {
        (void)load_asset(&s_music_asset, music_path);
    }
}

#if defined(__EMSCRIPTEN__)
EM_JS(void, game_audio_web_init_impl, (void), {
    if (Module.__rbDarkRpgAudio) {
        return;
    }
    const state = {
        enabled: true,
        master: 0.8,
        musicVolume: 0.85,
        sfxVolume: 0.9,
        cache: new Map(),
        music: null,
        musicPath: "",
        pendingMusicPath: "",
        activeSfx: new Set()
    };

    state.urlFor = (path) => {
        if (state.cache.has(path)) {
            return state.cache.get(path);
        }
        try {
            const fs = Module.FS || FS;
            const bytes = fs.readFile(path);
            const lower = path.toLowerCase();
            const mime = lower.endsWith(".ogg") ? "audio/ogg" : "audio/mpeg";
            const url = URL.createObjectURL(new Blob([bytes], { type: mime }));
            state.cache.set(path, url);
            return url;
        } catch (e) {
            return path;
        }
    };

    state.stopMusic = () => {
        if (!state.music) {
            return;
        }
        state.music.pause();
        state.music.currentTime = 0;
        state.music = null;
        state.musicPath = "";
    };

    state.playMusic = (path) => {
        state.pendingMusicPath = path;
        if (!state.enabled || !path) {
            return;
        }
        const volume = Math.max(0, Math.min(1, state.master * state.musicVolume));
        if (volume <= 0.001) {
            return;
        }
        if (state.music && state.musicPath === path) {
            state.music.volume = volume;
            return;
        }
        state.stopMusic();
        const audio = new Audio(state.urlFor(path));
        audio.loop = true;
        audio.preload = "auto";
        audio.volume = volume;
        state.music = audio;
        state.musicPath = path;
        const promise = audio.play();
        if (promise && promise.catch) {
            promise.catch(() => {
                state.pendingMusicPath = path;
            });
        }
    };

    state.playSfx = (path, volume) => {
        if (!state.enabled || !path || volume <= 0.001) {
            return;
        }
        const audio = new Audio(state.urlFor(path));
        audio.preload = "auto";
        audio.volume = Math.max(0, Math.min(1, volume));
        state.activeSfx.add(audio);
        audio.addEventListener("ended", () => state.activeSfx.delete(audio), { once: true });
        audio.addEventListener("error", () => state.activeSfx.delete(audio), { once: true });
        const promise = audio.play();
        if (promise && promise.catch) {
            promise.catch(() => state.activeSfx.delete(audio));
        }
    };

    const unlock = () => {
        if (state.pendingMusicPath) {
            state.playMusic(state.pendingMusicPath);
        }
    };
    window.addEventListener("pointerdown", unlock, { capture: true, passive: true });
    window.addEventListener("keydown", unlock, { capture: true, passive: true });
    window.addEventListener("touchstart", unlock, { capture: true, passive: true });
    Module.__rbDarkRpgAudio = state;
})

EM_JS(void, game_audio_web_shutdown_impl, (void), {
    const state = Module.__rbDarkRpgAudio;
    if (!state) {
        return;
    }
    state.stopMusic();
    for (const audio of state.activeSfx) {
        audio.pause();
    }
    state.activeSfx.clear();
})

EM_JS(void, game_audio_web_set_volume_impl, (float master, float music, float sfx), {
    const state = Module.__rbDarkRpgAudio;
    if (!state) {
        return;
    }
    state.master = master;
    state.musicVolume = music;
    state.sfxVolume = sfx;
    if (state.music) {
        state.music.volume = Math.max(0, Math.min(1, state.master * state.musicVolume));
    }
})

EM_JS(void, game_audio_web_set_enabled_impl, (int enabled), {
    const state = Module.__rbDarkRpgAudio;
    if (!state) {
        return;
    }
    state.enabled = !!enabled;
    if (!state.enabled) {
        state.stopMusic();
        for (const audio of state.activeSfx) {
            audio.pause();
        }
        state.activeSfx.clear();
    } else if (state.pendingMusicPath) {
        state.playMusic(state.pendingMusicPath);
    }
})

EM_JS(void, game_audio_web_start_music_impl, (const char *path), {
    const state = Module.__rbDarkRpgAudio;
    if (!state) {
        return;
    }
    state.playMusic(UTF8ToString(path));
})

EM_JS(void, game_audio_web_stop_music_impl, (void), {
    const state = Module.__rbDarkRpgAudio;
    if (!state) {
        return;
    }
    state.pendingMusicPath = "";
    state.stopMusic();
})

EM_JS(void, game_audio_web_play_sfx_impl, (const char *path, float volume), {
    const state = Module.__rbDarkRpgAudio;
    if (!state) {
        return;
    }
    state.playSfx(UTF8ToString(path), volume);
})

EM_JS(int, game_audio_web_music_active_impl, (void), {
    const state = Module.__rbDarkRpgAudio;
    return state && state.music && !state.music.paused ? 1 : 0;
})
#endif

#if defined(_WIN32) && !defined(NT_PLATFORM_WEB)
static void normalize_mci_path(char *out, size_t out_size, const char *path) {
    copy_string(out, out_size, path);
    for (size_t i = 0; out[i] != '\0'; ++i) {
        if (out[i] == '/') {
            out[i] = '\\';
        }
    }
}

static void mci_close_alias(const char *alias) {
    if (!alias || alias[0] == '\0') {
        return;
    }
    char command[96];
    (void)snprintf(command, sizeof(command), "stop %s", alias);
    (void)mciSendStringA(command, NULL, 0, NULL);
    (void)snprintf(command, sizeof(command), "close %s", alias);
    (void)mciSendStringA(command, NULL, 0, NULL);
}

static void cleanup_voice(AudioVoice *voice) {
    if (!voice->active) {
        return;
    }
    mci_close_alias(voice->alias);
    voice->alias[0] = '\0';
    voice->started_ms = 0U;
    voice->ttl_ms = 0U;
    voice->active = false;
    voice->music = false;
}

static void set_mci_volume(const char *alias, float volume) {
    if (!alias || alias[0] == '\0') {
        return;
    }
    const unsigned long mci_volume = (unsigned long)(clamp01(volume) * 1000.0F + 0.5F);
    char command[128];
    (void)snprintf(command, sizeof(command), "setaudio %s volume to %lu", alias, mci_volume);
    (void)mciSendStringA(command, NULL, 0, NULL);
}

static DWORD query_mci_length_ms(const char *alias) {
    char command[96];
    char result[64];
    result[0] = '\0';
    (void)snprintf(command, sizeof(command), "set %s time format milliseconds", alias);
    (void)mciSendStringA(command, NULL, 0, NULL);
    (void)snprintf(command, sizeof(command), "status %s length", alias);
    if (mciSendStringA(command, result, sizeof(result), NULL) != 0U) {
        return 0U;
    }
    const unsigned long value = strtoul(result, NULL, 10);
    return value > 0UL ? (DWORD)value : 0U;
}

static bool start_mci_voice(int voice_index, const char *path, bool music, float volume) {
    if (voice_index < 0 || voice_index >= GAME_AUDIO_VOICE_COUNT || !path || path[0] == '\0') {
        return false;
    }

    AudioVoice *voice = &s_voices[voice_index];
    cleanup_voice(voice);

    char mci_path[GAME_AUDIO_PATH_MAX];
    normalize_mci_path(mci_path, sizeof(mci_path), path);
    (void)snprintf(voice->alias, sizeof(voice->alias), "rbdr_%s_%d", music ? "music" : "sfx", voice_index);

    char command[GAME_AUDIO_PATH_MAX + 96];
    (void)snprintf(command, sizeof(command), "open \"%s\" type mpegvideo alias %s", mci_path, voice->alias);
    MCIERROR error = mciSendStringA(command, NULL, 0, NULL);
    if (error != 0U) {
        (void)snprintf(command, sizeof(command), "open \"%s\" alias %s", mci_path, voice->alias);
        error = mciSendStringA(command, NULL, 0, NULL);
    }
    if (error != 0U) {
        voice->alias[0] = '\0';
        return false;
    }

    set_mci_volume(voice->alias, volume);
    const DWORD length_ms = query_mci_length_ms(voice->alias);
    (void)snprintf(command, sizeof(command), "play %s from 0", voice->alias);
    if (mciSendStringA(command, NULL, 0, NULL) != 0U) {
        cleanup_voice(voice);
        return false;
    }

    voice->started_ms = timeGetTime();
    voice->ttl_ms = length_ms > 0U ? length_ms + 80U : GAME_AUDIO_DEFAULT_SFX_TTL_MS;
    voice->active = true;
    voice->music = music;
    return true;
}

static void update_expired_voices(void) {
    const DWORD now = timeGetTime();
    for (int i = 0; i < GAME_AUDIO_VOICE_COUNT; ++i) {
        AudioVoice *voice = &s_voices[i];
        if (voice->active && voice->ttl_ms > 0U && (DWORD)(now - voice->started_ms) >= voice->ttl_ms) {
            cleanup_voice(voice);
        }
    }
}

static void update_music_voice(void) {
    if (!s_music_requested || !s_initialized || !s_device_enabled || !s_music_asset.loaded) {
        return;
    }
    if (s_voices[GAME_AUDIO_MUSIC_VOICE].active) {
        return;
    }
    const float volume = clamp01(s_master_volume * s_music_volume);
    if (volume <= 0.001F) {
        return;
    }
    (void)start_mci_voice(GAME_AUDIO_MUSIC_VOICE, s_music_asset.path, true, volume);
}
#else
static void cleanup_voice(AudioVoice *voice) {
    voice->active = false;
    voice->music = false;
}
#endif

static void cleanup_all_voices(void) {
    for (int i = 0; i < GAME_AUDIO_VOICE_COUNT; ++i) {
        cleanup_voice(&s_voices[i]);
    }
}

void game_audio_init(void) {
    cleanup_all_voices();
    memset(s_voices, 0, sizeof(s_voices));
    s_master_volume = 0.80F;
    s_music_volume = 0.45F;
    s_sfx_volume = 0.90F;
    s_device_enabled = true;
    s_music_requested = false;
    s_total_play_count = 0;
    s_music_play_count = 0;
    memset(s_cue_play_count, 0, sizeof(s_cue_play_count));
#if defined(__EMSCRIPTEN__)
    game_audio_web_init_impl();
#endif
    load_audio_assets();
    s_initialized = true;
}

void game_audio_shutdown(void) {
#if defined(__EMSCRIPTEN__)
    game_audio_web_shutdown_impl();
#endif
    cleanup_all_voices();
    unload_audio_assets();
    s_initialized = false;
}

void game_audio_set_volume(float master_volume, float music_volume, float sfx_volume) {
    s_master_volume = clamp01(master_volume);
    s_music_volume = clamp01(music_volume);
    s_sfx_volume = clamp01(sfx_volume);
#if defined(_WIN32) && !defined(NT_PLATFORM_WEB)
    if (s_voices[GAME_AUDIO_MUSIC_VOICE].active) {
        set_mci_volume(s_voices[GAME_AUDIO_MUSIC_VOICE].alias, clamp01(s_master_volume * s_music_volume));
    }
#endif
#if defined(__EMSCRIPTEN__)
    game_audio_web_set_volume_impl(s_master_volume, s_music_volume, s_sfx_volume);
#endif
}

void game_audio_set_device_enabled(bool enabled) {
    s_device_enabled = enabled;
    if (!enabled) {
        cleanup_all_voices();
    }
#if defined(__EMSCRIPTEN__)
    game_audio_web_set_enabled_impl(enabled ? 1 : 0);
#endif
}

void game_audio_start_music(void) {
    if (!s_music_requested) {
        s_music_play_count++;
    }
    s_music_requested = true;
#if defined(__EMSCRIPTEN__)
    if (s_initialized && s_device_enabled && s_music_asset.loaded) {
        game_audio_web_start_music_impl(s_music_asset.path);
    }
#endif
#if defined(_WIN32) && !defined(NT_PLATFORM_WEB)
    update_music_voice();
#endif
}

void game_audio_stop_music(void) {
    s_music_requested = false;
    cleanup_voice(&s_voices[GAME_AUDIO_MUSIC_VOICE]);
#if defined(__EMSCRIPTEN__)
    game_audio_web_stop_music_impl();
#endif
}

void game_audio_update(void) {
#if defined(_WIN32) && !defined(NT_PLATFORM_WEB)
    update_expired_voices();
    update_music_voice();
#endif
}

void game_audio_play(GameAudioCue cue) {
    if (cue < 0 || cue >= GAME_AUDIO_CUE_COUNT) {
        return;
    }
    s_total_play_count++;
    s_cue_play_count[cue]++;

    if (!s_initialized || !s_device_enabled || !s_cue_assets[cue].loaded) {
        return;
    }
    const float volume = clamp01(s_master_volume * s_sfx_volume);
    if (volume <= 0.001F) {
        return;
    }

#if defined(__EMSCRIPTEN__)
    game_audio_web_play_sfx_impl(s_cue_assets[cue].path, volume);
#endif
#if defined(_WIN32) && !defined(NT_PLATFORM_WEB)
    game_audio_update();
    int voice_index = -1;
    for (int i = GAME_AUDIO_FIRST_SFX_VOICE; i < GAME_AUDIO_VOICE_COUNT; ++i) {
        if (!s_voices[i].active) {
            voice_index = i;
            break;
        }
    }
    if (voice_index < 0) {
        voice_index = GAME_AUDIO_FIRST_SFX_VOICE;
    }
    (void)start_mci_voice(voice_index, s_cue_assets[cue].path, false, volume);
#endif
}

GameAudioStatus game_audio_status(void) {
    GameAudioStatus status;
    memset(&status, 0, sizeof(status));
#if defined(__EMSCRIPTEN__)
    status.implemented = true;
    status.backend = "web-html-audio-mp3-assets";
#elif defined(_WIN32) && !defined(NT_PLATFORM_WEB)
    status.implemented = true;
    status.backend = "winmm-mci-mp3-assets";
#else
    status.implemented = false;
    status.backend = (s_loaded_clip_count > 0 || s_music_asset.loaded) ? "noop-mp3-assets" : "noop";
#endif
    status.initialized = s_initialized;
    status.device_enabled = s_device_enabled;
    status.music_requested = s_music_requested;
#if defined(__EMSCRIPTEN__)
    status.music_active = game_audio_web_music_active_impl() != 0;
#else
    status.music_active = s_voices[GAME_AUDIO_MUSIC_VOICE].active;
#endif
    status.music_asset_loaded = s_music_asset.loaded;
    status.asset_root = GAME_AUDIO_ASSET_DIR;
    status.master_volume = s_master_volume;
    status.music_volume = s_music_volume;
    status.sfx_volume = s_sfx_volume;
    status.loaded_clip_count = s_loaded_clip_count;
    status.total_play_count = s_total_play_count;
    status.music_play_count = s_music_play_count;
    for (int i = 0; i < GAME_AUDIO_CUE_COUNT; ++i) {
        status.cue_play_count[i] = s_cue_play_count[i];
    }
    return status;
}
