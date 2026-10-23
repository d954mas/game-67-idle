#include "game_music_pack.h"

#if GAME_MUSIC_PACK_ENABLED
#include "game_asset_paths.h"

static nt_hash32_t s_pack;
static nt_pack_state_t s_state;
static bool s_initialized, s_presented, s_mounted, s_wanted;

void game_music_pack_init(void) {
    game_music_pack_shutdown();
    s_pack = nt_hash32_str("music");
    s_initialized = true;
}

void game_music_pack_after_present(void) { s_presented = true; }

void game_music_pack_update(bool wanted) {
    if (!s_initialized) { return; }
    if (s_mounted && s_state != NT_PACK_STATE_FAILED) { s_state = nt_resource_pack_state(s_pack); }
    const bool requested_again = wanted && !s_wanted;
    s_wanted = wanted;
    if (!s_presented || !wanted) { return; }
    if (s_state == NT_PACK_STATE_FAILED) {
        if (!requested_again) { return; }
        if (s_mounted) { nt_resource_unmount(s_pack); }
        s_mounted = false;
    }
    if (!s_mounted) {
        char path[GAME_ASSET_PATH_MAX];
        if (!game_asset_paths_resolve("assets/music.ntpack", path, sizeof path) ||
            nt_resource_mount(s_pack, 100) != NT_OK) {
            s_state = NT_PACK_STATE_FAILED;
            return;
        }
        s_mounted = true;
        if (nt_resource_load_auto(s_pack, path) != NT_OK) {
            s_state = NT_PACK_STATE_FAILED;
            return;
        }
    }
    s_state = nt_resource_pack_state(s_pack);
}

nt_pack_state_t game_music_pack_state(void) { return s_state; }

void game_music_pack_shutdown(void) {
    if (s_mounted) { nt_resource_unmount(s_pack); }
    s_initialized = s_presented = s_mounted = s_wanted = false;
    s_state = NT_PACK_STATE_NONE;
}
#else
void game_music_pack_init(void) {}
void game_music_pack_after_present(void) {}
void game_music_pack_update(bool wanted) { (void)wanted; }
nt_pack_state_t game_music_pack_state(void) { return NT_PACK_STATE_NONE; }
void game_music_pack_shutdown(void) {}
#endif
