#include "render/render_2d_runtime.h"

#include "renderers/nt_sprite_renderer.h"

void render_2d_runtime_init(void) {
    nt_sprite_renderer_desc_t desc = nt_sprite_renderer_desc_defaults();
    nt_sprite_renderer_init(&desc);
}

void render_2d_runtime_restore_gpu(void) {
    nt_sprite_renderer_restore_gpu();
}

void render_2d_runtime_shutdown(void) {
    nt_sprite_renderer_shutdown();
}
