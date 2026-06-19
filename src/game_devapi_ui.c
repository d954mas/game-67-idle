#include "game_devapi_ui.h"

#if NT_DEVAPI_ENABLED

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "comp_storage/nt_comp_storage.h"
#include "devapi/nt_devapi.h"
#include "drawable_comp/nt_drawable_comp.h"
#include "entity/nt_entity.h"
#include "input/nt_input.h"
#include "input/nt_input_internal.h"
#include "transform_comp/nt_transform_comp.h"

#define GAME_UI_MAX 64
#define GAME_UI_ID_MAX 48
#define GAME_UI_STR_MAX 96

typedef struct {
    char id[GAME_UI_ID_MAX];
    char parent_id[GAME_UI_ID_MAX];
    char role[GAME_UI_ID_MAX];
    char label[GAME_UI_STR_MAX];
    char text[GAME_UI_STR_MAX];
    float x;
    float y;
    float w;
    float h;
    bool visible;
    bool enabled;
} ui_node_t;

static ui_node_t s_nodes[GAME_UI_MAX];
static int s_node_count;

void game_devapi_ui_clear(void) {
    s_node_count = 0;
}

static void copy_str(char *dst, size_t cap, const char *src) {
    (void)snprintf(dst, cap, "%s", src ? src : "");
}

bool game_devapi_ui_register_node(const char *id, const char *parent_id, const char *role,
                                  const char *label, const char *text,
                                  float x, float y, float w, float h, bool visible, bool enabled) {
    if (s_node_count >= GAME_UI_MAX || !id || !id[0]) {
        return false;
    }
    ui_node_t *node = &s_nodes[s_node_count++];
    copy_str(node->id, sizeof(node->id), id);
    copy_str(node->parent_id, sizeof(node->parent_id), parent_id);
    copy_str(node->role, sizeof(node->role), role);
    copy_str(node->label, sizeof(node->label), label);
    copy_str(node->text, sizeof(node->text), text);
    node->x = x;
    node->y = y;
    node->w = w;
    node->h = h;
    node->visible = visible;
    node->enabled = enabled;
    return true;
}

static const ui_node_t *find_node(const char *id) {
    if (!id || !id[0]) {
        return NULL;
    }
    for (int i = 0; i < s_node_count; ++i) {
        if (strcmp(s_nodes[i].id, id) == 0) {
            return &s_nodes[i];
        }
    }
    return NULL;
}

static void node_to_obj(const ui_node_t *node, cJSON *obj) {
    cJSON_AddStringToObject(obj, "id", node->id);
    cJSON_AddStringToObject(obj, "parent_id", node->parent_id);
    cJSON_AddStringToObject(obj, "role", node->role);
    cJSON_AddStringToObject(obj, "label", node->label);
    cJSON_AddStringToObject(obj, "text", node->text);
    cJSON_AddNumberToObject(obj, "x", (double)node->x);
    cJSON_AddNumberToObject(obj, "y", (double)node->y);
    cJSON_AddNumberToObject(obj, "w", (double)node->w);
    cJSON_AddNumberToObject(obj, "h", (double)node->h);
    cJSON_AddNumberToObject(obj, "center_x", (double)(node->x + (node->w * 0.5F)));
    cJSON_AddNumberToObject(obj, "center_y", (double)(node->y + (node->h * 0.5F)));
    cJSON_AddBoolToObject(obj, "visible", node->visible);
    cJSON_AddBoolToObject(obj, "enabled", node->enabled);
}

static bool ui_fail(nt_devapi_error *err, const char *message) {
    err->code = "bad_params";
    err->message = message;
    return false;
}

static bool ep_ui_tree(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    cJSON *nodes = cJSON_AddArrayToObject(result_obj, "nodes");
    for (int i = 0; i < s_node_count; ++i) {
        cJSON *obj = cJSON_CreateObject();
        node_to_obj(&s_nodes[i], obj);
        cJSON_AddItemToArray(nodes, obj);
    }
    return true;
}

static const ui_node_t *node_from_params(const cJSON *params) {
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(params, "id");
    return find_node(cJSON_IsString(id) ? id->valuestring : "");
}

static bool ep_ui_element(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)user;
    const ui_node_t *node = node_from_params(params);
    if (!node) {
        return ui_fail(err, "unknown ui id");
    }
    node_to_obj(node, result_obj);
    return true;
}

/* Click = synthetic pointer DOWN then UP at the node center, injected into the
   engine input queue (drains on the next sim-advance; bots inject then step). */
static bool ep_ui_click(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)user;
    const ui_node_t *node = node_from_params(params);
    if (!node) {
        return ui_fail(err, "unknown ui id");
    }
    const float x = node->x + (node->w * 0.5F);
    const float y = node->y + (node->h * 0.5F);
    (void)nt_input_inject_pointer(NT_INJECT_POINTER_DOWN, 0u, x, y, 1.0F, 0u, 1u);
    (void)nt_input_inject_pointer(NT_INJECT_POINTER_UP, 0u, x, y, 0.0F, 0u, 0u);
    cJSON_AddStringToObject(result_obj, "clicked", node->id);
    cJSON_AddNumberToObject(result_obj, "x", (double)x);
    cJSON_AddNumberToObject(result_obj, "y", (double)y);
    return true;
}

static bool ep_entity_list(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    nt_transform_comp_view_t transforms = nt_transform_comp_view();
    nt_drawable_comp_view_t drawables = nt_drawable_comp_view();
    const uint16_t max_entities = nt_entity_max();
    cJSON *items = cJSON_AddArrayToObject(result_obj, "entities");
    for (uint16_t e = 0; e < max_entities; ++e) {
        const uint16_t transform_idx = transforms.sparse_indices ? transforms.sparse_indices[e] : NT_INVALID_COMP_INDEX;
        const uint16_t drawable_idx = drawables.sparse_indices ? drawables.sparse_indices[e] : NT_INVALID_COMP_INDEX;
        if (transform_idx == NT_INVALID_COMP_INDEX && drawable_idx == NT_INVALID_COMP_INDEX) {
            continue;
        }
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "entity", (double)e);
        if (transform_idx != NT_INVALID_COMP_INDEX) {
            const float *m = transforms.world_matrices[transform_idx];
            cJSON_AddNumberToObject(obj, "x", (double)m[12]);
            cJSON_AddNumberToObject(obj, "y", (double)m[13]);
            cJSON_AddNumberToObject(obj, "z", (double)m[14]);
        }
        if (drawable_idx != NT_INVALID_COMP_INDEX) {
            cJSON_AddNumberToObject(obj, "color", (double)drawables.colors_packed[drawable_idx]);
            cJSON_AddBoolToObject(obj, "visible", drawables.visible[drawable_idx]);
        }
        cJSON_AddItemToArray(items, obj);
    }
    return true;
}

void game_devapi_ui_register(void) {
    static const nt_devapi_command_desc descs[] = {
        {"ui.tree", "game", "List the game's registered UI nodes.", "", "{nodes}", "immediate", "none"},
        {"ui.element", "game", "Get one UI node by id.", "id", "{node}", "immediate", "none"},
        {"ui.click", "game", "Click a UI node by id via synthetic pointer.", "id", "{clicked,x,y}", "next-frame", "injects input"},
        {"entity.list", "game", "List entities with transform/drawable.", "", "{entities}", "immediate", "none"},
    };
    (void)nt_devapi_register(&descs[0], ep_ui_tree, NULL);
    (void)nt_devapi_register(&descs[1], ep_ui_element, NULL);
    (void)nt_devapi_register(&descs[2], ep_ui_click, NULL);
    (void)nt_devapi_register(&descs[3], ep_entity_list, NULL);
}

#else

/* Keep the translation unit non-empty when the DevAPI is gated off (ISO C
   requires at least one declaration; -Werror,-Wempty-translation-unit). */
typedef int game_devapi_ui_disabled_tu;

#endif
