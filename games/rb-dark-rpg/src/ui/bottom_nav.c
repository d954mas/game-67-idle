#include "ui/bottom_nav.h"

#include "clay.h"
#include "atlas/nt_atlas.h"
#include "game_audio.h"
#include "generated/game_assets.h"
#include "hash/nt_hash.h"
#include "memory/nt_mem_scratch.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"
#include "ui/combat_flow.h"
#include "ui/equipment_screen.h"
#include "ui/location_screen.h"
#include "ui/nt_ui_label.h"
#include "ui/shop_screen.h"
#include "ui/world_map_screen.h"

#include <stddef.h>
#include <stdint.h>

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
    {BOTTOM_NAV_PLACE, "Здесь", "ui/nav_v11_place", NULL, NULL},
    {BOTTOM_NAV_MORE, "Еще", "ui/nav_v11_more", "Еще", "Настройки и помощь"},
};

static Clay_ElementId nav_slot_clay_id(bottom_nav_id_t id) {
  switch (id) {
  case BOTTOM_NAV_EQUIPMENT:
    return CLAY_ID("bottom_nav/slot/equipment");
  case BOTTOM_NAV_JOURNAL:
    return CLAY_ID("bottom_nav/slot/journal");
  case BOTTOM_NAV_MAP:
    return CLAY_ID("bottom_nav/slot/map");
  case BOTTOM_NAV_PLACE:
    return CLAY_ID("bottom_nav/slot/place");
  case BOTTOM_NAV_MORE:
    return CLAY_ID("bottom_nav/slot/more");
  case BOTTOM_NAV_NONE:
  default:
    return CLAY_ID("bottom_nav/slot/none");
  }
}

static const nt_hash64_t NAV_REGION_HASHES[] = {
    ASSET_ATLAS_REGION_UI_NAV_V11_EQUIPMENT,
    ASSET_ATLAS_REGION_UI_NAV_V11_JOURNAL,
    ASSET_ATLAS_REGION_UI_NAV_V11_MAP,
    ASSET_ATLAS_REGION_UI_NAV_V11_PLACE,
    ASSET_ATLAS_REGION_UI_NAV_V11_MORE,
};

static nt_resource_t s_atlas;
static nt_atlas_region_ref_t s_regions[sizeof NAV_ITEMS / sizeof NAV_ITEMS[0]];
static bottom_nav_id_t s_active = BOTTOM_NAV_NONE;
static bottom_nav_id_t s_open_sheet = BOTTOM_NAV_NONE;

static nt_ui_label_style_t label_style(float font_size, float r, float g,
                                       float b, float a) {
  return (nt_ui_label_style_t){
      .font_id = 0, .font_size = font_size, .color = {r, g, b, a}};
}

static float min_f(float a, float b) { return a < b ? a : b; }

static Clay_Color nav_tint(uint32_t packed) {
  if (packed == 0xFFFFFFFFU) {
    return (Clay_Color){0};
  }
  return (Clay_Color){(float)(packed & 0xFFU),
                      (float)((packed >> 8U) & 0xFFU),
                      (float)((packed >> 16U) & 0xFFU),
                      (float)((packed >> 24U) & 0xFFU)};
}

static const nt_ui_widget_def_t NAV_BUTTON_DEF = {
    .name = "bottom_nav",
    .pill_color = 0xFFB58A45U,
    ._reserved = 0U,
};

static void nav_image_visual(const nt_ui_element_data_t *data,
                             nt_atlas_region_ref_t *region,
                             uint32_t color_packed,
                             const Clay_ElementDeclaration *decl) {
  nt_atlas_resolve_ref(region);
  if (region->region == NT_ATLAS_INVALID_REGION) {
    return;
  }

  nt_ui_image_payload_t *payload = NT_MEM_SCRATCH_ALLOC(nt_ui_image_payload_t);
  if (!payload) {
    return;
  }
  *payload = (nt_ui_image_payload_t){
      .atlas = region->atlas,
      .region_index = region->region,
      .origin_x = 0.5F,
      .origin_y = 0.5F,
      .slice9_scale = 1.0F,
  };

  Clay_ElementDeclaration final =
      decl ? *decl
           : (Clay_ElementDeclaration){
                 .layout = {.sizing = {CLAY_SIZING_GROW(0),
                                       CLAY_SIZING_GROW(0)}}};
  final.image = (Clay_ImageElementConfig){.imageData = payload};
  final.backgroundColor = nav_tint(color_packed);
  final.userData = (void *)data;
  CLAY(final) {}
}

static void ensure_regions(void) {
  if (s_atlas.id == 0U) {
    s_atlas = nt_resource_request(ASSET_ATLAS_UI, NT_ASSET_ATLAS);
    for (int i = 0; i < (int)(sizeof NAV_ITEMS / sizeof NAV_ITEMS[0]); ++i) {
      s_regions[i] = nt_atlas_ref(s_atlas, NAV_REGION_HASHES[i].value);
    }
  }
}

static void nav_shadowed_label(nt_ui_context_t *ctx, int slot, const char *text,
                               const nt_ui_label_style_t *style) {
  nt_ui_label_style_t shadow = *style;
  shadow.color = (Clay_Color){5.0F, 3.0F, 2.0F, 190.0F};

  CLAY({.id = CLAY_IDI("bottom_nav/label_shell", slot),
        .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
    CLAY({.id = CLAY_IDI("bottom_nav/label_shadow", slot),
          .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                        .parent = CLAY_ATTACH_POINT_LEFT_TOP},
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
  const nt_ui_label_style_t badge_style =
      label_style(11.0F, 255.0F, 232.0F, 204.0F, 255.0F);
  CLAY(
      {.id = CLAY_IDI("bottom_nav/badge", slot),
       .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                    .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP,
                                     .parent = CLAY_ATTACH_POINT_RIGHT_TOP},
                    .offset = {-8.0F, 7.0F}},
       .layout = {.sizing = {CLAY_SIZING_FIXED(21), CLAY_SIZING_FIXED(18)},
                  .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
       .backgroundColor = {108.0F, 22.0F, 18.0F, 224.0F},
       .cornerRadius = CLAY_CORNER_RADIUS(4),
       .border = {.color = {222.0F, 155.0F, 82.0F, 210.0F},
                  .width = {1, 1, 1, 1, 0}}}) {
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

static void bottom_sheet_ui(nt_ui_context_t *ctx, float nav_h,
                            float bottom_gap) {
  const bottom_nav_item_t *item = find_item(s_open_sheet);
  if (!item || !item->sheet_title) {
    return;
  }

  const nt_ui_label_style_t title =
      label_style(16.0F, 250.0F, 231.0F, 190.0F, 255.0F);
  const nt_ui_label_style_t hint =
      label_style(13.0F, 226.0F, 207.0F, 175.0F, 255.0F);
  CLAY(
      {.id = CLAY_ID("bottom_nav/sheet"),
       .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                    .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_BOTTOM,
                                     .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM},
                    .offset = {0.0F, -(bottom_gap + nav_h + 10.0F)}},
       .layout = {.sizing = {CLAY_SIZING_FIXED(248.0F),
                             CLAY_SIZING_FIXED(54.0F)},
                  .padding = {.left = 14, .right = 14, .top = 7, .bottom = 7},
                  .layoutDirection = CLAY_TOP_TO_BOTTOM,
                  .childGap = 2,
                  .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
       .backgroundColor = {17.0F, 11.0F, 8.0F, 190.0F},
       .cornerRadius = CLAY_CORNER_RADIUS(6),
       .border = {.color = {126.0F, 92.0F, 56.0F, 150.0F},
                  .width = {1, 1, 1, 1, 0}}}) {
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_NAV_TEXT), item->sheet_title,
                &title);
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_NAV_TEXT), item->sheet_hint, &hint);
  }
}

bool bottom_nav_sheet_open(void) {
  return s_open_sheet != BOTTOM_NAV_NONE || equipment_screen_open() ||
         location_screen_open() || world_map_screen_open() ||
         shop_screen_open();
}

void bottom_nav_ui(nt_ui_context_t *ctx, World *w) {
  if (combat_flow_is_open(w)) {
    return;
  }
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
  bottom_sheet_ui(ctx, btn_h, bottom_gap);

  CLAY(
      {.id = CLAY_ID("bottom_nav/root"),
       .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                    .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_BOTTOM,
                                     .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM},
                    .offset = {0.0F, -bottom_gap}},
       .layout = {
           .sizing = {CLAY_SIZING_FIXED(btn_w * 5.0F + gap * 4.0F),
                      CLAY_SIZING_FIXED(btn_h)},
           .layoutDirection = CLAY_LEFT_TO_RIGHT,
           .childGap = (uint16_t)gap,
           .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
    for (int i = 0; i < (int)(sizeof NAV_ITEMS / sizeof NAV_ITEMS[0]); ++i) {
      const bool active = s_active == NAV_ITEMS[i].id;
      const Clay_ElementId slot_id = nav_slot_clay_id(NAV_ITEMS[i].id);
      const uint32_t id = slot_id.id;
      const int16_t hit_pad[4] = {4, 4, 4, 4};
      CLAY({.id = slot_id,
            .layout = {.sizing = {CLAY_SIZING_FIXED(btn_w),
                                  CLAY_SIZING_FIXED(btn_h)},
                       .childAlignment = {CLAY_ALIGN_X_CENTER,
                                          CLAY_ALIGN_Y_CENTER}},
            .userData = NT_UI_CLAY_DATA(LAYER_NAV_ART)}) {
        nt_ui_widget_register(ctx, id, &NAV_BUTTON_DEF, hit_pad, true);
        const nt_ui_interaction_t preview =
            nt_ui_query_interaction_padded(ctx, id, hit_pad);
        const float visual_scale =
            preview.pressed ? 0.965F
                           : (preview.hovered ? 1.045F
                                             : (active ? 1.015F : 1.0F));
        const uint32_t image_tint =
            preview.pressed ? 0xFFE3E3E3U
                           : ((preview.hovered || active) ? 0xFFFFFFFFU
                                                         : 0xFFF0F0F0U);
        const nt_ui_label_style_t label =
            label_style(label_font * visual_scale, 246.0F, 229.0F, 194.0F, 255.0F);

        CLAY({.id = CLAY_IDI("bottom_nav/art_anchor", i),
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints =
                               {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                .parent = CLAY_ATTACH_POINT_CENTER_CENTER}},
              .layout = {.sizing = {CLAY_SIZING_FIXED(btn_w * visual_scale),
                                    CLAY_SIZING_FIXED(btn_h * visual_scale)}}}) {
          nav_image_visual(NT_UI_DATA_LAYER(LAYER_NAV_ART), &s_regions[i],
                           image_tint, NULL);
        }

        CLAY({.id = CLAY_IDI("bottom_nav/label_anchor", i),
              .floating =
                  {.attachTo = CLAY_ATTACH_TO_PARENT,
                   .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_BOTTOM,
                                    .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM},
                   .offset = {0.0F, portrait ? -6.0F : -7.0F}},
              .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
          nav_shadowed_label(ctx, i, NAV_ITEMS[i].label, &label);
        }
        nav_badge(ctx, i, NULL);

        const nt_ui_events_t events = nt_ui_events_padded(ctx, id, NULL, hit_pad);
        if (events.clicked) {
          game_audio_play(GAME_AUDIO_CUE_UI_CLICK);
          s_active = NAV_ITEMS[i].id;
          if (NAV_ITEMS[i].id == BOTTOM_NAV_EQUIPMENT) {
            s_open_sheet = BOTTOM_NAV_NONE;
            world_map_screen_set_open(false);
            location_screen_set_open(false);
            shop_screen_set_open(false);
            equipment_screen_toggle();
          } else if (NAV_ITEMS[i].id == BOTTOM_NAV_MAP) {
            equipment_screen_set_open(false);
            location_screen_set_open(false);
            shop_screen_set_open(false);
            s_open_sheet = BOTTOM_NAV_NONE;
            world_map_screen_toggle_map();
          } else if (NAV_ITEMS[i].id == BOTTOM_NAV_PLACE) {
            equipment_screen_set_open(false);
            world_map_screen_set_open(false);
            shop_screen_set_open(false);
            s_open_sheet = BOTTOM_NAV_NONE;
            location_screen_toggle();
          } else if (NAV_ITEMS[i].id == BOTTOM_NAV_MORE) {
            equipment_screen_set_open(false);
            location_screen_set_open(false);
            world_map_screen_set_open(false);
            shop_screen_set_open(false);
            s_open_sheet = s_open_sheet == NAV_ITEMS[i].id ? BOTTOM_NAV_NONE
                                                           : NAV_ITEMS[i].id;
          } else {
            equipment_screen_set_open(false);
            location_screen_set_open(false);
            world_map_screen_set_open(false);
            shop_screen_set_open(false);
            s_open_sheet = BOTTOM_NAV_NONE;
          }
        }
      }
    }
  }
}
