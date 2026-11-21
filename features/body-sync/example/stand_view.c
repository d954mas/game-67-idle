#define _CRT_SECURE_NO_WARNINGS
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/nt_app.h"
#include "core/nt_core.h"
#include "graphics/nt_gfx.h"
#include "input/nt_input.h"
#include "math/nt_math.h"
#include "time/nt_time.h"
#include "window/nt_window.h"
#ifdef BODY_SYNC_VIEW_RECORDER
#include "fpng/nt_fpng.h"
#include "recorder/nt_recorder.h"
#endif
#include "stand_view.h"

/* The stand's clock is its own: a tick's pixels never depend on the wall. Recording runs one
   tick per frame, as fast as the encoder takes frames. Viewing runs the ticks the wall clock
   has reached, 60 a second, several in one frame when a frame came late, and draws the last;
   a tick that --hash-every or --shot asks for ends its frame's run, so it is drawn and read
   as in a recording. Vsync is off: a swap that waits on the display (an occluded window waits
   far longer than a refresh) would set the pace instead of the clock. */

enum { WINDOW_W = 800, WINDOW_H = 450, STATUS_EVERY = 60, CATCH_UP_MAX = 60 };
#define STEP (1.0f / 60.0f)
#define IDLE_DT (1.0f / 240.0f) /* the cap on a frame's rate while viewing or waiting for ready */
#define READY_WAIT_S 30.0
#define FOV_Y_DEG 50.0f
#define NEAR_M 0.1f
#define CLI_FAR_MIN_M 200.0f

typedef struct view {
    const body_sync_view_drawer_t *drawer;
    body_sync_scene_t *scene;
    body_sync_scenario_t *scenario;
    body_sync_stand_t *stand;
    uint64_t seed;
    bool override_seed;
    uint32_t endpoint;
    bool ghost;
    bool paused;
    uint32_t frames_max; /* 0: the scenario's own end */
    uint32_t ticks;
    bool ready;
    double ready_deadline;
    bool refused; /* the drawer was never ready, or a restart failed */
    bool ended;   /* the scenario reached its end: the checks are evaluated */
    bool done;
    bool redraw; /* the drawn picture changed without a tick: a switch, a pause, a restart */
    /* viewing's pace: ticks run since the anchor */
    double pace_anchor;
    uint32_t pace_done;
    double paced_from; /* the first tick's present: its frame carries the drawer's first-use cost */
    uint32_t paced_ticks; /* since paced_from */
    const char *record_path;
    bool record_started;
    bool recording;
    bool record_failed;
    uint32_t recorded_frames;
    const char *shot_path;
    uint32_t shot_tick;
    bool shot_taken;
    bool shot_failed;
    uint32_t hash_every;
    bool cli_camera_set;
    body_sync_view_camera_t cli_camera;
    bool camera_warned;
    bool framed;
    float centre[3];
    float radius;
    uint8_t *rgba;
    size_t rgba_size;
} view_t;

static view_t s_view;

static int usage(void) {
    fprintf(stderr, "usage: body_sync_stand_view <scenario.scn> [--view host|g1|g2|g3] [--ghost] [--seed N] [--frames N]\n"
                    "                            [--camera ex,ey,ez,tx,ty,tz[,fov]] [--record file.mp4] [--shot file.png]\n"
                    "                            [--shot-tick N] [--hash-every N]\n");
    return 2;
}

static int refuse(const char *arg, const char *value, const char *why) {
    if (value) {
        fprintf(stderr, "body_sync_stand_view: %s '%s': %s\n", arg, value, why);
    } else {
        fprintf(stderr, "body_sync_stand_view: %s: %s\n", arg, why);
    }
    return usage();
}

static bool parse_u64(const char *text, uint64_t *out) {
    uint64_t v = 0;
    if (!text || !*text) {
        return false;
    }
    for (; *text; text++) {
        if (*text < '0' || *text > '9' || v > (UINT64_MAX - (uint64_t)(*text - '0')) / 10u) {
            return false;
        }
        v = v * 10u + (uint64_t)(*text - '0');
    }
    *out = v;
    return true;
}

static bool create_stand(view_t *v, body_sync_error_t *error) {
    body_sync_stand_config_t config = {.scene = v->scene, .scenario = v->scenario, .override_seed = v->override_seed, .seed = v->seed};
    v->stand = body_sync_stand_create(&config, error);
    v->ticks = 0;
    v->framed = false;
    return v->stand != NULL;
}

/* Frames what the scene holds, not its bounds' empty margin: every body but the ones larger
   than a room (a floor) at the first frame, so no input is needed to see it all. */
static void frame_scene(view_t *v) {
    float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
    uint32_t n = body_sync_stand_body_count(v->stand, v->endpoint);
    for (uint32_t i = 0; i < n; i++) {
        body_sync_body_view_t b;
        if (!body_sync_stand_body(v->stand, v->endpoint, i, &b)) {
            continue;
        }
        float reach = b.shape == BODY_SYNC_SHAPE_BOX ? sqrtf(b.size[0] * b.size[0] + b.size[1] * b.size[1] + b.size[2] * b.size[2])
                                                     : b.size[0] + b.size[1];
        if (reach > 8.0f) {
            continue;
        }
        for (int k = 0; k < 3; k++) {
            lo[k] = fminf(lo[k], b.position[k] - reach);
            hi[k] = fmaxf(hi[k], b.position[k] + reach);
        }
    }
    if (lo[0] > hi[0]) {
        lo[0] = lo[1] = lo[2] = -5.0f;
        hi[0] = hi[1] = hi[2] = 5.0f;
    }
    float r = 0.0f;
    for (int k = 0; k < 3; k++) {
        v->centre[k] = 0.5f * (lo[k] + hi[k]);
        r += (hi[k] - lo[k]) * (hi[k] - lo[k]);
    }
    v->radius = 0.5f * sqrtf(r) + 0.5f;
    v->framed = true;
}

static bool camera_ok(const body_sync_view_camera_t *c) {
    float d2 = 0.0f;
    for (int k = 0; k < 3; k++) {
        if (!isfinite(c->eye[k]) || !isfinite(c->target[k])) {
            return false;
        }
        d2 += (c->target[k] - c->eye[k]) * (c->target[k] - c->eye[k]);
    }
    return d2 > 1e-8f && c->fov_y_deg > 1.0f && c->fov_y_deg < 179.0f && c->near_m > 0.0f && isfinite(c->far_m) && c->far_m > c->near_m;
}

/* The proposal: --camera, or the framing, which looks down at the scene's bodies from outside. */
static void propose_camera(const view_t *v, body_sync_view_camera_t *c) {
    if (v->cli_camera_set) {
        *c = v->cli_camera;
        return;
    }
    float distance = v->radius / tanf(0.5f * glm_rad(FOV_Y_DEG)); /* the sphere around the scene fills the height */
    for (int k = 0; k < 3; k++) {
        c->eye[k] = v->centre[k];
        c->target[k] = v->centre[k];
    }
    c->eye[1] += distance * 0.6f;
    c->eye[2] += distance * 0.8f;
    c->fov_y_deg = FOV_Y_DEG;
    c->near_m = NEAR_M;
    c->far_m = distance * 4.0f;
}

/* The viewed endpoint's own character, the one it simulates. */
static bool own_character(const view_t *v, body_sync_body_view_t *out) {
    uint32_t n = body_sync_stand_body_count(v->stand, v->endpoint);
    for (uint32_t i = 0; i < n; i++) {
        if (body_sync_stand_body(v->stand, v->endpoint, i, out) && (out->flags & BODY_SYNC_VIEW_CHARACTER) && out->player == v->endpoint) {
            return true;
        }
    }
    return false;
}

static void camera(view_t *v, uint32_t w, uint32_t h, body_sync_view_frame_t *f) {
    body_sync_view_camera_t proposal, c;
    propose_camera(v, &proposal);
    c = proposal;
    if (v->drawer->camera) {
        body_sync_body_view_t own;
        v->drawer->camera(&c, own_character(v, &own) ? &own : NULL, v->endpoint, v->drawer->ctx);
        if (!camera_ok(&c)) {
            if (!v->camera_warned) {
                fprintf(stderr, "body_sync_stand_view: the drawer's camera is out of range; the proposal is used\n");
                v->camera_warned = true;
            }
            c = proposal;
        }
    }
    memset(f, 0, sizeof(*f));
    vec3 eye = {c.eye[0], c.eye[1], c.eye[2]};
    vec3 target = {c.target[0], c.target[1], c.target[2]};
    vec3 forward;
    glm_vec3_sub(target, eye, forward);
    glm_vec3_normalize(forward);
    /* Straight down or up, +Y cannot be the up vector: the screen's up is then -Z. */
    vec3 up = {0.0f, 1.0f, 0.0f};
    if (fabsf(forward[1]) > 0.999f) {
        up[1] = 0.0f;
        up[2] = -1.0f;
    }
    mat4 view_m, proj;
    glm_lookat(eye, target, up, view_m);
    glm_perspective(glm_rad(c.fov_y_deg), h > 0 ? (float)w / (float)h : 1.0f, c.near_m, c.far_m, proj);
    memcpy(f->view, view_m, sizeof(f->view));
    memcpy(f->projection, proj, sizeof(f->projection));
    memcpy(f->eye, c.eye, sizeof(f->eye));
    memcpy(f->target, c.target, sizeof(f->target));
    f->fov_y_deg = c.fov_y_deg;
    f->near_m = c.near_m;
    f->far_m = c.far_m;
    f->width = w;
    f->height = h;
    f->endpoint = v->endpoint;
    f->tick = body_sync_stand_now(v->stand);
    f->ghost = v->ghost && v->endpoint != 0;
}

static void draw_endpoint(const view_t *v, uint32_t endpoint, bool ghost) {
    uint32_t n = body_sync_stand_body_count(v->stand, endpoint);
    for (uint32_t i = 0; i < n; i++) {
        body_sync_body_view_t b;
        if (body_sync_stand_body(v->stand, endpoint, i, &b)) {
            v->drawer->draw_body(&b, ghost, v->drawer->ctx);
        }
    }
}

static void clear_pass(void) { nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.06f, 0.06f, 0.08f, 1.0f}, .clear_depth = 1.0f}); }

static uint64_t fnv(const uint8_t *p, size_t n) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < n; i++) {
        h = (h ^ p[i]) * 0x100000001B3ull;
    }
    return h;
}

/* Reads the finished frame, between end_frame and the swap, as the engine requires. */
static bool read_frame(view_t *v, int w, int h) {
    size_t need = (size_t)w * (size_t)h * 4u;
    if (need > v->rgba_size) {
        free(v->rgba);
        v->rgba = malloc(need);
        v->rgba_size = v->rgba ? need : 0;
    }
    return v->rgba && nt_gfx_read_pixels((nt_render_target_t){0}, 0, 0, w, h, v->rgba, (uint32_t)need);
}

#ifdef BODY_SYNC_VIEW_RECORDER
static bool save_png(view_t *v, int w, int h, const char *path) {
    size_t pixels = (size_t)w * (size_t)h;
    uint8_t *rgb = malloc(pixels * 3u);
    size_t cap = pixels * 4u + 4096u;
    uint8_t *png = malloc(cap);
    bool ok = rgb && png;
    for (size_t i = 0; ok && i < pixels; i++) {
        memcpy(rgb + i * 3u, v->rgba + i * 4u, 3);
    }
    uint32_t size = ok ? nt_fpng_encode_rgb(rgb, (uint32_t)w, (uint32_t)h, png, (uint32_t)cap) : 0;
    FILE *file = size ? fopen(path, "wb") : NULL;
    ok = file && fwrite(png, 1, size, file) == size;
    if (file) {
        ok = fclose(file) == 0 && ok;
    }
    free(rgb);
    free(png);
    return ok;
}
#endif

static void finish(view_t *v) {
#ifdef BODY_SYNC_VIEW_RECORDER
    if (v->recording) {
        nt_recorder_stats_t stats = nt_recorder_stop();
        v->recording = false;
        v->recorded_frames = stats.frames;
        v->record_failed = stats.frames == 0;
    }
#endif
    if (v->paced_ticks && !v->record_path) {
        double seconds = nt_time_now() - v->paced_from;
        printf("paced: %u ticks after the first in %.2f s of wall time, %.1f ticks/s\n", v->paced_ticks, seconds, seconds > 0.0 ? v->paced_ticks / seconds : 0.0);
    }
    v->done = true;
    nt_app_quit();
}

/* Starts the pace again from now, `done` ticks already run against it. */
static void repace(view_t *v, uint32_t done) {
    v->pace_anchor = nt_time_now();
    v->pace_done = done;
}

/* Ticks to run this frame: one while recording; while viewing, those the wall clock reached,
   so late frames are caught up and the pace holds 60 a second. A stall over CATCH_UP_MAX ticks
   (a dragged window) is dropped instead. */
static uint32_t ticks_due(view_t *v) {
    if (v->paused) {
        return 0;
    }
    if (v->record_path) {
        return 1;
    }
    uint32_t due = (uint32_t)((nt_time_now() - v->pace_anchor) / (double)STEP) + 1u;
    if (due <= v->pace_done) {
        return 0;
    }
    if (due - v->pace_done > CATCH_UP_MAX) {
        repace(v, 0);
        return 1;
    }
    return due - v->pace_done;
}

static bool capture_tick(const view_t *v) {
    return (v->hash_every && v->ticks % v->hash_every == 0) || (v->shot_path && !v->shot_taken && v->ticks == v->shot_tick);
}

/* One stand tick; false when the run is over. */
static bool tick_once(view_t *v) {
    bool more = body_sync_stand_tick(v->stand);
    v->ticks++;
    v->pace_done++;
    v->paced_ticks += v->ticks > 1 ? 1u : 0u;
    if (v->ticks % STATUS_EVERY == 0 && !v->record_path) {
        printf("tick %u\n", body_sync_stand_now(v->stand));
        fflush(stdout); /* a watcher sees progress through a pipe too */
    }
    if (!more) {
        v->ended = true;
    }
    if (!more || (v->frames_max && v->ticks >= v->frames_max)) {
        v->done = true;
    }
    return !v->done;
}

static void wait_ready(view_t *v) {
    nt_gfx_begin_frame();
    clear_pass();
    nt_gfx_end_pass();
    nt_gfx_end_frame();
    nt_window_swap_buffers();
    if (!v->drawer->ready || v->drawer->ready(v->drawer->ctx)) {
        v->ready = true;
        repace(v, 0);
        if (v->record_path) {
            g_nt_app.target_dt = 0.0f; /* a recording runs as fast as the encoder takes frames */
        }
    } else if (nt_time_now() > v->ready_deadline) {
        fprintf(stderr, "body_sync_stand_view: the drawer was not ready in %.0f s\n", READY_WAIT_S);
        v->refused = true;
        finish(v);
    }
}

static void frame(void) {
    view_t *v = &s_view;
    nt_window_poll();
    nt_input_poll();
    if (nt_window_should_close() || nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
        finish(v);
        return;
    }
    if (!v->ready) {
        wait_ready(v);
        return;
    }
    /* A recording is one view from tick 1 to the end: no pause, switch or restart in it. */
    if (!v->record_path) {
        if (nt_input_key_is_pressed(NT_KEY_SPACE)) {
            v->paused = !v->paused;
            repace(v, 0);
        }
        if (nt_input_key_is_pressed(NT_KEY_V)) {
            v->endpoint = (v->endpoint + 1) % body_sync_stand_endpoint_count(v->stand);
            v->redraw = true;
        }
        if (nt_input_key_is_pressed(NT_KEY_R)) {
            body_sync_error_t error;
            body_sync_stand_destroy(v->stand);
            if (!create_stand(v, &error)) {
                fprintf(stderr, "body_sync_stand_view: restart refused: %s\n", error.text);
                v->refused = true;
                finish(v);
                return;
            }
            v->shot_taken = false;
            v->redraw = true;
            repace(v, 0);
        }
    }
    uint32_t due = ticks_due(v);
    bool stepped = due > 0;
    for (uint32_t i = 0; i < due; i++) {
        /* The first tick's frame and every captured tick are drawn on their own. */
        if (!tick_once(v) || v->ticks == 1 || capture_tick(v)) {
            break;
        }
    }
    if (!stepped && !v->redraw) {
        return;
    }
    v->redraw = false;
    if (!v->framed) {
        frame_scene(v);
    }

    nt_gfx_begin_frame();
    int w = (int)g_nt_window.fb_width, h = (int)g_nt_window.fb_height;
    body_sync_view_frame_t f;
    camera(v, (uint32_t)w, (uint32_t)h, &f);
    const body_sync_view_drawer_t *d = v->drawer;
    if (d->begin_frame) {
        d->begin_frame(&f, d->ctx);
    } else {
        clear_pass();
    }
    draw_endpoint(v, v->endpoint, false);
    if (f.ghost) {
        draw_endpoint(v, 0, true);
    }
    if (!d->begin_frame) {
        nt_gfx_end_pass();
    }
    if (d->end_frame) {
        d->end_frame(&f, d->ctx);
    }
    nt_gfx_end_frame();

    if (stepped) {
        bool want_hash = v->hash_every && v->ticks % v->hash_every == 0;
        bool want_shot = v->shot_path && !v->shot_taken && v->ticks == v->shot_tick;
        if ((want_hash || want_shot) && read_frame(v, w, h)) {
            if (want_hash) {
                printf("frame %u %016llx\n", v->ticks, (unsigned long long)fnv(v->rgba, (size_t)w * (size_t)h * 4u));
            }
#ifdef BODY_SYNC_VIEW_RECORDER
            if (want_shot) {
                v->shot_taken = true;
                v->shot_failed = !save_png(v, w, h, v->shot_path);
            }
#endif
        }
#ifdef BODY_SYNC_VIEW_RECORDER
        if (v->record_path && !v->record_started && v->ticks == 1) {
            /* 0.4 bit per pixel per frame: thin wires survive H.264 at this size. */
            uint32_t bitrate = (uint32_t)((double)w * (double)h * 60.0 * 0.4);
            v->record_started = nt_recorder_start(v->record_path, 60, bitrate);
            v->recording = v->record_started;
            v->record_failed = !v->record_started;
        }
        if (v->recording) {
            nt_recorder_frame((double)v->ticks * (double)STEP, (double)STEP);
        }
#endif
    }
    nt_window_swap_buffers();
    if (stepped && v->ticks == 1) {
        /* Tick 2 is due one step after tick 1 is on screen: the pace leaves out the first
           frame's one-time cost (shaders, buffers). */
        v->paced_from = nt_time_now();
        repace(v, 1);
    }
    if (v->done) {
        finish(v);
    }
}

static const body_sync_scene_entry_t *find_scene(const char *name, const body_sync_scene_entry_t *extra, uint32_t extra_count) {
    const body_sync_scene_entry_t *own = body_sync_scene_lookup(extra, extra_count, name);
    if (own) {
        return own;
    }
    uint32_t count = 0;
    const body_sync_scene_entry_t *catalog = body_sync_scene_catalog(&count);
    return body_sync_scene_lookup(catalog, count, name);
}

/* A cut take is kept as <name>.partial<ext>, so no full-length name holds a short run. */
static void keep_partial(const char *path) {
    char partial[1024];
    const char *dot = strrchr(path, '.');
    const char *slash = strpbrk(dot ? dot : path, "/\\");
    size_t stem = dot && !slash ? (size_t)(dot - path) : strlen(path);
    if (stem + 16u + strlen(path + stem) >= sizeof(partial)) {
        remove(path);
        fprintf(stderr, "%s: cut take removed (no room for a partial name)\n", path);
        return;
    }
    snprintf(partial, sizeof(partial), "%.*s.partial%s", (int)stem, path, path + stem);
    remove(partial);
    if (rename(path, partial) == 0) {
        printf("cut take kept as %s\n", partial);
    } else {
        remove(path);
        fprintf(stderr, "%s: cut take removed (could not rename it)\n", path);
    }
}

static bool parse_camera(const char *text, body_sync_view_camera_t *c) {
    float n[7];
    int count = 0;
    const char *p = text;
    for (;;) {
        char *end = NULL;
        n[count] = strtof(p, &end);
        if (end == p) {
            return false;
        }
        count++;
        if (*end == '\0') {
            break;
        }
        if (*end != ',' || count == 7) {
            return false;
        }
        p = end + 1;
    }
    if (count < 6) {
        return false;
    }
    memcpy(c->eye, n, sizeof(c->eye));
    memcpy(c->target, n + 3, sizeof(c->target));
    c->fov_y_deg = count == 7 ? n[6] : FOV_Y_DEG;
    c->near_m = NEAR_M;
    float d = sqrtf((n[3] - n[0]) * (n[3] - n[0]) + (n[4] - n[1]) * (n[4] - n[1]) + (n[5] - n[2]) * (n[5] - n[2]));
    c->far_m = fmaxf(CLI_FAR_MIN_M, 4.0f * d);
    return camera_ok(c);
}

static int parse_count(const char *arg, const char *value, uint32_t *out) {
    uint64_t n = 0;
    if (!value) {
        return refuse(arg, NULL, "needs a value");
    }
    if (!parse_u64(value, &n) || n == 0 || n >= 36000) {
        return refuse(arg, value, "must be a whole number 1..35999");
    }
    *out = (uint32_t)n;
    return 0;
}

int body_sync_view_main(int argc, char **argv, const body_sync_scene_entry_t *extra, uint32_t extra_count, const body_sync_view_drawer_t *drawer) {
    view_t *v = &s_view;
    memset(v, 0, sizeof(*v));
    v->drawer = drawer ? drawer : &body_sync_view_debug_drawer;
    if (!v->drawer->draw_body) {
        fprintf(stderr, "body_sync_stand_view: the drawer has no draw_body\n");
        return 2;
    }
    v->endpoint = 1;
    v->shot_tick = 180;
    const char *scenario_path = NULL;
    bool view_named = false;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = i + 1 < argc ? argv[i + 1] : NULL;
        int bad = 0;
        if (strcmp(arg, "--view") == 0) {
            if (!value) {
                return refuse(arg, NULL, "needs a value");
            }
            i++;
            if (strcmp(value, "host") == 0) {
                v->endpoint = 0;
            } else if (value[0] == 'g' && value[1] >= '1' && value[1] <= '3' && value[2] == '\0') {
                v->endpoint = (uint32_t)(value[1] - '0');
            } else {
                return refuse(arg, value, "must be host, g1, g2 or g3");
            }
            view_named = true;
        } else if (strcmp(arg, "--ghost") == 0) {
            v->ghost = true;
        } else if (strcmp(arg, "--seed") == 0) {
            if (!value || !parse_u64(value, &v->seed)) {
                return refuse(arg, value, value ? "must be a whole number" : "needs a value");
            }
            i++;
            v->override_seed = true;
        } else if (strcmp(arg, "--frames") == 0) {
            bad = parse_count(arg, value, &v->frames_max);
            i++;
        } else if (strcmp(arg, "--shot-tick") == 0) {
            bad = parse_count(arg, value, &v->shot_tick);
            i++;
        } else if (strcmp(arg, "--hash-every") == 0) {
            bad = parse_count(arg, value, &v->hash_every);
            i++;
        } else if (strcmp(arg, "--camera") == 0) {
            if (!value) {
                return refuse(arg, NULL, "needs ex,ey,ez,tx,ty,tz[,fov]");
            }
            if (!parse_camera(value, &v->cli_camera)) {
                return refuse(arg, value, "must be six or seven numbers ex,ey,ez,tx,ty,tz[,fov]: eye apart from target, fov 1..179 degrees");
            }
            v->cli_camera_set = true;
            i++;
        } else if (strcmp(arg, "--record") == 0 || strcmp(arg, "--shot") == 0) {
            if (!value) {
                return refuse(arg, NULL, "needs a file");
            }
            *(arg[2] == 'r' ? &v->record_path : &v->shot_path) = value;
            i++;
        } else if (arg[0] == '-') {
            return refuse(arg, NULL, "unknown option");
        } else if (scenario_path) {
            return refuse(arg, NULL, "a second scenario; one run takes one");
        } else {
            scenario_path = arg;
        }
        if (bad) {
            return bad;
        }
    }
#ifndef BODY_SYNC_VIEW_RECORDER
    if (v->record_path || v->shot_path) {
        fprintf(stderr, "this build has no engine recorder: --record and --shot are unavailable\n");
        return 2;
    }
#endif
    if (!scenario_path) {
        return refuse("<scenario.scn>", NULL, "missing");
    }
    int status = 2;
    v->scenario = malloc(sizeof(*v->scenario));
    v->scene = malloc(sizeof(*v->scene));
    body_sync_error_t error;
    bool engine = false, window = false, gfx = false, drawer_called = false;
    if (!v->scenario || !v->scene) {
        fprintf(stderr, "out of memory\n");
        goto done;
    }
    if (!body_sync_scenario_load(scenario_path, v->scenario, &error)) {
        fprintf(stderr, "%s:%u: %s\n", scenario_path, error.line, error.text);
        goto done;
    }
    const body_sync_scene_entry_t *entry = find_scene(v->scenario->scene, extra, extra_count);
    if (!entry) {
        fprintf(stderr, "%s: no scene '%s'\n", scenario_path, v->scenario->scene);
        goto done;
    }
    entry->build(v->scene);
    if (!create_stand(v, &error)) {
        fprintf(stderr, "%s:%u: %s\n", scenario_path, error.line, error.text);
        goto done;
    }
    uint32_t endpoints = body_sync_stand_endpoint_count(v->stand);
    if (v->endpoint >= endpoints) {
        if (view_named) {
            fprintf(stderr, "body_sync_stand_view: --view: %s runs %u endpoints\n", scenario_path, endpoints);
            goto done;
        }
        v->endpoint = 0;
    }

    static char title[160];
    snprintf(title, sizeof(title), "body-sync %s: %s%s", v->scenario->name, v->endpoint == 0 ? "host" : (const char *[]){"", "g1", "g2", "g3"}[v->endpoint],
             v->ghost ? " + host ghost" : "");
    nt_engine_config_t config = {0};
    config.app_name = "body_sync_stand_view";
    if (nt_engine_init(&config) != NT_OK) {
        fprintf(stderr, "the engine did not start\n");
        goto done;
    }
    engine = true;
    g_nt_window.width = WINDOW_W;
    g_nt_window.height = WINDOW_H;
    g_nt_window.title = title;
    g_nt_window.resizable = false;
    /* Both return nothing; what they publish says whether they came up. */
    nt_window_init();
    window = g_nt_window.platform_handle != NULL;
    if (!window) {
        fprintf(stderr, "the window did not open\n");
        goto down;
    }
    nt_input_init();
    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    if (v->drawer->configure_gfx) {
        v->drawer->configure_gfx(&gfx_desc, v->drawer->ctx);
    }
    nt_gfx_init(&gfx_desc);
    gfx = g_nt_gfx.initialized;
    if (!gfx) {
        fprintf(stderr, "the renderer did not start\n");
        goto down;
    }
    drawer_called = true;
    if (v->drawer->init && !v->drawer->init(v->drawer->ctx)) {
        fprintf(stderr, "body_sync_stand_view: the drawer refused to start\n");
        goto down;
    }
#ifdef BODY_SYNC_VIEW_RECORDER
    nt_fpng_init();
#endif
    nt_window_set_vsync(NT_VSYNC_OFF);
    g_nt_app.target_dt = IDLE_DT; /* wait_ready lifts it for a recording */
    v->ready_deadline = nt_time_now() + READY_WAIT_S;
    nt_app_run(frame);

    if (v->refused) {
        status = 2;
    } else if (!v->ended) {
        printf("cut at %u ticks: the checks are skipped\n", v->ticks);
        status = BODY_SYNC_VIEW_EXIT_CUT;
    } else {
        const body_sync_report_t *report = body_sync_stand_report(v->stand);
        body_sync_stand_print_report(report, false);
        status = report->passed ? 0 : 1;
    }
    if (v->record_path && v->record_started) {
        if (v->record_failed) {
            /* A failed take leaves no file behind to be mistaken for a good one. */
            remove(v->record_path);
            fprintf(stderr, "%s: the recording failed and was removed\n", v->record_path);
            status = BODY_SYNC_VIEW_EXIT_MEDIA;
        } else if (!v->ended) {
            keep_partial(v->record_path);
        } else {
            printf("recorded %u frames to %s\n", v->recorded_frames, v->record_path);
        }
    } else if (v->record_path && v->record_failed) {
        fprintf(stderr, "%s: the recorder did not start\n", v->record_path);
        status = BODY_SYNC_VIEW_EXIT_MEDIA;
    }
    if (v->shot_path && v->shot_failed) {
        fprintf(stderr, "%s: the screenshot was not written\n", v->shot_path);
        status = BODY_SYNC_VIEW_EXIT_MEDIA;
    } else if (v->shot_path && !v->shot_taken && !v->refused) {
        if (v->ended) {
            fprintf(stderr, "%s: never taken, the run ended before tick %u\n", v->shot_path, v->shot_tick);
            status = BODY_SYNC_VIEW_EXIT_MEDIA;
        } else {
            fprintf(stderr, "%s: never taken: cut at %u ticks\n", v->shot_path, v->ticks);
        }
    }
down:
    if (drawer_called && v->drawer->shutdown) {
        v->drawer->shutdown(v->drawer->ctx);
    }
    free(v->rgba);
    if (gfx) {
        nt_gfx_shutdown();
    }
    if (window) {
        nt_input_shutdown();
        nt_window_shutdown();
    }
    if (engine) {
        nt_engine_shutdown();
    }
done:
    body_sync_stand_destroy(v->stand);
    free(v->scene);
    free(v->scenario);
    return status;
}
