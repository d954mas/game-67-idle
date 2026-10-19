#include "shell/shell.h"

#include "app/nt_app.h"
#include "graphics/nt_gfx.h"
#include "nt_pack_format.h"
#include "window/nt_window.h"

#include "features/game_features.h"
#include "game_scenes.h"
#include "render/capture.h"
#include "render/render_mesh.h"
#include "render/shader_programs.h"
#include "ui/hud.h"
#include "ui/ui_runtime.h"

static const shell_options_t *s_options;
static int s_frame_count;
/* Set when the GPU objects were rebuilt, cleared when a rebuilt frame reached
   the screen: the shell's "restoring" overlay must not lift a frame early. */
static bool s_gfx_recovery_pending;

void shell_present_init(const shell_options_t *options) { s_options = options; }

void shell_present_upload(void) {
    /* While the context is lost every GPU handle is dead; uploading against one
       is what turns a recoverable loss into a crash. */
    if (!g_nt_gfx.context_lost) {
        nt_resource_step();
        shader_programs_update();
    }
}

/* Every GPU object is rebuilt here; the resource slots publish their
   invalidation on the next top-of-frame resource step, so this frame must not
   touch their stale handles and draws nothing. */
static void restore_gpu(void) {
    nt_resource_invalidate(NT_ASSET_SHADER_CODE);
    nt_resource_invalidate(NT_ASSET_FONT);
    nt_resource_invalidate(NT_ASSET_MESH);
    nt_resource_invalidate(NT_ASSET_TEXTURE);
    nt_font_step();
    shader_programs_restore_gpu();
    ui_runtime_restore_gpu();
    s_gfx_recovery_pending = true;
}

shell_shown_t shell_present(World *world, const game_input_frame_t *input, shell_tick_status_t tick) {
    if (g_nt_gfx.context_lost) {
        nt_gfx_end_frame();
        return (shell_shown_t){0};
    }
    if (g_nt_gfx.context_restored) {
        restore_gpu();
        nt_gfx_end_frame();
        return (shell_shown_t){.presented = true};
    }
    const bool gfx_recovery_frame_ready =
        s_gfx_recovery_pending && tick.playable && !g_nt_gfx.context_lost;

    // The engine's draw-pass gate (devapi render.set_enabled). A bot advancing a
    // run to a world state pays for frames nobody looks at, and the drawing is
    // the larger half of such a tick. The frame still begins, ends and presents
    // with the gate off, so the GL context and the devapi loop are untouched.
    // The UI is immediate mode: while the gate is off no widget exists to be
    // hit, so anything that clicks or photographs must turn rendering back on
    // and let a frame pass.
    nt_font_step();
    if (nt_app_render_enabled()) {
        // render systems read the world
        nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.50F, 0.75F, 0.96F, 1.0F}, .clear_depth = 1.0F});
        if (game_scenes_should_render_world()) {
            // draw_world stays a direct shell call until render systems become features.
            render_mesh_draw(world);
            const shell_text_t *text = shell_boot_text();
            hud_draw(text->material, text->font_resource, text->font);
        }
        if (tick.ready) {
            game_features_draw_ui(world, input);
        }
        nt_gfx_end_pass();
    }
    nt_gfx_end_frame();

    // --capture: after a few frames (resources loaded + rendered), grab + quit.
    if (tick.ready) {
        s_frame_count += 1;
    }
    if (s_options->capture && tick.ready && s_frame_count >= 10) {
        capture_write_ppm(s_options->capture_path);
        nt_app_quit();
    }

    const bool recovered = gfx_recovery_frame_ready;
    if (recovered) {
        s_gfx_recovery_pending = false;
    }
    return (shell_shown_t){.presented = true, .complete = true, .recovered = recovered};
}
