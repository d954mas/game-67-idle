#include "features/dress_room/dress_room.h"

#include "atlas/nt_atlas.h"
#include "clay.h"
#include "generated/game_assets.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"
#include "ui/nt_ui.h"
#include "ui/nt_ui_button.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_scroll.h"
#include "ui/theme.h"

#include <stdio.h>
#include <string.h>

#define LAYER_BG 0
#define LAYER_IMG 1
#define LAYER_TEXT 2
#define LAYER_STAGE 3

static nt_resource_t s_dress_atlas;
static bool s_atlas_req;
static bool s_static_regs;
static nt_atlas_region_ref_t s_body_reg;
static nt_atlas_region_ref_t s_layer_reg[DRESS_SLOT_COUNT];
static nt_atlas_region_ref_t s_thumb_reg[32];

/* DevAPI and accessibility need the actual semantic string, not only the same
   32-bit hash. Clay retains the string pointer for the completed-frame tree,
   so dynamic ids live in a file-static per-frame ring. */
#define DRESS_UI_ID_SLOTS 96
#define DRESS_UI_ID_LEN 64
static char s_ui_id_storage[DRESS_UI_ID_SLOTS][DRESS_UI_ID_LEN];
static int s_ui_id_cursor;
static bool s_lookbook_open;
static int s_lookbook_detail_recipe = -1;
static int s_lookbook_detail_slot = 0;

static Clay_ElementId semantic_clay_id(const char *id) {
    char *stored = s_ui_id_storage[s_ui_id_cursor % DRESS_UI_ID_SLOTS];
    s_ui_id_cursor += 1;
    (void)snprintf(stored, DRESS_UI_ID_LEN, "%s", id ? id : "dress/invalid");
    return Clay_GetElementId((Clay_String){
        .isStaticallyAllocated = false,
        .length = (int32_t)strlen(stored),
        .chars = stored,
    });
}

static uint32_t semantic_ui_id(const char *id) { return semantic_clay_id(id).id; }

typedef struct awakening_visual {
    Clay_Color backdrop;
    Clay_Color panel;
    Clay_Color primary;
    Clay_Color secondary;
    const char *power_line;
    const char *hero_word;
    int motif;
} awakening_visual_t;

static nt_ui_label_style_t light_label(float size) {
    return (nt_ui_label_style_t){
        .font_id = 0,
        .font_size = size,
        .color = {255.0F, 248.0F, 253.0F, 255.0F},
    };
}

static nt_ui_label_style_t ink_label(float size) {
    return (nt_ui_label_style_t){
        .font_id = 0,
        .font_size = size,
        .color = {228.0F, 222.0F, 255.0F, 255.0F},
    };
}

static awakening_visual_t awakening_visual(const dress_awakening_recipe_t *recipe) {
    awakening_visual_t visual = {
        .backdrop = {43.0F, 19.0F, 69.0F, 255.0F},
        .panel = {73.0F, 37.0F, 101.0F, 245.0F},
        .primary = {151.0F, 130.0F, 255.0F, 255.0F},
        .secondary = {247.0F, 205.0F, 255.0F, 255.0F},
        .power_line = "STARLIGHT ANSWERS YOU",
        .hero_word = "AWAKENED",
        .motif = 0,
    };
    if (!recipe || !recipe->id) {
        return visual;
    }
    if (strcmp(recipe->id, "moon-moon") == 0) {
        visual.backdrop = (Clay_Color){25.0F, 20.0F, 75.0F, 255.0F};
        visual.panel = (Clay_Color){31.0F, 25.0F, 83.0F, 238.0F};
        visual.primary = (Clay_Color){153.0F, 139.0F, 255.0F, 255.0F};
        visual.secondary = (Clay_Color){233.0F, 224.0F, 255.0F, 255.0F};
        visual.power_line = "TWIN MOONS ALIGN";
        visual.hero_word = "ORACLE";
        visual.motif = 0;
    } else if (strcmp(recipe->id, "bloom-bloom") == 0) {
        visual.backdrop = (Clay_Color){13.0F, 70.0F, 63.0F, 255.0F};
        visual.panel = (Clay_Color){18.0F, 58.0F, 58.0F, 238.0F};
        visual.primary = (Clay_Color){83.0F, 239.0F, 154.0F, 255.0F};
        visual.secondary = (Clay_Color){255.0F, 151.0F, 211.0F, 255.0F};
        visual.power_line = "THE GARDEN CROWNS YOU";
        visual.hero_word = "BLOSSOM";
        visual.motif = 1;
    } else if (strcmp(recipe->id, "flame-flame") == 0) {
        visual.backdrop = (Clay_Color){91.0F, 21.0F, 34.0F, 255.0F};
        visual.panel = (Clay_Color){75.0F, 21.0F, 38.0F, 238.0F};
        visual.primary = (Clay_Color){255.0F, 78.0F, 69.0F, 255.0F};
        visual.secondary = (Clay_Color){255.0F, 205.0F, 64.0F, 255.0F};
        visual.power_line = "TWO FLAMES BECOME A SUN";
        visual.hero_word = "SOLAR";
        visual.motif = 2;
    } else if (strcmp(recipe->id, "moon-bloom") == 0) {
        visual.backdrop = (Clay_Color){38.0F, 34.0F, 91.0F, 255.0F};
        visual.panel = (Clay_Color){24.0F, 27.0F, 74.0F, 238.0F};
        visual.primary = (Clay_Color){155.0F, 138.0F, 255.0F, 255.0F};
        visual.secondary = (Clay_Color){83.0F, 238.0F, 167.0F, 255.0F};
        visual.power_line = "A DREAMGARDEN OPENS";
        visual.hero_word = "ENCHANTED";
        visual.motif = 3;
    } else if (strcmp(recipe->id, "moon-flame") == 0) {
        visual.backdrop = (Clay_Color){43.0F, 17.0F, 64.0F, 255.0F};
        visual.panel = (Clay_Color){52.0F, 22.0F, 72.0F, 238.0F};
        visual.primary = (Clay_Color){156.0F, 139.0F, 255.0F, 255.0F};
        visual.secondary = (Clay_Color){255.0F, 81.0F, 74.0F, 255.0F};
        visual.power_line = "THE ECLIPSE IGNITES";
        visual.hero_word = "ECLIPSE";
        visual.motif = 4;
    } else if (strcmp(recipe->id, "bloom-flame") == 0) {
        visual.backdrop = (Clay_Color){83.0F, 24.0F, 58.0F, 255.0F};
        visual.panel = (Clay_Color){64.0F, 25.0F, 56.0F, 238.0F};
        visual.primary = (Clay_Color){255.0F, 91.0F, 103.0F, 255.0F};
        visual.secondary = (Clay_Color){79.0F, 235.0F, 148.0F, 255.0F};
        visual.power_line = "THE PHOENIX ROSE RISES";
        visual.hero_word = "REBORN";
        visual.motif = 5;
    }
    return visual;
}

static void ensure_atlas(void) {
    if (!s_atlas_req) {
        s_dress_atlas = nt_resource_request(ASSET_ATLAS_DRESS, NT_ASSET_ATLAS);
        s_atlas_req = true;
    }
    if (!s_static_regs) {
        s_body_reg = nt_atlas_ref(s_dress_atlas, ASSET_ATLAS_REGION_DRESS_BODY_BASE.value);
        s_static_regs = true;
    }
}

static nt_atlas_region_ref_t region_named(const char *sprite) {
    ensure_atlas();
    struct named_region {
        const char *name;
        uint64_t hash;
    };
    static const struct named_region table[] = {
        {"hair_bob", ASSET_ATLAS_REGION_DRESS_HAIR_BOB.value},
        {"hair_long", ASSET_ATLAS_REGION_DRESS_HAIR_LONG.value},
        {"hair_pink", ASSET_ATLAS_REGION_DRESS_HAIR_PINK.value},
        {"hair_gold", ASSET_ATLAS_REGION_DRESS_HAIR_GOLD.value},
        {"top_tee", ASSET_ATLAS_REGION_DRESS_TOP_TEE.value},
        {"top_hoodie", ASSET_ATLAS_REGION_DRESS_TOP_HOODIE.value},
        {"top_blazer", ASSET_ATLAS_REGION_DRESS_TOP_BLAZER.value},
        {"top_crop", ASSET_ATLAS_REGION_DRESS_TOP_CROP.value},
        {"bot_jeans", ASSET_ATLAS_REGION_DRESS_BOT_JEANS.value},
        {"bot_skirt", ASSET_ATLAS_REGION_DRESS_BOT_SKIRT.value},
        {"bot_shorts", ASSET_ATLAS_REGION_DRESS_BOT_SHORTS.value},
        {"bot_cargo", ASSET_ATLAS_REGION_DRESS_BOT_CARGO.value},
        {"shoe_sneak", ASSET_ATLAS_REGION_DRESS_SHOE_SNEAK.value},
        {"shoe_boot", ASSET_ATLAS_REGION_DRESS_SHOE_BOOT.value},
        {"shoe_heel", ASSET_ATLAS_REGION_DRESS_SHOE_HEEL.value},
        {"shoe_sandal", ASSET_ATLAS_REGION_DRESS_SHOE_SANDAL.value},
        {"acc_glasses", ASSET_ATLAS_REGION_DRESS_ACC_GLASSES.value},
        {"acc_hat", ASSET_ATLAS_REGION_DRESS_ACC_HAT.value},
        {"acc_bag", ASSET_ATLAS_REGION_DRESS_ACC_BAG.value},
        {"acc_scarf", ASSET_ATLAS_REGION_DRESS_ACC_SCARF.value},
    };
    for (int i = 0; i < (int)(sizeof table / sizeof table[0]); ++i) {
        if (strcmp(sprite, table[i].name) == 0) {
            return nt_atlas_ref(s_dress_atlas, table[i].hash);
        }
    }
    return nt_atlas_ref(s_dress_atlas, ASSET_ATLAS_REGION_DRESS_BODY_BASE.value);
}

static bool styled_button(nt_ui_context_t *ctx, const char *id, const char *text, float w, float h,
                          int kind, bool enabled) {
    nt_ui_button_style_t style = g_theme.button;
    nt_ui_label_style_t text_style = g_theme.button_label_light;
    Clay_Color border = {72.0F, 58.0F, 121.0F, 255.0F};
    style.idle.bg = (nt_atlas_region_ref_t){0};
    style.hover.bg = (nt_atlas_region_ref_t){0};
    style.pressed.bg = (nt_atlas_region_ref_t){0};
    style.disabled.bg = (nt_atlas_region_ref_t){0};
    style.idle.bg_tint = 0xFF461821U;
    style.hover.bg_tint = 0xFF702B39U;
    style.pressed.bg_tint = 0xFF8A3248U;
    if (kind == 1) {
        style = g_theme.button_success;
        style.idle.bg = (nt_atlas_region_ref_t){0};
        style.hover.bg = (nt_atlas_region_ref_t){0};
        style.pressed.bg = (nt_atlas_region_ref_t){0};
        style.disabled.bg = (nt_atlas_region_ref_t){0};
        text_style = g_theme.button_label_light;
        style.idle.bg_tint = 0xFFA72BD1U;
        style.hover.bg_tint = 0xFFC849E8U;
        style.pressed.bg_tint = 0xFF8D219FU;
        border = (Clay_Color){255.0F, 69.0F, 212.0F, 255.0F};
    } else if (kind == 2) {
        style = g_theme.button_selected;
        style.idle.bg = (nt_atlas_region_ref_t){0};
        style.hover.bg = (nt_atlas_region_ref_t){0};
        style.pressed.bg = (nt_atlas_region_ref_t){0};
        style.disabled.bg = (nt_atlas_region_ref_t){0};
        style.idle.bg_tint = 0xFF593A16U;
        style.hover.bg_tint = 0xFF745128U;
        style.pressed.bg_tint = 0xFF452B10U;
        border = (Clay_Color){69.0F, 231.0F, 255.0F, 255.0F};
    }
    char control_id[DRESS_UI_ID_LEN];
    (void)snprintf(control_id, sizeof control_id, "%s/control", id);
    if (!enabled) {
        nt_ui_label_style_t disabled_text = light_label(text_style.font_size);
        disabled_text.color = (Clay_Color){165.0F, 149.0F, 174.0F, 255.0F};
        CLAY({.id = semantic_clay_id(id),
              .layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
              .cornerRadius = CLAY_CORNER_RADIUS(16),
              .backgroundColor = {55.0F, 40.0F, 69.0F, 220.0F},
              .userData = NT_UI_CLAY_DATA(LAYER_IMG)}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), text, &disabled_text);
        }
        return false;
    }
    bool clicked = false;
    CLAY({.id = semantic_clay_id(id),
          .layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                     .padding = CLAY_PADDING_ALL(2)},
          .cornerRadius = CLAY_CORNER_RADIUS(18),
          .backgroundColor = border,
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), semantic_ui_id(control_id), &style,
                           &(Clay_ElementDeclaration){
                               .layout = {
                                   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                   .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                               },
                           },
                           true, NULL);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), text, &text_style);
        clicked = nt_ui_button_end(ctx);
    }
    return clicked;
}

static void draw_sprite(nt_ui_context_t *ctx, nt_atlas_region_ref_t *region, float w, float h) {
    nt_ui_image_style_t style = nt_ui_image_style_defaults();
    nt_ui_image(ctx, NT_UI_DATA_LAYER(LAYER_STAGE), region, &style,
                &(Clay_ElementDeclaration){
                    .layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)}},
                });
}

static void draw_outfit_figure(nt_ui_context_t *ctx, float char_w, float char_h) {
    ensure_atlas();
    CLAY({.id = CLAY_ID("dress/figure"),
          .layout = {.sizing = {CLAY_SIZING_FIXED(char_w), CLAY_SIZING_FIXED(char_h)}}}) {
        CLAY({.id = CLAY_ID("dress/figure/body"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(char_w), CLAY_SIZING_FIXED(char_h)}},
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                            .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                           .zIndex = 1}}) {
            draw_sprite(ctx, &s_body_reg, char_w, char_h);
        }
        static const dress_slot_t order[DRESS_SLOT_COUNT] = {
            DRESS_SLOT_BOTTOM, DRESS_SLOT_TOP, DRESS_SLOT_SHOES, DRESS_SLOT_HAIR, DRESS_SLOT_ACC,
        };
        for (int i = 0; i < DRESS_SLOT_COUNT; ++i) {
            const dress_slot_t slot = order[i];
            if (slot == DRESS_SLOT_BOTTOM && dress_room_main_covers_bottom()) {
                continue;
            }
            const dress_item_t *item = dress_room_catalog_item(dress_room_equipped(slot));
            if (!item || !item->atlas_layer) {
                continue;
            }
            s_layer_reg[slot] = region_named(item->atlas_layer);
            CLAY({.id = CLAY_IDI("dress/figure/layer", (uint32_t)slot),
                  .layout = {.sizing = {CLAY_SIZING_FIXED(char_w), CLAY_SIZING_FIXED(char_h)}},
                  .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                               .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                                .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                               .zIndex = (int16_t)(2 + i)}}) {
                draw_sprite(ctx, &s_layer_reg[slot], char_w, char_h);
            }
        }
    }
}

static void draw_stage(nt_ui_context_t *ctx, float stage_w, float stage_h, bool magical,
                       awakening_visual_t visual) {
    ensure_atlas();
    float char_h = stage_h * 0.98F;
    float char_w = char_h * (512.0F / 896.0F);
    if (char_w > stage_w * 0.96F) {
        char_w = stage_w * 0.96F;
        char_h = char_w * (896.0F / 512.0F);
    }
    CLAY({.id = CLAY_ID("dress/stage"),
          .layout = {.sizing = {CLAY_SIZING_FIXED(stage_w), CLAY_SIZING_FIXED(stage_h)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .cornerRadius = CLAY_CORNER_RADIUS(24),
          .backgroundColor = magical ? visual.panel : (Clay_Color){13.0F, 7.0F, 38.0F, 255.0F},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        CLAY({.id = CLAY_ID("dress/stage/room"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(stage_w - 10.0F),
                                    CLAY_SIZING_FIXED(stage_h - 10.0F)}},
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                            .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                           .zIndex = 0},
              .cornerRadius = CLAY_CORNER_RADIUS(20),
              .backgroundColor = {17.0F, 9.0F, 49.0F, 255.0F},
              .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
            CLAY({.id = CLAY_ID("atelier/portal"),
                  .layout = {.sizing = {CLAY_SIZING_FIXED(stage_w * 0.52F),
                                       CLAY_SIZING_FIXED(stage_h * 0.70F)}},
                  .cornerRadius = CLAY_CORNER_RADIUS(180),
                  .backgroundColor = magical ? visual.primary
                                             : (Clay_Color){47.0F, 207.0F, 255.0F, 52.0F},
                  .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                               .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                                .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                               .zIndex = 0},
                  .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {}
            CLAY({.id = CLAY_ID("atelier/podium"),
                  .layout = {.sizing = {CLAY_SIZING_FIXED(stage_w * 0.68F),
                                       CLAY_SIZING_FIXED(26.0F)}},
                  .cornerRadius = CLAY_CORNER_RADIUS(24),
                  .backgroundColor = magical ? visual.secondary
                                             : (Clay_Color){171.0F, 83.0F, 255.0F, 210.0F},
                  .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                               .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_BOTTOM,
                                                .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM},
                               .offset = {0.0F, -16.0F},
                               .zIndex = 1},
                  .userData = NT_UI_CLAY_DATA(LAYER_IMG)}) {}
        }
        CLAY({.id = CLAY_ID("dress/stage/hero"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(char_w), CLAY_SIZING_FIXED(char_h)}},
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                            .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                           .zIndex = 4}}) {
            draw_outfit_figure(ctx, char_w, char_h);
        }
    }
}

static const char *short_slot_label(dress_slot_t slot, float width) {
    if (width >= 58.0F) {
        return dress_room_slot_label(slot);
    }
    switch (slot) {
    case DRESS_SLOT_BOTTOM:
        return "Bot";
    case DRESS_SLOT_SHOES:
        return "Shoe";
    case DRESS_SLOT_ACC:
        return "Acc";
    default:
        return dress_room_slot_label(slot);
    }
}

static void draw_categories(nt_ui_context_t *ctx, float width, float height, bool interactive) {
    float chip_w = (width - 24.0F) / 5.0F;
    if (chip_w > 104.0F) {
        chip_w = 104.0F;
    }
    CLAY({.id = CLAY_ID("dress/categories"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 6,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        for (int slot = 0; slot < DRESS_SLOT_COUNT; ++slot) {
            char id[32];
            char label[24];
            (void)snprintf(id, sizeof id, "dress/category/%d", slot);
            const bool support = slot == DRESS_SLOT_HAIR || slot == DRESS_SLOT_BOTTOM ||
                                 slot == DRESS_SLOT_SHOES;
            if (support && dress_room_support_confirmed((dress_slot_t)slot)) {
                (void)snprintf(label, sizeof label, "%s [x]",
                               short_slot_label((dress_slot_t)slot, chip_w));
            } else {
                (void)snprintf(label, sizeof label, "%s",
                               short_slot_label((dress_slot_t)slot, chip_w));
            }
            if (styled_button(ctx, id, label, chip_w, height,
                              dress_room_category() == (dress_slot_t)slot ? 2 : 0, interactive)) {
                dress_room_set_category((dress_slot_t)slot);
            }
        }
    }
}

static void draw_focus_button(nt_ui_context_t *ctx, const char *id, const char *prefix,
                              dress_essence_t essence, dress_slot_t slot, float width,
                              float height, bool interactive) {
    char line[48];
    const char *value = essence == DRESS_ESSENCE_NONE ? "CHOOSE" : dress_room_essence_label(essence);
    (void)snprintf(line, sizeof line, "%s  %s", prefix, value);
    if (styled_button(ctx, id, line, width, height,
                      dress_room_category() == slot ? 2 : 0, interactive)) {
        dress_room_set_category(slot);
    }
}

static const char *collection_unlock_name(int milestone) {
    switch (milestone) {
        case 1: return "LOOKBOOK";
        case 3: return "REMIX MARKS";
        case 6: return "ALL MAGIC MASTERY";
        default: return "";
    }
}

static void collection_status_line(char *out, size_t out_size) {
    const int milestone = dress_room_collection_milestone();
    const int next = dress_room_collection_next_target();
    if (milestone == 0) {
        (void)snprintf(out, out_size, "NEXT UNLOCK 1/6: LOOKBOOK");
    } else if (next > 0) {
        (void)snprintf(out, out_size, "UNLOCKED: %s - NEXT %d/6: %s",
                       collection_unlock_name(milestone), next,
                       collection_unlock_name(next));
    } else {
        (void)snprintf(out, out_size, "MASTERED: ALL MAGIC UNLOCKED");
    }
}

static void draw_collection_strip(nt_ui_context_t *ctx, float width, bool compact,
                                  bool interactive) {
    const int found = dress_room_discovered_count();
    const int looks = dress_room_lookbook_count();
    char progress[32];
    char next_line[64];
    if (compact && dress_room_collection_milestone() >= 6) {
        (void)snprintf(progress, sizeof progress, "LOOKBOOK MASTERED");
    } else if (compact && dress_room_collection_milestone() >= 3) {
        (void)snprintf(progress, sizeof progress, "LOOKBOOK + REMIX");
    } else if (compact && dress_room_collection_milestone() >= 1) {
        (void)snprintf(progress, sizeof progress, "LOOKBOOK UNLOCKED");
    } else {
        (void)snprintf(progress, sizeof progress, "LOOKBOOK %d/18", looks);
    }
    collection_status_line(next_line, sizeof next_line);
    nt_ui_label_style_t label = light_label(13.0F);
    const float slot_w = (width - 30.0F) / 6.0F;
    CLAY({.id = CLAY_ID("collection/strip"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0),
                                CLAY_SIZING_FIXED(compact ? 44.0F : 52.0F)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 5,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_ID("collection/header"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0),
                                    CLAY_SIZING_FIXED(compact ? 44.0F : 32.0F)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = 10,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            if (styled_button(ctx, "lookbook/open", progress, 154.0F,
                              compact ? 44.0F : 32.0F, 0, interactive)) {
                s_lookbook_open = true;
                s_lookbook_detail_recipe = -1;
                s_lookbook_detail_slot = 0;
                dress_room_lookbook_opened();
            }
            if (!compact) {
                nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), next_line, &label);
            }
        }
        if (!compact) {
            CLAY({.id = CLAY_ID("collection/slots"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(10.0F)},
                             .layoutDirection = CLAY_LEFT_TO_RIGHT,
                             .childGap = 6,
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
                for (int i = 0; i < 6; ++i) {
                    CLAY({.id = CLAY_IDI("collection/slot", (uint32_t)i),
                          .layout = {.sizing = {CLAY_SIZING_FIXED(slot_w), CLAY_SIZING_FIXED(10.0F)}},
                          .cornerRadius = CLAY_CORNER_RADIUS(8),
                          .backgroundColor = i < found ? (Clay_Color){69.0F, 231.0F, 255.0F, 255.0F}
                                                             : (Clay_Color){67.0F, 54.0F, 104.0F, 255.0F},
                          .userData = NT_UI_CLAY_DATA(LAYER_IMG)}) {}
                }
            }
        }
    }
}

static const char *catalog_label_at(int index) {
    const dress_item_t *item = dress_room_catalog_item(index);
    return item ? item->label : "?";
}

static void draw_lookbook_detail(nt_ui_context_t *ctx, float panel_w, bool compact,
                                 bool interactive) {
    const int recipe_index = s_lookbook_detail_recipe;
    const dress_awakening_recipe_t *recipe = dress_room_recipe_at(recipe_index);
    const int count = dress_room_saved_look_count_for_recipe(recipe_index);
    if (!recipe) {
        s_lookbook_detail_recipe = -1;
        return;
    }
    if (count > 0 && s_lookbook_detail_slot >= count) s_lookbook_detail_slot = count - 1;
    if (s_lookbook_detail_slot < 0) s_lookbook_detail_slot = 0;

    char progress[32];
    (void)snprintf(progress, sizeof progress, "%d / 3 SAVED LOOKS", count);
    nt_ui_label_style_t title = light_label(compact ? 22.0F : 28.0F);
    nt_ui_label_style_t copy = light_label(compact ? 11.0F : 14.0F);
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), recipe->label, &title);
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), progress, &copy);

    if (count <= 0) {
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT),
                    "No outfit saved yet. Create this magic and make it yours.", &copy);
        if (compact) {
            CLAY({.id = CLAY_ID("lookbook/detail/actions"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(44.0F)},
                             .layoutDirection = CLAY_LEFT_TO_RIGHT,
                             .childGap = 8,
                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
                if (styled_button(ctx, "lookbook/create", "CREATE MAGIC", (panel_w - 88.0F) * 0.5F,
                                  44.0F, 2, interactive)) {
                    (void)dress_room_prepare_recipe(recipe_index);
                    s_lookbook_open = false;
                    s_lookbook_detail_recipe = -1;
                }
                if (styled_button(ctx, "lookbook/back", "BACK", (panel_w - 88.0F) * 0.5F,
                                  44.0F, 1, interactive)) {
                    s_lookbook_detail_recipe = -1;
                }
            }
        } else if (styled_button(ctx, "lookbook/create", "CREATE THIS MAGIC", panel_w - 72.0F,
                                 62.0F, 2, interactive)) {
                (void)dress_room_prepare_recipe(recipe_index);
                s_lookbook_open = false;
                s_lookbook_detail_recipe = -1;
        }
    } else {
        int indices[DRESS_SLOT_COUNT];
        if (dress_room_saved_look_indices(recipe_index, s_lookbook_detail_slot, indices)) {
            char slot_line[32];
            (void)snprintf(slot_line, sizeof slot_line, "LOOK %d OF %d",
                           s_lookbook_detail_slot + 1, count);
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), slot_line, &copy);
            const char *prefix[DRESS_SLOT_COUNT] = {"HAIR", "MAIN", "BOTTOM", "SHOES", "ACCENT"};
            for (int slot = 0; slot < DRESS_SLOT_COUNT; ++slot) {
                char item_line[64];
                (void)snprintf(item_line, sizeof item_line, "%s: %s", prefix[slot],
                               catalog_label_at(indices[slot]));
                nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), item_line, &copy);
            }
            CLAY({.id = CLAY_ID("lookbook/detail/nav"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(compact ? 44.0F : 52.0F)},
                             .layoutDirection = CLAY_LEFT_TO_RIGHT,
                             .childGap = 8,
                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
                if (styled_button(ctx, "lookbook/slot/prev", "<", 52.0F, compact ? 44.0F : 48.0F, 0,
                                  interactive && count > 1)) {
                    s_lookbook_detail_slot = (s_lookbook_detail_slot + count - 1) % count;
                }
                if (styled_button(ctx, "lookbook/wear", "WEAR THIS LOOK", panel_w - 192.0F,
                                  compact ? 44.0F : 52.0F, 2, interactive)) {
                    (void)dress_room_equip_saved_look(recipe_index, s_lookbook_detail_slot);
                    s_lookbook_open = false;
                    s_lookbook_detail_recipe = -1;
                }
                if (styled_button(ctx, "lookbook/slot/next", ">", 52.0F, compact ? 44.0F : 48.0F, 0,
                                  interactive && count > 1)) {
                    s_lookbook_detail_slot = (s_lookbook_detail_slot + 1) % count;
                }
            }
            if (compact) {
                CLAY({.id = CLAY_ID("lookbook/detail/actions"),
                      .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(44.0F)},
                                 .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                 .childGap = 8,
                                 .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
                    if (count < 3 && styled_button(ctx, "lookbook/create-another", "CREATE LOOK",
                                                   (panel_w - 88.0F) * 0.5F, 44.0F, 1, interactive)) {
                        (void)dress_room_prepare_recipe(recipe_index);
                        s_lookbook_open = false;
                        s_lookbook_detail_recipe = -1;
                    }
                    if (styled_button(ctx, "lookbook/back", "BACK", (panel_w - 88.0F) * 0.5F,
                                      44.0F, 1, interactive)) {
                        s_lookbook_detail_recipe = -1;
                        s_lookbook_detail_slot = 0;
                    }
                }
            } else if (count < 3 && styled_button(ctx, "lookbook/create-another", "CREATE ANOTHER LOOK",
                                                  panel_w - 72.0F, 48.0F, 1, interactive)) {
                (void)dress_room_prepare_recipe(recipe_index);
                s_lookbook_open = false;
                s_lookbook_detail_recipe = -1;
            }
        }
    }
    if (!compact && styled_button(ctx, "lookbook/back", "BACK TO RECIPES", panel_w - 72.0F,
                                  48.0F, 1, interactive)) {
        s_lookbook_detail_recipe = -1;
        s_lookbook_detail_slot = 0;
    }
}

static void draw_lookbook(nt_ui_context_t *ctx, float width, float height, bool interactive) {
    const float panel_w = width > 640.0F ? 608.0F : width - 32.0F;
    const bool compact = height < 500.0F;
    CLAY({.id = CLAY_ID("lookbook/root"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                        .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                       .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
                       .zIndex = 100},
          .backgroundColor = {7.0F, 3.0F, 24.0F, 248.0F},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        CLAY({.id = CLAY_ID("lookbook/panel"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(panel_w),
                                    CLAY_SIZING_FIXED(height - 32.0F)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = compact ? 4 : 12,
                         .padding = CLAY_PADDING_ALL(compact ? 12 : 18),
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP}},
              .cornerRadius = CLAY_CORNER_RADIUS(24),
              .backgroundColor = {25.0F, 14.0F, 67.0F, 255.0F},
              .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
            if (s_lookbook_detail_recipe >= 0) {
                draw_lookbook_detail(ctx, panel_w, compact, interactive);
            } else {
              nt_ui_label_style_t title = light_label(width < 480.0F ? 26.0F : 32.0F);
              nt_ui_label_style_t subtitle = light_label(14.0F);
              nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "MAGIC LOOKBOOK", &title);
              nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT),
                          "Every saved card is one exact five-piece outfit.", &subtitle);
              for (int row = 0; row < 3; ++row) {
                CLAY({.id = CLAY_IDI("lookbook/row", (uint32_t)row),
                      .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(102.0F)},
                                 .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                 .childGap = 10,
                                 .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
                    for (int col = 0; col < 2; ++col) {
                        const int recipe_index = row * 2 + col;
                        const dress_awakening_recipe_t *recipe = dress_room_recipe_at(recipe_index);
                        if (!recipe) {
                            continue;
                        }
                        char line[80];
                        char id[32];
                        const int saved_count = dress_room_saved_look_count_for_recipe(recipe_index);
                        if (dress_room_recipe_is_discovered(recipe_index)) {
                            (void)snprintf(line, sizeof line, "%s  %d/3", recipe->label, saved_count);
                        } else {
                            (void)snprintf(line, sizeof line, "%s + %s  0/3",
                                           dress_room_essence_label(recipe->first),
                                           dress_room_essence_label(recipe->second));
                        }
                        (void)snprintf(id, sizeof id, "lookbook/recipe/%d", recipe_index);
                        if (styled_button(ctx, id, line, (panel_w - 48.0F) * 0.5F, 92.0F,
                                          dress_room_recipe_is_discovered(recipe_index) ? 2 : 0,
                                          interactive)) {
                            s_lookbook_detail_recipe = recipe_index;
                            s_lookbook_detail_slot = 0;
                        }
                    }
                }
              }
            if (styled_button(ctx, "lookbook/close", "BACK TO CLOSET", panel_w - 72.0F,
                              52.0F, 1, interactive)) {
                s_lookbook_open = false;
            }
            }
        }
    }
}

static void draw_essence_pair(nt_ui_context_t *ctx, float width, float height,
                              bool interactive) {
    const float gap = 8.0F;
    const float pill_w = (width - gap) * 0.5F;
    CLAY({.id = CLAY_ID("essence/pair"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 8,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        draw_focus_button(ctx, "focus/main", "1. MAIN", dress_room_primary_essence(),
                          DRESS_SLOT_TOP, pill_w, height, interactive);
        draw_focus_button(ctx, "focus/accent", "2. ACCENT", dress_room_secondary_essence(),
                          DRESS_SLOT_ACC, pill_w, height, interactive);
    }
}

static void draw_catalog(nt_ui_context_t *ctx, float cell, float catalog_h,
                         bool interactive) {
    const dress_slot_t category = dress_room_category();
    int indices[16];
    int count = 0;
    for (int i = 0; i < dress_room_catalog_count() && count < 16; ++i) {
        const dress_item_t *item = dress_room_catalog_item(i);
        if (item && item->slot == category) {
            indices[count++] = i;
        }
    }
    CLAY({.id = CLAY_ID("dress/catalog"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(catalog_h)},
                     .padding = {.left = 8, .right = 8, .top = 8, .bottom = 8}},
          .cornerRadius = CLAY_CORNER_RADIUS(18),
          .backgroundColor = {20.0F, 12.0F, 55.0F, 248.0F},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        nt_ui_scroll_style_t scroll_style = nt_ui_scroll_style_defaults();
        scroll_style.scroll_x = true;
        scroll_style.scroll_y = false;
        scroll_style.bar_visibility = NT_UI_SCROLLBAR_AUTO_HIDE;
        scroll_style.bar_thickness = 4.0F;
        scroll_style.bar_thumb_min_px = 44.0F;
        nt_ui_scroll_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG),
                           semantic_ui_id("dress/catalog/scroll"), &scroll_style,
                           &(Clay_ElementDeclaration){
                               .layout = {
                                   .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                               },
                           });
        CLAY({.id = CLAY_ID("dress/catalog/row"),
              .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = 8,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
                for (int column = 0; column < count; ++column) {
                    const int item_index = indices[column];
                    const dress_item_t *item = dress_room_catalog_item(item_index);
                    if (!item) {
                        continue;
                    }
                    const bool selected = dress_room_equipped(category) == item_index;
                    nt_ui_button_style_t style = selected ? g_theme.button_selected : g_theme.button;
                    style.idle.bg = (nt_atlas_region_ref_t){0};
                    style.hover.bg = (nt_atlas_region_ref_t){0};
                    style.pressed.bg = (nt_atlas_region_ref_t){0};
                    style.disabled.bg = (nt_atlas_region_ref_t){0};
                    style.idle.bg_tint = 0xFF361117U;
                    style.hover.bg_tint = 0xFF521A24U;
                    style.pressed.bg_tint = 0xFF6E2030U;
                    char id[48];
                    (void)snprintf(id, sizeof id, "dress/item/%s", item->id);
                    char control_id[DRESS_UI_ID_LEN];
                    (void)snprintf(control_id, sizeof control_id, "%s/control", id);
                    float card_h = cell * 1.34F;
                    if (card_h > catalog_h - 16.0F) {
                        card_h = catalog_h - 16.0F;
                    }
                    bool clicked = false;
                    CLAY({.id = semantic_clay_id(id),
                          .layout = {.sizing = {CLAY_SIZING_FIXED(cell), CLAY_SIZING_FIXED(card_h)},
                                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                                     .padding = CLAY_PADDING_ALL(selected ? 3 : 2)},
                          .cornerRadius = CLAY_CORNER_RADIUS(16),
                          .backgroundColor = selected ? (Clay_Color){69.0F, 231.0F, 255.0F, 255.0F}
                                                      : (Clay_Color){72.0F, 58.0F, 121.0F, 255.0F},
                          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                        nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), semantic_ui_id(control_id), &style,
                                           &(Clay_ElementDeclaration){
                                               .layout = {
                                                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                                   .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                                   .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                                                   .childGap = 2,
                                                   .padding = CLAY_PADDING_ALL(5),
                                               },
                                           },
                                           interactive, NULL);
                        if (item->atlas_thumb && item_index < 32) {
                            s_thumb_reg[item_index] = region_named(item->atlas_thumb);
                            nt_ui_image_style_t image_style = nt_ui_image_style_defaults();
                            nt_ui_image(ctx, NT_UI_DATA_LAYER(LAYER_IMG), &s_thumb_reg[item_index], &image_style,
                                        &(Clay_ElementDeclaration){
                                            .layout = {.sizing = {CLAY_SIZING_FIXED(cell - 14.0F),
                                                                  CLAY_SIZING_FIXED(card_h - 49.0F)}},
                                        });
                        }
                        nt_ui_label_style_t item_label = ink_label(cell < 96.0F ? 13.0F : 15.0F);
                        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), item->label, &item_label);
                        if (item->essence != DRESS_ESSENCE_NONE && card_h >= 80.0F) {
                            nt_ui_label_style_t essence_label = ink_label(11.0F);
                            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT),
                                        dress_room_essence_label(item->essence), &essence_label);
                        }
                        clicked = nt_ui_button_end(ctx);
                    }
                    if (interactive && clicked) {
                        (void)dress_room_equip(item_index);
                    }
            }
        }
        nt_ui_scroll_end(ctx);
    }
}

static void draw_dress_actions(nt_ui_context_t *ctx, float width, bool compact,
                               bool interactive) {
    const bool focus_complete = dress_room_focus_complete();
    const bool complete = focus_complete && dress_room_support_complete();
    const bool recipe_known = focus_complete && dress_room_current_recipe_is_discovered();
    const bool look_known = complete && dress_room_current_look_is_recorded();
    const bool has_unknown = dress_room_next_undiscovered_recipe() != NULL;
    const dress_essence_t main = dress_room_primary_essence();
    const dress_essence_t accent = dress_room_secondary_essence();
    const int support_count = dress_room_support_confirmed_count();
    const char *hint = focus_complete && !dress_room_support_complete()
                               ? (support_count == 0 ? "STYLE 0/3 - confirm HAIR, then BOTTOM, then SHOES"
                                  : support_count == 1 ? "STYLE 1/3 - confirm BOTTOM, then SHOES"
                                                       : "STYLE 2/3 - confirm SHOES")
                           : look_known ? (has_unknown ? "KNOWN LOOK - replay or try a new pair"
                                                 : "KNOWN LOOK - change support pieces to remix")
                               : recipe_known ? "NEW REMIX - support pieces changed the signature"
                               : focus_complete ? "Your exact five-piece look is ready"
                               : (main == DRESS_ESSENCE_NONE ? "Choose a Top with a Main Essence"
                                                            : "Choose an Acc with an Accent Essence");
    nt_ui_label_style_t hint_style = light_label(complete ? 14.0F : 15.0F);
    CLAY({.id = CLAY_ID("dress/action-zone"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 5,
                     .padding = {.bottom = compact ? 0 : 18},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        if (!compact) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), hint, &hint_style);
        }
        CLAY({.id = CLAY_ID("dress/action-row"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = 8,
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
            if (look_known && has_unknown) {
                if (styled_button(ctx, "awakening/cta", "REPLAY", width * 0.24F, 48.0F, 0,
                                  interactive)) {
                    (void)dress_room_begin_awakening();
                }
                if (styled_button(ctx, "dress/try-next", "TRY NEW MAGIC", width * 0.68F,
                                  compact ? 52.0F : 58.0F,
                                  1, interactive)) {
                    (void)dress_room_prepare_next_undiscovered();
                }
            } else {
                if (styled_button(ctx, "dress/reset", "Reset", width * 0.24F, 48.0F, 0,
                                  interactive)) {
                    dress_room_reset_outfit();
                }
                const char *cta = complete ? (look_known ? "REPLAY" : recipe_known ? "AWAKEN NEW LOOK"
                                                                                : "STEP ON RUNWAY")
                                           : !focus_complete
                                                 ? (main == DRESS_ESSENCE_NONE ? "PICK MAIN" : "PICK ACCENT")
                                                 : support_count == 0 ? "CHOOSE HAIR"
                                                 : support_count == 1 ? "CHOOSE BOTTOM"
                                                                      : "CHOOSE SHOES";
                if (styled_button(ctx, "awakening/cta", cta,
                                  width * 0.68F, compact ? 52.0F : 58.0F, 1, interactive)) {
                    if (complete) {
                        (void)dress_room_begin_awakening();
                    } else if (!focus_complete) {
                        dress_room_set_category(main == DRESS_ESSENCE_NONE ? DRESS_SLOT_TOP
                                                                           : DRESS_SLOT_ACC);
                    } else {
                        dress_room_set_category(support_count == 0 ? DRESS_SLOT_HAIR
                                               : support_count == 1 ? DRESS_SLOT_BOTTOM
                                                                    : DRESS_SLOT_SHOES);
                    }
                }
            }
        }
        (void)accent;
    }
}

static const char *phase_title(dress_awakening_phase_t phase) {
    switch (phase) {
    case DRESS_AWAKENING_INTRO:
        return "THE RUNWAY LISTENS";
    case DRESS_AWAKENING_CHARGE:
        return "ESSENCES RESONATE";
    case DRESS_AWAKENING_FLASH:
        return "MAGIC IGNITES";
    case DRESS_AWAKENING_REVEAL:
        return "YOUR POWER AWAKENS";
    case DRESS_AWAKENING_VICTORY:
        return "VICTORY IS YOURS";
    case DRESS_AWAKENING_RECIPE_CARD:
        return dress_room_awakening_is_new() ? "AWAKENING DISCOVERED" : "KNOWN MAGIC REPLAYED";
    case DRESS_AWAKENING_IDLE:
    default:
        return "RUNWAY AWAKENING";
    }
}

static void draw_power_marks(nt_ui_context_t *ctx, awakening_visual_t visual, float width) {
    const int count = 3 + (visual.motif % 3);
    const float mark_w = width > 420.0F ? 64.0F : 42.0F;
    CLAY({.id = CLAY_ID("awakening/marks"),
          .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 8,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        for (int i = 0; i < count; ++i) {
            CLAY({.id = CLAY_IDI("awakening/mark", (uint32_t)i),
                  .layout = {.sizing = {CLAY_SIZING_FIXED(mark_w),
                                       CLAY_SIZING_FIXED((i + visual.motif) % 2 == 0 ? 8.0F : 16.0F)}},
                  .cornerRadius = CLAY_CORNER_RADIUS(12),
                  .backgroundColor = (i & 1) != 0 ? visual.secondary : visual.primary,
                  .userData = NT_UI_CLAY_DATA(LAYER_IMG)}) {}
        }
    }
}

static void draw_recipe_item(nt_ui_context_t *ctx, int item_index, uint32_t formula_index,
                             dress_essence_t essence) {
    const dress_item_t *item = dress_room_catalog_item(item_index);
    if (!item) {
        return;
    }
    CLAY({.id = CLAY_IDI("awakening/formula/item", formula_index),
          .layout = {.sizing = {CLAY_SIZING_FIXED(112.0F), CLAY_SIZING_FIXED(112.0F)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 2,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                     .padding = CLAY_PADDING_ALL(5)},
          .cornerRadius = CLAY_CORNER_RADIUS(16),
          .backgroundColor = {20.0F, 13.0F, 58.0F, 235.0F},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        if (item->atlas_thumb && item_index >= 0 && item_index < 32) {
            s_thumb_reg[item_index] = region_named(item->atlas_thumb);
            nt_ui_image_style_t image_style = nt_ui_image_style_defaults();
            nt_ui_image(ctx, NT_UI_DATA_LAYER(LAYER_IMG), &s_thumb_reg[item_index], &image_style,
                        &(Clay_ElementDeclaration){
                            .layout = {.sizing = {CLAY_SIZING_FIXED(54.0F),
                                                  CLAY_SIZING_FIXED(72.0F)}},
                        });
        }
        nt_ui_label_style_t name = light_label(13.0F);
        nt_ui_label_style_t chip = light_label(11.0F);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), item->label, &name);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), dress_room_essence_label(essence), &chip);
    }
}

static void draw_recipe_formula(nt_ui_context_t *ctx, dress_essence_t main,
                                dress_essence_t accent) {
    nt_ui_label_style_t symbol = light_label(22.0F);
    CLAY({.id = CLAY_ID("awakening/formula"),
          .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 9,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        draw_recipe_item(ctx, dress_room_equipped(DRESS_SLOT_TOP), 0u, main);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "+", &symbol);
        draw_recipe_item(ctx, dress_room_equipped(DRESS_SLOT_ACC), 1u, accent);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "=> MAGIC", &symbol);
    }
}

static void draw_recipe_card(nt_ui_context_t *ctx, const dress_awakening_recipe_t *recipe,
                             awakening_visual_t visual, float width, bool interactive) {
    char progress_line[48];
    char signature_line[64];
    char unlock_line[48];
    const int milestone = dress_room_collection_milestone();
    const bool unlocked_now = dress_room_awakening_is_new() &&
                              (milestone == 1 || milestone == 3 || milestone == 6);
    (void)snprintf(progress_line, sizeof progress_line, "%d / 6 AWAKENINGS",
                   dress_room_discovered_count());
    (void)snprintf(signature_line, sizeof signature_line, "%s SIGNATURE",
                   dress_room_style_signature_label());
    if (unlocked_now) {
        (void)snprintf(unlock_line, sizeof unlock_line, "UNLOCK: %s",
                       collection_unlock_name(milestone));
    } else if (dress_room_collection_next_target() > 0) {
        const int next = dress_room_collection_next_target();
        (void)snprintf(unlock_line, sizeof unlock_line, "NEXT UNLOCK %d/6: %s", next,
                       collection_unlock_name(next));
    } else {
        (void)snprintf(unlock_line, sizeof unlock_line, "ALL MAGIC MASTERED");
    }
    const float card_w = width > 640.0F ? 608.0F : width - 32.0F;
    CLAY({.id = CLAY_ID("awakening/card"),
          .layout = {.sizing = {CLAY_SIZING_FIXED(card_w), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 9,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                     .padding = {.left = 20, .right = 20, .top = 18, .bottom = 18}},
          .cornerRadius = CLAY_CORNER_RADIUS(24),
          .backgroundColor = visual.panel,
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        nt_ui_label_style_t kicker = light_label(14.0F);
        nt_ui_label_style_t title = light_label(card_w < 430.0F ? 25.0F : 32.0F);
        nt_ui_label_style_t body = light_label(17.0F);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT),
                dress_room_awakening_lookbook_full() ? "LOOKBOOK FULL - VICTORY UNSAVED"
                : dress_room_awakening_is_new() ? "NEW MAGIC"
                    : dress_room_awakening_is_new_remix() ? "NEW REMIX" : "KNOWN LOOK", &kicker);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), recipe->label, &title);
        draw_recipe_formula(ctx, recipe->first, recipe->second);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), progress_line, &body);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), signature_line, &body);
        nt_ui_label_style_t unlock = light_label(14.0F);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), unlock_line, &unlock);
        if (styled_button(ctx, "awakening/restyle", "CREATE NEXT LOOK", card_w - 56.0F, 58.0F, 1, interactive)) {
            dress_room_restyle();
        }
    }
}

static void draw_awakening(nt_ui_context_t *ctx, float width, float height, bool interactive) {
    const dress_awakening_phase_t phase = dress_room_awakening_phase();
    const dress_awakening_recipe_t *recipe = dress_room_awakening_recipe();
    awakening_visual_t visual = awakening_visual(recipe);
    visual.motif = (visual.motif + dress_room_style_signature()) % 6;
    const bool portrait = height > width;
    nt_ui_label_style_t phase_style = light_label(width < 480.0F ? 19.0F : 25.0F);
    nt_ui_label_style_t hero_style = light_label(width < 480.0F ? 28.0F : 38.0F);
    nt_ui_label_style_t power_style = light_label(width < 480.0F ? 14.0F : 18.0F);

    CLAY({.id = CLAY_ID("awakening/root"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 7,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                     .padding = CLAY_PADDING_ALL(10)},
          .backgroundColor = visual.backdrop,
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        if (phase != DRESS_AWAKENING_RECIPE_CARD) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), phase_title(phase), &phase_style);
        }
        if (phase != DRESS_AWAKENING_RECIPE_CARD) {
            draw_power_marks(ctx, visual, width * 0.7F);
        }
        if (phase == DRESS_AWAKENING_RECIPE_CARD && recipe) {
            const float stage_h = portrait ? height * 0.50F : height * 0.38F;
            const float stage_w = portrait ? width - 32.0F : width * 0.40F;
            draw_stage(ctx, stage_w, stage_h, true, visual);
            draw_recipe_card(ctx, recipe, visual, width, interactive);
        } else {
            const float stage_h = portrait ? height * 0.67F : height * 0.72F;
            const float stage_w = portrait ? width * 0.92F : width * 0.56F;
            draw_stage(ctx, stage_w, stage_h, true, visual);
            if (recipe) {
                if (phase == DRESS_AWAKENING_REVEAL || phase == DRESS_AWAKENING_VICTORY) {
                    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), recipe->label, &hero_style);
                } else {
                    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), visual.hero_word, &hero_style);
                }
                nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), visual.power_line, &power_style);
            }
            if (!dress_room_awakening_is_new() && !dress_room_awakening_is_new_remix()) {
                if (styled_button(ctx, "awakening/skip", "SKIP REPLAY", 220.0F, 48.0F, 0,
                                  interactive)) {
                    (void)dress_room_skip_replay();
                }
            }
            const float progress = dress_room_awakening_phase_t();
            const float track_w = width > 520.0F ? 360.0F : width * 0.7F;
            CLAY({.id = CLAY_ID("awakening/progress"),
                  .layout = {.sizing = {CLAY_SIZING_FIXED(track_w), CLAY_SIZING_FIXED(7.0F)}},
                  .cornerRadius = CLAY_CORNER_RADIUS(8),
                  .backgroundColor = {255.0F, 255.0F, 255.0F, 70.0F},
                  .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                CLAY({.id = CLAY_ID("awakening/progress/fill"),
                      .layout = {.sizing = {CLAY_SIZING_FIXED(track_w * progress), CLAY_SIZING_FIXED(7.0F)}},
                      .cornerRadius = CLAY_CORNER_RADIUS(8),
                      .backgroundColor = visual.secondary,
                      .userData = NT_UI_CLAY_DATA(LAYER_IMG)}) {}
            }
        }
    }
}

static void draw_dress_room(nt_ui_context_t *ctx, float width, float height, bool interactive) {
    const bool portrait = height > width;
    CLAY({.id = CLAY_ID("dress/root"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .layoutDirection = portrait ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                     .childGap = 8,
                     .padding = CLAY_PADDING_ALL(16)},
          .backgroundColor = {10.0F, 5.0F, 31.0F, 255.0F},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        if (portrait) {
            const float inner = width - 32.0F;
            const bool compact = height < 720.0F;
            float cell = (inner - 40.0F) * 0.25F;
            if (cell > 112.0F) {
                cell = 112.0F;
            }
            draw_stage(ctx, inner, compact ? 155.0F : height * 0.36F, false,
                       awakening_visual(NULL));
            draw_essence_pair(ctx, inner, 52.0F, interactive);
            draw_categories(ctx, inner, compact ? 44.0F : 48.0F, interactive);
            draw_catalog(ctx, cell, compact ? 122.0F : 184.0F, interactive);
            draw_collection_strip(ctx, inner, false, interactive);
            draw_dress_actions(ctx, inner, compact, interactive);
        } else {
            const bool compact = height < 500.0F;
            const float available = width - 32.0F;
            const float stage_w = available * 0.58F;
            const float side_w = available - stage_w - 8.0F;
            draw_stage(ctx, stage_w, compact ? height - 32.0F : height - 12.0F, false,
                       awakening_visual(NULL));
            CLAY({.id = CLAY_ID("dress/closet"),
                  .layout = {.sizing = {CLAY_SIZING_FIXED(side_w), CLAY_SIZING_GROW(0)},
                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                             .childGap = 8,
                             .padding = {.left = 8, .right = 8, .top = 8, .bottom = 8}},
                  .cornerRadius = CLAY_CORNER_RADIUS(22),
                  .backgroundColor = {23.0F, 12.0F, 61.0F, 248.0F},
                  .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                const float inner = side_w - 16.0F;
                if (!compact) {
                    nt_ui_label_style_t closet_title = light_label(20.0F);
                    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "MAGIC CLOSET", &closet_title);
                }
                draw_essence_pair(ctx, inner, compact ? 48.0F : 52.0F, interactive);
                draw_categories(ctx, inner, compact ? 44.0F : 48.0F, interactive);
                float catalog_cell = (inner - 40.0F) * 0.25F;
                if (catalog_cell > 90.0F) {
                    catalog_cell = 90.0F;
                }
                draw_catalog(ctx, catalog_cell, compact ? 92.0F : 184.0F, interactive);
                draw_collection_strip(ctx, inner, compact, interactive);
                draw_dress_actions(ctx, inner, compact, interactive);
            }
        }
    }
}

void dress_room_draw_ui(nt_ui_context_t *ctx, bool interactive) {
    if (!ctx) {
        return;
    }
    s_ui_id_cursor = 0;
    ensure_atlas();
    float width = 0.0F;
    float height = 0.0F;
    nt_ui_context_layout_size(ctx, &width, &height);
    if (width < 1.0F || height < 1.0F) {
        width = 960.0F;
        height = 540.0F;
    }
    if (dress_room_awakening_phase() != DRESS_AWAKENING_IDLE) {
        draw_awakening(ctx, width, height, interactive);
    } else {
        draw_dress_room(ctx, width, height, interactive && !s_lookbook_open);
        if (s_lookbook_open) {
            draw_lookbook(ctx, width, height, interactive);
        }
    }
}
