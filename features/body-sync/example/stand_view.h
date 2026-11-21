#ifndef BODY_SYNC_STAND_VIEW_H
#define BODY_SYNC_STAND_VIEW_H

/* The stand in an engine window: one endpoint's world, a camera that frames the scene or one
   the drawer places, and an offline recording of one frame per tick. What a body looks like is the
   drawer's: this example ships debug shapes, and a game passes a drawer with its models.

   The viewer owns the window, the stepping and the present, and proposes the camera, which
   the drawer may move. Start: configure_gfx
   edits the renderer's description, the viewer starts the renderer, init runs, then ready is
   polled once a frame; until it answers true the viewer only clears and presents (no frame
   callback runs), the stand does not tick, no recording starts and --shot-tick does not count
   (at most 30 s, then the run is refused).

   Each drawn frame: camera adjusts the proposal; the viewer begins the engine's graphics frame; begin_frame begins the drawer's
   passes, and the first of them clears colour and depth; draw_body runs for every body of the
   viewed endpoint, then for the host's bodies with `ghost` set when the ghost is on; end_frame
   ends the drawer's passes; the viewer ends the graphics frame, takes the screenshot and the
   recording frame from it, and presents. Without begin_frame the viewer begins one clearing
   pass and ends it after the last draw_body, before end_frame. */

#include <stdbool.h>
#include <stdint.h>

#include "features/body_sync/body_sync_stand.h"
#include "graphics/nt_gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Built from the camera the drawer left. */
typedef struct body_sync_view_frame {
    float view[16];       /* world to eye, laid out as the engine's matrices */
    float projection[16];
    float eye[3];
    float target[3];      /* the point the camera looks at */
    float fov_y_deg;      /* vertical field of view */
    float near_m, far_m;
    uint32_t width, height; /* framebuffer pixels */
    uint32_t endpoint;      /* whose world is drawn */
    uint32_t tick;
    bool ghost;             /* the host's outlines are drawn this frame */
} body_sync_view_frame_t;

typedef struct body_sync_view_camera {
    float eye[3], target[3];
    float fov_y_deg, near_m, far_m; /* fov in (1, 179), 0 < near < far */
} body_sync_view_camera_t;

/* Every call but draw_body may be NULL. */
typedef struct body_sync_view_drawer {
    void (*configure_gfx)(nt_gfx_desc_t *desc, void *ctx); /* before the renderer starts */
    bool (*init)(void *ctx); /* once the window and renderer are up; false refuses the run */
    bool (*ready)(void *ctx); /* NULL: ready at once */
    /* Before each frame: the viewer's proposal (its framing of the scene, or --camera). The
       drawer may change it, for example to follow the viewed endpoint's own character as the
       game's camera does. `own` is that character's view, NULL for an endpoint without one.
       `dt_s` is the simulated time since the previous frame: 0 when no tick ran (paused, a
       redraw), one tick's 1/60 s, or more when a late frame ran several. A camera that breaks
       the ranges above is replaced by the proposal, with one warning. */
    void (*camera)(body_sync_view_camera_t *camera, const body_sync_body_view_t *own, uint32_t endpoint, float dt_s, void *ctx);
    void (*begin_frame)(const body_sync_view_frame_t *frame, void *ctx);
    void (*draw_body)(const body_sync_body_view_t *body, bool ghost, void *ctx);
    void (*end_frame)(const body_sync_view_frame_t *frame, void *ctx);
    void (*shutdown)(void *ctx); /* before the renderer goes down; runs whenever init was called, also after init answered false */
    void *ctx;
} body_sync_view_drawer_t;

/* Debug shapes coloured by what a body is, on a floor grid (see the feature README). */
extern const body_sync_view_drawer_t body_sync_view_debug_drawer;

/* Exit codes beyond the command line's 0 (checks passed), 1 (a check failed), 2 (refused). */
enum {
    BODY_SYNC_VIEW_EXIT_MEDIA = 3, /* the recording or the screenshot could not be written */
    BODY_SYNC_VIEW_EXIT_CUT = 4,   /* cut short (--frames, Esc, the window closed): no checks */
};

/* body_sync_stand_view <scenario> [--view host|g1|g2|g3] [--ghost] [--seed N] [--frames N]
                        [--size WxH] [--camera ex,ey,ez,tx,ty,tz[,fov]] [--record file.mp4] [--shot file.png]
                        [--shot-tick N] [--hash-every N]
   Scenes are the catalog's and then `extra`'s, as for body_sync_stand_main. */
int body_sync_view_main(int argc, char **argv, const body_sync_scene_entry_t *extra, uint32_t extra_count, const body_sync_view_drawer_t *drawer);

#ifdef __cplusplus
}
#endif

#endif
