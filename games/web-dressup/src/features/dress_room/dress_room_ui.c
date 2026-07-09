#include "features/dress_room/dress_room.h"

#include "atlas/nt_atlas.h"
#include "clay.h"
#include "generated/game_assets.h"
#include "hash/nt_hash.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"
#include "ui/nt_ui.h"
#include "ui/nt_ui_button.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_panel.h"
#include "ui/theme.h"

#include <stdio.h>
#include <string.h>

#define LAYER_BG 0
#define LAYER_IMG 1
#define LAYER_TEXT 2
#define LAYER_STAGE 3

static nt_resource_t s_dress_atlas;
static bool s_atlas_req;
static nt_atlas_region_ref_t s_body_reg;
static nt_atlas_region_ref_t s_bg_reg;
static nt_atlas_region_ref_t s_layer_reg[DRESS_SLOT_COUNT];
static nt_atlas_region_ref_t s_thumb_reg[32];
static bool s_static_regs;

static nt_ui_label_style_t label_light(float size) {
    return (nt_ui_label_style_t){.font_id = 0, .font_size = size, .color = {255.0F, 245.0F, 250.0F, 255.0F}};
}
static void ensure_atlas(void) {
    if (!s_atlas_req) {
        s_dress_atlas = nt_resource_request(ASSET_ATLAS_DRESS, NT_ASSET_ATLAS);
        s_atlas_req = true;
    }
    if (!s_static_regs) {
        s_body_reg = nt_atlas_ref(s_dress_atlas, ASSET_ATLAS_REGION_DRESS_BODY_BASE.value);
        s_bg_reg = nt_atlas_ref(s_dress_atlas, ASSET_ATLAS_REGION_DRESS_STAGE_BG.value);
        s_static_regs = true;
    }
}

static nt_atlas_region_ref_t region_named(const char *sprite) {
    ensure_atlas();
    /* Use pack-generated region hashes only (string hash may differ from builder). */
    struct {
        const char *name;
        uint64_t hash;
    } const table[] = {
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
        {"hair_bob_full", ASSET_ATLAS_REGION_DRESS_HAIR_BOB_FULL.value},
        {"hair_long_full", ASSET_ATLAS_REGION_DRESS_HAIR_LONG_FULL.value},
        {"hair_pink_full", ASSET_ATLAS_REGION_DRESS_HAIR_PINK_FULL.value},
        {"hair_gold_full", ASSET_ATLAS_REGION_DRESS_HAIR_GOLD_FULL.value},
        {"top_tee_full", ASSET_ATLAS_REGION_DRESS_TOP_TEE_FULL.value},
        {"top_hoodie_full", ASSET_ATLAS_REGION_DRESS_TOP_HOODIE_FULL.value},
        {"top_blazer_full", ASSET_ATLAS_REGION_DRESS_TOP_BLAZER_FULL.value},
        {"top_crop_full", ASSET_ATLAS_REGION_DRESS_TOP_CROP_FULL.value},
        {"bot_jeans_full", ASSET_ATLAS_REGION_DRESS_BOT_JEANS_FULL.value},
        {"bot_skirt_full", ASSET_ATLAS_REGION_DRESS_BOT_SKIRT_FULL.value},
        {"bot_shorts_full", ASSET_ATLAS_REGION_DRESS_BOT_SHORTS_FULL.value},
        {"bot_cargo_full", ASSET_ATLAS_REGION_DRESS_BOT_CARGO_FULL.value},
        {"shoe_sneak_full", ASSET_ATLAS_REGION_DRESS_SHOE_SNEAK_FULL.value},
        {"shoe_boot_full", ASSET_ATLAS_REGION_DRESS_SHOE_BOOT_FULL.value},
        {"shoe_heel_full", ASSET_ATLAS_REGION_DRESS_SHOE_HEEL_FULL.value},
        {"shoe_sandal_full", ASSET_ATLAS_REGION_DRESS_SHOE_SANDAL_FULL.value},
        {"acc_glasses_full", ASSET_ATLAS_REGION_DRESS_ACC_GLASSES_FULL.value},
        {"acc_hat_full", ASSET_ATLAS_REGION_DRESS_ACC_HAT_FULL.value},
        {"acc_bag_full", ASSET_ATLAS_REGION_DRESS_ACC_BAG_FULL.value},
        {"acc_scarf_full", ASSET_ATLAS_REGION_DRESS_ACC_SCARF_FULL.value},
    };
    for (int i = 0; i < (int)(sizeof table / sizeof table[0]); ++i) {
        if (strcmp(sprite, table[i].name) == 0) {
            return nt_atlas_ref(s_dress_atlas, table[i].hash);
        }
    }
    return nt_atlas_ref(s_dress_atlas, ASSET_ATLAS_REGION_DRESS_BODY_BASE.value);
}

/* kind: 0 cream, 1 rose CTA, 2 wine danger, 3 selected gold */
static bool styled_button(nt_ui_context_t *ctx, const char *id, const char *text, float w, float h, int kind,
                          bool interactive) {
    nt_ui_button_style_t style = g_theme.button;
    nt_ui_label_style_t text_style = g_theme.button_label;
    if (kind == 1) {
        style = g_theme.button_success;
        text_style = g_theme.button_label_light;
    } else if (kind == 2) {
        style = g_theme.button_danger;
        text_style = g_theme.button_label;
    } else if (kind == 3) {
        style = g_theme.button_selected;
        text_style = g_theme.button_label;
    }
    const uint32_t bid = nt_ui_id(id);
    nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), bid, &style,
                       &(Clay_ElementDeclaration){
                           .layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)},
                                      .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                       interactive, NULL);
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), text, &text_style);
    /* Always end — never short-circuit (nested-button assert if settings open). */
    const bool clicked = nt_ui_button_end(ctx);
    return interactive && clicked;
}

static bool chip_button(nt_ui_context_t *ctx, const char *id, const char *text, bool selected, float w, float h,
                        bool interactive) {
    return styled_button(ctx, id, text, w, h, selected ? 3 : 0, interactive);
}

static bool action_button(nt_ui_context_t *ctx, const char *id, const char *text, float w, float h, bool danger,
                          bool interactive) {
    return styled_button(ctx, id, text, w, h, danger ? 2 : 0, interactive);
}

static bool success_button(nt_ui_context_t *ctx, const char *id, const char *text, float w, float h, bool interactive) {
    return styled_button(ctx, id, text, w, h, 1, interactive);
}

static void draw_sprite(nt_ui_context_t *ctx, nt_atlas_region_ref_t *region, float w, float h, int z) {
    nt_ui_image_style_t st = nt_ui_image_style_defaults();
    (void)z;
    nt_ui_image(ctx, NT_UI_DATA_LAYER(LAYER_STAGE), region, &st,
                &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)}}});
}

/* equipped_fn: NULL uses player outfit; otherwise (slot)->catalog index. */
typedef int (*dress_equip_fn)(dress_slot_t slot, void *user);

static int player_equip_fn(dress_slot_t slot, void *user) {
    (void)user;
    return dress_room_equipped(slot);
}

typedef struct rival_equip_ctx {
    int rival_index;
} rival_equip_ctx_t;

static int rival_equip_fn(dress_slot_t slot, void *user) {
    const rival_equip_ctx_t *rc = (const rival_equip_ctx_t *)user;
    return dress_room_rival_equipped(rc->rival_index, slot);
}

static void draw_outfit_figure(nt_ui_context_t *ctx, uint32_t id_base, float char_w, float char_h,
                               dress_equip_fn equip_fn, void *user) {
    ensure_atlas();
    if (!equip_fn) {
        equip_fn = player_equip_fn;
    }
    CLAY({.id = CLAY_IDI("dress/fig", id_base),
          .layout = {.sizing = {CLAY_SIZING_FIXED(char_w), CLAY_SIZING_FIXED(char_h)}}}) {
        CLAY({.id = CLAY_IDI("dress/fig/bg", id_base),
              .layout = {.sizing = {CLAY_SIZING_FIXED(char_w), CLAY_SIZING_FIXED(char_h)}},
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                            .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                           .zIndex = 0}}) {
            draw_sprite(ctx, &s_bg_reg, char_w, char_h, 0);
        }
        CLAY({.id = CLAY_IDI("dress/fig/body", id_base),
              .layout = {.sizing = {CLAY_SIZING_FIXED(char_w), CLAY_SIZING_FIXED(char_h)}},
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                            .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                           .zIndex = 1}}) {
            draw_sprite(ctx, &s_body_reg, char_w, char_h, 1);
        }
        static const dress_slot_t order[DRESS_SLOT_COUNT] = {
            DRESS_SLOT_BOTTOM, DRESS_SLOT_TOP, DRESS_SLOT_SHOES, DRESS_SLOT_HAIR, DRESS_SLOT_ACC};
        for (int i = 0; i < DRESS_SLOT_COUNT; ++i) {
            const dress_slot_t slot = order[i];
            const int idx = equip_fn(slot, user);
            const dress_item_t *it = dress_room_catalog_item(idx);
            if (!it || !it->atlas_layer) {
                continue;
            }
            /* Local static regs only for player main stage; rivals re-resolve each draw. */
            nt_atlas_region_ref_t reg = region_named(it->atlas_layer);
            if (id_base == 0u) {
                s_layer_reg[slot] = reg;
            }
            CLAY({.id = CLAY_IDI("dress/fig/lyr", id_base * 16u + (uint32_t)slot + 1u),
                  .layout = {.sizing = {CLAY_SIZING_FIXED(char_w), CLAY_SIZING_FIXED(char_h)}},
                  .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                               .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                                .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                               .zIndex = (int16_t)(2 + i)}}) {
                if (id_base == 0u) {
                    draw_sprite(ctx, &s_layer_reg[slot], char_w, char_h, 2 + i);
                } else {
                    /* Stack-local region: keep until frame ends (Clay holds pointer). */
                    static nt_atlas_region_ref_t s_rival_layer_reg[3][DRESS_SLOT_COUNT];
                    const int r = (int)((id_base - 1u) % 3u);
                    s_rival_layer_reg[r][slot] = reg;
                    draw_sprite(ctx, &s_rival_layer_reg[r][slot], char_w, char_h, 2 + i);
                }
            }
        }
    }
}

static void draw_stage(nt_ui_context_t *ctx, float stage_w, float stage_h) {
    ensure_atlas();
    /* Room fills the whole stage (no purple letterbox); doll centered on top. */
    float char_h = stage_h * 0.99F;
    float char_w = char_h * (512.0F / 896.0F);
    if (char_w > stage_w * 0.98F) {
        char_w = stage_w * 0.98F;
        char_h = char_w * (896.0F / 512.0F);
    }
    CLAY({.id = CLAY_ID("dress/stage"),
          .layout = {.sizing = {CLAY_SIZING_FIXED(stage_w), CLAY_SIZING_FIXED(stage_h)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                     .padding = CLAY_PADDING_ALL(0)},
          .cornerRadius = CLAY_CORNER_RADIUS(14),
          .backgroundColor = {48.0F, 24.0F, 40.0F, 255.0F},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        /* Full-bleed boutique room behind the doll. */
        CLAY({.id = CLAY_ID("dress/stage/room"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(stage_w), CLAY_SIZING_FIXED(stage_h)}},
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                            .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                           .zIndex = 0}}) {
            draw_sprite(ctx, &s_bg_reg, stage_w, stage_h, 0);
        }
        draw_outfit_figure(ctx, 0u, char_w, char_h, player_equip_fn, NULL);
    }
}

static void draw_rival_card(nt_ui_context_t *ctx, int r, float card_w, float card_h) {
    ensure_atlas();
    nt_ui_label_style_t cap = label_light(13.0F);
    rival_equip_ctx_t rc = {.rival_index = r};
    char stars_line[32];
    /* ASCII only — Lilita font has no ★ glyph (was tofu on podium). */
    (void)snprintf(stars_line, sizeof stars_line, "R%d  %d stars", r + 1, dress_room_rival_stars(r));
    float char_h = card_h * 0.78F;
    if (char_h < 70.0F) {
        char_h = 70.0F;
    }
    float char_w = char_h * (512.0F / 896.0F);
    if (char_w > card_w - 10.0F) {
        char_w = card_w - 10.0F;
        char_h = char_w * (896.0F / 512.0F);
    }
    CLAY({.id = CLAY_IDI("dress/rival", (uint32_t)r),
          .layout = {.sizing = {CLAY_SIZING_FIXED(card_w), CLAY_SIZING_FIXED(card_h)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                     .childGap = 4,
                     .padding = CLAY_PADDING_ALL(4)},
          .cornerRadius = CLAY_CORNER_RADIUS(10),
          .backgroundColor = {60.0F, 30.0F, 45.0F, 240.0F},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        draw_outfit_figure(ctx, (uint32_t)(r + 1), char_w, char_h, rival_equip_fn, &rc);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), stars_line, &cap);
    }
}

static void draw_rival_lineup(nt_ui_context_t *ctx, float layout_w, float row_h) {
    const float card_w = layout_w > 420.0F ? (layout_w - 48.0F) / 3.0F : 110.0F;
    CLAY({.id = CLAY_ID("dress/rivals"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 10,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        for (int r = 0; r < 3; ++r) {
            draw_rival_card(ctx, r, card_w, row_h);
        }
    }
}

static const char *slot_chip_label(dress_slot_t slot, float chip_w) {
    /* Narrow rail: short labels so chips never clip (portal density). */
    if (chip_w < 58.0F) {
        switch (slot) {
        case DRESS_SLOT_HAIR:
            return "Hair";
        case DRESS_SLOT_TOP:
            return "Top";
        case DRESS_SLOT_BOTTOM:
            return "Bot";
        case DRESS_SLOT_SHOES:
            return "Shoe";
        case DRESS_SLOT_ACC:
            return "Acc";
        default:
            return "?";
        }
    }
    return dress_room_slot_label(slot);
}

static void draw_categories(nt_ui_context_t *ctx, float available_w, float chip_h, bool interactive) {
    const float gaps = 6.0F * 4.0F;
    float chip_w = (available_w - gaps) / 5.0F;
    if (chip_w > 110.0F) {
        chip_w = 110.0F;
    }
    /* Never force min width past available — overflow was P0 on landscape rail. */
    if (chip_w < 40.0F) {
        chip_w = 40.0F;
    }
    CLAY({.id = CLAY_ID("dress/cats"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 6,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        for (int s = 0; s < DRESS_SLOT_COUNT; ++s) {
            char id[32];
            (void)snprintf(id, sizeof id, "dress/cat/%d", s);
            if (chip_button(ctx, id, slot_chip_label((dress_slot_t)s, chip_w),
                            dress_room_category() == (dress_slot_t)s, chip_w, chip_h, interactive)) {
                dress_room_set_category((dress_slot_t)s);
            }
        }
    }
}

static void draw_catalog(nt_ui_context_t *ctx, float cell, int columns, bool interactive) {
    if (columns < 1) {
        columns = 1;
    }
    const dress_slot_t cat = dress_room_category();
    int indices[16];
    int n = 0;
    for (int i = 0; i < dress_room_catalog_count() && n < 16; ++i) {
        const dress_item_t *it = dress_room_catalog_item(i);
        if (it && it->slot == cat) {
            indices[n++] = i;
        }
    }
    nt_ui_label_style_t cat_title = g_theme.title;
    cat_title.font_size = 18.0F;

    /* Soft cream rail — magazine catalog, not tech demo grid. */
    CLAY({.id = CLAY_ID("dress/catalog"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 10,
                     .padding = {.left = 14, .right = 14, .top = 12, .bottom = 12}},
          .cornerRadius = CLAY_CORNER_RADIUS(18),
          .backgroundColor = {255.0F, 246.0F, 250.0F, 235.0F},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), dress_room_slot_label(cat), &cat_title);
        CLAY({.id = CLAY_ID("dress/catalog/grid"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = 10}}) {
            for (int start = 0; start < n; start += columns) {
                CLAY({.id = CLAY_IDI("dress/catrow", (uint32_t)start),
                      .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                 .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                 .childGap = 10}}) {
                    for (int c = 0; c < columns && start + c < n; ++c) {
                        const int i = indices[start + c];
                        const dress_item_t *it = dress_room_catalog_item(i);
                        if (!it) {
                            continue;
                        }
                        const bool selected = dress_room_equipped(cat) == i;
                        nt_ui_button_style_t style = selected ? g_theme.button_selected : g_theme.button;
                        if (selected) {
                            style.idle.scale = 1.03F;
                        }
                        char bid[48];
                        (void)snprintf(bid, sizeof bid, "dress/item/%s", it->id);
                        const uint32_t id = nt_ui_id(bid);
                        const float card_h = cell * 1.35F;
                        nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), id, &style,
                                           &(Clay_ElementDeclaration){
                                               .layout = {.sizing = {CLAY_SIZING_FIXED(cell), CLAY_SIZING_FIXED(card_h)},
                                                          .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                                          .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                                                          .padding = CLAY_PADDING_ALL(6)}},
                                           interactive, NULL);
                        if (it->atlas_thumb && i >= 0 && i < 32) {
                            s_thumb_reg[i] = region_named(it->atlas_thumb);
                            nt_ui_image_style_t ist = nt_ui_image_style_defaults();
                            nt_ui_image(ctx, NT_UI_DATA_LAYER(LAYER_IMG), &s_thumb_reg[i], &ist,
                                        &(Clay_ElementDeclaration){
                                            .layout = {.sizing = {CLAY_SIZING_FIXED(cell - 18.0F),
                                                                  CLAY_SIZING_FIXED(cell * 0.95F)}}});
                        }
                        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), it->label, &g_theme.button_label);
                        const bool clicked = nt_ui_button_end(ctx);
                        if (interactive && clicked) {
                            (void)dress_room_equip(i);
                        }
                    }
                }
            }
        }
    }
}

/* Compact theme pills — short labels so chips never clip on rail width. */
static const char *theme_chip_label(dress_theme_t theme) {
    switch (theme) {
    case DRESS_THEME_CASUAL:
        return "Casual";
    case DRESS_THEME_STREET:
        return "Street";
    case DRESS_THEME_GLAM:
        return "Glam";
    case DRESS_THEME_ELEGANT:
        return "Elegant";
    case DRESS_THEME_Y2K:
        return "Y2K";
    default:
        return "?";
    }
}

static void draw_theme_bar(nt_ui_context_t *ctx, float available_w, bool interactive) {
    const float chip_h = 34.0F;
    /* 5 themes + Surprise — chip_w always fits available_w (no min overflow). */
    float chip_w = (available_w - 5.0F * 5.0F) / 6.0F;
    if (chip_w > 88.0F) {
        chip_w = 88.0F;
    }
    if (chip_w < 36.0F) {
        chip_w = 36.0F;
    }
    CLAY({.id = CLAY_ID("dress/themes"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 5,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        for (int t = 0; t < DRESS_THEME_COUNT; ++t) {
            char id[32];
            (void)snprintf(id, sizeof id, "dress/theme/%d", t);
            const bool sel = dress_room_theme() == (dress_theme_t)t;
            const char *lab = theme_chip_label((dress_theme_t)t);
            /* Ultra-narrow: first letter only. */
            char short_lab[8];
            if (chip_w < 48.0F) {
                short_lab[0] = lab[0];
                short_lab[1] = '\0';
                lab = short_lab;
            }
            if (chip_button(ctx, id, lab, sel, chip_w, chip_h, interactive)) {
                dress_room_set_theme((dress_theme_t)t);
            }
        }
        if (action_button(ctx, "dress/theme/rand", chip_w < 48.0F ? "?" : "Surp", chip_w, chip_h, false,
                          interactive)) {
            dress_room_random_theme(0u);
        }
    }
}

static void draw_actions(nt_ui_context_t *ctx, float available_w, float btn_h, bool interactive) {
    /* Fit three buttons into rail/portrait width — SHOW is primary but must not clip. */
    const float gap = 8.0F;
    float unit = (available_w - 2.0F * gap) / 3.05F;
    if (unit > 120.0F) {
        unit = 120.0F;
    }
    if (unit < 56.0F) {
        unit = 56.0F;
    }
    const float shuffle_w = unit * 0.95F;
    const float reset_w = unit * 0.85F;
    const float show_w = unit * 1.25F;
    CLAY({.id = CLAY_ID("dress/actions"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 8,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        if (action_button(ctx, "dress/random", "Shuffle", shuffle_w, btn_h, false, interactive)) {
            dress_room_randomize_outfit(0u);
        }
        if (action_button(ctx, "dress/reset", "Reset", reset_w, btn_h, true, interactive)) {
            dress_room_reset_outfit();
        }
        if (success_button(ctx, "dress/show", "SHOW", show_w, btn_h + 2.0F, interactive)) {
            dress_room_begin_show();
        }
    }
}

static void draw_show_overlay(nt_ui_context_t *ctx, float layout_w, float layout_h, bool interactive) {
    const dress_mode_t mode = dress_room_mode();
    const bool landscape = layout_w >= layout_h;
    nt_ui_label_style_t title = label_light(24.0F);
    nt_ui_label_style_t body = label_light(18.0F);
    char line[96];

    CLAY({.id = CLAY_ID("dress/show"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
                     .childGap = 6,
                     .padding = CLAY_PADDING_ALL(8)},
          .backgroundColor = {42.0F, 18.0F, 32.0F, 252.0F},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        (void)snprintf(line, sizeof line, "FASHION SHOW  ·  %s", dress_room_theme_label(dress_room_theme()));
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), line, &title);

        if (landscape) {
            /* DTI-style dense podium: big hero + side rivals (less void). */
            float stage_h = layout_h * 0.78F;
            float side_w = layout_w * 0.20F;
            if (side_w < 150.0F) {
                side_w = 150.0F;
            }
            if (side_w > 220.0F) {
                side_w = 220.0F;
            }
            float stage_w = layout_w - side_w * 2.0F - 24.0F;
            if (stage_w < 240.0F) {
                stage_w = 240.0F;
            }
            float side_h = stage_h * 0.95F;
            CLAY({.id = CLAY_ID("dress/show/row"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = CLAY_LEFT_TO_RIGHT,
                             .childGap = 10,
                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
                CLAY({.id = CLAY_ID("dress/show/left"),
                      .layout = {.sizing = {CLAY_SIZING_FIXED(side_w), CLAY_SIZING_FIXED(side_h)},
                                 .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                 .childGap = 6,
                                 .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
                    draw_rival_card(ctx, 0, side_w, side_h * 0.49F);
                    draw_rival_card(ctx, 1, side_w, side_h * 0.49F);
                }
                draw_stage(ctx, stage_w, stage_h);
                draw_rival_card(ctx, 2, side_w, side_h);
            }
        } else {
            float stage_h = layout_h * 0.52F;
            float stage_w = layout_w * 0.94F;
            draw_stage(ctx, stage_w, stage_h);
            draw_rival_lineup(ctx, layout_w * 0.98F, layout_h * 0.20F);
        }

        if (mode == DRESS_MODE_SHOW_RUNWAY) {
            (void)snprintf(line, sizeof line, "Runway… %.0f%%", (double)(dress_room_show_t() * 100.0F));
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), line, &body);
            if (action_button(ctx, "dress/show/skip", "Skip", 160.0F, 48.0F, false, interactive)) {
                dress_room_show_advance();
            }
        } else {
            /* Competitor gap: readable reward (no tofu) + “look complete” beat. */
            (void)snprintf(line, sizeof line, "YOUR LOOK  %d stars   Rank #%d", dress_room_player_stars(),
                           dress_room_player_rank());
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), line, &body);
            CLAY({.id = CLAY_ID("dress/show/actions"),
                  .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = CLAY_LEFT_TO_RIGHT,
                             .childGap = 12}}) {
                if (action_button(ctx, "dress/show/restyle", "Restyle", 180.0F, 52.0F, false, interactive)) {
                    dress_room_return_freeplay();
                }
                if (success_button(ctx, "dress/show/again", "Show again", 180.0F, 52.0F, interactive)) {
                    dress_room_begin_show();
                }
            }
        }
    }
}

void dress_room_draw_ui(nt_ui_context_t *ctx, bool interactive) {
    if (!ctx) {
        return;
    }
    ensure_atlas();

    float layout_w = 0.0F;
    float layout_h = 0.0F;
    nt_ui_context_layout_size(ctx, &layout_w, &layout_h);
    if (layout_w < 1.0F || layout_h < 1.0F) {
        layout_w = 960.0F;
        layout_h = 540.0F;
    }
    const bool portrait = layout_h > layout_w;
    const dress_mode_t mode = dress_room_mode();

    /* Legacy theme-pick mode: fall through to freeplay UI (theme bar lives there). */
    if (mode == DRESS_MODE_THEME_PICK) {
        dress_room_return_freeplay();
    }
    if (mode == DRESS_MODE_SHOW_RUNWAY || mode == DRESS_MODE_SHOW_PODIUM) {
        draw_show_overlay(ctx, layout_w, layout_h, interactive);
        return;
    }

    /* Hero stage + closet rail. Warmer root reduces “void” vs competitors. */
    CLAY({.id = CLAY_ID("dress_root"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .padding = CLAY_PADDING_ALL(4),
                     .layoutDirection = portrait ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                     .childGap = 6},
          .backgroundColor = {36.0F, 16.0F, 28.0F, 255.0F},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        if (portrait) {
            const float rail_w = layout_w - 8.0F;
            draw_stage(ctx, rail_w, layout_h * 0.46F);
            draw_theme_bar(ctx, rail_w, interactive);
            draw_categories(ctx, rail_w, 36.0F, interactive);
            draw_actions(ctx, rail_w, 42.0F, interactive);
            /* 2-col catalog: all four items readable (portal freeplay density). */
            draw_catalog(ctx, 100.0F, 2, interactive);
        } else {
            /* Give rail enough width so chips/SHOW never clip (was stage 0.68 P0). */
            const float stage_w = layout_w * 0.58F;
            const float side_w = layout_w - stage_w - 12.0F;
            draw_stage(ctx, stage_w, layout_h - 8.0F);
            CLAY({.id = CLAY_ID("dress/side"),
                  .layout = {.sizing = {CLAY_SIZING_FIXED(side_w > 80.0F ? side_w : 280.0F), CLAY_SIZING_GROW(0)},
                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                             .childGap = 6,
                             .padding = {.left = 8, .right = 8, .top = 6, .bottom = 8}},
                  .cornerRadius = CLAY_CORNER_RADIUS(16),
                  .backgroundColor = {52.0F, 24.0F, 40.0F, 235.0F},
                  .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                const float inner = (side_w > 40.0F ? side_w : 280.0F) - 16.0F;
                /* Theme first = competitor freeplay→theme→show ladder. */
                draw_theme_bar(ctx, inner, interactive);
                {
                    nt_ui_label_style_t tip = label_light(14.0F);
                    char tip_line[64];
                    (void)snprintf(tip_line, sizeof tip_line, "Theme: %s  →  SHOW",
                                   dress_room_theme_label(dress_room_theme()));
                    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), tip_line, &tip);
                }
                draw_categories(ctx, inner, 36.0F, interactive);
                draw_actions(ctx, inner, 44.0F, interactive);
                draw_catalog(ctx, 104.0F, 2, interactive);
            }
        }
    }
}
