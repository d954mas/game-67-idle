// Little Lives — a tiny 3D life sim (a Sims-like). Milestone 1: one house lot,
// several Sims with decaying needs, free will + player commands, furniture
// interactions, Sim-to-Sim socializing, day/night, going to work for money, and
// a lightweight Build/Buy mode. Rendered with the engine shape renderer
// (procedural primitives) as acknowledged debug-only art debt; real meshes + UI font
// text come in follow-up tasks. The game owns the main loop (engine is code-first).
#include "app/nt_app.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "devapi/nt_devapi.h"
#include "devapi/nt_devapi_net.h"
#include "game_devapi_ui.h"
#include "game_state.h"
#include "graphics/nt_gfx.h"
#include "input/nt_input.h"
#include "math/nt_math.h"
#include "renderers/nt_shape_renderer.h"
#include "window/nt_window.h"

#include "ll_meshes.h" // generated Kenney Furniture Kit (CC0) geometry
#include "ll_art.h"    // frozen art-direction contract: palette + baked lighting

#ifndef NT_PLATFORM_WEB
#include <glad/gl.h> // native framebuffer readback for DevAPI screenshots
#endif

#ifdef LL_HAVE_TEXT
#include "font/nt_font.h"
#include "fs/nt_fs.h"
#include "hash/nt_hash.h"
#include "http/nt_http.h"
#include "material/nt_material.h"
#include "render/nt_render_defs.h"
#include "renderers/nt_text_renderer.h"
#include "resource/nt_resource.h"
#include "time/nt_time.h"
#include "nt_pack_format.h"
#include "little_lives_assets.h"
#endif

#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #region tunables
#define LL_DEVAPI_PORT_DEFAULT 9123
#define LL_MAX_SIMS 16
#define LL_MAX_OBJECTS 64
#define LL_MAX_LOTS 4
#define LOT_SIMS 3 // sims per household

#define NEED_COUNT 6
enum { NEED_ENERGY = 0, NEED_HUNGER, NEED_HYGIENE, NEED_BLADDER, NEED_FUN, NEED_SOCIAL };

#define SKILL_COUNT 3
enum { SKILL_COOKING = 0, SKILL_LOGIC, SKILL_CHARISMA };
#define CAREER_MAX_LEVEL 5

// Object kinds. DOOR is the "go to work" object; others each serve one need.
enum {
    OBJ_BED = 0,
    OBJ_FRIDGE,
    OBJ_SHOWER,
    OBJ_TOILET,
    OBJ_SOFA,
    OBJ_COMPUTER,
    OBJ_DOOR,
    OBJ_KIND_COUNT,
};
#define BUILD_PALETTE_COUNT 6 // buyable kinds (everything except DOOR)

#define ROOM_HW 9.5F  // room half-width  (X: -9.5..9.5)
#define ROOM_HD 7.5F  // room half-depth  (Z: -7.5..7.5)
#define WALL_H 3.2F

#define TIME_SCALE_DEFAULT 3.0F // game-minutes per real second
#define WORK_SECONDS 22.0F      // real seconds of a work shift
#define WORK_SALARY 320         // simoleons per completed shift
#define SIM_SPEED 2.7F          // world units / second
#define ARRIVE_R 0.55F
#define SOCIAL_R 2.1F
#define NEED_SEEK_THRESHOLD 58.0F
#define NEED_FULL 96.0F
// #endregion

// #region types
typedef struct {
    float x;
    float y;
    float w;
    float h;
} Box;

typedef struct {
    int kind;        // OBJ_*
    int lot;         // owning lot index
    float x, z;      // footprint center on the floor (world)
    float usx, usz;  // "use spot" where a Sim stands
    int used_by;     // sim index currently using it, or -1
} Object;

typedef struct {
    char name[16];
    int lot;          // owning household lot
    float col[4];
    float x, z;       // position on floor (world)
    float facing;     // radians, 0 = +Z
    float bob;        // animation phase
    int action;       // GAME_STATE_SIM_ACTION_*
    int target_obj;   // index into s_objects, or -1
    int drive_need;   // need this action is meant to satisfy, or -1
    int social_with;  // sim index for social action, or -1
    bool player_dir;  // player issued the current command (suppress AI override)
    float use_timer;  // real seconds spent in current use/work/social
    float ai_cd;      // re-decide cooldown
    float needs[NEED_COUNT];
    bool at_work;     // hidden, off-lot
    float skills[SKILL_COUNT]; // 0..10 (cooking, logic, charisma)
    int career_level;          // 0..CAREER_MAX_LEVEL
    float career_perf;         // 0..100 toward next promotion
    int shifts;                // completed work shifts
} Sim;

typedef struct {
    char name[16];
    float ox, oz; // lot origin (house center) in world space
} Lot;
// #endregion

// #region state
static bool s_devapi_enabled;
static uint16_t s_devapi_port = LL_DEVAPI_PORT_DEFAULT;
static int s_window_width = 1280;
static int s_window_height = 720;

static Sim s_sims[LL_MAX_SIMS];
static int s_sim_count;
static Object s_objects[LL_MAX_OBJECTS];
static int s_object_count;
static Lot s_lots[LL_MAX_LOTS];
static int s_lot_count;
static int s_active_lot;   // household the player controls / camera focuses
static bool s_overview;    // neighborhood overview camera
static float s_rel[LL_MAX_SIMS][LL_MAX_SIMS]; // pairwise relationship -100..100

static bool s_paused;
static float s_time_scale = TIME_SCALE_DEFAULT;
static float s_daylight = 1.0F; // 0.4 (night) .. 1.0 (midday)

// pending framebuffer screenshot (written after rendering, before swap)
static bool s_capture_pending;
static char s_capture_path[512];

// build mode
static int s_build_kind; // OBJ_* palette selection
static float s_build_cx; // grid cursor
static float s_build_cz;

// camera view-projection for this frame (for screen-space picking + billboards)
static float s_vp[16];
static float s_cam_eye[3] = {0.0F, 12.0F, 18.0F}; // updated each frame (fog distance)
static float s_view_w = 1280.0F;
static float s_view_h = 720.0F;

// HUD layout (computed each frame; shared by draw + input)
static Box s_need_btn[NEED_COUNT];
static Box s_work_btn;
static Box s_mode_btn;
static Box s_sim_portrait[LOT_SIMS];
static Box s_palette_btn[BUILD_PALETTE_COUNT];
static Box s_lot_btn[LL_MAX_LOTS];
static Box s_view_btn;
// #endregion

static int lot_sim_base(int lot) { return lot * LOT_SIMS; }

// #region small helpers
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static bool box_has(Box b, float x, float y) { return x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h; }

static int object_price(int kind) {
    switch (kind) {
    case OBJ_BED: return 250;
    case OBJ_FRIDGE: return 400;
    case OBJ_SHOWER: return 200;
    case OBJ_TOILET: return 150;
    case OBJ_SOFA: return 300;
    case OBJ_COMPUTER: return 500;
    default: return 0;
    }
}

// Which skill a furniture kind trains while in use (-1 = none).
static int object_skill(int kind) {
    switch (kind) {
    case OBJ_FRIDGE: return SKILL_COOKING;
    case OBJ_COMPUTER: return SKILL_LOGIC;
    case OBJ_SOFA: return SKILL_CHARISMA;
    default: return -1;
    }
}

// Which need a furniture kind serves (-1 = none, e.g. the work door).
static int object_need(int kind) {
    switch (kind) {
    case OBJ_BED: return NEED_ENERGY;
    case OBJ_FRIDGE: return NEED_HUNGER;
    case OBJ_SHOWER: return NEED_HYGIENE;
    case OBJ_TOILET: return NEED_BLADDER;
    case OBJ_SOFA: return NEED_FUN;
    case OBJ_COMPUTER: return NEED_FUN;
    default: return -1;
    }
}

static const float *need_color(int need) {
    static const float c[NEED_COUNT][4] = {
        {1.0F, 0.84F, 0.20F, 1.0F}, // energy
        {1.0F, 0.55F, 0.16F, 1.0F}, // hunger
        {0.30F, 0.80F, 1.0F, 1.0F}, // hygiene
        {0.38F, 0.56F, 1.0F, 1.0F}, // bladder
        {0.92F, 0.42F, 0.92F, 1.0F},// fun
        {1.0F, 0.46F, 0.62F, 1.0F}, // social
    };
    return c[need];
}

static const float *object_color(int kind) {
    static const float c[OBJ_KIND_COUNT][4] = {
        {0.55F, 0.40F, 0.78F, 1.0F}, // bed (purple)
        {0.85F, 0.92F, 0.96F, 1.0F}, // fridge (white)
        {0.35F, 0.78F, 0.92F, 1.0F}, // shower (teal)
        {0.92F, 0.92F, 0.95F, 1.0F}, // toilet (porcelain)
        {0.90F, 0.45F, 0.40F, 1.0F}, // sofa (coral)
        {0.30F, 0.34F, 0.42F, 1.0F}, // computer (charcoal)
        {0.62F, 0.42F, 0.26F, 1.0F}, // door (wood)
    };
    return c[kind];
}

static float decay_per_min(int need) {
    static const float d[NEED_COUNT] = {0.11F, 0.20F, 0.10F, 0.24F, 0.15F, 0.10F};
    return d[need];
}

static float sim_mood(const Sim *s) {
    float sum = 0.0F;
    for (int i = 0; i < NEED_COUNT; i++) {
        sum += s->needs[i];
    }
    return sum / (float)NEED_COUNT;
}

// green (high) -> amber -> red (low)
static void bar_color(float v01, float out[4]) {
    float t = clampf(v01, 0.0F, 1.0F);
    if (t > 0.5F) {
        float k = (t - 0.5F) * 2.0F; // 0..1 amber->green
        out[0] = 0.95F - 0.55F * k;
        out[1] = 0.75F + 0.15F * k;
        out[2] = 0.20F;
    } else {
        float k = t * 2.0F; // 0..1 red->amber
        out[0] = 0.92F;
        out[1] = 0.25F + 0.50F * k;
        out[2] = 0.18F;
    }
    out[3] = 1.0F;
}

// Generic flat-lit shade (top-down key + warm/cool grade) for shapes whose
// normal isn't worth computing (sims, small props). Surfaces with a known
// orientation use ll_shade_n() with the real normal instead.
static void shade(float out[4], float r, float g, float b, float a) {
    ll_shade_flat(out, r, g, b, a, s_daylight);
}

// Distance from the camera eye to a world point on the floor (for aerial fog).
static float cam_dist(float x, float z) {
    float dx = x - s_cam_eye[0];
    float dy = s_cam_eye[1];
    float dz = z - s_cam_eye[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

static void mat4_mul_vec4(const float m[16], const float v[4], float out[4]) {
    for (int i = 0; i < 4; i++) {
        out[i] = m[0 * 4 + i] * v[0] + m[1 * 4 + i] * v[1] + m[2 * 4 + i] * v[2] + m[3 * 4 + i] * v[3];
    }
}

// Project a world point to framebuffer pixels. Returns false if behind camera.
static bool world_to_screen(float x, float y, float z, float *sx, float *sy) {
    float v[4] = {x, y, z, 1.0F};
    float clip[4];
    mat4_mul_vec4(s_vp, v, clip);
    if (clip[3] <= 0.0001F) {
        return false;
    }
    float ndx = clip[0] / clip[3];
    float ndy = clip[1] / clip[3];
    *sx = (ndx * 0.5F + 0.5F) * s_view_w;
    *sy = (1.0F - (ndy * 0.5F + 0.5F)) * s_view_h;
    return true;
}
// #endregion

// #region world setup
// Local (lot-relative) coords; converted to world via the lot origin.
static void place_object(int lot, int kind, float lx, float lz) {
    if (s_object_count >= LL_MAX_OBJECTS) {
        return;
    }
    float ox = s_lots[lot].ox;
    float oz = s_lots[lot].oz;
    Object *o = &s_objects[s_object_count++];
    o->kind = kind;
    o->lot = lot;
    o->x = ox + lx;
    o->z = oz + lz;
    o->used_by = -1;
    // Use spot: stand just inside the room, toward the lot center, from the object.
    float dx = -lx;
    float dz = -lz;
    float len = sqrtf(dx * dx + dz * dz);
    if (len < 0.001F) {
        dx = 0.0F;
        dz = 1.0F;
        len = 1.0F;
    }
    o->usx = o->x + (dx / len) * 1.05F;
    o->usz = o->z + (dz / len) * 1.05F;
}

static void furnish_lot(int lot) {
    place_object(lot, OBJ_FRIDGE, -4.5F, -ROOM_HD + 0.7F);
    place_object(lot, OBJ_COMPUTER, 2.5F, -ROOM_HD + 0.7F);
    place_object(lot, OBJ_BED, -ROOM_HW + 1.0F, -4.0F);
    place_object(lot, OBJ_SHOWER, -ROOM_HW + 0.8F, 0.5F);
    place_object(lot, OBJ_TOILET, -ROOM_HW + 0.8F, 4.5F);
    place_object(lot, OBJ_SOFA, 4.5F, -1.0F);
    place_object(lot, OBJ_DOOR, ROOM_HW - 1.0F, ROOM_HD - 1.0F);
}

static void init_sim(int i, int lot, const char *name, float r, float g, float b, float lx, float lz) {
    Sim *s = &s_sims[i];
    memset(s, 0, sizeof(*s));
    (void)snprintf(s->name, sizeof(s->name), "%s", name);
    s->lot = lot;
    s->col[0] = r;
    s->col[1] = g;
    s->col[2] = b;
    s->col[3] = 1.0F;
    s->x = s_lots[lot].ox + lx;
    s->z = s_lots[lot].oz + lz;
    s->facing = 0.0F;
    s->action = GAME_STATE_SIM_ACTION_IDLE;
    s->target_obj = -1;
    s->drive_need = -1;
    s->social_with = -1;
    for (int n = 0; n < NEED_COUNT; n++) {
        s->needs[n] = 50.0F + (float)((i * 7 + n * 13) % 40);
    }
}

static void world_init(void) {
    // Neighborhood: 2x2 lots around the origin, roads in the cross gaps.
    s_lot_count = LL_MAX_LOTS;
    s_lots[0] = (Lot){"Maple St 1", -16.0F, -13.0F};
    s_lots[1] = (Lot){"Oak St 2", 16.0F, -13.0F};
    s_lots[2] = (Lot){"Birch St 3", -16.0F, 13.0F};
    s_lots[3] = (Lot){"Cedar St 4", 16.0F, 13.0F};

    s_object_count = 0;
    for (int l = 0; l < s_lot_count; l++) {
        furnish_lot(l);
    }

    // 3 households populated; vary palettes + names per lot.
    static const char *names[LL_MAX_LOTS][LOT_SIMS] = {
        {"Alex", "Bella", "Cory"},
        {"Dana", "Evan", "Faye"},
        {"Gil", "Hana", "Ivo"},
        {"Jo", "Kit", "Lux"},
    };
    static const float cols[LL_MAX_LOTS][3] = {
        {0.95F, 0.74F, 0.45F},
        {0.45F, 0.80F, 0.95F},
        {0.80F, 0.55F, 0.95F},
        {0.95F, 0.55F, 0.65F},
    };
    static const float starts[LOT_SIMS][2] = {{-2.0F, 1.0F}, {2.0F, -1.5F}, {5.0F, 2.5F}};
    s_sim_count = 0;
    for (int l = 0; l < s_lot_count; l++) {
        for (int k = 0; k < LOT_SIMS; k++) {
            float r = cols[l][0] * (0.78F + 0.14F * (float)k);
            float g = cols[l][1] * (0.78F + 0.14F * (float)k);
            float b = cols[l][2] * (0.78F + 0.14F * (float)k);
            init_sim(s_sim_count++, l, names[l][k], r, g, b, starts[k][0], starts[k][1]);
        }
    }

    // Relationships: housemates start as family (warm), strangers neutral.
    for (int a = 0; a < s_sim_count; a++) {
        for (int b = 0; b < s_sim_count; b++) {
            s_rel[a][b] = (a == b) ? 0.0F : (s_sims[a].lot == s_sims[b].lot ? 45.0F : 0.0F);
        }
    }

    s_active_lot = 0;
    s_overview = false;
    g_game_state.selected_sim = 0;

    // Frame the active dollhouse by default.
    g_game_state.camera_distance = 19.0F;
    g_game_state.camera_pitch = 0.74F;
    g_game_state.camera_yaw = 0.72F;
    g_game_state.camera_target_x = s_lots[0].ox;
    g_game_state.camera_target_z = s_lots[0].oz - 0.5F;

    s_build_kind = OBJ_BED;
    s_build_cx = s_lots[0].ox + 2.0F;
    s_build_cz = s_lots[0].oz;
    s_paused = false;
    s_time_scale = TIME_SCALE_DEFAULT;
}

static void game_reset(void) {
    game_state_init_defaults(&g_game_state);
    world_init();
    game_state_mark_dirty();
}
// #endregion

// #region sim behavior
static int find_free_object(int lot, int kind) {
    for (int i = 0; i < s_object_count; i++) {
        if (s_objects[i].lot == lot && s_objects[i].kind == kind && s_objects[i].used_by < 0) {
            return i;
        }
    }
    // fall back to any of that kind in the lot (queue/steal not modeled yet)
    for (int i = 0; i < s_object_count; i++) {
        if (s_objects[i].lot == lot && s_objects[i].kind == kind) {
            return i;
        }
    }
    return -1;
}

static int object_for_need(int lot, int need) {
    switch (need) {
    case NEED_ENERGY: return find_free_object(lot, OBJ_BED);
    case NEED_HUNGER: return find_free_object(lot, OBJ_FRIDGE);
    case NEED_HYGIENE: return find_free_object(lot, OBJ_SHOWER);
    case NEED_BLADDER: return find_free_object(lot, OBJ_TOILET);
    case NEED_FUN: {
        int o = find_free_object(lot, OBJ_SOFA);
        return o >= 0 ? o : find_free_object(lot, OBJ_COMPUTER);
    }
    default: return -1;
    }
}

static void release_target(Sim *s) {
    if (s->target_obj >= 0 && s->target_obj < s_object_count && s_objects[s->target_obj].used_by == (int)(s - s_sims)) {
        s_objects[s->target_obj].used_by = -1;
    }
    s->target_obj = -1;
    s->drive_need = -1;
    s->social_with = -1;
    s->player_dir = false;
}

static void go_idle(Sim *s) {
    release_target(s);
    s->action = GAME_STATE_SIM_ACTION_IDLE;
    s->use_timer = 0.0F;
}

static void command_need(int sim_index, int need, bool player) {
    if (sim_index < 0 || sim_index >= s_sim_count) {
        return;
    }
    Sim *s = &s_sims[sim_index];
    if (s->at_work) {
        return;
    }
    release_target(s);
    if (need == NEED_SOCIAL) {
        // Find the nearest other sim in the same household to chat with.
        int best = -1;
        float bestd = 1e9F;
        for (int j = 0; j < s_sim_count; j++) {
            if (j == sim_index || s_sims[j].at_work || s_sims[j].lot != s->lot) {
                continue;
            }
            float dx = s_sims[j].x - s->x;
            float dz = s_sims[j].z - s->z;
            float d = dx * dx + dz * dz;
            if (d < bestd) {
                bestd = d;
                best = j;
            }
        }
        if (best < 0) {
            return;
        }
        s->social_with = best;
        s->drive_need = NEED_SOCIAL;
        s->action = GAME_STATE_SIM_ACTION_WALKING;
        s->player_dir = player;
        return;
    }
    int obj = object_for_need(s->lot, need);
    if (obj < 0) {
        return;
    }
    s_objects[obj].used_by = sim_index; // reserve
    s->target_obj = obj;
    s->drive_need = need;
    s->action = GAME_STATE_SIM_ACTION_WALKING;
    s->player_dir = player;
}

static void command_work(int sim_index, bool player) {
    if (sim_index < 0 || sim_index >= s_sim_count) {
        return;
    }
    Sim *s = &s_sims[sim_index];
    if (s->at_work) {
        return;
    }
    release_target(s);
    int door = find_free_object(s->lot, OBJ_DOOR);
    if (door < 0) {
        return;
    }
    s->target_obj = door;
    s->drive_need = -1;
    s->action = GAME_STATE_SIM_ACTION_WALKING;
    s->player_dir = player;
}

static void sim_ai(int idx) {
    Sim *s = &s_sims[idx];
    if (s->at_work || s->player_dir) {
        return;
    }
    if (s->action != GAME_STATE_SIM_ACTION_IDLE) {
        return;
    }
    if (s->ai_cd > 0.0F) {
        return;
    }
    s->ai_cd = 0.6F;
    // Pick the lowest need; act on it if it dipped below the seek threshold.
    int low = 0;
    for (int n = 1; n < NEED_COUNT; n++) {
        if (s->needs[n] < s->needs[low]) {
            low = n;
        }
    }
    if (s->needs[low] < NEED_SEEK_THRESHOLD) {
        command_need(idx, low, false);
    }
}

static void move_toward(Sim *s, float tx, float tz, float dt, bool *arrived) {
    float dx = tx - s->x;
    float dz = tz - s->z;
    float d = sqrtf(dx * dx + dz * dz);
    if (d <= ARRIVE_R) {
        *arrived = true;
        return;
    }
    *arrived = false;
    float step = SIM_SPEED * dt;
    if (step > d) {
        step = d;
    }
    s->x += (dx / d) * step;
    s->z += (dz / d) * step;
    s->facing = atan2f(dx, dz);
    s->bob += dt * 9.0F;
}

static void sim_update(int idx, float dt, float dt_min) {
    Sim *s = &s_sims[idx];

    // Needs decay for everyone (slower while at work / sleeping handled per-need).
    for (int n = 0; n < NEED_COUNT; n++) {
        float scale = 1.0F;
        if (s->at_work) {
            scale = 0.5F;
        }
        s->needs[n] = clampf(s->needs[n] - decay_per_min(n) * dt_min * scale, 0.0F, 100.0F);
    }

    if (s->ai_cd > 0.0F) {
        s->ai_cd -= dt;
    }

    switch (s->action) {
    case GAME_STATE_SIM_ACTION_WALKING: {
        bool arrived = false;
        if (s->drive_need == NEED_SOCIAL && s->social_with >= 0) {
            Sim *p = &s_sims[s->social_with];
            move_toward(s, p->x, p->z, dt, &arrived);
            if (arrived) {
                s->action = GAME_STATE_SIM_ACTION_SOCIAL;
                s->use_timer = 0.0F;
            }
        } else if (s->target_obj >= 0) {
            Object *o = &s_objects[s->target_obj];
            move_toward(s, o->usx, o->usz, dt, &arrived);
            if (arrived) {
                if (o->kind == OBJ_DOOR) {
                    s->action = GAME_STATE_SIM_ACTION_WORK;
                    s->at_work = true;
                    s->use_timer = 0.0F;
                } else {
                    s->action = (o->kind == OBJ_BED) ? GAME_STATE_SIM_ACTION_SLEEP : GAME_STATE_SIM_ACTION_USING;
                    s->use_timer = 0.0F;
                    // face the object
                    s->facing = atan2f(o->x - s->x, o->z - s->z);
                }
            }
        } else {
            go_idle(s);
        }
        break;
    }
    case GAME_STATE_SIM_ACTION_USING:
    case GAME_STATE_SIM_ACTION_SLEEP: {
        s->use_timer += dt;
        s->bob += dt * 3.0F;
        int need = (s->target_obj >= 0) ? object_need(s_objects[s->target_obj].kind) : s->drive_need;
        int skill = (s->target_obj >= 0) ? object_skill(s_objects[s->target_obj].kind) : -1;
        if (skill >= 0) {
            s->skills[skill] = clampf(s->skills[skill] + 0.05F * dt_min, 0.0F, 10.0F);
        }
        if (need >= 0) {
            float rate = (need == NEED_ENERGY) ? 3.4F : 4.2F; // per game-minute
            s->needs[need] = clampf(s->needs[need] + rate * dt_min, 0.0F, 100.0F);
            // Eating/showering also nudges fun a touch.
            if (need != NEED_FUN) {
                s->needs[NEED_FUN] = clampf(s->needs[NEED_FUN] + 0.4F * dt_min, 0.0F, 100.0F);
            }
            if (s->needs[need] >= NEED_FULL || s->use_timer > 24.0F) {
                g_game_state.stats_interactions += 1;
                go_idle(s);
            }
        } else {
            go_idle(s);
        }
        break;
    }
    case GAME_STATE_SIM_ACTION_SOCIAL: {
        s->use_timer += dt;
        s->bob += dt * 5.0F;
        s->needs[NEED_SOCIAL] = clampf(s->needs[NEED_SOCIAL] + 4.0F * dt_min, 0.0F, 100.0F);
        s->needs[NEED_FUN] = clampf(s->needs[NEED_FUN] + 2.0F * dt_min, 0.0F, 100.0F);
        s->skills[SKILL_CHARISMA] = clampf(s->skills[SKILL_CHARISMA] + 0.03F * dt_min, 0.0F, 10.0F);
        if (s->social_with >= 0 && s->social_with < s_sim_count) {
            // Better chatters (charisma) build friendship faster.
            float gain = (0.6F + 0.06F * s->skills[SKILL_CHARISMA]) * dt_min;
            s_rel[idx][s->social_with] = clampf(s_rel[idx][s->social_with] + gain, -100.0F, 100.0F);
        }
        if (s->use_timer > 6.0F || s->needs[NEED_SOCIAL] >= NEED_FULL) {
            g_game_state.stats_interactions += 1;
            go_idle(s);
        }
        break;
    }
    case GAME_STATE_SIM_ACTION_WORK: {
        s->use_timer += dt;
        if (s->use_timer >= WORK_SECONDS) {
            // Pay scales with career level + relevant (logic) skill.
            int pay = (int)((float)WORK_SALARY * (1.0F + 0.4F * (float)s->career_level) * (0.7F + 0.06F * s->skills[SKILL_LOGIC]));
            g_game_state.wallet_simoleons += pay;
            g_game_state.stats_interactions += 1;
            s->shifts += 1;
            // Performance toward promotion: better when mood + logic are high.
            s->career_perf += 22.0F + 0.18F * sim_mood(s) + 2.0F * s->skills[SKILL_LOGIC];
            if (s->career_perf >= 100.0F && s->career_level < CAREER_MAX_LEVEL) {
                s->career_level += 1;
                s->career_perf = 0.0F;
            }
            s->at_work = false;
            // Work is tiring.
            s->needs[NEED_ENERGY] = clampf(s->needs[NEED_ENERGY] - 18.0F, 0.0F, 100.0F);
            s->needs[NEED_HUNGER] = clampf(s->needs[NEED_HUNGER] - 14.0F, 0.0F, 100.0F);
            s->needs[NEED_BLADDER] = clampf(s->needs[NEED_BLADDER] - 16.0F, 0.0F, 100.0F);
            go_idle(s);
        }
        break;
    }
    case GAME_STATE_SIM_ACTION_IDLE:
    default: {
        s->bob += dt * 1.5F;
        sim_ai(idx);
        break;
    }
    }
}

// Two nearby idle Sims with low-ish social will spontaneously chat.
static void social_autopair(void) {
    for (int i = 0; i < s_sim_count; i++) {
        Sim *a = &s_sims[i];
        if (a->at_work || a->action != GAME_STATE_SIM_ACTION_IDLE || a->player_dir) {
            continue;
        }
        if (a->needs[NEED_SOCIAL] > 70.0F) {
            continue;
        }
        for (int j = i + 1; j < s_sim_count; j++) {
            Sim *b = &s_sims[j];
            if (b->at_work || b->action != GAME_STATE_SIM_ACTION_IDLE || b->lot != a->lot) {
                continue;
            }
            float dx = b->x - a->x;
            float dz = b->z - a->z;
            if (dx * dx + dz * dz <= SOCIAL_R * SOCIAL_R) {
                a->social_with = j;
                a->drive_need = NEED_SOCIAL;
                a->action = GAME_STATE_SIM_ACTION_SOCIAL;
                a->use_timer = 0.0F;
                b->social_with = i;
                b->drive_need = NEED_SOCIAL;
                b->action = GAME_STATE_SIM_ACTION_SOCIAL;
                b->use_timer = 0.0F;
                a->facing = atan2f(dx, dz);
                b->facing = atan2f(-dx, -dz);
                break;
            }
        }
    }
}
// #endregion

// #region game update
static void update_clock(float dt) {
    if (!s_paused) {
        g_game_state.clock_minutes += dt * s_time_scale;
        while (g_game_state.clock_minutes >= 1440.0F) {
            g_game_state.clock_minutes -= 1440.0F;
            if (g_game_state.clock_day < 9999) {
                g_game_state.clock_day += 1;
            }
        }
    }
    // Daylight curve: dark before 6:00 and after 20:00, bright midday.
    float h = g_game_state.clock_minutes / 60.0F;
    float day01 = 0.5F - 0.5F * cosf((clampf(h, 5.0F, 21.0F) - 5.0F) / 16.0F * 6.2831853F);
    s_daylight = 0.58F + 0.42F * clampf(day01, 0.0F, 1.0F);
}

static void game_update(void) {
    float dt = g_nt_app.dt;
    if (dt > 0.1F) {
        dt = 0.1F; // clamp huge first/stall frames
    }
    update_clock(dt);
    float dt_min = s_paused ? 0.0F : dt * s_time_scale;

    for (int i = 0; i < s_sim_count; i++) {
        sim_update(i, dt, dt_min);
    }
    social_autopair();

    // Mirror runtime summary into persistent state (DevAPI/save visible).
    g_game_state.sim_count = s_sim_count;
    if (g_game_state.selected_sim >= s_sim_count) {
        g_game_state.selected_sim = 0;
    }
}
// #endregion

// #region camera
static void compute_camera(float aspect, float eye_out[3]) {
    float yaw = g_game_state.camera_yaw;
    float pitch = clampf(g_game_state.camera_pitch, 0.12F, 1.45F);
    float dist = clampf(g_game_state.camera_distance, 6.0F, 48.0F);
    float tx = g_game_state.camera_target_x;
    float tz = g_game_state.camera_target_z;
    if (s_overview) {
        pitch = 0.98F; // near top-down for the neighborhood map
        dist = 64.0F;
        tx = 0.0F;
        tz = 0.0F;
    }

    vec3 eye = {
        tx + dist * cosf(pitch) * sinf(yaw),
        dist * sinf(pitch),
        tz + dist * cosf(pitch) * cosf(yaw),
    };
    vec3 center = {tx, 1.0F, tz};
    vec3 up = {0.0F, 1.0F, 0.0F};

    mat4 view;
    mat4 proj;
    mat4 vp;
    glm_lookat(eye, center, up, view);
    glm_perspective(glm_rad(52.0F), aspect, 0.1F, 200.0F, proj);
    glm_mat4_mul(proj, view, vp);
    memcpy(s_vp, vp, sizeof(s_vp));
    eye_out[0] = eye[0];
    eye_out[1] = eye[1];
    eye_out[2] = eye[2];
    s_cam_eye[0] = eye[0];
    s_cam_eye[1] = eye[1];
    s_cam_eye[2] = eye[2];
}
// #endregion

// #region 3D render
static const float LL_NORMAL_UP[3] = {0.0F, 1.0F, 0.0F};

static void draw_ground(void) {
    float col[4];
    float flat[4] = {0.7071068F, 0.0F, 0.0F, 0.7071068F};
    // Neighborhood lawn (up-facing, fogged toward the far horizon for depth)
    ll_shade_n(col, 0.42F, 0.66F, 0.33F, 1.0F, LL_NORMAL_UP, s_daylight);
    ll_fog_mix(col, cam_dist(0.0F, 0.0F) + 18.0F, s_daylight);
    nt_shape_renderer_rect_rot((float[3]){0.0F, -0.04F, 0.0F}, (float[2]){90.0F, 80.0F}, flat, col);
    // Roads: a cross between the four lots
    ll_shade_n(col, 0.31F, 0.31F, 0.35F, 1.0F, LL_NORMAL_UP, s_daylight);
    nt_shape_renderer_rect_rot((float[3]){0.0F, 0.005F, 0.0F}, (float[2]){70.0F, 7.0F}, flat, col);  // E-W road
    nt_shape_renderer_rect_rot((float[3]){0.0F, 0.005F, 0.0F}, (float[2]){7.0F, 60.0F}, flat, col);  // N-S road
    // Road centerlines
    ll_shade_n(col, 0.84F, 0.78F, 0.40F, 1.0F, LL_NORMAL_UP, s_daylight);
    for (int i = -16; i <= 16; i += 4) {
        nt_shape_renderer_rect_rot((float[3]){(float)i, 0.01F, 0.0F}, (float[2]){2.0F, 0.3F}, flat, col);
        nt_shape_renderer_rect_rot((float[3]){0.0F, 0.01F, (float)i}, (float[2]){0.3F, 2.0F}, flat, col);
    }
}

static const float LL_NORMAL_PX[3] = {1.0F, 0.0F, 0.0F};  // left wall inner face -> +x
static const float LL_NORMAL_PZ[3] = {0.0F, 0.0F, 1.0F};  // back wall inner face -> +z

static void draw_lot(const Lot *lot, bool active) {
    float hw = ROOM_HW;
    float hd = ROOM_HD;
    float ox = lot->ox;
    float oz = lot->oz;
    float col[4];
    float dim = active ? 1.0F : 0.84F; // focus the active household
    float flat[4] = {0.7071068F, 0.0F, 0.0F, 0.7071068F};
    float fog = cam_dist(ox, oz);

    // Plot pad (slightly raised) + wood floor
    ll_shade_n(col, 0.38F * dim, 0.56F * dim, 0.30F * dim, 1.0F, LL_NORMAL_UP, s_daylight);
    ll_fog_mix(col, fog, s_daylight);
    nt_shape_renderer_rect_rot((float[3]){ox, -0.015F, oz}, (float[2]){hw * 2.0F + 3.0F, hd * 2.0F + 3.0F}, flat, col);
    ll_shade_n(col, 0.88F * dim, 0.73F * dim, 0.54F * dim, 1.0F, LL_NORMAL_UP, s_daylight);
    ll_fog_mix(col, fog, s_daylight);
    nt_shape_renderer_rect_rot((float[3]){ox, 0.0F, oz}, (float[2]){hw * 2.0F, hd * 2.0F}, flat, col);

    if (active) {
        ll_shade_n(col, 0.80F, 0.64F, 0.46F, 1.0F, LL_NORMAL_UP, s_daylight);
        for (int ix = 0; ix <= (int)(hw * 2.0F); ix++) {
            float x = ox - hw + (float)ix;
            nt_shape_renderer_line((float[3]){x, 0.012F, oz - hd}, (float[3]){x, 0.012F, oz + hd}, col);
        }
        for (int iz = 0; iz <= (int)(hd * 2.0F); iz++) {
            float z = oz - hd + (float)iz;
            nt_shape_renderer_line((float[3]){ox - hw, 0.012F, z}, (float[3]){ox + hw, 0.012F, z}, col);
        }
    }

    // Far walls only (cutaway) so the camera sees inside. Each wall takes its
    // real inward normal so one catches sun and one falls into cool shadow.
    ll_shade_n(col, 0.78F * dim, 0.82F * dim, 0.86F * dim, 1.0F, LL_NORMAL_PX, s_daylight);
    ll_fog_mix(col, fog, s_daylight);
    {
        float rot[4] = {0.0F, 0.7071068F, 0.0F, 0.7071068F};
        nt_shape_renderer_rect_rot((float[3]){ox - hw, WALL_H * 0.5F, oz}, (float[2]){hd * 2.0F, WALL_H}, rot, col);
    }
    ll_shade_n(col, 0.93F * dim, 0.85F * dim, 0.72F * dim, 1.0F, LL_NORMAL_PZ, s_daylight);
    ll_fog_mix(col, fog, s_daylight);
    nt_shape_renderer_rect((float[3]){ox, WALL_H * 0.5F, oz - hd}, (float[2]){hw * 2.0F, WALL_H}, col);

    // Active-lot marker ring on the plot
    if (active) {
        float ring[4];
        ll_shade_flat(ring, 1.0F, 0.92F, 0.35F, 1.0F, s_daylight);
        nt_shape_renderer_rect_wire_rot((float[3]){ox, 0.02F, oz}, (float[2]){hw * 2.0F + 2.4F, hd * 2.0F + 2.4F}, flat, ring);
    }
}

// Map a furniture kind to a Kenney mesh (or -1 for shape-drawn, e.g. the door).
static int mesh_for_kind(int kind) {
    switch (kind) {
    case OBJ_BED: return LL_MESH_BED;
    case OBJ_FRIDGE: return LL_MESH_FRIDGE;
    case OBJ_SHOWER: return LL_MESH_SHOWER;
    case OBJ_TOILET: return LL_MESH_TOILET;
    case OBJ_SOFA: return LL_MESH_SOFA;
    case OBJ_COMPUTER: return LL_MESH_DESK;
    default: return -1;
    }
}

static float s_mesh_scratch[3 * 1024];

// Round contact shadow (fake AO) on the floor to ground a prop/sim. Opaque (the
// renderer can't blend), faked soft with 3 concentric rings: subtle edge -> dark
// core, each lifted slightly so the inner ring wins the depth test.
static void draw_contact_shadow(float x, float z, float radius) {
    float flat[4] = {0.7071068F, 0.0F, 0.0F, 0.7071068F};
    static const float ring_r[3] = {1.0F, 0.68F, 0.40F};
    static const float ring_t[3] = {1.0F, 0.5F, 0.0F}; // edge..core
    for (int i = 0; i < 3; i++) {
        float t[3];
        ll_ao_tone(ring_t[i], s_daylight, t);
        nt_shape_renderer_circle_rot((float[3]){x, 0.018F + 0.004F * (float)i, z},
                                     radius * ring_r[i], flat, (float[4]){t[0], t[1], t[2], 1.0F});
    }
}

// Draw every per-material sub-mesh of a model FACETED: each triangle is shaded by
// its own world-space face normal against the baked sun, giving crisp low-poly
// facets (the target look) instead of one flat colour per material.
static void draw_mesh_object(const Object *o, int mesh_id, float yaw) {
    const LLMesh *m = &LL_MESHES[mesh_id];
    float cs = cosf(yaw);
    float sn = sinf(yaw);
    float fog = cam_dist(o->x, o->z);
    for (int s = 0; s < m->nsubs; s++) {
        const LLSub *sm = &m->subs[s];
        if (sm->nverts > 1024) {
            continue;
        }
        for (int i = 0; i < sm->nverts; i++) {
            float x = sm->pos[i * 3 + 0];
            float y = sm->pos[i * 3 + 1];
            float z = sm->pos[i * 3 + 2];
            s_mesh_scratch[i * 3 + 0] = o->x + (x * cs + z * sn);
            s_mesh_scratch[i * 3 + 1] = y;
            s_mesh_scratch[i * 3 + 2] = o->z + (-x * sn + z * cs);
        }
        for (int t = 0; t + 2 < sm->nidx; t += 3) {
            const float *a = &s_mesh_scratch[sm->idx[t + 0] * 3];
            const float *b = &s_mesh_scratch[sm->idx[t + 1] * 3];
            const float *c = &s_mesh_scratch[sm->idx[t + 2] * 3];
            float n[3];
            ll_face_normal(a, b, c, n);
            float col[4];
            ll_shade_n(col, sm->r, sm->g, sm->b, 1.0F, n, s_daylight);
            ll_fog_mix(col, fog, s_daylight);
            nt_shape_renderer_triangle(a, b, c, col);
        }
    }
}

static void draw_object(const Object *o) {
    float col[4];
    int mid = mesh_for_kind(o->kind);
    float shadow_r = (o->kind == OBJ_BED) ? 1.45F : (o->kind == OBJ_SOFA ? 1.15F : 0.95F);
    draw_contact_shadow(o->x, o->z, shadow_r);
    if (mid >= 0) {
        // Face the room: the use-spot points inward from the object.
        float yaw = atan2f(o->usx - o->x, o->usz - o->z);
        draw_mesh_object(o, mid, yaw);
        if (o->kind == OBJ_COMPUTER) { // add a monitor on the desk
            shade(col, 0.20F, 0.22F, 0.28F, 1.0F);
            nt_shape_renderer_cube((float[3]){o->x, 1.18F, o->z}, (float[3]){0.5F, 0.36F, 0.06F}, col);
            shade(col, 0.40F, 0.85F, 1.0F, 1.0F);
            nt_shape_renderer_cube((float[3]){o->x, 1.20F, o->z + 0.04F}, (float[3]){0.42F, 0.28F, 0.02F}, col);
        }
        return;
    }
    const float *base = object_color(o->kind);
    float x = o->x;
    float z = o->z;
    switch (o->kind) {
    case OBJ_DOOR:
        shade(col, base[0], base[1], base[2], 1.0F);
        nt_shape_renderer_cube((float[3]){x, 1.1F, z}, (float[3]){1.3F, 2.2F, 0.25F}, col);
        shade(col, 0.95F, 0.85F, 0.2F, 1.0F); // knob
        nt_shape_renderer_sphere((float[3]){x + 0.45F, 1.1F, z + 0.18F}, 0.1F, col);
        break;
    default:
        break;
    }
}

static void draw_sim(int idx) {
    Sim *s = &s_sims[idx];
    if (s->at_work) {
        return;
    }
    draw_contact_shadow(s->x, s->z, 0.40F);
    bool selected = (idx == g_game_state.selected_sim);
    bool walking = (s->action == GAME_STATE_SIM_ACTION_WALKING);
    float lift = walking ? fabsf(sinf(s->bob)) * 0.05F : (s->action == GAME_STATE_SIM_ACTION_SLEEP ? -0.25F : 0.0F);

    // Blocky humanoid: legs, torso, arms, head, hair. Limbs offset by the
    // facing's right vector; legs/arms swing along forward while walking.
    float fx = sinf(s->facing);
    float fz = cosf(s->facing);
    float rx = cosf(s->facing);
    float rz = -sinf(s->facing);
    float sw = walking ? sinf(s->bob) : 0.0F;
    float bx = s->x;
    float bz = s->z;
    float col[4];

    shade(col, s->col[0] * 0.45F, s->col[1] * 0.45F, s->col[2] * 0.55F, 1.0F); // pants
    for (int leg = 0; leg < 2; leg++) {
        float side = (leg == 0) ? 1.0F : -1.0F;
        float ox = rx * 0.12F * side + fx * sw * 0.16F * side;
        float oz = rz * 0.12F * side + fz * sw * 0.16F * side;
        nt_shape_renderer_cube((float[3]){bx + ox, 0.30F + lift, bz + oz}, (float[3]){0.18F, 0.6F, 0.22F}, col);
    }
    shade(col, s->col[0], s->col[1], s->col[2], 1.0F); // shirt/torso
    nt_shape_renderer_cube((float[3]){bx, 0.92F + lift, bz}, (float[3]){0.52F, 0.64F, 0.34F}, col);
    shade(col, s->col[0] * 0.85F + 0.1F, s->col[1] * 0.85F + 0.1F, s->col[2] * 0.85F + 0.1F, 1.0F); // arms
    for (int arm = 0; arm < 2; arm++) {
        float side = (arm == 0) ? 1.0F : -1.0F;
        float ox = rx * 0.34F * side - fx * sw * 0.16F * side;
        float oz = rz * 0.34F * side - fz * sw * 0.16F * side;
        nt_shape_renderer_cube((float[3]){bx + ox, 0.92F + lift, bz + oz}, (float[3]){0.14F, 0.52F, 0.18F}, col);
    }
    shade(col, 0.98F, 0.84F, 0.66F, 1.0F); // head (skin)
    nt_shape_renderer_sphere((float[3]){bx, 1.5F + lift, bz}, 0.27F, col);
    shade(col, s->col[0] * 0.4F, s->col[1] * 0.32F, s->col[2] * 0.28F, 1.0F); // hair cap
    nt_shape_renderer_cube((float[3]){bx, 1.66F + lift, bz}, (float[3]){0.44F, 0.18F, 0.44F}, col);

    // Plumbob (mood diamond) above head
    float mood = sim_mood(s) / 100.0F;
    float pc[4];
    bar_color(mood, pc);
    pc[0] *= s_daylight;
    pc[1] *= s_daylight;
    pc[2] *= s_daylight;
    float py = 2.15F + lift + (selected ? 0.06F * sinf(s->bob * 0.6F) : 0.0F);
    float dia[4] = {0.0F, 0.0F, 0.7071068F, 0.7071068F}; // 45deg about Z
    nt_shape_renderer_cube_rot((float[3]){s->x, py, s->z}, (float[3]){0.18F, 0.18F, 0.18F}, dia, pc);

    if (selected) {
        float ring[4];
        shade(ring, 1.0F, 0.95F, 0.4F, 1.0F);
        nt_shape_renderer_rect_wire_rot((float[3]){s->x, 0.04F, s->z}, (float[2]){1.1F, 1.1F},
                                        (float[4]){0.7071068F, 0.0F, 0.0F, 0.7071068F}, ring);
    }
}

static void draw_build_ghost(void) {
    if (g_game_state.mode_index != GAME_STATE_GAME_MODE_BUILD) {
        return;
    }
    float col[4] = {0.4F, 1.0F, 0.6F, 0.55F};
    nt_shape_renderer_cube((float[3]){s_build_cx, 0.6F, s_build_cz}, (float[3]){1.2F, 1.2F, 1.2F}, col);
    float ring[4] = {0.3F, 1.0F, 0.5F, 1.0F};
    nt_shape_renderer_rect_wire_rot((float[3]){s_build_cx, 0.05F, s_build_cz}, (float[2]){1.6F, 1.6F},
                                    (float[4]){0.7071068F, 0.0F, 0.0F, 0.7071068F}, ring);
}

static void render_world(void) {
    draw_ground();
    for (int l = 0; l < s_lot_count; l++) {
        draw_lot(&s_lots[l], l == s_active_lot);
    }
    for (int i = 0; i < s_object_count; i++) {
        draw_object(&s_objects[i]);
    }
    nt_shape_renderer_flush(); // bound the vertex batch (ground + walls + furniture meshes)
    for (int i = 0; i < s_sim_count; i++) {
        draw_sim(i);
    }
    draw_build_ghost();
}
// #endregion

// #region 2D HUD
static void rect2(float x, float y, float w, float h, const float color[4]) {
    nt_shape_renderer_rect((float[3]){x + w * 0.5F, y + h * 0.5F, 0.0F}, (float[2]){w, h}, color);
}

static void rect2_wire(float x, float y, float w, float h, const float color[4]) {
    nt_shape_renderer_rect_wire((float[3]){x + w * 0.5F, y + h * 0.5F, 0.0F}, (float[2]){w, h}, color);
}

static void hud_layout(float w, float h) {
    // bottom need bars + buttons for selected sim
    float bw = 150.0F;
    float bh = 30.0F;
    float gap = 8.0F;
    float total = (float)NEED_COUNT * (bw + gap) - gap;
    float startx = (w - total) * 0.5F;
    float y = h - 54.0F;
    for (int i = 0; i < NEED_COUNT; i++) {
        s_need_btn[i] = (Box){startx + (float)i * (bw + gap), y, bw, bh};
    }
    s_work_btn = (Box){w - 150.0F, h - 100.0F, 130.0F, 34.0F};
    s_mode_btn = (Box){20.0F, h - 100.0F, 130.0F, 34.0F};
    s_view_btn = (Box){160.0F, h - 100.0F, 130.0F, 34.0F};

    // active household portraits (top of screen, Y-up)
    for (int i = 0; i < LOT_SIMS; i++) {
        s_sim_portrait[i] = (Box){20.0F + (float)i * 64.0F, 20.0F, 54.0F, 54.0F};
    }
    // neighborhood lot selector strip (under portraits)
    for (int i = 0; i < s_lot_count; i++) {
        s_lot_btn[i] = (Box){20.0F + (float)i * 40.0F, 84.0F, 32.0F, 22.0F};
    }

    // build palette (top-center) — only meaningful in build mode
    float pw = 70.0F;
    float pgap = 8.0F;
    float ptotal = (float)BUILD_PALETTE_COUNT * (pw + pgap) - pgap;
    float px = (w - ptotal) * 0.5F;
    for (int i = 0; i < BUILD_PALETTE_COUNT; i++) {
        s_palette_btn[i] = (Box){px + (float)i * (pw + pgap), 20.0F, pw, 44.0F};
    }
}

static void draw_panel(float x, float y, float w, float h) {
    rect2(x, y, w, h, (float[4]){0.10F, 0.12F, 0.18F, 0.82F});
    rect2(x, y, w, 3.0F, (float[4]){1.0F, 1.0F, 1.0F, 0.18F});
}

static void draw_sim_billboards(void) {
    // Small mood bar floating above every Sim's head (read all Sims at a glance).
    for (int i = 0; i < s_sim_count; i++) {
        if (s_sims[i].at_work) {
            continue;
        }
        float sx;
        float sy;
        if (!world_to_screen(s_sims[i].x, 2.95F, s_sims[i].z, &sx, &sy)) {
            continue;
        }
        float bw = 46.0F;
        float bh = 7.0F;
        float mood = sim_mood(&s_sims[i]) / 100.0F;
        rect2(sx - bw * 0.5F - 1.0F, sy - 1.0F, bw + 2.0F, bh + 2.0F, (float[4]){0.0F, 0.0F, 0.0F, 0.5F});
        float mc[4];
        bar_color(mood, mc);
        rect2(sx - bw * 0.5F, sy, bw * mood, bh, mc);
    }
}

static void draw_hud(float w, float h) {
    int sel = g_game_state.selected_sim;
    bool build = (g_game_state.mode_index == GAME_STATE_GAME_MODE_BUILD);

    draw_sim_billboards();

    // Top status strip: day/night dot (money + clock text drawn by draw_text_hud)
    draw_panel(w - 220.0F, 14.0F, 206.0F, 46.0F);
    float dn[4];
    bar_color(clampf((s_daylight - 0.58F) / 0.42F, 0.0F, 1.0F) * 0.5F + 0.5F, dn);
    nt_shape_renderer_circle((float[3]){w - 196.0F, 37.0F, 0.0F}, 11.0F, (float[4]){dn[0], dn[1], dn[2], 1.0F});

    // Active household portraits (selectable)
    int base = lot_sim_base(s_active_lot);
    for (int k = 0; k < LOT_SIMS; k++) {
        int gi = base + k;
        Box b = s_sim_portrait[k];
        rect2(b.x - 3.0F, b.y - 3.0F, b.w + 6.0F, b.h + 6.0F,
              (gi == sel) ? (float[4]){1.0F, 0.95F, 0.4F, 1.0F} : (float[4]){0.0F, 0.0F, 0.0F, 0.5F});
        rect2(b.x, b.y, b.w, b.h, (float[4]){s_sims[gi].col[0], s_sims[gi].col[1], s_sims[gi].col[2], 1.0F});
        float mc[4];
        bar_color(sim_mood(&s_sims[gi]) / 100.0F, mc);
        rect2(b.x, b.y + b.h - 8.0F, b.w, 8.0F, mc);
        if (s_sims[gi].at_work) {
            rect2(b.x + b.w - 14.0F, b.y + 2.0F, 12.0F, 12.0F, (float[4]){0.2F, 0.5F, 1.0F, 1.0F});
        }
    }

    // Neighborhood lot strip (which household is active)
    for (int l = 0; l < s_lot_count; l++) {
        Box b = s_lot_btn[l];
        rect2(b.x, b.y, b.w, b.h,
              (l == s_active_lot) ? (float[4]){0.95F, 0.8F, 0.3F, 1.0F} : (float[4]){0.3F, 0.34F, 0.42F, 1.0F});
        rect2_wire(b.x, b.y, b.w, b.h, (float[4]){1.0F, 1.0F, 1.0F, 0.25F});
        // house glyph
        rect2(b.x + b.w * 0.5F - 6.0F, b.y + 6.0F, 12.0F, 9.0F, (float[4]){0.9F, 0.9F, 0.95F, 0.9F});
    }

    // Mode button (LIVE / BUILD)
    rect2(s_mode_btn.x, s_mode_btn.y, s_mode_btn.w, s_mode_btn.h,
          build ? (float[4]){0.3F, 0.8F, 0.5F, 1.0F} : (float[4]){0.25F, 0.4F, 0.7F, 1.0F});
    rect2_wire(s_mode_btn.x, s_mode_btn.y, s_mode_btn.w, s_mode_btn.h, (float[4]){1.0F, 1.0F, 1.0F, 0.4F});

    // Overview/map toggle button
    rect2(s_view_btn.x, s_view_btn.y, s_view_btn.w, s_view_btn.h,
          s_overview ? (float[4]){0.85F, 0.6F, 0.25F, 1.0F} : (float[4]){0.3F, 0.5F, 0.45F, 1.0F});
    rect2_wire(s_view_btn.x, s_view_btn.y, s_view_btn.w, s_view_btn.h, (float[4]){1.0F, 1.0F, 1.0F, 0.4F});
    // map glyph (2x2 houses)
    rect2(s_view_btn.x + 10.0F, s_view_btn.y + 9.0F, 7.0F, 7.0F, (float[4]){0.95F, 0.95F, 1.0F, 0.9F});
    rect2(s_view_btn.x + 20.0F, s_view_btn.y + 9.0F, 7.0F, 7.0F, (float[4]){0.95F, 0.95F, 1.0F, 0.9F});
    rect2(s_view_btn.x + 10.0F, s_view_btn.y + 19.0F, 7.0F, 7.0F, (float[4]){0.95F, 0.95F, 1.0F, 0.9F});
    rect2(s_view_btn.x + 20.0F, s_view_btn.y + 19.0F, 7.0F, 7.0F, (float[4]){0.95F, 0.95F, 1.0F, 0.9F});

    if (build) {
        // Build palette
        draw_panel((w - (float)BUILD_PALETTE_COUNT * 78.0F) * 0.5F - 8.0F, 12.0F,
                   (float)BUILD_PALETTE_COUNT * 78.0F + 16.0F, 60.0F);
        for (int i = 0; i < BUILD_PALETTE_COUNT; i++) {
            Box b = s_palette_btn[i];
            const float *oc = object_color(i);
            rect2(b.x, b.y, b.w, b.h, (float[4]){oc[0], oc[1], oc[2], 1.0F});
            rect2_wire(b.x, b.y, b.w, b.h,
                       (i == s_build_kind) ? (float[4]){1.0F, 0.95F, 0.3F, 1.0F} : (float[4]){0.0F, 0.0F, 0.0F, 0.5F});
            // price pips
            int pp = object_price(i) / 100;
            for (int k = 0; k < pp; k++) {
                rect2(b.x + 4.0F + (float)k * 7.0F, b.y + b.h - 8.0F, 5.0F, 4.0F, (float[4]){1.0F, 0.85F, 0.2F, 1.0F});
            }
        }
        return; // hide need bars in build mode
    }

    // Selected Sim need bars + command buttons (bottom)
    draw_panel(0.0F, h - 70.0F, w, 70.0F);
    for (int i = 0; i < NEED_COUNT; i++) {
        Box b = s_need_btn[i];
        // icon (need color) on the left
        rect2(b.x, b.y, 26.0F, b.h, (float[4]){need_color(i)[0], need_color(i)[1], need_color(i)[2], 1.0F});
        // bar track + fill
        float v = s_sims[sel].needs[i] / 100.0F;
        rect2(b.x + 30.0F, b.y + 6.0F, b.w - 34.0F, b.h - 12.0F, (float[4]){0.0F, 0.0F, 0.0F, 0.5F});
        float fc[4];
        bar_color(v, fc);
        rect2(b.x + 30.0F, b.y + 6.0F, (b.w - 34.0F) * v, b.h - 12.0F, fc);
        // hover/lowest highlight
        rect2_wire(b.x, b.y, b.w, b.h, (float[4]){1.0F, 1.0F, 1.0F, 0.18F});
    }

    // Work button
    rect2(s_work_btn.x, s_work_btn.y, s_work_btn.w, s_work_btn.h, (float[4]){0.2F, 0.45F, 0.85F, 1.0F});
    rect2_wire(s_work_btn.x, s_work_btn.y, s_work_btn.w, s_work_btn.h, (float[4]){1.0F, 1.0F, 1.0F, 0.4F});
    // briefcase glyph
    rect2(s_work_btn.x + s_work_btn.w * 0.5F - 12.0F, s_work_btn.y + 9.0F, 24.0F, 16.0F,
          (float[4]){0.85F, 0.7F, 0.3F, 1.0F});

    // Selected Sim skills (3 mini bars) + career-level pips, bottom-left of panel.
    const Sim *ss = &s_sims[sel];
    static const float skill_col[SKILL_COUNT][4] = {
        {0.95F, 0.6F, 0.3F, 1.0F}, // cooking
        {0.4F, 0.7F, 1.0F, 1.0F},  // logic
        {1.0F, 0.5F, 0.7F, 1.0F},  // charisma
    };
    for (int k = 0; k < SKILL_COUNT; k++) {
        float by = h - 64.0F + (float)k * 13.0F;
        rect2(14.0F, by, 12.0F, 10.0F, (float[4]){skill_col[k][0], skill_col[k][1], skill_col[k][2], 1.0F});
        rect2(30.0F, by + 1.0F, 90.0F, 8.0F, (float[4]){0.0F, 0.0F, 0.0F, 0.5F});
        rect2(30.0F, by + 1.0F, 90.0F * (ss->skills[k] / 10.0F), 8.0F,
              (float[4]){skill_col[k][0], skill_col[k][1], skill_col[k][2], 1.0F});
    }
    // career level pips
    for (int k = 0; k < CAREER_MAX_LEVEL; k++) {
        rect2(130.0F + (float)k * 9.0F, h - 62.0F, 7.0F, 7.0F,
              (k < ss->career_level) ? (float[4]){1.0F, 0.85F, 0.2F, 1.0F} : (float[4]){0.0F, 0.0F, 0.0F, 0.45F});
    }
}
// #endregion

// #region input
static void set_active_lot(int lot) {
    if (lot < 0 || lot >= s_lot_count) {
        return;
    }
    s_active_lot = lot;
    s_overview = false;
    g_game_state.selected_sim = lot_sim_base(lot);
    g_game_state.camera_target_x = s_lots[lot].ox;
    g_game_state.camera_target_z = s_lots[lot].oz - 0.5F;
    s_build_cx = s_lots[lot].ox;
    s_build_cz = s_lots[lot].oz;
    game_state_mark_dirty();
}

static void pick_world(float mx, float my) {
    // Overview map: click a house to enter that lot.
    if (s_overview) {
        int bl = -1;
        float bld = 120.0F * 120.0F;
        for (int l = 0; l < s_lot_count; l++) {
            float sx;
            float sy;
            if (!world_to_screen(s_lots[l].ox, 1.0F, s_lots[l].oz, &sx, &sy)) {
                continue;
            }
            float d = (sx - mx) * (sx - mx) + (sy - my) * (sy - my);
            if (d < bld) {
                bld = d;
                bl = l;
            }
        }
        if (bl >= 0) {
            set_active_lot(bl);
        }
        return;
    }
    int sel = g_game_state.selected_sim;
    int base = lot_sim_base(s_active_lot);
    // 1) select a Sim in the active household
    int best = -1;
    float bestd = 34.0F * 34.0F;
    for (int k = 0; k < LOT_SIMS; k++) {
        int i = base + k;
        if (s_sims[i].at_work) {
            continue;
        }
        float sx;
        float sy;
        if (!world_to_screen(s_sims[i].x, 1.0F, s_sims[i].z, &sx, &sy)) {
            continue;
        }
        float d = (sx - mx) * (sx - mx) + (sy - my) * (sy - my);
        if (d < bestd) {
            bestd = d;
            best = i;
        }
    }
    if (best >= 0) {
        g_game_state.selected_sim = best;
        game_state_mark_dirty();
        return;
    }
    // 2) otherwise command the selected Sim to a clicked object in this lot
    int bo = -1;
    float bod = 44.0F * 44.0F;
    for (int i = 0; i < s_object_count; i++) {
        if (s_objects[i].lot != s_active_lot) {
            continue;
        }
        float sx;
        float sy;
        if (!world_to_screen(s_objects[i].x, 0.8F, s_objects[i].z, &sx, &sy)) {
            continue;
        }
        float d = (sx - mx) * (sx - mx) + (sy - my) * (sy - my);
        if (d < bod) {
            bod = d;
            bo = i;
        }
    }
    if (bo >= 0) {
        if (s_objects[bo].kind == OBJ_DOOR) {
            command_work(sel, true);
        } else {
            command_need(sel, object_need(s_objects[bo].kind), true);
        }
    }
}

static void try_place_object(void) {
    int price = object_price(s_build_kind);
    if (g_game_state.wallet_simoleons < price) {
        return;
    }
    float lx = clampf(s_build_cx - s_lots[s_active_lot].ox, -ROOM_HW + 0.6F, ROOM_HW - 0.6F);
    float lz = clampf(s_build_cz - s_lots[s_active_lot].oz, -ROOM_HD + 0.6F, ROOM_HD - 0.6F);
    int before = s_object_count;
    place_object(s_active_lot, s_build_kind, lx, lz);
    if (s_object_count > before) {
        g_game_state.wallet_simoleons -= price;
        game_state_mark_dirty();
    }
}

static void remove_object_at_cursor(void) {
    int best = -1;
    float bestd = 1.4F * 1.4F;
    for (int i = 0; i < s_object_count; i++) {
        if (s_objects[i].kind == OBJ_DOOR || s_objects[i].lot != s_active_lot) {
            continue;
        }
        float dx = s_objects[i].x - s_build_cx;
        float dz = s_objects[i].z - s_build_cz;
        float d = dx * dx + dz * dz;
        if (d < bestd) {
            bestd = d;
            best = i;
        }
    }
    if (best < 0) {
        return;
    }
    g_game_state.wallet_simoleons += object_price(s_objects[best].kind) / 2; // refund half
    // free any sim using it
    for (int i = 0; i < s_sim_count; i++) {
        if (s_sims[i].target_obj == best) {
            go_idle(&s_sims[i]);
        }
    }
    s_objects[best] = s_objects[s_object_count - 1];
    s_object_count--;
    game_state_mark_dirty();
}

static void handle_input(float w, float h) {
    const nt_pointer_t *p = &g_nt_input.pointers[0];
    bool build = (g_game_state.mode_index == GAME_STATE_GAME_MODE_BUILD);

    // ---- keyboard: global ----
    if (nt_input_key_is_pressed(NT_KEY_TAB)) {
        int base = lot_sim_base(s_active_lot);
        int cur = g_game_state.selected_sim - base;
        if (cur < 0 || cur >= LOT_SIMS) {
            cur = 0;
        }
        g_game_state.selected_sim = base + (cur + 1) % LOT_SIMS;
        game_state_mark_dirty();
    }
    if (nt_input_key_is_pressed(NT_KEY_N)) {
        set_active_lot((s_active_lot + 1) % s_lot_count); // travel to next household
    }
    if (nt_input_key_is_pressed(NT_KEY_M)) {
        s_overview = !s_overview; // neighborhood map toggle
    }
    if (nt_input_key_is_pressed(NT_KEY_B)) {
        g_game_state.mode_index = build ? GAME_STATE_GAME_MODE_LIVE : GAME_STATE_GAME_MODE_BUILD;
        game_state_mark_dirty();
        build = !build;
    }
    if (nt_input_key_is_pressed(NT_KEY_SPACE)) {
        s_paused = !s_paused;
    }

    // ---- camera: arrows rotate, WASD pan, wheel zoom ----
    float dt = g_nt_app.dt;
    if (nt_input_key_is_down(NT_KEY_ARROW_LEFT)) {
        g_game_state.camera_yaw -= 1.4F * dt;
    }
    if (nt_input_key_is_down(NT_KEY_ARROW_RIGHT)) {
        g_game_state.camera_yaw += 1.4F * dt;
    }
    if (nt_input_key_is_down(NT_KEY_ARROW_UP)) {
        g_game_state.camera_pitch = clampf(g_game_state.camera_pitch + 1.0F * dt, 0.12F, 1.45F);
    }
    if (nt_input_key_is_down(NT_KEY_ARROW_DOWN)) {
        g_game_state.camera_pitch = clampf(g_game_state.camera_pitch - 1.0F * dt, 0.12F, 1.45F);
    }
    float pan = 7.0F * dt;
    float yaw = g_game_state.camera_yaw;
    if (nt_input_key_is_down(NT_KEY_W)) {
        g_game_state.camera_target_x -= sinf(yaw) * pan;
        g_game_state.camera_target_z -= cosf(yaw) * pan;
    }
    if (nt_input_key_is_down(NT_KEY_S)) {
        g_game_state.camera_target_x += sinf(yaw) * pan;
        g_game_state.camera_target_z += cosf(yaw) * pan;
    }
    if (nt_input_key_is_down(NT_KEY_A)) {
        g_game_state.camera_target_x -= cosf(yaw) * pan;
        g_game_state.camera_target_z += sinf(yaw) * pan;
    }
    if (nt_input_key_is_down(NT_KEY_D)) {
        g_game_state.camera_target_x += cosf(yaw) * pan;
        g_game_state.camera_target_z -= sinf(yaw) * pan;
    }
    g_game_state.camera_target_x = clampf(g_game_state.camera_target_x, -40.0F, 40.0F);
    g_game_state.camera_target_z = clampf(g_game_state.camera_target_z, -40.0F, 40.0F);

    if (fabsf(p->wheel_dy) > 0.001F) {
        g_game_state.camera_distance = clampf(g_game_state.camera_distance + p->wheel_dy * 1.6F, 6.0F, 48.0F);
    }
    // right-drag orbit
    if (p->buttons[NT_BUTTON_RIGHT].is_down) {
        g_game_state.camera_yaw += p->dx * 0.006F;
        g_game_state.camera_pitch = clampf(g_game_state.camera_pitch - p->dy * 0.006F, 0.12F, 1.45F);
    }

    // ---- build-mode controls ----
    if (build) {
        for (int i = 0; i < BUILD_PALETTE_COUNT; i++) {
            if (nt_input_key_is_pressed((nt_key_t)(NT_KEY_1 + i))) {
                s_build_kind = i;
            }
        }
        float step = 1.0F;
        if (nt_input_key_is_pressed(NT_KEY_I)) {
            s_build_cz -= step;
        }
        if (nt_input_key_is_pressed(NT_KEY_K)) {
            s_build_cz += step;
        }
        if (nt_input_key_is_pressed(NT_KEY_J)) {
            s_build_cx -= step;
        }
        if (nt_input_key_is_pressed(NT_KEY_L)) {
            s_build_cx += step;
        }
        // keep the build cursor inside the active lot
        s_build_cx = clampf(s_build_cx, s_lots[s_active_lot].ox - ROOM_HW + 0.6F, s_lots[s_active_lot].ox + ROOM_HW - 0.6F);
        s_build_cz = clampf(s_build_cz, s_lots[s_active_lot].oz - ROOM_HD + 0.6F, s_lots[s_active_lot].oz + ROOM_HD - 0.6F);
        if (nt_input_key_is_pressed(NT_KEY_ENTER)) {
            try_place_object();
        }
        if (nt_input_key_is_pressed(NT_KEY_X) || nt_input_key_is_pressed(NT_KEY_DELETE)) {
            remove_object_at_cursor();
        }
    } else {
        // ---- live-mode command keys: 1..6 needs, 7/G work ----
        for (int i = 0; i < NEED_COUNT; i++) {
            if (nt_input_key_is_pressed((nt_key_t)(NT_KEY_1 + i))) {
                command_need(g_game_state.selected_sim, i, true);
            }
        }
        if (nt_input_key_is_pressed(NT_KEY_7) || nt_input_key_is_pressed(NT_KEY_G)) {
            command_work(g_game_state.selected_sim, true);
        }
    }

    // ---- left click: HUD first, then world ----
    if (p->buttons[NT_BUTTON_LEFT].is_pressed) {
        float mx = p->x;
        float my = p->y;
        if (box_has(s_mode_btn, mx, my)) {
            g_game_state.mode_index = build ? GAME_STATE_GAME_MODE_LIVE : GAME_STATE_GAME_MODE_BUILD;
            game_state_mark_dirty();
            return;
        }
        if (box_has(s_view_btn, mx, my)) {
            s_overview = !s_overview;
            return;
        }
        for (int l = 0; l < s_lot_count; l++) {
            if (box_has(s_lot_btn[l], mx, my)) {
                set_active_lot(l);
                return;
            }
        }
        int base = lot_sim_base(s_active_lot);
        for (int k = 0; k < LOT_SIMS; k++) {
            if (box_has(s_sim_portrait[k], mx, my)) {
                g_game_state.selected_sim = base + k;
                game_state_mark_dirty();
                return;
            }
        }
        if (build) {
            for (int i = 0; i < BUILD_PALETTE_COUNT; i++) {
                if (box_has(s_palette_btn[i], mx, my)) {
                    s_build_kind = i;
                    return;
                }
            }
            return;
        }
        for (int i = 0; i < NEED_COUNT; i++) {
            if (box_has(s_need_btn[i], mx, my)) {
                command_need(g_game_state.selected_sim, i, true);
                return;
            }
        }
        if (box_has(s_work_btn, mx, my)) {
            command_work(g_game_state.selected_sim, true);
            return;
        }
        pick_world(mx, my);
    }
    (void)w;
    (void)h;
}
// #endregion

// #region devapi
#if NT_DEVAPI_ENABLED
static const char *action_name(int a) { return game_state_sim_action_name(a); }

static const char *object_kind_name(int k) {
    static const char *n[OBJ_KIND_COUNT] = {"bed", "fridge", "shower", "toilet", "sofa", "computer", "door"};
    return (k >= 0 && k < OBJ_KIND_COUNT) ? n[k] : "?";
}

static cJSON *sims_json(void) {
    cJSON *arr = cJSON_CreateArray();
    static const char *need_names[NEED_COUNT] = {"energy", "hunger", "hygiene", "bladder", "fun", "social"};
    for (int i = 0; i < s_sim_count; i++) {
        const Sim *s = &s_sims[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", s->name);
        cJSON_AddNumberToObject(o, "lot", s->lot);
        cJSON_AddStringToObject(o, "action", action_name(s->action));
        cJSON_AddBoolToObject(o, "at_work", s->at_work);
        cJSON_AddBoolToObject(o, "player_directed", s->player_dir);
        cJSON_AddNumberToObject(o, "x", (double)s->x);
        cJSON_AddNumberToObject(o, "z", (double)s->z);
        cJSON_AddNumberToObject(o, "mood", (double)sim_mood(s));
        cJSON *needs = cJSON_AddObjectToObject(o, "needs");
        for (int n = 0; n < NEED_COUNT; n++) {
            cJSON_AddNumberToObject(needs, need_names[n], (double)s->needs[n]);
        }
        cJSON *skills = cJSON_AddObjectToObject(o, "skills");
        cJSON_AddNumberToObject(skills, "cooking", (double)s->skills[SKILL_COOKING]);
        cJSON_AddNumberToObject(skills, "logic", (double)s->skills[SKILL_LOGIC]);
        cJSON_AddNumberToObject(skills, "charisma", (double)s->skills[SKILL_CHARISMA]);
        cJSON *career = cJSON_AddObjectToObject(o, "career");
        cJSON_AddNumberToObject(career, "level", s->career_level);
        cJSON_AddNumberToObject(career, "perf", (double)s->career_perf);
        cJSON_AddNumberToObject(career, "shifts", s->shifts);
        // Relationships to housemates (same lot).
        cJSON *rel = cJSON_AddObjectToObject(o, "relationships");
        for (int j = 0; j < s_sim_count; j++) {
            if (j != i && s_sims[j].lot == s->lot) {
                cJSON_AddNumberToObject(rel, s_sims[j].name, (double)s_rel[i][j]);
            }
        }
        cJSON_AddItemToArray(arr, o);
    }
    return arr;
}

static cJSON *objects_json(void) {
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < s_object_count; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "kind", object_kind_name(s_objects[i].kind));
        cJSON_AddNumberToObject(o, "x", (double)s_objects[i].x);
        cJSON_AddNumberToObject(o, "z", (double)s_objects[i].z);
        cJSON_AddNumberToObject(o, "used_by", s_objects[i].used_by);
        cJSON_AddItemToArray(arr, o);
    }
    return arr;
}

static void emit_state(cJSON *result_obj) {
    cJSON *root = game_state_to_json(&g_game_state);
    cJSON_AddStringToObject(root, "runtime", "little_lives");
    cJSON_AddStringToObject(root, "mode", game_state_game_mode_name(g_game_state.mode_index));
    cJSON_AddBoolToObject(root, "paused", s_paused);
    cJSON_AddNumberToObject(root, "active_lot", s_active_lot);
    cJSON_AddNumberToObject(root, "lot_count", s_lot_count);
    cJSON_AddBoolToObject(root, "overview", s_overview);
    cJSON_AddItemToObject(root, "sims", sims_json());
    cJSON_AddItemToObject(root, "objects", objects_json());
    cJSON_AddItemToObject(result_obj, "state", root);
}

static bool ep_state(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    emit_state(result_obj);
    return true;
}

static bool ep_reset(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    game_reset();
    emit_state(result_obj);
    return true;
}

static bool ep_command(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)err;
    (void)user;
    const cJSON *sim = cJSON_GetObjectItemCaseSensitive(params, "sim");
    const cJSON *need = cJSON_GetObjectItemCaseSensitive(params, "need");
    const cJSON *work = cJSON_GetObjectItemCaseSensitive(params, "work");
    int si = (cJSON_IsNumber(sim)) ? (int)sim->valuedouble : g_game_state.selected_sim;
    if (cJSON_IsBool(work) && cJSON_IsTrue(work)) {
        command_work(si, true);
    } else if (cJSON_IsNumber(need)) {
        command_need(si, (int)need->valuedouble, true);
    }
    emit_state(result_obj);
    return true;
}

static bool ep_select(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)err;
    (void)user;
    const cJSON *sim = cJSON_GetObjectItemCaseSensitive(params, "sim");
    if (cJSON_IsNumber(sim)) {
        int si = (int)sim->valuedouble;
        if (si >= 0 && si < s_sim_count) {
            s_active_lot = s_sims[si].lot; // selecting follows the household
            s_overview = false;
            g_game_state.selected_sim = si;
            g_game_state.camera_target_x = s_lots[s_active_lot].ox;
            g_game_state.camera_target_z = s_lots[s_active_lot].oz - 0.5F;
            game_state_mark_dirty();
        }
    }
    emit_state(result_obj);
    return true;
}

static bool ep_travel(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)err;
    (void)user;
    const cJSON *lot = cJSON_GetObjectItemCaseSensitive(params, "lot");
    if (cJSON_IsNumber(lot)) {
        set_active_lot((int)lot->valuedouble);
    }
    emit_state(result_obj);
    return true;
}

// Debug aid: pin the clock (and optionally pause) for consistent screenshots.
static bool ep_set_time(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)err;
    (void)user;
    const cJSON *minutes = cJSON_GetObjectItemCaseSensitive(params, "minutes");
    const cJSON *pause = cJSON_GetObjectItemCaseSensitive(params, "pause");
    if (cJSON_IsNumber(minutes)) {
        g_game_state.clock_minutes = clampf((float)minutes->valuedouble, 0.0F, 1439.0F);
    }
    if (cJSON_IsBool(pause)) {
        s_paused = cJSON_IsTrue(pause);
    }
    update_clock(0.0F); // refresh daylight from the new time immediately
    emit_state(result_obj);
    return true;
}

static bool ep_capture(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)err;
    (void)user;
    const cJSON *out = cJSON_GetObjectItemCaseSensitive(params, "output");
    const char *path = (cJSON_IsString(out) && out->valuestring) ? out->valuestring : "tmp/little_lives_capture.ppm";
    (void)snprintf(s_capture_path, sizeof(s_capture_path), "%s", path);
    s_capture_pending = true;
    cJSON_AddStringToObject(result_obj, "path", s_capture_path);
    return true;
}

void game_state_register_devapi(void);

static void register_endpoints(void) {
    static const nt_devapi_command_desc descs[] = {
        {"game.state", "game", "Return Little Lives runtime state (sims, objects, clock).", "", "{state}", "immediate", "none"},
        {"game.reset_playtest", "game", "Reset world + state for automation.", "", "{state}", "immediate", "resets state"},
        {"game.action.command", "game", "Command a sim: {sim,need} or {sim,work:true}.", "{sim,need|work}", "{state}", "next-frame", "mutates sim"},
        {"game.action.select", "game", "Select a sim: {sim}.", "{sim}", "{state}", "immediate", "selects sim"},
        {"game.capture.framebuffer", "game", "Write the framebuffer to a PPM file: {output}.", "{output}", "{path}", "next-frame", "writes a file"},
        {"game.action.travel", "game", "Travel to a household lot: {lot}.", "{lot}", "{state}", "immediate", "switches lot"},
        {"game.debug.set_time", "game", "Pin clock for screenshots: {minutes,pause}.", "{minutes,pause}", "{state}", "immediate", "sets clock"},
    };
    game_state_register_devapi();
    game_devapi_ui_register();
    (void)nt_devapi_register(&descs[0], ep_state, NULL);
    (void)nt_devapi_register(&descs[1], ep_reset, NULL);
    (void)nt_devapi_register(&descs[2], ep_command, NULL);
    (void)nt_devapi_register(&descs[3], ep_select, NULL);
    (void)nt_devapi_register(&descs[4], ep_capture, NULL);
    (void)nt_devapi_register(&descs[5], ep_travel, NULL);
    (void)nt_devapi_register(&descs[6], ep_set_time, NULL);
}

static void register_ui_devapi(float w, float h) {
    game_devapi_ui_clear();
    (void)game_devapi_ui_register_node("root", "", "screen", "Little Lives", "Life sim", 0.0F, 0.0F, w, h, true, true);
    (void)game_devapi_ui_register_node("mode", "root", "button", "Mode", "live/build toggle", s_mode_btn.x, s_mode_btn.y, s_mode_btn.w, s_mode_btn.h, true, true);
    int base = lot_sim_base(s_active_lot);
    for (int k = 0; k < LOT_SIMS; k++) {
        char id[24];
        (void)snprintf(id, sizeof(id), "sim.%d", base + k);
        (void)game_devapi_ui_register_node(id, "root", "button", s_sims[base + k].name, "select sim", s_sim_portrait[k].x, s_sim_portrait[k].y, s_sim_portrait[k].w, s_sim_portrait[k].h, true, true);
    }
    if (g_game_state.mode_index == GAME_STATE_GAME_MODE_LIVE) {
        static const char *need_ids[NEED_COUNT] = {"need.energy", "need.hunger", "need.hygiene", "need.bladder", "need.fun", "need.social"};
        for (int i = 0; i < NEED_COUNT; i++) {
            (void)game_devapi_ui_register_node(need_ids[i], "root", "button", need_ids[i], "command need", s_need_btn[i].x, s_need_btn[i].y, s_need_btn[i].w, s_need_btn[i].h, true, true);
        }
        (void)game_devapi_ui_register_node("work", "root", "button", "Work", "go to work", s_work_btn.x, s_work_btn.y, s_work_btn.w, s_work_btn.h, true, true);
    }
}
#endif
// #endregion

#ifdef LL_HAVE_TEXT
// #region text (engine font via Slug renderer)
static nt_font_t s_font;
static nt_material_t s_text_mat;
static nt_buffer_t s_frame_ubo;
static nt_hash32_t s_pack_id;

static void text_init(void) {
    nt_gfx_register_global_block("Globals", 0);
    nt_http_init();
    nt_fs_init();
    nt_hash_init(&(nt_hash_desc_t){0});
    nt_resource_init(&(nt_resource_desc_t){0});
    nt_resource_set_activator(NT_ASSET_SHADER_CODE, nt_gfx_activate_shader, nt_gfx_deactivate_shader);
    nt_font_init(&(nt_font_desc_t){.max_fonts = 2});
    nt_material_init(&(nt_material_desc_t){.max_materials = 4});
    nt_text_renderer_init();
    s_frame_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_UNIFORM,
        .usage = NT_USAGE_DYNAMIC,
        .size = sizeof(nt_frame_uniforms_t),
        .label = "ll_frame_ubo",
    });
    s_pack_id = nt_hash32_str("little_lives");
    nt_resource_mount(s_pack_id, 10);
    nt_resource_load_auto(s_pack_id, "assets/packs/little_lives.ntpack");
    nt_resource_t vs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SLUG_TEXT_VERT, NT_ASSET_SHADER_CODE);
    nt_resource_t fs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SLUG_TEXT_FRAG, NT_ASSET_SHADER_CODE);
    s_text_mat = nt_material_create(&(nt_material_create_desc_t){
        .vs = vs,
        .fs = fs,
        .blend_mode = NT_BLEND_MODE_ALPHA,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .params[0] = {.name = "u_alpha_cutoff", .value = {NT_TEXT_ALPHA_CUTOFF_DEFAULT}},
        .param_count = 1,
        .label = "ll_text",
    });
    s_font = nt_font_create(&(nt_font_create_desc_t){
        .curve_texture_width = 1024,
        .curve_texture_height = 512,
        .band_texture_height = 256,
        .band_count = 8,
        .measure_cache_size = 256,
    });
    nt_resource_t font_res = nt_resource_request(ASSET_FONT_LITTLE_LIVES_UI_FONT, NT_ASSET_FONT);
    nt_font_add(s_font, font_res);
    nt_resource_set_activate_time_budget(0);
}

static void text_step(void) {
    nt_resource_step();
    nt_material_step();
}

// One HUD label at the screen/UI boundary: (x, y_top) screen pixels (y-down), h_px = em.
static void text_at(float x, float y_top, float h_px, const float color[4], const char *str) {
    float model[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    model[12] = x;
    model[13] = s_view_h - (y_top + h_px); // convert y-down screen to y-up ortho baseline
    nt_text_renderer_draw(str, model, h_px, color, 0.0F, 0.0F);
}

static void draw_text_hud(void) {
    float w = s_view_w;
    float h = s_view_h;
    nt_frame_uniforms_t u = {0};
    float o[16] = {0};
    o[0] = 2.0F / w;
    o[5] = 2.0F / h;
    o[10] = -1.0F;
    o[12] = -1.0F;
    o[13] = -1.0F;
    o[15] = 1.0F;
    float id[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    memcpy(u.view_proj, o, 64);
    memcpy(u.view, id, 64);
    memcpy(u.proj, o, 64);
    u.resolution[0] = w;
    u.resolution[1] = h;
    u.resolution[2] = 1.0F / w;
    u.resolution[3] = 1.0F / h;
    u.near_far[0] = 0.1F;
    u.near_far[1] = 100.0F;
    nt_gfx_update_buffer(s_frame_ubo, &u, sizeof(u));
    nt_gfx_bind_uniform_buffer(s_frame_ubo, 0);
    nt_font_step();
    nt_text_renderer_set_material(s_text_mat);
    nt_text_renderer_set_font(s_font);

    float white[4] = {0.97F, 0.97F, 1.0F, 1.0F};
    float dark[4] = {0.10F, 0.12F, 0.18F, 1.0F};
    char buf[64];
    int sel = g_game_state.selected_sim;
    bool build = (g_game_state.mode_index == GAME_STATE_GAME_MODE_BUILD);

    (void)snprintf(buf, sizeof(buf), "$%d", g_game_state.wallet_simoleons);
    text_at(w - 176.0F, 19.0F, 15.0F, white, buf);
    int mins = (int)g_game_state.clock_minutes;
    (void)snprintf(buf, sizeof(buf), "Day %d  %02d:%02d", g_game_state.clock_day, mins / 60, mins % 60);
    text_at(w - 176.0F, 41.0F, 12.0F, white, buf);

    text_at(20.0F, 112.0F, 13.0F, white, s_lots[s_active_lot].name);
    text_at(20.0F, 130.0F, 17.0F, white, s_sims[sel].name);

    text_at(s_mode_btn.x + 14.0F, s_mode_btn.y + 9.0F, 15.0F, white, build ? "BUILD" : "LIVE");
    text_at(s_view_btn.x + 36.0F, s_view_btn.y + 9.0F, 14.0F, white, "MAP");
    if (!build) {
        text_at(s_work_btn.x + 42.0F, s_work_btn.y + 9.0F, 15.0F, white, "WORK");
        static const char *tag[NEED_COUNT] = {"NRG", "HUN", "HYG", "BLA", "FUN", "SOC"};
        for (int i = 0; i < NEED_COUNT; i++) {
            text_at(s_need_btn[i].x + 33.0F, s_need_btn[i].y + 9.0F, 11.0F, dark, tag[i]);
        }
    }
    nt_text_renderer_flush();
}
// #endregion
#endif

// #region args + frame
static void parse_args(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--devapi") == 0) {
            s_devapi_enabled = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                s_devapi_port = (uint16_t)strtoul(argv[++i], NULL, 10);
            }
        } else if (strcmp(argv[i], "--window-size") == 0 && i + 1 < argc) {
            int width = 0;
            int height = 0;
            if (sscanf(argv[++i], "%dx%d", &width, &height) == 2 && width > 0 && height > 0) {
                s_window_width = width;
                s_window_height = height;
            }
        } else if (strcmp(argv[i], "--fresh-state") == 0 || strcmp(argv[i], "--disable-autosave") == 0) {
            // accepted no-ops (M1 has no disk persistence yet)
        }
    }
}

#ifndef NT_PLATFORM_WEB
static void maybe_write_capture(int w, int h) {
    if (!s_capture_pending) {
        return;
    }
    s_capture_pending = false;
    size_t n = (size_t)w * (size_t)h * 4U;
    unsigned char *buf = (unsigned char *)malloc(n);
    if (!buf) {
        return;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    FILE *f = fopen(s_capture_path, "wb");
    if (f) {
        (void)fprintf(f, "P6\n%d %d\n255\n", w, h);
        for (int y = h - 1; y >= 0; y--) { // glReadPixels is bottom-up; flip to top-down PPM
            for (int x = 0; x < w; x++) {
                const unsigned char *p = buf + ((size_t)y * (size_t)w + (size_t)x) * 4U;
                (void)fwrite(p, 1, 3, f);
            }
        }
        (void)fclose(f);
    }
    free(buf);
}
#else
static void maybe_write_capture(int w, int h) {
    (void)w;
    (void)h;
}
#endif

// Stacked horizontal bands → a low-poly vertical sky gradient (zenith → horizon).
// Drawn in ortho with depth off so the 3D world (depth on) overwrites it.
static void draw_sky_gradient(float w, float h) {
    const int BANDS = 30;
    for (int i = 0; i < BANDS; i++) {
        float y0 = (float)i / (float)BANDS * h;
        float bh = h / (float)BANDS + 1.5F;
        float t = 1.0F - ((float)i + 0.5F) / (float)BANDS; // top=zenith, bottom=horizon
        float c[3];
        ll_sky_color(t, s_daylight, c);
        rect2(0.0F, y0, w, bh, (float[4]){c[0], c[1], c[2], 1.0F});
    }
}

static void frame(void) {
    nt_window_poll();
#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_update();
    }
#endif
    nt_input_poll();

    const float w = (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
    const float h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height : g_nt_window.height);
    s_view_w = w;
    s_view_h = h;

    hud_layout(w, h);
    handle_input(w, h);
    game_update();
#ifdef LL_HAVE_TEXT
    text_step();
#endif

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        register_ui_devapi(w, h);
    }
#endif

#ifndef NT_PLATFORM_WEB
    if (nt_window_should_close() || nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
        nt_app_quit();
    }
#endif

    float aspect = (h > 0.0F) ? (w / h) : 1.0F;
    float eye[3];
    compute_camera(aspect, eye);

    nt_gfx_begin_frame();
    if (g_nt_gfx.context_restored) {
        nt_shape_renderer_restore_gpu();
#ifdef LL_HAVE_TEXT
        nt_resource_invalidate(NT_ASSET_SHADER_CODE);
        nt_resource_invalidate(NT_ASSET_FONT);
        s_frame_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
            .type = NT_BUFFER_UNIFORM,
            .usage = NT_USAGE_DYNAMIC,
            .size = sizeof(nt_frame_uniforms_t),
            .label = "ll_frame_ubo",
        });
        nt_text_renderer_restore_gpu();
#endif
    }
    float horizon[3];
    ll_sky_color(0.0F, s_daylight, horizon);
    nt_gfx_begin_pass(&(nt_pass_desc_t){
        .clear_color = {horizon[0], horizon[1], horizon[2], 1.0F},
        .clear_depth = 1.0F,
    });

    // Sky gradient backdrop (ortho, depth off) — 3D world draws over it.
    float ortho_sky[16];
    memset(ortho_sky, 0, sizeof(ortho_sky));
    ortho_sky[0] = 2.0F / w;
    ortho_sky[5] = -2.0F / h;
    ortho_sky[10] = -1.0F;
    ortho_sky[12] = -1.0F;
    ortho_sky[13] = 1.0F;
    ortho_sky[15] = 1.0F;
    nt_shape_renderer_set_depth(false);
    nt_shape_renderer_set_vp(ortho_sky);
    draw_sky_gradient(w, h);
    nt_shape_renderer_flush();

    // 3D pass
    nt_shape_renderer_set_depth(true);
    nt_shape_renderer_set_vp(s_vp);
    nt_shape_renderer_set_cam_pos(eye);
    nt_shape_renderer_set_depth(true);
    nt_shape_renderer_set_line_width(2.0F);
    render_world();
    nt_shape_renderer_flush();

    // 2D overlay pass (ortho, depth off)
    float ortho[16];
    memset(ortho, 0, sizeof(ortho));
    ortho[0] = 2.0F / w;
    ortho[5] = -2.0F / h;
    ortho[10] = -1.0F;
    ortho[12] = -1.0F;
    ortho[13] = 1.0F;
    ortho[15] = 1.0F;
    nt_shape_renderer_set_vp(ortho);
    nt_shape_renderer_set_depth(false);
    nt_shape_renderer_set_line_width(2.0F);
    draw_hud(w, h);
    nt_shape_renderer_flush();

#ifdef LL_HAVE_TEXT
    draw_text_hud();
#endif

    nt_gfx_end_pass();
    nt_gfx_end_frame();
    maybe_write_capture((int)w, (int)h);
    nt_window_swap_buffers();
}
// #endregion

int main(int argc, char **argv) {
    nt_engine_config_t config = {0};
    config.app_name = "Little Lives";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    parse_args(argc, argv);
    game_reset();

    g_nt_window.title = "Little Lives";
    g_nt_window.width = (uint32_t)s_window_width;
    g_nt_window.height = (uint32_t)s_window_height;
    nt_window_init();
    nt_input_init();

    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    gfx_desc.depth = true;
    nt_gfx_init(&gfx_desc);
    nt_shape_renderer_init();
#ifdef LL_HAVE_TEXT
    text_init();
#endif

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_init();
        register_endpoints();
        if (!nt_devapi_net_start(s_devapi_port)) {
            (void)fprintf(stderr, "Failed to start DevAPI on port %u\n", (unsigned)s_devapi_port);
        }
    }
#endif

#ifdef NT_PLATFORM_WEB
    nt_platform_web_loading_complete();
#endif

    g_nt_app.target_dt = 1.0F / 60.0F;
    nt_app_run(frame);

#ifndef NT_PLATFORM_WEB
#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_net_stop();
        nt_devapi_shutdown();
    }
#endif
    nt_shape_renderer_shutdown();
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
    nt_engine_shutdown();
#endif

    return 0;
}
