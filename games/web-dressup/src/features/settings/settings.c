#include "features/settings/settings.h"

#include "game_save.h"      /* game_save_mark_dirty */
#include "settings_state.h" /* generated: SettingsState + settings_state instance */

static float clamp01(float v) { return v < 0.0F ? 0.0F : (v > 1.0F ? 1.0F : v); }

float settings_master(void) { return settings_state.master_volume; }
float settings_music(void)  { return settings_state.music_volume; }
float settings_sfx(void)    { return settings_state.sfx_volume; }

void settings_set_master(float value) {
    settings_state.master_volume = clamp01(value);
    game_save_mark_dirty();
    /* apply seam: a real game pushes master/sfx to its audio mixer here. */
}
void settings_set_music(float value) {
    settings_state.music_volume = clamp01(value);
    game_save_mark_dirty();
}
void settings_set_sfx(float value) {
    settings_state.sfx_volume = clamp01(value);
    game_save_mark_dirty();
}
