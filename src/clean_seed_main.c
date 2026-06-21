#include "app/nt_app.h"
/* Dragon Grove first slice uses nt_shape_renderer as temporary debug debt.
   Replace with generated runtime art and engine text before product pass. */
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "devapi/nt_devapi.h"
#include "devapi/nt_devapi_net.h"
#include "game_devapi_ui.h"
#include "game_state.h"
#include "graphics/nt_gfx.h"
#include "input/nt_input.h"
#include "renderers/nt_shape_renderer.h"
#include "window/nt_window.h"

#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DRAGON_GROVE_DEVAPI_PORT_DEFAULT 9123
#define GROVE_GRID 5
#define GROVE_CELLS (GROVE_GRID * GROVE_GRID)
#define GROVE_TEXT_MAX 96

typedef struct UiBox {
    float x;
    float y;
    float w;
    float h;
} UiBox;

typedef enum GroveItem {
    GROVE_ITEM_EMPTY = 0,
    GROVE_ITEM_EGG = 1,
    GROVE_ITEM_HATCHLING = 2,
    GROVE_ITEM_SPROUT = 3,
    GROVE_ITEM_BLOOM = 4,
} GroveItem;

typedef struct GroveCell {
    GroveItem item;
    bool shadowed;
} GroveCell;

typedef struct GroveMerge {
    bool found;
    GroveItem item;
    int cells[3];
} GroveMerge;

typedef struct GroveLayout {
    float x;
    float y;
    float cell;
    float gap;
} GroveLayout;

static bool s_devapi_enabled;
static uint16_t s_devapi_port = DRAGON_GROVE_DEVAPI_PORT_DEFAULT;
static int s_window_width = 960;
static int s_window_height = 540;
static UiBox s_merge_box;
static UiBox s_board_box;
static GroveCell s_cells[GROVE_CELLS];
static int s_merge_count;
static int s_restored_tiles;
static int s_target_restored_tiles = 3;
static char s_feedback[GROVE_TEXT_MAX];
static char s_blocked_reason[GROVE_TEXT_MAX];
static GroveItem s_next_merge_kind;

static void ortho(float left, float right, float bottom, float top, float near_z, float far_z, float out[16]) {
    memset(out, 0, sizeof(float) * 16);
    out[0] = 2.0F / (right - left);
    out[5] = 2.0F / (top - bottom);
    out[10] = -2.0F / (far_z - near_z);
    out[12] = -(right + left) / (right - left);
    out[13] = -(top + bottom) / (top - bottom);
    out[14] = -(far_z + near_z) / (far_z - near_z);
    out[15] = 1.0F;
}

static bool contains(UiBox box, float x, float y) {
    return x >= box.x && x <= box.x + box.w && y >= box.y && y <= box.y + box.h;
}

static void set_text(char *target, size_t cap, const char *text) {
    if (cap == 0) {
        return;
    }
    (void)snprintf(target, cap, "%s", text ? text : "");
    target[cap - 1] = '\0';
}

static int cell_index(int x, int y_up) {
    return y_up * GROVE_GRID + x;
}

static const char *item_name(GroveItem item) {
    switch (item) {
    case GROVE_ITEM_EGG:
        return "egg";
    case GROVE_ITEM_HATCHLING:
        return "hatchling";
    case GROVE_ITEM_SPROUT:
        return "sprout";
    case GROVE_ITEM_BLOOM:
        return "bloom";
    case GROVE_ITEM_EMPTY:
    default:
        return "empty";
    }
}

static GroveItem reward_for(GroveItem item) {
    if (item == GROVE_ITEM_EGG) {
        return GROVE_ITEM_HATCHLING;
    }
    if (item == GROVE_ITEM_SPROUT) {
        return GROVE_ITEM_BLOOM;
    }
    return GROVE_ITEM_EMPTY;
}

static void place_item(int x, int y_up, GroveItem item) {
    if (x < 0 || x >= GROVE_GRID || y_up < 0 || y_up >= GROVE_GRID) {
        return;
    }
    s_cells[cell_index(x, y_up)].item = item;
}

static GroveMerge find_merge_kind(GroveItem item) {
    GroveMerge merge = {.found = false, .item = item, .cells = {-1, -1, -1}};
    int count = 0;
    for (int i = 0; i < GROVE_CELLS && count < 3; ++i) {
        if (s_cells[i].item == item) {
            merge.cells[count++] = i;
        }
    }
    merge.found = count == 3;
    return merge;
}

static GroveMerge find_merge(void) {
    GroveMerge eggs = find_merge_kind(GROVE_ITEM_EGG);
    if (eggs.found) {
        return eggs;
    }
    return find_merge_kind(GROVE_ITEM_SPROUT);
}

static void restore_one_shadow(void) {
    for (int i = 0; i < GROVE_CELLS; ++i) {
        if (s_cells[i].shadowed) {
            s_cells[i].shadowed = false;
            s_restored_tiles += 1;
            return;
        }
    }
}

static void sync_merge_status(void) {
    GroveMerge merge = find_merge();
    s_next_merge_kind = merge.found ? merge.item : GROVE_ITEM_EMPTY;
    if (merge.found) {
        set_text(s_blocked_reason, sizeof(s_blocked_reason), "");
        return;
    }
    set_text(s_blocked_reason, sizeof(s_blocked_reason), "No group of three is ready");
}

static void reset_grove(void) {
    game_state_init_defaults(&g_game_state);
    memset(s_cells, 0, sizeof(s_cells));
    for (int i = 0; i < GROVE_CELLS; ++i) {
        s_cells[i].shadowed = i >= 14;
    }
    place_item(0, 4, GROVE_ITEM_EGG);
    place_item(1, 4, GROVE_ITEM_EGG);
    place_item(2, 4, GROVE_ITEM_EGG);
    place_item(1, 3, GROVE_ITEM_EGG);
    place_item(2, 3, GROVE_ITEM_EGG);
    place_item(3, 3, GROVE_ITEM_EGG);
    place_item(0, 1, GROVE_ITEM_SPROUT);
    place_item(1, 1, GROVE_ITEM_SPROUT);
    place_item(2, 1, GROVE_ITEM_SPROUT);
    s_merge_count = 0;
    s_restored_tiles = 0;
    set_text(s_feedback, sizeof(s_feedback), "Merge 3 eggs to hatch a helper");
    set_text(s_blocked_reason, sizeof(s_blocked_reason), "");
    sync_merge_status();
    game_state_mark_dirty();
}

static void merge_ready_group(void) {
    GroveMerge merge = find_merge();
    if (!merge.found) {
        set_text(s_feedback, sizeof(s_feedback), "Blocked: find or create 3 matching objects");
        set_text(s_blocked_reason, sizeof(s_blocked_reason), "No group of three is ready");
        game_state_mark_dirty();
        return;
    }

    GroveItem reward = reward_for(merge.item);
    s_cells[merge.cells[0]].item = reward;
    s_cells[merge.cells[1]].item = GROVE_ITEM_EMPTY;
    s_cells[merge.cells[2]].item = GROVE_ITEM_EMPTY;
    s_merge_count += 1;
    restore_one_shadow();

    if (merge.item == GROVE_ITEM_EGG) {
        set_text(s_feedback, sizeof(s_feedback), "Egg merge: hatchling restored a grove tile");
    } else {
        set_text(s_feedback, sizeof(s_feedback), "Sprout merge: bloom restored a grove tile");
    }
    sync_merge_status();
    game_state_mark_dirty();
}

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
        }
    }
}

static void layout(float w, float h) {
    const float board_size = (h < w ? h : w) * 0.62F;
    s_board_box = (UiBox){
        .x = w * 0.10F,
        .y = h * 0.18F,
        .w = board_size,
        .h = board_size,
    };
    const float button_w = w < 720.0F ? w * 0.34F : 260.0F;
    s_merge_box = (UiBox){
        .x = w - button_w - 48.0F,
        .y = h - 112.0F,
        .w = button_w,
        .h = 64.0F,
    };
}

static GroveLayout grove_layout(void) {
    const float gap = s_board_box.w * 0.018F;
    const float cell = (s_board_box.w - gap * (float)(GROVE_GRID - 1)) / (float)GROVE_GRID;
    return (GroveLayout){.x = s_board_box.x, .y = s_board_box.y, .cell = cell, .gap = gap};
}

/* Gameplay/layout is Y-up. Boundary convert to screen coordinates happens here. */
static UiBox grid_to_screen(int x, int y_up) {
    GroveLayout l = grove_layout();
    return (UiBox){
        .x = l.x + (float)x * (l.cell + l.gap),
        .y = l.y + (float)(GROVE_GRID - 1 - y_up) * (l.cell + l.gap),
        .w = l.cell,
        .h = l.cell,
    };
}

static void rect(float x, float y, float w, float h, const float color[4]) {
    nt_shape_renderer_rect((float[3]){x + w * 0.5F, y + h * 0.5F, 0.0F}, (float[2]){w, h}, color);
}

static void rect_wire(float x, float y, float w, float h, const float color[4]) {
    nt_shape_renderer_rect_wire((float[3]){x + w * 0.5F, y + h * 0.5F, 0.0F}, (float[2]){w, h}, color);
}

static void circle(float x, float y, float radius, const float color[4]) {
    nt_shape_renderer_circle((float[3]){x, y, 0.0F}, radius, color);
}

static void triangle(float ax, float ay, float bx, float by, float cx, float cy, const float color[4]) {
    nt_shape_renderer_triangle((float[3]){ax, ay, 0.0F}, (float[3]){bx, by, 0.0F}, (float[3]){cx, cy, 0.0F}, color);
}

static void capsule(float x, float y, float w, float h, const float color[4]) {
    const float r = h * 0.5F;
    rect(x + r, y, w - r * 2.0F, h, color);
    circle(x + r, y + r, r, color);
    circle(x + w - r, y + r, r, color);
}

static bool is_highlighted(int cell, GroveMerge merge) {
    return merge.found && (merge.cells[0] == cell || merge.cells[1] == cell || merge.cells[2] == cell);
}

static void draw_item(GroveItem item, UiBox box) {
    const float cx = box.x + box.w * 0.5F;
    const float cy = box.y + box.h * 0.5F;
    const float r = box.w * 0.25F;
    switch (item) {
    case GROVE_ITEM_EGG:
        circle(cx, cy, r, (float[4]){0.95F, 0.88F, 0.98F, 1.0F});
        circle(cx - r * 0.28F, cy - r * 0.22F, r * 0.28F, (float[4]){0.66F, 0.44F, 0.88F, 1.0F});
        break;
    case GROVE_ITEM_HATCHLING:
        triangle(cx - r * 1.55F, cy, cx - r * 0.35F, cy - r * 0.35F, cx - r * 0.35F, cy + r * 0.35F, (float[4]){0.44F, 0.78F, 0.92F, 1.0F});
        triangle(cx + r * 1.55F, cy, cx + r * 0.35F, cy - r * 0.35F, cx + r * 0.35F, cy + r * 0.35F, (float[4]){0.44F, 0.78F, 0.92F, 1.0F});
        circle(cx, cy, r * 0.72F, (float[4]){0.18F, 0.64F, 0.78F, 1.0F});
        circle(cx + r * 0.18F, cy - r * 0.18F, r * 0.12F, (float[4]){0.02F, 0.08F, 0.10F, 1.0F});
        break;
    case GROVE_ITEM_SPROUT:
        rect(cx - r * 0.12F, cy - r * 0.65F, r * 0.24F, r * 1.25F, (float[4]){0.26F, 0.48F, 0.16F, 1.0F});
        circle(cx - r * 0.36F, cy - r * 0.18F, r * 0.42F, (float[4]){0.30F, 0.78F, 0.32F, 1.0F});
        circle(cx + r * 0.38F, cy - r * 0.08F, r * 0.36F, (float[4]){0.40F, 0.86F, 0.38F, 1.0F});
        break;
    case GROVE_ITEM_BLOOM:
        circle(cx, cy, r * 0.36F, (float[4]){0.98F, 0.78F, 0.24F, 1.0F});
        for (int i = 0; i < 6; ++i) {
            const float ox = (float)((i % 3) - 1) * r * 0.52F;
            const float oy = (float)((i / 3) * 2 - 1) * r * 0.45F;
            circle(cx + ox, cy + oy, r * 0.30F, (float[4]){0.92F, 0.28F, 0.58F, 1.0F});
        }
        break;
    case GROVE_ITEM_EMPTY:
    default:
        break;
    }
}

static void draw_board(void) {
    GroveMerge merge = find_merge();
    rect(s_board_box.x - 12.0F, s_board_box.y - 12.0F, s_board_box.w + 24.0F, s_board_box.h + 24.0F, (float[4]){0.16F, 0.30F, 0.22F, 1.0F});
    for (int y = 0; y < GROVE_GRID; ++y) {
        for (int x = 0; x < GROVE_GRID; ++x) {
            const int idx = cell_index(x, y);
            UiBox box = grid_to_screen(x, y);
            const bool hot = is_highlighted(idx, merge);
            const float *cell_color = s_cells[idx].shadowed ? (float[4]){0.20F, 0.24F, 0.22F, 1.0F} : (float[4]){0.48F, 0.72F, 0.38F, 1.0F};
            rect(box.x, box.y, box.w, box.h, cell_color);
            rect(box.x + box.w * 0.08F, box.y + box.h * 0.08F, box.w * 0.84F, box.h * 0.12F, (float[4]){1.0F, 1.0F, 1.0F, 0.16F});
            if (hot) {
                rect_wire(box.x + 3.0F, box.y + 3.0F, box.w - 6.0F, box.h - 6.0F, (float[4]){1.0F, 0.92F, 0.20F, 1.0F});
            }
            draw_item(s_cells[idx].item, box);
        }
    }
}

static void draw_hud(float w, float h) {
    (void)h;
    rect(0.0F, 0.0F, w, 84.0F, (float[4]){0.10F, 0.20F, 0.23F, 1.0F});
    for (int i = 0; i < s_target_restored_tiles; ++i) {
        const bool filled = i < s_restored_tiles;
        circle(44.0F + (float)i * 34.0F, 40.0F, 12.0F, filled ? (float[4]){0.34F, 0.90F, 0.42F, 1.0F} : (float[4]){0.42F, 0.50F, 0.48F, 1.0F});
    }
    for (int i = 0; i < 3; ++i) {
        const bool filled = i < s_merge_count;
        rect(180.0F + (float)i * 28.0F, 28.0F, 18.0F, 24.0F, filled ? (float[4]){0.98F, 0.72F, 0.24F, 1.0F} : (float[4]){0.38F, 0.44F, 0.42F, 1.0F});
    }
    if (s_next_merge_kind == GROVE_ITEM_EMPTY) {
        rect(w - 220.0F, 28.0F, 160.0F, 24.0F, (float[4]){0.78F, 0.22F, 0.28F, 1.0F});
    } else {
        rect(w - 220.0F, 28.0F, 160.0F, 24.0F, (float[4]){0.26F, 0.74F, 0.86F, 1.0F});
    }
}

static void draw_merge_button(void) {
    const bool hot = nt_input_mouse_is_down(NT_BUTTON_LEFT) && contains(s_merge_box, g_nt_input.pointers[0].x, g_nt_input.pointers[0].y);
    const bool enabled = s_next_merge_kind != GROVE_ITEM_EMPTY;
    const float *base = enabled ? (hot ? (float[4]){0.88F, 0.50F, 0.12F, 1.0F} : (float[4]){0.96F, 0.62F, 0.16F, 1.0F}) : (float[4]){0.42F, 0.46F, 0.46F, 1.0F};
    capsule(s_merge_box.x, s_merge_box.y + 5.0F, s_merge_box.w, s_merge_box.h, (float[4]){0.05F, 0.08F, 0.08F, 0.30F});
    capsule(s_merge_box.x, s_merge_box.y, s_merge_box.w, s_merge_box.h, base);
    for (int i = 0; i < 3; ++i) {
        circle(s_merge_box.x + 58.0F + (float)i * 38.0F, s_merge_box.y + s_merge_box.h * 0.5F, 13.0F, (float[4]){0.98F, 0.90F, 0.98F, 1.0F});
    }
    triangle(s_merge_box.x + s_merge_box.w - 72.0F, s_merge_box.y + 18.0F, s_merge_box.x + s_merge_box.w - 32.0F, s_merge_box.y + 32.0F, s_merge_box.x + s_merge_box.w - 72.0F, s_merge_box.y + 46.0F, (float[4]){0.16F, 0.20F, 0.18F, 1.0F});
}

static void draw_scene(float w, float h) {
    float vp[16];
    ortho(0.0F, w, h, 0.0F, -1.0F, 1.0F, vp);
    nt_shape_renderer_set_vp(vp);
    nt_shape_renderer_set_cam_pos((float[3]){0.0F, 0.0F, 1.0F});
    nt_shape_renderer_set_depth(false);
    nt_shape_renderer_set_line_width(4.0F);

    rect(0.0F, 0.0F, w, h, (float[4]){0.74F, 0.90F, 0.82F, 1.0F});
    rect(0.0F, h * 0.70F, w, h * 0.30F, (float[4]){0.30F, 0.56F, 0.32F, 1.0F});
    draw_hud(w, h);
    draw_board();
    draw_merge_button();
}

static void handle_input(void) {
    if (nt_input_key_is_pressed(NT_KEY_SPACE) || nt_input_key_is_pressed(NT_KEY_ENTER)) {
        merge_ready_group();
    }
    if (nt_input_mouse_is_pressed(NT_BUTTON_LEFT)) {
        for (int i = 0; i < NT_INPUT_MAX_POINTERS; ++i) {
            const nt_pointer_t pointer = g_nt_input.pointers[i];
            if (pointer.active && contains(s_merge_box, pointer.x, pointer.y)) {
                merge_ready_group();
                return;
            }
        }
    }
}

#if NT_DEVAPI_ENABLED
void game_state_register_devapi(void);

static void add_board_json(cJSON *root) {
    cJSON *board = cJSON_AddArrayToObject(root, "board");
    for (int y = 0; y < GROVE_GRID; ++y) {
        for (int x = 0; x < GROVE_GRID; ++x) {
            const int idx = cell_index(x, y);
            cJSON *cell = cJSON_CreateObject();
            cJSON_AddNumberToObject(cell, "x", x);
            cJSON_AddNumberToObject(cell, "y_up", y);
            cJSON_AddStringToObject(cell, "item", item_name(s_cells[idx].item));
            cJSON_AddNumberToObject(cell, "level", s_cells[idx].item == GROVE_ITEM_HATCHLING || s_cells[idx].item == GROVE_ITEM_BLOOM ? 2 : (s_cells[idx].item == GROVE_ITEM_EMPTY ? 0 : 1));
            cJSON_AddBoolToObject(cell, "shadowed", s_cells[idx].shadowed);
            cJSON_AddItemToArray(board, cell);
        }
    }
}

static cJSON *state_json(void) {
    cJSON *root = cJSON_CreateObject();
    GroveMerge merge = find_merge();
    cJSON_AddStringToObject(root, "runtime", "dragon_grove");
    cJSON_AddNumberToObject(root, "grid_size", GROVE_GRID);
    cJSON_AddNumberToObject(root, "merge_count", s_merge_count);
    cJSON_AddNumberToObject(root, "restored_tiles", s_restored_tiles);
    cJSON_AddNumberToObject(root, "target_restored_tiles", s_target_restored_tiles);
    cJSON_AddStringToObject(root, "last_feedback_text", s_feedback);
    cJSON_AddStringToObject(root, "blocked_reason_text", s_blocked_reason);
    cJSON_AddStringToObject(root, "next_merge_kind", item_name(s_next_merge_kind));
    cJSON_AddBoolToObject(root, "has_merge", merge.found);
    add_board_json(root);
    return root;
}

static void emit_state(cJSON *result_obj) {
    cJSON_AddItemToObject(result_obj, "state", state_json());
}

static bool ep_game_state(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    emit_state(result_obj);
    return true;
}

static bool ep_game_reset_playtest(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    reset_grove();
    emit_state(result_obj);
    return true;
}

static bool ep_game_action_merge_ready(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    merge_ready_group();
    emit_state(result_obj);
    return true;
}

static void register_game_endpoints(void) {
    static const nt_devapi_command_desc descs[] = {
        {"game.state", "game", "Return Dragon Grove runtime state.", "", "{state}", "immediate", "none"},
        {"game.reset_playtest", "game", "Reset Dragon Grove state for automation.", "", "{state}", "immediate", "resets state"},
        {"game.action.merge_ready", "game", "Merge the first available group of three objects.", "", "{state}", "next-frame", "mutates board"},
    };
    game_state_register_devapi();
    game_devapi_ui_register();
    (void)nt_devapi_register(&descs[0], ep_game_state, NULL);
    (void)nt_devapi_register(&descs[1], ep_game_reset_playtest, NULL);
    (void)nt_devapi_register(&descs[2], ep_game_action_merge_ready, NULL);
}

static void register_ui_devapi(float w, float h) {
    game_devapi_ui_clear();
    (void)game_devapi_ui_register_node("root", "", "screen", "Dragon Grove", "Merge 3 to restore the grove", 0.0F, 0.0F, w, h, true, true);
    (void)game_devapi_ui_register_node("grove.board", "root", "board", "Y-up grove grid", "5x5 merge board", s_board_box.x, s_board_box.y, s_board_box.w, s_board_box.h, true, true);
    (void)game_devapi_ui_register_node("grove.merge_ready", "root", "button", "Merge ready group", s_next_merge_kind == GROVE_ITEM_EMPTY ? s_blocked_reason : item_name(s_next_merge_kind), s_merge_box.x, s_merge_box.y, s_merge_box.w, s_merge_box.h, true, s_next_merge_kind != GROVE_ITEM_EMPTY);
    (void)game_devapi_ui_register_node("grove.progress", "root", "meter", "Restored grove tiles", s_feedback, 30.0F, 18.0F, 260.0F, 48.0F, true, true);
    if (s_blocked_reason[0]) {
        (void)game_devapi_ui_register_node("grove.blocked", "root", "status", "Blocked reason", s_blocked_reason, w - 260.0F, 18.0F, 220.0F, 48.0F, true, false);
    }
}
#endif

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
    layout(w, h);
    handle_input();

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

    nt_gfx_begin_frame();
    if (g_nt_gfx.context_restored) {
        nt_shape_renderer_restore_gpu();
    }
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.74F, 0.90F, 0.82F, 1.0F}, .clear_depth = 1.0F});
    draw_scene(w, h);
    nt_shape_renderer_flush();
    nt_gfx_end_pass();
    nt_gfx_end_frame();
    nt_window_swap_buffers();
}

int main(int argc, char **argv) {
    nt_engine_config_t config = {0};
    config.app_name = "Dragon Grove";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    parse_args(argc, argv);
    reset_grove();

    g_nt_window.title = "Dragon Grove";
    g_nt_window.width = (uint32_t)s_window_width;
    g_nt_window.height = (uint32_t)s_window_height;
    nt_window_init();
    nt_input_init();

    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    gfx_desc.depth = true;
    nt_gfx_init(&gfx_desc);
    nt_shape_renderer_init();

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_init();
        register_game_endpoints();
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
