#include "shell/shell.h"

#include "app/nt_app.h"
#include "features/game_features.h"
#include "graphics/nt_gfx.h"
#include "render/blank_renderer.h"

/* Set when the context came back, cleared by the first frame drawn after it:
   the web shell's recovery overlay must not lift before a restored frame. */
static bool s_gfx_recovery_pending;

shell_shown_t shell_present(World *world, const game_input_frame_t *input) {
    if (g_nt_gfx.context_lost) {
        nt_gfx_end_frame();
        return (shell_shown_t){0};
    }
    if (g_nt_gfx.context_restored) {
        s_gfx_recovery_pending = true;
    }
    const bool render_enabled = nt_app_render_enabled();
    if (render_enabled) {
        blank_renderer_begin();
        game_features_draw_world(world);
        game_features_draw_ui(world, input);
        blank_renderer_end();
    }
    nt_gfx_end_frame();
    if (!render_enabled) {
        return (shell_shown_t){0};
    }
    const bool recovered = s_gfx_recovery_pending;
    s_gfx_recovery_pending = false;
    return (shell_shown_t){.presented = true, .complete = true, .recovered = recovered};
}
