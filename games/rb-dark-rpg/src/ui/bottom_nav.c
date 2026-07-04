#include "ui/bottom_nav.h"

#include "clay.h"
#include "generated/game_assets.h"
#include "hash/nt_hash.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define LAYER_NAV_ART 5
#define LAYER_NAV_TEXT_SHADOW 6
#define LAYER_NAV_TEXT 7
#define LAYER_NAV_BADGE 8

typedef enum bottom_nav_id_t {
    BOTTOM_NAV_EQUIPMENT = 0,
    BOTTOM_NAV_JOURNAL = 1,
    BOTTOM_NAV_MAP = 2,
    BOTTOM_NAV_PLACE = 3,
    BOTTOM_NAV_MORE = 4,
    BOTTOM_NAV_NONE = 5,
} bottom_nav_id_t;

typedef struct bottom_nav_item_t {
    bottom_nav_id_t id;
    const char *label;
    const char *region_name;
    const char *sheet_title;
    const char *sheet_hint;
} bottom_nav_item_t;

static const bottom_nav_item_t NAV_ITEMS[] = {
    {BOTTOM_NAV_EQUIPMENT, "Снаряж.", "ui/nav_v11_equipment", NULL, NULL},
    {BOTTOM_NAV_JOURNAL, "Дневник", "ui/nav_v11_journal", NULL, NULL},
    {BOTTOM_NAV_MAP, "Карта", "ui/nav_v11_map", NULL, NULL},
    {BOTTOM_NAV_PLACE, "Место", "ui/nav_v11_place", "Место", "Осмотреть текущую локацию"},
    {BOTTOM_NAV_MORE, "Еще", "ui/nav_v11_more", "Еще", "Настройки и помощь"},
};

static const nt_hash64_t NAV_REGION_HASHES[] = {
    ASSET_ATLAS_REGION_UI_NAV_V11_EQUIPMENT,
    ASSET_ATLAS_REGION_UI_NAV_V11_JOURNAL,
    ASSET_ATLAS_REGION_UI_NAV_V11_MAP,
    ASSET_ATLAS_REGION_UI_NAV_V11_PLACE,
    ASSET_ATLAS_REGION_UI_NAV_V11_MORE,
};

static const nt_ui_widget_def_t NAV_BUTTON_DEF = {
    .name = "bottom_nav",
    .pill_color = 0xFFB58A45U,
    ._reserved = 0U,
};

static nt_resource_t s_atlas;
static nt_atlas_region_ref_t s_regions[sizeof NAV_ITEMS / sizeof NAV_ITEMS[0]];
static bottom_nav_id_t s_active = BOTTOM_NAV_NONE;
static bottom_nav_id_t s_open_sheet = BOTTOM_NAV_NONE;

static nt_ui_label_style_t label_style(float font_size, float r, float g, float b, float a) {
    return (nt_ui_label_style_t){.font_id = 0, .font_size = font_size, .color = {r, g, b, a}};
}

static float min_f(float a, float b) { return a < b ? a : b; }

static Clay_String clay_cstr(const char *text) {
    return (Clay_String){.length = (int32_t)strlen(text), .chars = text};
}

static void ensure_regions(void) {
    if (s_atlas.id == 0U) {
        s_atlas = nt_resource_request(ASSET_ATLAS_UI, NT_ASSET_ATLAS);
        for (int i = 0; i < (int)(sizeof NAV_ITEMS / sizeof NAV_ITEMS[0]); ++i) {
            s_regions[i] = nt_atlas_ref(s_atlas, NAV_REGION_HASHES[i].value);
        }
    }
}

static void nav_shadowed_label(nt_ui_context_t *ctx, int slot, const char *text, const nt_ui_label_style_t *style) {
    nt_ui_label_style_t shadow = *style;
    shadow.color = (Clay_Color){5.0F, 3.0F, 2.0F, 190.0F};

    CLAY({.id = CLAY_IDI("bottom_nav/label_shell", slot),
          .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
        CLAY({.id = CLAY_IDI("bottom_nav/label_shadow", slot),
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP, .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                           .offset = {1.0F, 1.0F}},
              .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_NAV_TEXT_SHADOW), text, &shadow);
        }
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_NAV_TEXT), text, style);
    }
}

static void nav_badge(nt_ui_context_t *ctx, int slot, const char *text) {
    if (!text) {
        return;
    }
    const nt_ui_label_style_t badge_style = label_style(11.0F, 255.0F, 232.0F, 204.0F, 255.0F);
    CLAY({.id = CLAY_IDI("bottom_nav/badge", slot),
          .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_TOP},
                       .offset = {-8.0F, 7.0F}},
          .layout = {.sizing = {CLAY_SIZING_FIXED(21), CLAY_SIZING_FIXED(18)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {108.0F, 22.0F, 18.0F, 224.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = {222.0F, 155.0F, 82.0F, 210.0F}, .width = {1, 1, 1, 1, 0}}}) {
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_NAV_BADGE), text, &badge_style);
    }
}

static const bottom_nav_item_t *find_item(bottom_nav_id_t id) {
    for (int i = 0; i < (int)(sizeof NAV_ITEMS / sizeof NAV_ITEMS[0]); ++i) {
        if (NAV_ITEMS[i].id == id) {
            return &NAV_ITEMS[i];
        }
    }
    return NULL;
}

static void bottom_sheet_ui(nt_ui_context_t *ctx, float nav_h, float bottom_gap) {
    const bottom_nav_item_t *item = find_item(s_open_sheet);
    if (!item || !item->sheet_title) {
        return;
    }

    const nt_ui_label_style_t title = label_style(16.0F, 250.0F, 231.0F, 190.0F, 255.0F);
    const nt_ui_label_style_t hint = label_style(13.0F, 226.0F, 207.0F, 175.0F, 255.0F);
    CLAY({.id = CLAY_ID("bottom_nav/sheet"),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_BOTTOM, .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM},
                       .offset = {0.0F, -(bottom_gap + nav_h + 10.0F)}},
          .layout = {.sizing = {CLAY_SIZING_FIXED(248), CLAY_SIZING_FIXED(54)},
                     .padding = {.left = 14, .right = 14, .top = 7, .bottom = 7},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 2,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {17.0F, 11.0F, 8.0F, 190.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(6),
          .border = {.color = {126.0F, 92.0F, 56.0F, 150.0F}, .width = {1, 1, 1, 1, 0}}}) {
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_NAV_TEXT), item->sheet_title, &title);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_NAV_TEXT), item->sheet_hint, &hint);
    }
}

bool bottom_nav_sheet_open(void) { return s_open_sheet != BOTTOM_NAV_NONE; }

void bottom_nav_ui(nt_ui_context_t *ctx, const World *w) {
    (void)w;
    ensure_regions();

    float layout_w = 0.0F;
    float layout_h = 0.0F;
    nt_ui_context_layout_size(ctx, &layout_w, &layout_h);
    const bool portrait = layout_h > layout_w;
    const float side_margin = portrait ? 14.0F : 18.0F;
    const float gap = portrait ? 8.0F : 12.0F;
    const float max_btn_w = portrait ? 66.0F : 78.0F;
    const float fit_btn_w = (layout_w - side_margin * 2.0F - gap * 4.0F) / 5.0F;
    const float btn_w = min_f(max_btn_w, fit_btn_w);
    const float btn_h = btn_w * (365.0F / 299.0F);
    const float bottom_gap = portrait ? 12.0F : 10.0F;
    const float label_font = portrait ? 11.0F : 13.0F;
    const nt_ui_label_style_t label = label_style(label_font, 246.0F, 229.0F, 194.0F, 255.0F);

    bottom_sheet_ui(ctx, btn_h, bottom_gap);

    CLAY({.id = CLAY_ID("bottom_nav/root"),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_BOTTOM, .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM},
                       .offset = {0.0F, -bottom_gap}},
          .layout = {.sizing = {CLAY_SIZING_FIXED(btn_w * 5.0F + gap * 4.0F), CLAY_SIZING_FIXED(btn_h)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = (uint16_t)gap,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        for (int i = 0; i < (int)(sizeof NAV_ITEMS / sizeof NAV_ITEMS[0]); ++i) {
            const bool active = s_active == NAV_ITEMS[i].id;
            const Clay_ElementId clay_id = CLAY_SID(clay_cstr(NAV_ITEMS[i].region_name));
            const uint32_t id = clay_id.id;
            CLAY({.id = clay_id,
                  .layout = {.sizing = {CLAY_SIZING_FIXED(btn_w), CLAY_SIZING_FIXED(btn_h)},
                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
                const int16_t hit_pad[4] = {4, 4, 4, 4};
                const nt_ui_events_t events = nt_ui_events_padded(ctx, id, NULL, hit_pad);
                nt_ui_widget_register(ctx, id, &NAV_BUTTON_DEF, hit_pad, true);
                const float visual_scale = events.pressed ? 0.965F : (events.hovered ? 1.045F : (active ? 1.015F : 1.0F));
                nt_ui_image_style_t image_style = nt_ui_image_style_defaults();
                image_style.color_packed = events.pressed ? 0xFFE3E3E3U : (events.hovered || active ? 0xFFFFFFFFU : 0xFFF0F0F0U);

                CLAY({.id = CLAY_IDI("bottom_nav/art_anchor", i),
                      .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                                   .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER}},
                      .layout = {.sizing = {CLAY_SIZING_FIXED(btn_w * visual_scale), CLAY_SIZING_FIXED(btn_h * visual_scale)}}}) {
                    nt_ui_image(ctx, NT_UI_DATA_LAYER(LAYER_NAV_ART), &s_regions[i], &image_style, NULL);
                }

                CLAY({.id = CLAY_IDI("bottom_nav/label_anchor", i),
                      .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                                   .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_BOTTOM, .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM},
                                   .offset = {0.0F, portrait ? -6.0F : -7.0F}},
                      .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
                    nav_shadowed_label(ctx, i, NAV_ITEMS[i].label, &label);
                }
                nav_badge(ctx, i, NULL);

                if (events.clicked) {
                    s_active = NAV_ITEMS[i].id;
                    if (NAV_ITEMS[i].id == BOTTOM_NAV_PLACE || NAV_ITEMS[i].id == BOTTOM_NAV_MORE) {
                        s_open_sheet = s_open_sheet == NAV_ITEMS[i].id ? BOTTOM_NAV_NONE : NAV_ITEMS[i].id;
                    } else {
                        s_open_sheet = BOTTOM_NAV_NONE;
                    }
                }
            }
        }
    }
}
