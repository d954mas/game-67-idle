#include "render/blank_renderer.h"

#include "graphics/nt_gfx.h"
void blank_renderer_begin(void) {
    nt_gfx_begin_pass(&(nt_pass_desc_t){
        .clear_color = {0.06F, 0.08F, 0.11F, 1.0F},
        .clear_depth = 1.0F,
    });
}

void blank_renderer_end(void) {
    nt_gfx_end_pass();
}
