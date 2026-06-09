#include "devapi/nt_devapi.h"

#if NT_DEVAPI_ENABLED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comp_storage/nt_comp_storage.h"
#include "drawable_comp/nt_drawable_comp.h"
#include "entity/nt_entity.h"
#include "input/nt_input_internal.h"
#include "transform_comp/nt_transform_comp.h"

#define GAME_DEVAPI_MAX_ENDPOINTS 48
#define GAME_DEVAPI_MAX_KEY_EVENTS 128
#define GAME_DEVAPI_MAX_POINTER_EVENTS 256
#define GAME_DEVAPI_MAX_GROUPS 8
#define GAME_DEVAPI_MAX_GROUP_REQUESTS 64
#define GAME_DEVAPI_MAX_READY_RESPONSES 16
#define GAME_DEVAPI_MAX_UI_ELEMENTS 64
#define GAME_DEVAPI_UI_ID_MAX 64
#define GAME_DEVAPI_UI_LABEL_MAX 96

typedef struct {
    const char *name;
    nt_devapi_handler_fn fn;
    void *user;
} endpoint_t;

typedef struct {
    uint64_t frame;
    nt_key_t key;
    bool down;
} key_event_t;

typedef enum {
    POINTER_EVENT_DOWN = 0,
    POINTER_EVENT_MOVE,
    POINTER_EVENT_UP,
    POINTER_EVENT_WHEEL,
} pointer_event_kind_t;

typedef struct {
    uint64_t frame;
    pointer_event_kind_t kind;
    uint32_t id;
    float x;
    float y;
    float wheel_dx;
    float wheel_dy;
    uint8_t pointer_type;
    uint8_t buttons_mask;
} pointer_event_t;

typedef struct {
    bool active;
    bool is_batch;
    bool waiting;
    uint64_t wait_until_frame;
    cJSON *requests[GAME_DEVAPI_MAX_GROUP_REQUESTS];
    int request_count;
    int request_index;
    cJSON *response;
} request_group_t;

typedef struct {
    char id[GAME_DEVAPI_UI_ID_MAX];
    char label[GAME_DEVAPI_UI_LABEL_MAX];
    float x;
    float y;
    float w;
    float h;
} ui_element_t;

static endpoint_t s_eps[GAME_DEVAPI_MAX_ENDPOINTS];
static int s_ep_count;
static uint64_t s_frame;
static float s_fb_w = 800.0F;
static float s_fb_h = 600.0F;
static float s_log_w = 800.0F;
static float s_log_h = 600.0F;

static bool s_held[NT_KEY_COUNT];
static uint64_t s_key_next_frame[NT_KEY_COUNT];
static key_event_t s_key_events[GAME_DEVAPI_MAX_KEY_EVENTS];
static int s_key_event_count;
static pointer_event_t s_pointer_events[GAME_DEVAPI_MAX_POINTER_EVENTS];
static int s_pointer_event_count;
static float s_mx;
static float s_my;
static uint8_t s_btn_mask;
static uint8_t s_click;
static uint8_t s_click_btn;
static bool s_ptr_engaged;
static bool s_ptr_active;
static bool s_wheel_pending;
static float s_wheel_dx;
static float s_wheel_dy;
static request_group_t s_groups[GAME_DEVAPI_MAX_GROUPS];
static int s_group_head;
static int s_group_count;
static char *s_ready_responses[GAME_DEVAPI_MAX_READY_RESPONSES];
static int s_ready_head;
static int s_ready_count;
static ui_element_t s_ui_elements[GAME_DEVAPI_MAX_UI_ELEMENTS];
static int s_ui_element_count;

static void set_error(char *error, int error_cap, const char *message) {
    if (error_cap <= 0) {
        return;
    }
    (void)snprintf(error, (size_t)error_cap, "%s", message);
}

static const cJSON *object_item(const cJSON *obj, const char *name) {
    return cJSON_IsObject(obj) ? cJSON_GetObjectItemCaseSensitive(obj, name) : NULL;
}

static const char *param_string(const cJSON *params, const char *name, const char *fallback) {
    const cJSON *item = object_item(params, name);
    return cJSON_IsString(item) ? item->valuestring : fallback;
}

static double param_number(const cJSON *params, const char *name, double fallback) {
    const cJSON *item = object_item(params, name);
    return cJSON_IsNumber(item) ? item->valuedouble : fallback;
}

static bool copy_fixed(char *dst, int dst_cap, const char *src) {
    if (!dst || dst_cap <= 0 || !src || !src[0]) {
        return false;
    }
    (void)snprintf(dst, (size_t)dst_cap, "%s", src);
    return true;
}

static int ci_eq(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        int ca = (*a >= 'a' && *a <= 'z') ? (*a - ('a' - 'A')) : *a;
        int cb = (*b >= 'a' && *b <= 'z') ? (*b - ('a' - 'A')) : *b;
        if (ca != cb) {
            return 0;
        }
    }
    return (*a == '\0') && (*b == '\0');
}

static nt_key_t key_from_name(const char *s) {
    if (!s || !s[0]) {
        return NT_KEY_COUNT;
    }
    if (s[1] == '\0') {
        char ch = s[0];
        if (ch >= 'a' && ch <= 'z') {
            return (nt_key_t)(NT_KEY_A + (ch - 'a'));
        }
        if (ch >= 'A' && ch <= 'Z') {
            return (nt_key_t)(NT_KEY_A + (ch - 'A'));
        }
        if (ch >= '0' && ch <= '9') {
            return (nt_key_t)(NT_KEY_0 + (ch - '0'));
        }
    }

    static const struct {
        const char *name;
        nt_key_t key;
    } table[] = {
        {"SPACE", NT_KEY_SPACE},         {"ENTER", NT_KEY_ENTER}, {"ESCAPE", NT_KEY_ESCAPE},   {"ESC", NT_KEY_ESCAPE},
        {"TAB", NT_KEY_TAB},             {"BACKSPACE", NT_KEY_BACKSPACE},                      {"UP", NT_KEY_ARROW_UP},
        {"DOWN", NT_KEY_ARROW_DOWN},     {"LEFT", NT_KEY_ARROW_LEFT},                          {"RIGHT", NT_KEY_ARROW_RIGHT},
        {"DELETE", NT_KEY_DELETE},       {"INSERT", NT_KEY_INSERT},                            {"HOME", NT_KEY_HOME},
        {"END", NT_KEY_END},             {"PAGEUP", NT_KEY_PAGE_UP},                           {"PAGEDOWN", NT_KEY_PAGE_DOWN},
        {"SHIFT", NT_KEY_LSHIFT},        {"CTRL", NT_KEY_LCTRL},                               {"ALT", NT_KEY_LALT},
        {"F1", NT_KEY_F1},               {"F2", NT_KEY_F2},                                    {"F3", NT_KEY_F3},
        {"F4", NT_KEY_F4},               {"F5", NT_KEY_F5},                                    {"F6", NT_KEY_F6},
        {"F7", NT_KEY_F7},               {"F8", NT_KEY_F8},                                    {"F9", NT_KEY_F9},
        {"F10", NT_KEY_F10},             {"F11", NT_KEY_F11},                                  {"F12", NT_KEY_F12},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (ci_eq(s, table[i].name)) {
            return table[i].key;
        }
    }
    return NT_KEY_COUNT;
}

static uint8_t button_from_name(const char *s) {
    if (s && (s[0] == 'r' || s[0] == 'R')) {
        return (uint8_t)NT_BUTTON_RIGHT;
    }
    if (s && (s[0] == 'm' || s[0] == 'M')) {
        return (uint8_t)NT_BUTTON_MIDDLE;
    }
    return (uint8_t)NT_BUTTON_LEFT;
}

void nt_devapi_set_view(float fb_w, float fb_h, float logical_w, float logical_h) {
    s_fb_w = fb_w;
    s_fb_h = fb_h;
    s_log_w = logical_w;
    s_log_h = logical_h;
}

void nt_devapi_set_frame(uint64_t frame) { s_frame = frame; }

void nt_devapi_clear_ui_elements(void) { s_ui_element_count = 0; }

bool nt_devapi_register_ui_element(const char *id, const char *label, float x, float y, float w, float h) {
    if (s_ui_element_count >= GAME_DEVAPI_MAX_UI_ELEMENTS || !id || !id[0] || w < 0.0F || h < 0.0F) {
        return false;
    }
    ui_element_t *element = &s_ui_elements[s_ui_element_count++];
    if (!copy_fixed(element->id, (int)sizeof(element->id), id)) {
        s_ui_element_count--;
        return false;
    }
    (void)snprintf(element->label, sizeof(element->label), "%s", label ? label : "");
    element->x = x;
    element->y = y;
    element->w = w;
    element->h = h;
    return true;
}

bool nt_devapi_register(const char *name, nt_devapi_handler_fn fn, void *user) {
    if (!name || !fn || s_ep_count >= GAME_DEVAPI_MAX_ENDPOINTS) {
        return false;
    }
    s_eps[s_ep_count].name = name;
    s_eps[s_ep_count].fn = fn;
    s_eps[s_ep_count].user = user;
    s_ep_count++;
    return true;
}

static bool enqueue_key_event(nt_key_t key, bool down, uint64_t frame) {
    if (s_key_event_count >= GAME_DEVAPI_MAX_KEY_EVENTS) {
        return false;
    }
    s_key_events[s_key_event_count].frame = frame;
    s_key_events[s_key_event_count].key = key;
    s_key_events[s_key_event_count].down = down;
    s_key_event_count++;
    return true;
}

static bool enqueue_pointer_event(const pointer_event_t *event) {
    if (s_pointer_event_count >= GAME_DEVAPI_MAX_POINTER_EVENTS || !event) {
        return false;
    }
    s_pointer_events[s_pointer_event_count] = *event;
    s_pointer_event_count++;
    return true;
}

static uint8_t button_mask(uint8_t button) {
    return button < NT_BUTTON_MAX ? (uint8_t)(1U << button) : (uint8_t)0;
}

static void apply_pointer_event(const pointer_event_t *event) {
    switch (event->kind) {
    case POINTER_EVENT_DOWN:
        nt_input_pointer_down(event->id, event->x, event->y, 1.0F, event->pointer_type, event->buttons_mask);
        break;
    case POINTER_EVENT_MOVE:
        nt_input_pointer_move(event->id, event->x, event->y, 1.0F, event->pointer_type, event->buttons_mask);
        break;
    case POINTER_EVENT_UP:
        nt_input_pointer_up(event->id);
        break;
    case POINTER_EVENT_WHEEL:
        nt_input_pointer_move(event->id, event->x, event->y, 1.0F, event->pointer_type, event->buttons_mask);
        nt_input_wheel(event->wheel_dx, event->wheel_dy);
        break;
    default:
        break;
    }
}

void nt_devapi_apply_pending(void) {
    for (int k = 0; k < NT_KEY_COUNT; k++) {
        if (s_held[k]) {
            nt_input_set_key((nt_key_t)k, true);
        }
    }

    int write = 0;
    for (int i = 0; i < s_key_event_count; i++) {
        if (s_key_events[i].frame <= s_frame) {
            nt_input_set_key(s_key_events[i].key, s_key_events[i].down);
        } else {
            if (write != i) {
                s_key_events[write] = s_key_events[i];
            }
            write++;
        }
    }
    s_key_event_count = write;

    write = 0;
    for (int i = 0; i < s_pointer_event_count; i++) {
        if (s_pointer_events[i].frame <= s_frame) {
            apply_pointer_event(&s_pointer_events[i]);
        } else {
            if (write != i) {
                s_pointer_events[write] = s_pointer_events[i];
            }
            write++;
        }
    }
    s_pointer_event_count = write;

    if (!s_ptr_engaged) {
        return;
    }
    uint8_t mask = s_btn_mask;
    if (s_click == 1) {
        mask |= (uint8_t)(1U << s_click_btn);
    }
    if (!s_ptr_active) {
        nt_input_pointer_down(0, s_mx, s_my, 1.0F, (uint8_t)NT_POINTER_MOUSE, mask);
        s_ptr_active = true;
    } else {
        nt_input_pointer_move(0, s_mx, s_my, 1.0F, (uint8_t)NT_POINTER_MOUSE, mask);
    }
    if (s_click == 1) {
        s_click = 2;
    } else if (s_click == 2) {
        s_click = 0;
    }
    if (s_wheel_pending) {
        nt_input_wheel(s_wheel_dx, s_wheel_dy);
        s_wheel_dx = 0.0F;
        s_wheel_dy = 0.0F;
        s_wheel_pending = false;
    }
}

static bool ep_ping(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(obj, "pong", true);
    *result = obj;
    return true;
}

static bool ep_endpoints(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    cJSON *items = cJSON_CreateArray();
    for (int i = 0; i < s_ep_count; i++) {
        cJSON_AddItemToArray(items, cJSON_CreateString(s_eps[i].name));
    }
    *result = items;
    return true;
}

static bool ep_view(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "fb_w", (double)s_fb_w);
    cJSON_AddNumberToObject(obj, "fb_h", (double)s_fb_h);
    cJSON_AddNumberToObject(obj, "logical_w", (double)s_log_w);
    cJSON_AddNumberToObject(obj, "logical_h", (double)s_log_h);
    *result = obj;
    return true;
}

static bool ep_frame_current(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "frame", (double)s_frame);
    *result = obj;
    return true;
}

static bool ep_entity_list(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    nt_transform_comp_view_t transforms = nt_transform_comp_view();
    nt_drawable_comp_view_t drawables = nt_drawable_comp_view();
    uint16_t max_entities = nt_entity_max();
    cJSON *items = cJSON_CreateArray();
    for (uint16_t e = 0; e < max_entities; e++) {
        uint16_t transform_idx = transforms.sparse_indices ? transforms.sparse_indices[e] : NT_INVALID_COMP_INDEX;
        uint16_t drawable_idx = drawables.sparse_indices ? drawables.sparse_indices[e] : NT_INVALID_COMP_INDEX;
        if (transform_idx == NT_INVALID_COMP_INDEX && drawable_idx == NT_INVALID_COMP_INDEX) {
            continue;
        }
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "entity", e);
        if (transform_idx != NT_INVALID_COMP_INDEX) {
            const float *m = transforms.world_matrices[transform_idx];
            cJSON_AddNumberToObject(obj, "x", (double)m[12]);
            cJSON_AddNumberToObject(obj, "y", (double)m[13]);
            cJSON_AddNumberToObject(obj, "z", (double)m[14]);
        }
        if (drawable_idx != NT_INVALID_COMP_INDEX) {
            cJSON_AddNumberToObject(obj, "color", drawables.colors_packed[drawable_idx]);
            cJSON_AddBoolToObject(obj, "visible", drawables.visible[drawable_idx]);
        }
        cJSON_AddItemToArray(items, obj);
    }
    *result = items;
    return true;
}

static const ui_element_t *find_ui_element(const char *id) {
    if (!id || !id[0]) {
        return NULL;
    }
    for (int i = 0; i < s_ui_element_count; i++) {
        if (strcmp(s_ui_elements[i].id, id) == 0) {
            return &s_ui_elements[i];
        }
    }
    return NULL;
}

static cJSON *ui_element_json(const ui_element_t *element) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "id", element->id);
    cJSON_AddStringToObject(obj, "label", element->label);
    cJSON_AddNumberToObject(obj, "x", (double)element->x);
    cJSON_AddNumberToObject(obj, "y", (double)element->y);
    cJSON_AddNumberToObject(obj, "w", (double)element->w);
    cJSON_AddNumberToObject(obj, "h", (double)element->h);
    cJSON_AddNumberToObject(obj, "center_x", (double)(element->x + (element->w * 0.5F)));
    cJSON_AddNumberToObject(obj, "center_y", (double)(element->y + (element->h * 0.5F)));
    return obj;
}

static bool ep_ui_tree(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    cJSON *items = cJSON_CreateArray();
    for (int i = 0; i < s_ui_element_count; i++) {
        cJSON_AddItemToArray(items, ui_element_json(&s_ui_elements[i]));
    }
    *result = items;
    return true;
}

static bool ep_ui_element(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)user;
    const ui_element_t *element = find_ui_element(param_string(params, "id", ""));
    if (!element) {
        set_error(error, error_cap, "unknown ui id");
        return false;
    }
    *result = ui_element_json(element);
    return true;
}

static bool resolve_ui_point(const cJSON *params, float *out_x, float *out_y, char *error, int error_cap) {
    const ui_element_t *element = find_ui_element(param_string(params, "id", ""));
    if (!element) {
        set_error(error, error_cap, "unknown ui id");
        return false;
    }
    *out_x = element->x + (element->w * 0.5F) + (float)param_number(params, "offset_x", 0.0);
    *out_y = element->y + (element->h * 0.5F) + (float)param_number(params, "offset_y", 0.0);
    return true;
}

static bool ep_ui_click(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)user;
    if (!resolve_ui_point(params, &s_mx, &s_my, error, error_cap)) {
        return false;
    }
    s_click_btn = button_from_name(param_string(params, "button", "left"));
    s_click = 1;
    s_ptr_engaged = true;
    *result = cJSON_CreateObject();
    return true;
}

static bool ep_ui_scroll(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)user;
    if (!resolve_ui_point(params, &s_mx, &s_my, error, error_cap)) {
        return false;
    }
    s_ptr_engaged = true;
    s_wheel_dx += (float)param_number(params, "dx", 0.0);
    s_wheel_dy += (float)param_number(params, "dy", 0.0);
    s_wheel_pending = true;
    *result = cJSON_CreateObject();
    return true;
}

static bool ep_ui_drag(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)user;
    float from_x = 0.0F;
    float from_y = 0.0F;
    if (!resolve_ui_point(params, &from_x, &from_y, error, error_cap)) {
        return false;
    }

    const char *to_id = param_string(params, "to_id", NULL);
    float to_x = from_x + (float)param_number(params, "to_offset_x", param_number(params, "dx", 0.0));
    float to_y = from_y + (float)param_number(params, "to_offset_y", param_number(params, "dy", 0.0));
    if (to_id && to_id[0]) {
        cJSON *to_params = cJSON_CreateObject();
        cJSON_AddStringToObject(to_params, "id", to_id);
        bool ok = resolve_ui_point(to_params, &to_x, &to_y, error, error_cap);
        cJSON_Delete(to_params);
        if (!ok) {
            return false;
        }
        to_x += (float)param_number(params, "to_offset_x", 0.0);
        to_y += (float)param_number(params, "to_offset_y", 0.0);
    }

    int frames = (int)param_number(params, "frames", 8.0);
    if (frames < 1) {
        frames = 1;
    }
    uint8_t btn = button_from_name(param_string(params, "button", "left"));
    uint8_t mask = button_mask(btn);
    pointer_event_t event = {.frame = s_frame, .kind = POINTER_EVENT_DOWN, .id = 0, .x = from_x, .y = from_y, .pointer_type = (uint8_t)NT_POINTER_MOUSE, .buttons_mask = mask};
    if (!enqueue_pointer_event(&event)) {
        set_error(error, error_cap, "pointer event queue full");
        return false;
    }
    for (int i = 1; i <= frames; i++) {
        float t = (float)i / (float)frames;
        event.frame = s_frame + (uint64_t)i;
        event.kind = POINTER_EVENT_MOVE;
        event.x = from_x + ((to_x - from_x) * t);
        event.y = from_y + ((to_y - from_y) * t);
        event.buttons_mask = mask;
        if (!enqueue_pointer_event(&event)) {
            set_error(error, error_cap, "pointer event queue full");
            return false;
        }
    }
    event.frame = s_frame + (uint64_t)frames + 1U;
    event.kind = POINTER_EVENT_UP;
    event.buttons_mask = 0;
    if (!enqueue_pointer_event(&event)) {
        set_error(error, error_cap, "pointer event queue full");
        return false;
    }
    *result = cJSON_CreateObject();
    return true;
}

static bool ep_input_key(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)user;
    nt_key_t key = key_from_name(param_string(params, "key", ""));
    if (key >= NT_KEY_COUNT) {
        set_error(error, error_cap, "invalid key");
        return false;
    }
    const char *mode = param_string(params, "mode", "tap");
    if (strcmp(mode, "down") == 0) {
        s_held[key] = true;
        if (!enqueue_key_event(key, true, s_frame)) {
            set_error(error, error_cap, "key event queue full");
            return false;
        }
    } else if (strcmp(mode, "up") == 0) {
        s_held[key] = false;
        s_key_next_frame[key] = s_frame;
        if (!enqueue_key_event(key, false, s_frame)) {
            set_error(error, error_cap, "key event queue full");
            return false;
        }
    } else if (strcmp(mode, "tap") == 0) {
        int hold_frames = (int)param_number(params, "hold_frames", 1.0);
        if (hold_frames < 1) {
            hold_frames = 1;
        }
        uint64_t down_frame = s_key_next_frame[key] > s_frame ? s_key_next_frame[key] : s_frame;
        uint64_t up_frame = down_frame + (uint64_t)hold_frames;
        if (!enqueue_key_event(key, true, down_frame) || !enqueue_key_event(key, false, up_frame)) {
            set_error(error, error_cap, "key event queue full");
            return false;
        }
        s_key_next_frame[key] = up_frame + 1U;
    } else {
        set_error(error, error_cap, "invalid key mode");
        return false;
    }
    *result = cJSON_CreateObject();
    return true;
}

static bool ep_input_move(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)error;
    (void)error_cap;
    (void)user;
    s_mx = (float)param_number(params, "x", (double)s_mx);
    s_my = (float)param_number(params, "y", (double)s_my);
    s_ptr_engaged = true;
    *result = cJSON_CreateObject();
    return true;
}

static bool ep_input_click(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)error;
    (void)error_cap;
    (void)user;
    s_mx = (float)param_number(params, "x", (double)s_mx);
    s_my = (float)param_number(params, "y", (double)s_my);
    s_click_btn = button_from_name(param_string(params, "button", "left"));
    s_click = 1;
    s_ptr_engaged = true;
    *result = cJSON_CreateObject();
    return true;
}

static bool ep_input_pointer(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)user;
    const char *phase = param_string(params, "phase", "move");
    uint32_t id = (uint32_t)param_number(params, "id", 0.0);
    uint8_t btn = button_from_name(param_string(params, "button", "left"));
    pointer_event_t event = {
        .frame = s_frame,
        .id = id,
        .x = (float)param_number(params, "x", (double)(s_fb_w * 0.5F)),
        .y = (float)param_number(params, "y", (double)(s_fb_h * 0.5F)),
        .pointer_type = (uint8_t)NT_POINTER_MOUSE,
        .buttons_mask = 0,
    };
    if (strcmp(phase, "down") == 0) {
        event.kind = POINTER_EVENT_DOWN;
        event.buttons_mask = button_mask(btn);
    } else if (strcmp(phase, "move") == 0) {
        event.kind = POINTER_EVENT_MOVE;
        event.buttons_mask = (uint8_t)param_number(params, "buttons_mask", 0.0);
    } else if (strcmp(phase, "up") == 0) {
        event.kind = POINTER_EVENT_UP;
    } else {
        set_error(error, error_cap, "invalid pointer phase");
        return false;
    }
    if (!enqueue_pointer_event(&event)) {
        set_error(error, error_cap, "pointer event queue full");
        return false;
    }
    *result = cJSON_CreateObject();
    return true;
}

static bool ep_input_wheel(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)error;
    (void)error_cap;
    (void)user;
    s_mx = (float)param_number(params, "x", (double)(s_fb_w * 0.5F));
    s_my = (float)param_number(params, "y", (double)(s_fb_h * 0.5F));
    s_ptr_engaged = true;
    s_wheel_dx += (float)param_number(params, "dx", 0.0);
    s_wheel_dy += (float)param_number(params, "dy", 0.0);
    s_wheel_pending = true;
    *result = cJSON_CreateObject();
    return true;
}

static bool ep_input_gesture(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)user;
    const char *type = param_string(params, "type", "tap");
    uint32_t id = (uint32_t)param_number(params, "id", 0.0);
    uint8_t btn = button_from_name(param_string(params, "button", "left"));
    uint8_t mask = button_mask(btn);
    int frames = (int)param_number(params, "frames", 1.0);
    if (frames < 1) {
        frames = 1;
    }

    if (strcmp(type, "tap") == 0) {
        float x = (float)param_number(params, "x", (double)(s_fb_w * 0.5F));
        float y = (float)param_number(params, "y", (double)(s_fb_h * 0.5F));
        pointer_event_t down = {.frame = s_frame, .kind = POINTER_EVENT_DOWN, .id = id, .x = x, .y = y, .pointer_type = (uint8_t)NT_POINTER_MOUSE, .buttons_mask = mask};
        pointer_event_t up = down;
        up.frame = s_frame + (uint64_t)frames;
        up.kind = POINTER_EVENT_UP;
        up.buttons_mask = 0;
        if (!enqueue_pointer_event(&down) || !enqueue_pointer_event(&up)) {
            set_error(error, error_cap, "pointer event queue full");
            return false;
        }
    } else if (strcmp(type, "drag") == 0) {
        float from_x = (float)param_number(params, "from_x", (double)(s_fb_w * 0.5F));
        float from_y = (float)param_number(params, "from_y", (double)(s_fb_h * 0.5F));
        float to_x = (float)param_number(params, "to_x", (double)from_x);
        float to_y = (float)param_number(params, "to_y", (double)from_y);
        pointer_event_t event = {.frame = s_frame, .kind = POINTER_EVENT_DOWN, .id = id, .x = from_x, .y = from_y, .pointer_type = (uint8_t)NT_POINTER_MOUSE, .buttons_mask = mask};
        if (!enqueue_pointer_event(&event)) {
            set_error(error, error_cap, "pointer event queue full");
            return false;
        }
        for (int i = 1; i <= frames; i++) {
            float t = (float)i / (float)frames;
            event.frame = s_frame + (uint64_t)i;
            event.kind = POINTER_EVENT_MOVE;
            event.x = from_x + ((to_x - from_x) * t);
            event.y = from_y + ((to_y - from_y) * t);
            event.buttons_mask = mask;
            if (!enqueue_pointer_event(&event)) {
                set_error(error, error_cap, "pointer event queue full");
                return false;
            }
        }
        event.frame = s_frame + (uint64_t)frames + 1U;
        event.kind = POINTER_EVENT_UP;
        event.buttons_mask = 0;
        if (!enqueue_pointer_event(&event)) {
            set_error(error, error_cap, "pointer event queue full");
            return false;
        }
    } else if (strcmp(type, "scroll") == 0) {
        pointer_event_t event = {
            .frame = s_frame,
            .kind = POINTER_EVENT_WHEEL,
            .id = id,
            .x = (float)param_number(params, "x", (double)(s_fb_w * 0.5F)),
            .y = (float)param_number(params, "y", (double)(s_fb_h * 0.5F)),
            .wheel_dx = (float)param_number(params, "dx", 0.0),
            .wheel_dy = (float)param_number(params, "dy", 0.0),
            .pointer_type = (uint8_t)NT_POINTER_MOUSE,
            .buttons_mask = 0,
        };
        if (!enqueue_pointer_event(&event)) {
            set_error(error, error_cap, "pointer event queue full");
            return false;
        }
    } else {
        set_error(error, error_cap, "invalid gesture type");
        return false;
    }

    *result = cJSON_CreateObject();
    return true;
}

static bool ep_input_button(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)user;
    uint8_t btn = button_from_name(param_string(params, "button", "left"));
    const char *state = param_string(params, "state", "down");
    uint8_t bit = (uint8_t)(1U << btn);
    if (strcmp(state, "down") == 0) {
        s_btn_mask |= bit;
    } else if (strcmp(state, "up") == 0) {
        s_btn_mask &= (uint8_t)~bit;
    } else {
        set_error(error, error_cap, "invalid button state");
        return false;
    }
    s_ptr_engaged = true;
    *result = cJSON_CreateObject();
    return true;
}

void nt_devapi_register_builtins(void) {
    nt_devapi_register("ping", ep_ping, NULL);
    nt_devapi_register("endpoints", ep_endpoints, NULL);
    nt_devapi_register("view", ep_view, NULL);
    nt_devapi_register("frame.current", ep_frame_current, NULL);
    nt_devapi_register("frame.wait", ep_frame_current, NULL);
    nt_devapi_register("entity.list", ep_entity_list, NULL);
    nt_devapi_register("ui.tree", ep_ui_tree, NULL);
    nt_devapi_register("ui.element", ep_ui_element, NULL);
    nt_devapi_register("ui.click", ep_ui_click, NULL);
    nt_devapi_register("ui.drag", ep_ui_drag, NULL);
    nt_devapi_register("ui.scroll", ep_ui_scroll, NULL);
    nt_devapi_register("input.key", ep_input_key, NULL);
    nt_devapi_register("input.move", ep_input_move, NULL);
    nt_devapi_register("input.click", ep_input_click, NULL);
    nt_devapi_register("input.pointer", ep_input_pointer, NULL);
    nt_devapi_register("input.wheel", ep_input_wheel, NULL);
    nt_devapi_register("input.gesture", ep_input_gesture, NULL);
    nt_devapi_register("input.button", ep_input_button, NULL);
}

static endpoint_t *find_endpoint(const char *name) {
    for (int i = 0; i < s_ep_count; i++) {
        if (strcmp(s_eps[i].name, name) == 0) {
            return &s_eps[i];
        }
    }
    return NULL;
}

static void copy_request_id(cJSON *response, const cJSON *request) {
    const cJSON *request_id = object_item(request, "request_id");
    if (request_id) {
        cJSON_AddItemToObject(response, "request_id", cJSON_Duplicate(request_id, true));
    }
}

static const char *request_method(const cJSON *request) {
    const cJSON *method = object_item(request, "method");
    return cJSON_IsString(method) ? method->valuestring : NULL;
}

static cJSON *make_error_response(const cJSON *request, const char *message) {
    cJSON *response = cJSON_CreateObject();
    copy_request_id(response, request);
    cJSON_AddBoolToObject(response, "ok", false);
    cJSON_AddStringToObject(response, "error", message);
    return response;
}

static cJSON *make_ok_response(const cJSON *request, cJSON *result) {
    cJSON *response = cJSON_CreateObject();
    copy_request_id(response, request);
    cJSON_AddBoolToObject(response, "ok", true);
    cJSON_AddItemToObject(response, "result", result ? result : cJSON_CreateObject());
    return response;
}

static cJSON *dispatch_request_now(const cJSON *request) {
    if (!cJSON_IsObject(request)) {
        return make_error_response(request, "request must be an object");
    }
    const char *method = request_method(request);
    if (!method) {
        return make_error_response(request, "method must be a string");
    }
    endpoint_t *ep = find_endpoint(method);
    if (!ep) {
        return make_error_response(request, "unknown method");
    }

    const cJSON *params = object_item(request, "params");
    cJSON *empty_params = NULL;
    if (!params) {
        empty_params = cJSON_CreateObject();
        params = empty_params;
    }

    cJSON *result = NULL;
    char error[128] = {0};
    bool ok = ep->fn(params, &result, error, (int)sizeof(error), ep->user);
    cJSON_Delete(empty_params);
    if (!ok) {
        if (result) {
            cJSON_Delete(result);
        }
        return make_error_response(request, error[0] ? error : "handler error");
    }

    return make_ok_response(request, result);
}

static int write_error_response(const char *message, char *out, int out_cap) {
    return snprintf(out, (size_t)out_cap, "{\"ok\":false,\"error\":\"%s\"}", message);
}

static void free_group(request_group_t *group) {
    for (int i = 0; i < group->request_count; i++) {
        cJSON_Delete(group->requests[i]);
        group->requests[i] = NULL;
    }
    if (group->response) {
        cJSON_Delete(group->response);
    }
    memset(group, 0, sizeof(*group));
}

static bool append_group_response(request_group_t *group, cJSON *response) {
    if (group->is_batch) {
        cJSON_AddItemToArray(group->response, response);
        return true;
    }
    if (group->response) {
        cJSON_Delete(group->response);
    }
    group->response = response;
    return true;
}

static char *dup_cstr(const char *s) {
    size_t len = strlen(s) + 1U;
    char *copy = (char *)malloc(len);
    if (copy) {
        memcpy(copy, s, len);
    }
    return copy;
}

static bool push_ready_response(cJSON *response) {
    if (s_ready_count >= GAME_DEVAPI_MAX_READY_RESPONSES) {
        cJSON_Delete(response);
        return false;
    }
    char *printed = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);
    if (!printed) {
        printed = dup_cstr("{\"ok\":false,\"error\":\"print failed\"}");
        if (!printed) {
            return false;
        }
    }
    int tail = (s_ready_head + s_ready_count) % GAME_DEVAPI_MAX_READY_RESPONSES;
    s_ready_responses[tail] = printed;
    s_ready_count++;
    return true;
}

static int pop_ready_response(char *out, int out_cap) {
    if (s_ready_count <= 0) {
        return 0;
    }
    char *response = s_ready_responses[s_ready_head];
    s_ready_responses[s_ready_head] = NULL;
    s_ready_head = (s_ready_head + 1) % GAME_DEVAPI_MAX_READY_RESPONSES;
    s_ready_count--;
    int len = (int)strlen(response);
    if (len >= out_cap) {
        cJSON_free(response);
        return write_error_response("response too large", out, out_cap);
    }
    memcpy(out, response, (size_t)len);
    out[len] = '\0';
    cJSON_free(response);
    return len;
}

static cJSON *make_frame_wait_response(const cJSON *request) {
    cJSON *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "frame", (double)s_frame);
    return make_ok_response(request, result);
}

static bool process_frame_wait(request_group_t *group, const cJSON *request) {
    const cJSON *params = object_item(request, "params");
    if (!group->waiting) {
        double frames_value = param_number(params, "frames", 1.0);
        if (frames_value < 0.0) {
            append_group_response(group, make_error_response(request, "frames must be >= 0"));
            group->request_index++;
            return true;
        }
        group->wait_until_frame = s_frame + (uint64_t)frames_value;
        group->waiting = true;
    }
    if (s_frame < group->wait_until_frame) {
        return false;
    }
    group->waiting = false;
    append_group_response(group, make_frame_wait_response(request));
    group->request_index++;
    return true;
}

static void process_groups(void) {
    while (s_group_count > 0) {
        request_group_t *group = &s_groups[s_group_head];
        while (group->request_index < group->request_count) {
            const cJSON *request = group->requests[group->request_index];
            const char *method = request_method(request);
            if (method && strcmp(method, "frame.wait") == 0) {
                if (!process_frame_wait(group, request)) {
                    return;
                }
                continue;
            }
            append_group_response(group, dispatch_request_now(request));
            group->request_index++;
        }

        cJSON *response = group->response;
        group->response = NULL;
        if (!response) {
            response = group->is_batch ? cJSON_CreateArray() : make_error_response(NULL, "empty request");
        }
        (void)push_ready_response(response);
        free_group(group);
        s_group_head = (s_group_head + 1) % GAME_DEVAPI_MAX_GROUPS;
        s_group_count--;
    }
}

static request_group_t *alloc_group(void) {
    if (s_group_count >= GAME_DEVAPI_MAX_GROUPS) {
        return NULL;
    }
    int tail = (s_group_head + s_group_count) % GAME_DEVAPI_MAX_GROUPS;
    request_group_t *group = &s_groups[tail];
    memset(group, 0, sizeof(*group));
    group->active = true;
    s_group_count++;
    return group;
}

static bool add_group_request(request_group_t *group, const cJSON *request) {
    if (group->request_count >= GAME_DEVAPI_MAX_GROUP_REQUESTS) {
        return false;
    }
    cJSON *copy = cJSON_Duplicate(request, true);
    if (!copy) {
        return false;
    }
    group->requests[group->request_count] = copy;
    group->request_count++;
    return true;
}

static bool enqueue_request_group(cJSON *request) {
    request_group_t *group = alloc_group();
    if (!group) {
        return false;
    }
    if (cJSON_IsArray(request)) {
        group->is_batch = true;
        group->response = cJSON_CreateArray();
        const cJSON *item = NULL;
        cJSON_ArrayForEach(item, request) {
            if (!add_group_request(group, item)) {
                free_group(group);
                s_group_count--;
                return false;
            }
        }
    } else {
        group->is_batch = false;
        if (!add_group_request(group, request)) {
            free_group(group);
            s_group_count--;
            return false;
        }
    }
    return true;
}

int nt_devapi_submit(const char *line, char *out, int out_cap) {
    cJSON *request = cJSON_Parse(line);
    if (!request) {
        return write_error_response("invalid json", out, out_cap);
    }
    if (!enqueue_request_group(request)) {
        cJSON_Delete(request);
        return write_error_response("request queue full", out, out_cap);
    }
    cJSON_Delete(request);
    process_groups();
    return pop_ready_response(out, out_cap);
}

int nt_devapi_poll_response(char *out, int out_cap) {
    process_groups();
    return pop_ready_response(out, out_cap);
}

void nt_devapi_init(void) {
    s_ep_count = 0;
    s_frame = 0;
    memset(s_held, 0, sizeof(s_held));
    memset(s_key_next_frame, 0, sizeof(s_key_next_frame));
    s_key_event_count = 0;
    s_pointer_event_count = 0;
    s_btn_mask = 0;
    s_click = 0;
    s_ptr_engaged = false;
    s_ptr_active = false;
    s_wheel_pending = false;
    s_wheel_dx = 0.0F;
    s_wheel_dy = 0.0F;
    memset(s_groups, 0, sizeof(s_groups));
    s_group_head = 0;
    s_group_count = 0;
    memset(s_ready_responses, 0, sizeof(s_ready_responses));
    s_ready_head = 0;
    s_ready_count = 0;
    s_ui_element_count = 0;
}

void nt_devapi_shutdown(void) {
    for (int i = 0; i < GAME_DEVAPI_MAX_GROUPS; i++) {
        free_group(&s_groups[i]);
    }
    for (int i = 0; i < GAME_DEVAPI_MAX_READY_RESPONSES; i++) {
        if (s_ready_responses[i]) {
            cJSON_free(s_ready_responses[i]);
            s_ready_responses[i] = NULL;
        }
    }
    s_ep_count = 0;
    s_group_head = 0;
    s_group_count = 0;
    s_ready_head = 0;
    s_ready_count = 0;
}

#endif
