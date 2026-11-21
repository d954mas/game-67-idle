#include <math.h>

#include "color/nt_color.h"
#include "graphics/nt_gfx.h"
#include "math/nt_math.h"
#include "renderers/nt_shape_renderer.h"
#include "stand_view.h"

/* Debug shapes: the colour says what a body is, so a glance tells copies from local bodies and
   shows a carry, a throw or a knockdown arriving. Characters get a white outline, items a dark
   one, static bodies a light one; a floor shows a metre grid so the scene has a ground. */

#define WIRE_PX 2.0f
#define GRID_LIFT 0.005f /* above the floor's top face, so the lines do not fight its depth */

static uint32_t darker(uint32_t c) {
    uint32_t r = (c & 0xFFu) * 55u / 100u, g = ((c >> 8) & 0xFFu) * 55u / 100u, b = ((c >> 16) & 0xFFu) * 55u / 100u;
    return (c & 0xFF000000u) | (b << 16) | (g << 8) | r;
}

static uint32_t colour_of(const body_sync_body_view_t *b) {
    uint32_t c;
    if (b->flags & BODY_SYNC_VIEW_STATIC) {
        return NT_RGBA8(70, 72, 82, 255);
    } else if (b->flags & BODY_SYNC_VIEW_KNOCKED) {
        c = NT_RGBA8(240, 130, 40, 255);
    } else if (b->flags & BODY_SYNC_VIEW_HELD) {
        c = NT_RGBA8(240, 214, 60, 255);
    } else if (b->flags & BODY_SYNC_VIEW_FLYING) {
        c = NT_RGBA8(222, 80, 222, 255);
    } else if (b->flags & BODY_SYNC_VIEW_COPY) {
        c = NT_RGBA8(70, 130, 225, 255);
    } else {
        c = NT_RGBA8(80, 195, 95, 255);
    }
    return (b->flags & BODY_SYNC_VIEW_ASLEEP) ? darker(c) : c;
}

static void shape(const body_sync_body_view_t *b, bool wire, uint32_t colour) {
    float size[3] = {2.0f * b->size[0], 2.0f * b->size[1], 2.0f * b->size[2]};
    float capsule_height = 2.0f * (b->size[0] + b->size[1]);
    switch (b->shape) {
    case BODY_SYNC_SHAPE_BOX:
        if (wire) {
            nt_shape_renderer_cube_wire(b->position, size, b->rotation, WIRE_PX, colour);
        } else {
            nt_shape_renderer_cube(b->position, size, b->rotation, colour);
        }
        break;
    case BODY_SYNC_SHAPE_SPHERE:
        if (wire) {
            nt_shape_renderer_sphere_wire(b->position, b->size[0], b->rotation, WIRE_PX, colour);
        } else {
            nt_shape_renderer_sphere(b->position, b->size[0], b->rotation, colour);
        }
        break;
    case BODY_SYNC_SHAPE_CAPSULE:
        if (wire) {
            nt_shape_renderer_capsule_wire(b->position, b->size[0], capsule_height, b->rotation, WIRE_PX, colour);
        } else {
            nt_shape_renderer_capsule(b->position, b->size[0], capsule_height, b->rotation, colour);
        }
        break;
    }
}

/* A line every metre over a static box's top face, a heavier one every five. Only for an
   unrotated box wider than a room: a floor. */
static void grid(const body_sync_body_view_t *b) {
    const float *q = b->rotation;
    if (b->shape != BODY_SYNC_SHAPE_BOX || b->size[0] < 4.0f || b->size[2] < 4.0f || fabsf(q[3]) < 0.9999f) {
        return;
    }
    float y = b->position[1] + b->size[1] + GRID_LIFT;
    float x0 = b->position[0] - b->size[0], x1 = b->position[0] + b->size[0];
    float z0 = b->position[2] - b->size[2], z1 = b->position[2] + b->size[2];
    for (float x = ceilf(x0); x <= x1; x += 1.0f) {
        bool heavy = fmodf(fabsf(x), 5.0f) < 0.5f;
        const float a[3] = {x, y, z0}, c[3] = {x, y, z1};
        nt_shape_renderer_line(a, c, heavy ? 2.0f : 1.0f, heavy ? NT_RGBA8(130, 132, 146, 255) : NT_RGBA8(96, 98, 110, 255));
    }
    for (float z = ceilf(z0); z <= z1; z += 1.0f) {
        bool heavy = fmodf(fabsf(z), 5.0f) < 0.5f;
        const float a[3] = {x0, y, z}, c[3] = {x1, y, z};
        nt_shape_renderer_line(a, c, heavy ? 2.0f : 1.0f, heavy ? NT_RGBA8(130, 132, 146, 255) : NT_RGBA8(96, 98, 110, 255));
    }
}

static void configure_debug(nt_gfx_desc_t *desc, void *ctx) {
    (void)ctx;
    desc->frame_capacity[NT_GFX_FRAME_VERTEX] = 256U * 1024U; /* a few hundred wire shapes and the grid */
}

static bool init_debug(void *ctx) {
    (void)ctx;
    nt_shape_renderer_init();
    return true;
}

static void shutdown_debug(void *ctx) {
    (void)ctx;
    nt_shape_renderer_shutdown();
}

static void begin_debug(const body_sync_view_frame_t *frame, void *ctx) {
    (void)ctx;
    mat4 view, projection, vp;
    glm_mat4_make(frame->view, view);
    glm_mat4_make(frame->projection, projection);
    glm_mat4_mul(projection, view, vp);
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.06f, 0.06f, 0.08f, 1.0f}, .clear_depth = 1.0f});
    nt_shape_renderer_set_vp((float *)vp);
    nt_shape_renderer_set_depth(true);
    nt_shape_renderer_set_width_units_pixels(frame->width, frame->height);
}

static void draw_debug(const body_sync_body_view_t *b, bool ghost, void *ctx) {
    (void)ctx;
    if (ghost) {
        if (!(b->flags & BODY_SYNC_VIEW_STATIC)) {
            shape(b, true, NT_RGBA8(255, 70, 70, 255));
        }
        return;
    }
    shape(b, false, colour_of(b));
    if (b->flags & BODY_SYNC_VIEW_STATIC) {
        shape(b, true, NT_RGBA8(150, 152, 166, 255));
        grid(b);
    } else if (b->flags & BODY_SYNC_VIEW_CHARACTER) {
        shape(b, true, NT_RGBA8(255, 255, 255, 255));
    } else {
        shape(b, true, NT_RGBA8(20, 20, 26, 255));
    }
}

static void end_debug(const body_sync_view_frame_t *frame, void *ctx) {
    (void)frame;
    (void)ctx;
    nt_shape_renderer_flush();
    nt_gfx_end_pass();
}

const body_sync_view_drawer_t body_sync_view_debug_drawer = {
    .configure_gfx = configure_debug,
    .init = init_debug,
    .ready = NULL,
    .begin_frame = begin_debug,
    .draw_body = draw_debug,
    .end_frame = end_debug,
    .shutdown = shutdown_debug,
    .ctx = NULL,
};
