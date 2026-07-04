#include "ui/dialogue_panel.h"

#include "clay.h"
#include "game_dialogue.h"
#include "generated/game_assets.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"
#include "ui/nt_ui_button.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_modal.h"
#include "ui/nt_ui_scroll.h"
#include "ui/nt_ui_tooltip.h"
#include "ui/theme.h"

#include <stdio.h>
#include <string.h>

#define LAYER_BG 30
#define LAYER_IMG 31
#define LAYER_TEXT 32

#define DIALOGUE_MODAL_ID 0xD1A106U
#define DIALOGUE_REWARD_DETAIL_MODAL_ID 0xD1A107U

static const dialogue_reward_t *s_selected_reward;
static bool s_reward_detail_open;
static nt_resource_t s_dialogue_ui_atlas;
static nt_atlas_region_ref_t s_gate_guard_portrait_region;
static bool s_dialogue_regions_requested;

static nt_ui_label_style_t make_label(float size, float r, float g, float b, float a) {
    return (nt_ui_label_style_t){.font_id = 0, .font_size = size, .color = {r, g, b, a}};
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static nt_ui_button_style_t dialogue_choice_button(void) {
    nt_ui_button_style_t s = g_theme.button;
    s.idle.bg = (nt_atlas_region_ref_t){0};
    s.hover.bg = (nt_atlas_region_ref_t){0};
    s.pressed.bg = (nt_atlas_region_ref_t){0};
    s.disabled.bg = (nt_atlas_region_ref_t){0};
    s.idle.bg_tint = 0xFF7CB8D7U;
    s.hover.bg_tint = 0xFF8FCAE7U;
    s.pressed.bg_tint = 0xFF528BB2U;
    s.disabled.bg_tint = 0xFF7CB8D7U;
    s.transition_speed = 16.0F;
    return s;
}

static nt_ui_button_style_t dialogue_topic_button(void) {
    nt_ui_button_style_t s = dialogue_choice_button();
    s.idle.bg_tint = 0xFF7CBDDDU;
    s.hover.bg_tint = 0xFF8ECCE8U;
    s.pressed.bg_tint = 0xFF5D9DC9U;
    s.disabled.bg_tint = 0xFF7CBDDDU;
    return s;
}

static nt_ui_button_style_t dialogue_reward_button(void) {
    nt_ui_button_style_t s = dialogue_choice_button();
    s.idle.bg_tint = 0xFFE0B56AU;
    s.hover.bg_tint = 0xFFF1C981U;
    s.pressed.bg_tint = 0xFFC7904BU;
    s.disabled.bg_tint = 0xFFE0B56AU;
    return s;
}

static nt_ui_scroll_style_t dialogue_scroll_style(void) {
    nt_ui_scroll_style_t s = nt_ui_scroll_style_defaults();
    s.bar_visibility = NT_UI_SCROLLBAR_AUTO;
    s.bar_thickness = 9.0F;
    s.bar_thumb_min_px = 34.0F;
    s.track_tint = 0x55362418U;
    s.thumb_tint = 0xDDA96A36U;
    return s;
}

static nt_ui_button_style_t dialogue_close_button(void) {
    nt_ui_button_style_t s = dialogue_choice_button();
    s.idle.bg_tint = 0xFF835339U;
    s.hover.bg_tint = 0xFFA66845U;
    s.pressed.bg_tint = 0xFF5A3325U;
    s.disabled.bg_tint = 0xFF835339U;
    return s;
}

static nt_ui_modal_style_t dialogue_modal_style(void) {
    nt_ui_modal_style_t s = nt_ui_modal_style_defaults();
    s.backdrop_alpha = 0.72F;
    s.backdrop_color = 0xFF050302U;
    s.flags = 0U;
    s.open = (nt_ui_modal_anim_t){.type = NT_UI_MODAL_ANIM_FADE};
    s.close = (nt_ui_modal_anim_t){.type = NT_UI_MODAL_ANIM_FADE};
    s.ease_speed = 18.0F;
    return s;
}

static nt_ui_modal_style_t reward_detail_modal_style(void) {
    nt_ui_modal_style_t s = dialogue_modal_style();
    s.backdrop_alpha = 0.28F;
    s.flags = (uint8_t)(NT_UI_MODAL_CLOSE_ON_BACKDROP | NT_UI_MODAL_LISTEN_ESC);
    return s;
}

static nt_ui_tooltip_style_t reward_tooltip_style(void) {
    nt_ui_tooltip_style_t s = nt_ui_tooltip_style_defaults();
    s.delay_secs = 0.15F;
    s.font_size = 14.0F;
    s.max_width = 260U;
    s.pad = 8U;
    s.panel_bg = 0xFF1A2028U;
    s.text_color = 0xFFCFE7F6U;
    s.border_color = 0xFF659ED6U;
    s.border_px = 1U;
    s.corner_radius = 4U;
    return s;
}

static void reward_tooltip_text(const dialogue_reward_t *reward, char *buf, int cap) {
    if (!reward || !buf || cap <= 0) {
        return;
    }
    if (reward->amount > 1 && reward->kind == DIALOGUE_REWARD_ITEM) {
        (void)snprintf(buf, (size_t)cap, "%s x%d: %s", reward->name, reward->amount, reward->summary);
        return;
    }
    (void)snprintf(buf, (size_t)cap, "%s: %s", reward->name, reward->summary);
}

static void reward_detail_modal_ui(nt_ui_context_t *ctx, bool portrait) {
    if (!s_reward_detail_open || !s_selected_reward) {
        return;
    }

    bool detail_open = s_reward_detail_open;
    nt_ui_modal_style_t modal_style = reward_detail_modal_style();
    if (!nt_ui_modal_visible(ctx, DIALOGUE_REWARD_DETAIL_MODAL_ID, &modal_style, &detail_open)) {
        s_reward_detail_open = detail_open;
        if (!s_reward_detail_open) {
            s_selected_reward = NULL;
        }
        return;
    }

    const float panel_w = portrait ? 282.0F : 360.0F;
    const dialogue_reward_t *reward = s_selected_reward;
    const nt_ui_label_style_t title_style = make_label(22.0F, 255.0F, 238.0F, 202.0F, 255.0F);
    const nt_ui_label_style_t icon_style = make_label(28.0F, 246.0F, 220.0F, 170.0F, 255.0F);
    const nt_ui_label_style_t body_style = make_label(16.0F, 236.0F, 211.0F, 166.0F, 255.0F);
    const nt_ui_label_style_t close_label = make_label(18.0F, 255.0F, 239.0F, 206.0F, 255.0F);
    nt_ui_button_style_t close_button = dialogue_close_button();

    CLAY({.id = CLAY_ID("dialogue/reward_detail_frame"),
          .layout = {.sizing = {CLAY_SIZING_FIXED(panel_w), CLAY_SIZING_FIT(0)},
                     .padding = CLAY_PADDING_ALL(14),
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 12,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}},
          .backgroundColor = {49.0F, 31.0F, 22.0F, 255.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(6),
          .border = {.color = {224.0F, 169.0F, 93.0F, 255.0F}, .width = {2, 2, 2, 2, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        CLAY({.id = CLAY_ID("dialogue/reward_detail_head"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = 12,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            CLAY({.id = CLAY_ID("dialogue/reward_detail_icon"),
                  .layout = {.sizing = {CLAY_SIZING_FIXED(62.0F), CLAY_SIZING_FIXED(62.0F)},
                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
                  .backgroundColor = {27.0F, 22.0F, 18.0F, 255.0F},
                  .cornerRadius = CLAY_CORNER_RADIUS(4),
                  .border = {.color = {176.0F, 122.0F, 64.0F, 255.0F}, .width = {1, 1, 1, 1, 0}},
                  .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), reward->icon_label, &icon_style);
            }
            CLAY({.id = CLAY_ID("dialogue/reward_detail_title"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                             .childGap = 4,
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
                nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), reward->name, &title_style);
                nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), reward->summary, &body_style);
            }
        }

        CLAY({.id = CLAY_ID("dialogue/reward_detail_text"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .padding = {.left = 2, .right = 2, .top = 2, .bottom = 2}}}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), reward->detail, &body_style);
        }

        CLAY({.id = CLAY_ID("dialogue/reward_detail_close"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(42.0F)}}}) {
            const uint32_t close_id = nt_ui_id("dialogue/reward_detail_close/button");
            nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), close_id, &close_button,
                               &(Clay_ElementDeclaration){
                                   .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                              .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
                                   .cornerRadius = CLAY_CORNER_RADIUS(4),
                                   .border = {.color = {176.0F, 122.0F, 64.0F, 255.0F}, .width = {1, 1, 1, 1, 0}}},
                               true, NULL);
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "Закрыть", &close_label);
            if (nt_ui_button_end(ctx)) {
                detail_open = false;
            }
        }
    }
    nt_ui_modal_end(ctx);

    s_reward_detail_open = detail_open;
    if (!s_reward_detail_open) {
        s_selected_reward = NULL;
    }
}

static void reward_cell_ui(nt_ui_context_t *ctx, const char *scope, int slot, const dialogue_reward_t *reward, bool portrait,
                           const nt_ui_label_style_t *icon_style, const nt_ui_label_style_t *amount_style) {
    if (!reward) {
        return;
    }

    const float cell_size = portrait ? 54.0F : 48.0F;
    const uint32_t group_id = nt_ui_child_id(nt_ui_id("dialogue/reward_group"), scope);
    const uint32_t button_id = nt_ui_child_id(group_id, reward->id);
    nt_ui_button_style_t cell_button = dialogue_reward_button();
    char amount_buf[16];
    amount_buf[0] = '\0';
    if (reward->kind == DIALOGUE_REWARD_XP) {
        (void)snprintf(amount_buf, sizeof amount_buf, "+%d", reward->amount);
    } else if (reward->amount > 1) {
        (void)snprintf(amount_buf, sizeof amount_buf, "x%d", reward->amount);
    }

    CLAY({.id = CLAY_IDI("dialogue/reward_cell_box", slot),
          .layout = {.sizing = {CLAY_SIZING_FIXED(cell_size), CLAY_SIZING_FIXED(cell_size)}}}) {
        nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), button_id, &cell_button,
                           &(Clay_ElementDeclaration){
                               .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                          .padding = CLAY_PADDING_ALL(5),
                                          .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                          .childGap = 2,
                                          .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
                               .cornerRadius = CLAY_CORNER_RADIUS(4),
                               .border = {.color = {116.0F, 72.0F, 35.0F, 255.0F}, .width = {1, 1, 1, 1, 0}}},
                           true, NULL);
        CLAY({.id = CLAY_IDI("dialogue/reward_icon", slot),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = {36.0F, 27.0F, 20.0F, 245.0F},
              .cornerRadius = CLAY_CORNER_RADIUS(3),
              .border = {.color = {209.0F, 154.0F, 82.0F, 255.0F}, .width = {1, 1, 1, 1, 0}},
              .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), reward->icon_label, icon_style);
        }
        if (amount_buf[0] != '\0') {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), amount_buf, amount_style);
        }
        if (nt_ui_button_end(ctx)) {
            s_selected_reward = reward;
            s_reward_detail_open = true;
        }
    }

    char tip[192];
    reward_tooltip_text(reward, tip, (int)sizeof tip);
    nt_ui_tooltip_style_t tip_style = reward_tooltip_style();
    (void)nt_ui_tooltip(ctx, NT_UI_DATA_LAYER(LAYER_IMG), LAYER_TEXT, button_id, tip, &tip_style);
}

static void reward_section_ui(nt_ui_context_t *ctx, const char *scope, int slot_base, const char *title, const dialogue_reward_t *rewards, int count, bool portrait,
                              const nt_ui_label_style_t *section_style, const nt_ui_label_style_t *icon_style, const nt_ui_label_style_t *amount_style) {
    if (!rewards || count <= 0) {
        return;
    }
    CLAY({.id = CLAY_IDI("dialogue/reward_section", slot_base),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .padding = {.left = 10, .right = 10, .top = portrait ? 8 : 7, .bottom = portrait ? 9 : 7},
                     .layoutDirection = portrait ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                     .childGap = portrait ? 6 : 12,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, portrait ? CLAY_ALIGN_Y_TOP : CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {229.0F, 199.0F, 139.0F, 140.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = {139.0F, 91.0F, 45.0F, 205.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        CLAY({.id = CLAY_IDI("dialogue/reward_section_title", slot_base),
              .layout = {.sizing = {portrait ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIXED(160.0F), CLAY_SIZING_FIT(0)}}}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), title, section_style);
        }
        CLAY({.id = CLAY_IDI("dialogue/reward_row", slot_base),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = portrait ? 5 : 6,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            for (int i = 0; i < count; ++i) {
                reward_cell_ui(ctx, scope, slot_base + i, &rewards[i], portrait, icon_style, amount_style);
            }
        }
    }
}

static void rewards_preview_ui(nt_ui_context_t *ctx, const dialogue_quest_preview_t *preview, bool portrait,
                               const nt_ui_label_style_t *section_style, const nt_ui_label_style_t *icon_style, const nt_ui_label_style_t *amount_style) {
    if (!preview || !preview->completion_rewards || preview->completion_reward_count <= 0) {
        return;
    }
    CLAY({.id = CLAY_ID("dialogue/reward_preview"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 8,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
        reward_section_ui(ctx, "completion", 32, "Награда за выполнение", preview->completion_rewards, preview->completion_reward_count, portrait,
                          section_style, icon_style, amount_style);
    }
}

static void dialogue_grant_ui(nt_ui_context_t *ctx, const dialogue_quest_preview_t *preview, bool portrait,
                              const nt_ui_label_style_t *section_style, const nt_ui_label_style_t *icon_style,
                              const nt_ui_label_style_t *amount_style) {
    if (!preview || !preview->immediate_rewards || preview->immediate_reward_count <= 0) {
        return;
    }

    CLAY({.id = CLAY_ID("dialogue/grant_now"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .padding = {.left = 10, .right = 10, .top = 7, .bottom = 8},
                     .layoutDirection = portrait ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                     .childGap = portrait ? 6 : 10,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {226.0F, 192.0F, 132.0F, 135.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = {137.0F, 89.0F, 43.0F, 185.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        CLAY({.id = CLAY_ID("dialogue/grant_now_label"),
              .layout = {.sizing = {portrait ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIXED(154.0F), CLAY_SIZING_FIT(0)}}}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "Страж выдаст сейчас", section_style);
        }
        CLAY({.id = CLAY_ID("dialogue/grant_now_row"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = portrait ? 5 : 6,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            for (int i = 0; i < preview->immediate_reward_count; ++i) {
                reward_cell_ui(ctx, "grant_now", 64 + i, &preview->immediate_rewards[i], portrait, icon_style, amount_style);
            }
        }
    }
}

static const char *dialogue_window_title(const dialogue_definition_t *definition) {
    return definition && definition->quest_preview ? "Этап квеста" : (definition ? definition->title : "");
}

static const char *speaker_bio_text(const dialogue_node_t *node) {
    if (node && node->speaker_id && strcmp(node->speaker_id, "gate_guard") == 0) {
        return "Старший у ворот. Не выпускает новичков без снаряжения и проверки.";
    }
    return "Персонаж, связанный с текущим заданием.";
}

static nt_atlas_region_ref_t *speaker_portrait_region(const dialogue_node_t *node) {
    if (!node || !node->speaker_id || strcmp(node->speaker_id, "gate_guard") != 0) {
        return NULL;
    }
    if (!s_dialogue_regions_requested) {
        s_dialogue_ui_atlas = nt_resource_request(ASSET_ATLAS_UI, NT_ASSET_ATLAS);
        s_gate_guard_portrait_region = nt_atlas_ref(s_dialogue_ui_atlas, ASSET_ATLAS_REGION_UI_GATE_GUARD_PORTRAIT.value);
        s_dialogue_regions_requested = true;
    }
    return &s_gate_guard_portrait_region;
}

static void quest_header_ui(nt_ui_context_t *ctx, const char *quest_name, bool portrait,
                            const nt_ui_label_style_t *quest_header_style) {
    if (!quest_name) {
        return;
    }

    const nt_ui_label_style_t icon_style = make_label(portrait ? 21.0F : 24.0F, 108.0F, 24.0F, 12.0F, 255.0F);
    CLAY({.id = CLAY_ID("dialogue/quest_header"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 42.0F : 48.0F)},
                     .padding = {.left = 10, .right = 12, .top = 6, .bottom = 6},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 10,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {244.0F, 216.0F, 151.0F, 190.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = {153.0F, 91.0F, 43.0F, 230.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        CLAY({.id = CLAY_ID("dialogue/quest_header_icon"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(portrait ? 30.0F : 34.0F),
                                    CLAY_SIZING_FIXED(portrait ? 30.0F : 34.0F)},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = {232.0F, 177.0F, 44.0F, 255.0F},
              .cornerRadius = CLAY_CORNER_RADIUS(15),
              .border = {.color = {117.0F, 50.0F, 24.0F, 255.0F}, .width = {2, 2, 2, 2, 0}},
              .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "!", &icon_style);
        }
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), quest_name, quest_header_style);
    }
}

static void task_section_ui(nt_ui_context_t *ctx, const char *objective, bool portrait, const nt_ui_label_style_t *section_style,
                            const nt_ui_label_style_t *objective_style) {
    if (!objective) {
        return;
    }
    CLAY({.id = CLAY_ID("dialogue/task_section"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 118.0F : 84.0F)},
                     .padding = {.left = 12, .right = 14, .top = 10, .bottom = 10},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 11,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}},
          .backgroundColor = {250.0F, 224.0F, 154.0F, 230.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = {139.0F, 53.0F, 28.0F, 255.0F}, .width = {2, 2, 2, 2, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        CLAY({.id = CLAY_ID("dialogue/task_icon"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(42.0F), CLAY_SIZING_FIXED(42.0F)},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = {229.0F, 169.0F, 41.0F, 255.0F},
              .cornerRadius = CLAY_CORNER_RADIUS(21),
              .border = {.color = {117.0F, 50.0F, 24.0F, 255.0F}, .width = {2, 2, 2, 2, 0}},
              .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "!", section_style);
        }
        CLAY({.id = CLAY_ID("dialogue/current_objective_column"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = 6,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "Цель", section_style);
            CLAY({.id = CLAY_ID("dialogue/current_objective"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                             .padding = {.left = 0, .right = 6, .top = 0, .bottom = 2},
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
                nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), objective, objective_style);
            }
        }
    }
}

static bool dialogue_choices_ui(nt_ui_context_t *ctx, World *w, const dialogue_node_t *node, bool portrait,
                                const nt_ui_label_style_t *topic_label, const nt_ui_label_style_t *primary_label,
                                nt_ui_button_style_t *topic_button, nt_ui_button_style_t *primary_button) {
    if (!node) {
        return false;
    }
    int topic_count = 0;
    const dialogue_choice_t *primary = NULL;
    for (int i = 0; i < node->choice_count; ++i) {
        if (node->choices[i].kind == DIALOGUE_CHOICE_BRANCH) {
            ++topic_count;
        } else if (node->choices[i].kind == DIALOGUE_CHOICE_PROGRESS) {
            primary = &node->choices[i];
        }
    }
    if (topic_count <= 0 && !primary) {
        return false;
    }

    bool selected = false;
    CLAY({.id = CLAY_ID("dialogue/choices"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = portrait ? 9 : 10,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
        if (topic_count > 0) {
            CLAY({.id = CLAY_ID("dialogue/topic_choices"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = portrait ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                             .childGap = portrait ? 8 : 12,
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
                for (int i = 0; i < node->choice_count; ++i) {
                    const dialogue_choice_t *choice = &node->choices[i];
                    if (choice->kind != DIALOGUE_CHOICE_BRANCH) {
                        continue;
                    }
                    CLAY({.id = CLAY_IDI("dialogue/topic_choice", i),
                          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 40.0F : 32.0F)}}}) {
                        const uint32_t button_id = nt_ui_id(choice->id);
                        nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), button_id, topic_button,
                                           &(Clay_ElementDeclaration){
                                               .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                                          .padding = {.left = 14, .right = 14, .top = 6, .bottom = 6},
                                                          .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
                                               .cornerRadius = CLAY_CORNER_RADIUS(4),
                                               .border = {.color = {148.0F, 98.0F, 48.0F, 245.0F}, .width = {1, 1, 1, 1, 0}}},
                                           true, NULL);
                        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), choice->text, topic_label);
                        if (nt_ui_button_end(ctx)) {
                            (void)game_dialogue_select_choice(w, choice->id);
                            selected = true;
                        }
                    }
                    if (selected) {
                        break;
                    }
                }
            }
        }

        if (!selected && primary) {
            CLAY({.id = CLAY_ID("dialogue/primary_choice_row"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .padding = {.left = 0, .right = 0, .top = portrait ? 1 : 2, .bottom = 0},
                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
                CLAY({.id = CLAY_ID("dialogue/primary_choice_inline"),
                      .layout = {.sizing = {portrait ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIXED(340.0F),
                                            CLAY_SIZING_FIXED(portrait ? 46.0F : 42.0F)}}}) {
                    const uint32_t button_id = nt_ui_id(primary->id);
                    nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), button_id, primary_button,
                                       &(Clay_ElementDeclaration){
                                           .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                                      .padding = {.left = 18, .right = 18, .top = 8, .bottom = 8},
                                                      .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
                                           .cornerRadius = CLAY_CORNER_RADIUS(4),
                                           .border = {.color = {91.0F, 35.0F, 24.0F, 255.0F}, .width = {2, 2, 2, 2, 0}}},
                                       true, NULL);
                    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), primary->text, primary_label);
                    if (nt_ui_button_end(ctx)) {
                        (void)game_dialogue_select_choice(w, primary->id);
                        selected = true;
                    }
                }
            }
        }
    }
    return selected;
}

void dialogue_panel_ui(nt_ui_context_t *ctx, World *w) {
    if (!w || !w->dialogue.open || !w->dialogue.current_node) {
        s_selected_reward = NULL;
        s_reward_detail_open = false;
        return;
    }
    const dialogue_node_t *node = w->dialogue.current_node;
    const dialogue_definition_t *definition = w->dialogue.definition;
    const dialogue_quest_preview_t *preview = definition ? definition->quest_preview : NULL;
    float layout_w = 0.0F;
    float layout_h = 0.0F;
    nt_ui_context_layout_size(ctx, &layout_w, &layout_h);
    const bool portrait = layout_h > layout_w;
    const float panel_w = portrait ? clampf(layout_w - 28.0F, 292.0F, 520.0F)
                                   : clampf(layout_w * 0.84F, 660.0F, layout_w - 52.0F);
    const float panel_h = portrait ? clampf(layout_h * 0.94F, 390.0F, layout_h - 12.0F)
                                   : clampf(layout_h * 0.99F, 430.0F, layout_h - 2.0F);
    const float portrait_w = portrait ? panel_w - 56.0F : clampf(panel_w * 0.18F, 140.0F, 170.0F);
    const float portrait_badge_w = portrait ? 54.0F : portrait_w - 22.0F;
    const float portrait_badge_h = portrait ? 60.0F : 132.0F;

    nt_ui_modal_style_t modal_style = dialogue_modal_style();
    if (!nt_ui_modal_visible(ctx, DIALOGUE_MODAL_ID, &modal_style, &w->dialogue.open)) {
        return;
    }

    const nt_ui_label_style_t title_style = make_label(portrait ? 21.0F : 24.0F, 255.0F, 240.0F, 205.0F, 255.0F);
    const nt_ui_label_style_t quest_header_style = make_label(portrait ? 17.0F : 19.0F, 116.0F, 35.0F, 20.0F, 255.0F);
    const nt_ui_label_style_t body_style = make_label(portrait ? 17.0F : 18.0F, 54.0F, 34.0F, 22.0F, 255.0F);
    const nt_ui_label_style_t portrait_style = make_label(18.0F, 246.0F, 220.0F, 170.0F, 255.0F);
    const nt_ui_label_style_t portrait_hint_style = make_label(portrait ? 12.0F : 13.0F, 218.0F, 188.0F, 136.0F, 255.0F);
    const nt_ui_label_style_t objective_style = make_label(portrait ? 15.0F : 16.0F, 91.0F, 50.0F, 27.0F, 255.0F);
    const nt_ui_label_style_t reward_section_style = make_label(14.0F, 116.0F, 40.0F, 22.0F, 255.0F);
    const nt_ui_label_style_t reward_icon_style = make_label(portrait ? 17.0F : 19.0F, 248.0F, 226.0F, 184.0F, 255.0F);
    const nt_ui_label_style_t reward_amount_style = make_label(12.0F, 50.0F, 31.0F, 21.0F, 255.0F);
    const nt_ui_label_style_t topic_label = make_label(portrait ? 14.0F : 15.0F, 64.0F, 42.0F, 27.0F, 255.0F);
    const nt_ui_label_style_t primary_label = make_label(portrait ? 18.0F : 20.0F, 42.0F, 29.0F, 22.0F, 255.0F);
    const nt_ui_label_style_t answer_zone_label = make_label(portrait ? 13.0F : 14.0F, 238.0F, 205.0F, 145.0F, 255.0F);
    nt_ui_button_style_t topic_button = dialogue_topic_button();
    nt_ui_button_style_t primary_button = dialogue_choice_button();
    nt_ui_scroll_style_t scroll_style = dialogue_scroll_style();

    CLAY({.id = CLAY_ID("dialogue/modal_frame"),
          .layout = {.sizing = {CLAY_SIZING_FIXED(panel_w), CLAY_SIZING_FIXED(panel_h)},
                     .padding = CLAY_PADDING_ALL(7),
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 0,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}},
          .backgroundColor = {54.0F, 34.0F, 23.0F, 252.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(6),
          .border = {.color = {214.0F, 169.0F, 103.0F, 255.0F}, .width = {2, 2, 2, 2, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        CLAY({.id = CLAY_ID("dialogue/title_bar"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 42.0F : 48.0F)},
                         .padding = {.left = 18, .right = 18, .top = 7, .bottom = 7},
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = {91.0F, 35.0F, 24.0F, 248.0F},
              .cornerRadius = CLAY_CORNER_RADIUS(4),
              .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), dialogue_window_title(definition), &title_style);
        }

        CLAY({.id = CLAY_ID("dialogue/parchment"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .padding = CLAY_PADDING_ALL(portrait ? 16 : 20),
                         .layoutDirection = portrait ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                         .childGap = portrait ? 14 : 18,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}},
              .backgroundColor = {198.0F, 164.0F, 111.0F, 250.0F},
              .cornerRadius = CLAY_CORNER_RADIUS(5),
              .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
            CLAY({.id = CLAY_ID("dialogue/portrait_column"),
                  .layout = {.sizing = {CLAY_SIZING_FIXED(portrait_w), portrait ? CLAY_SIZING_FIXED(100.0F) : CLAY_SIZING_FIT(0)},
                             .padding = CLAY_PADDING_ALL(10),
                             .layoutDirection = portrait ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM,
                             .childGap = portrait ? 12 : 9,
                             .childAlignment = {portrait ? CLAY_ALIGN_X_LEFT : CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
                  .backgroundColor = {59.0F, 43.0F, 33.0F, 245.0F},
                  .cornerRadius = CLAY_CORNER_RADIUS(4),
                  .border = {.color = {226.0F, 184.0F, 112.0F, 255.0F}, .width = {1, 1, 1, 1, 0}},
                  .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                CLAY({.id = CLAY_ID("dialogue/portrait_badge"),
                      .layout = {.sizing = {CLAY_SIZING_FIXED(portrait_badge_w),
                                            CLAY_SIZING_FIXED(portrait_badge_h)},
                                 .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
                      .backgroundColor = {28.0F, 23.0F, 20.0F, 255.0F},
                      .cornerRadius = CLAY_CORNER_RADIUS(4),
                      .border = {.color = {178.0F, 126.0F, 66.0F, 255.0F}, .width = {1, 1, 1, 1, 0}},
                      .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                    nt_atlas_region_ref_t *portrait_region = speaker_portrait_region(node);
                    if (portrait_region) {
                        nt_ui_image_style_t portrait_image = nt_ui_image_style_defaults();
                        portrait_image.color_packed = 0xFFFFFFFFU;
                        nt_ui_image(ctx, NT_UI_DATA_LAYER(LAYER_IMG), portrait_region, &portrait_image, NULL);
                    } else {
                        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), portrait ? "NPC" : "СТРАЖ", &portrait_style);
                    }
                }
                if (portrait) {
                    CLAY({.id = CLAY_ID("dialogue/portrait_identity"),
                          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                     .childGap = 4,
                                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
                        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), node->speaker_name, &portrait_style);
                        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), speaker_bio_text(node), &portrait_hint_style);
                    }
                } else {
                    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), node->speaker_name, &portrait_style);
                    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), speaker_bio_text(node), &portrait_hint_style);
                }
            }

            CLAY({.id = CLAY_ID("dialogue/content_column"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                             .padding = {2, 2, 0, 0},
                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                             .childGap = 8,
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
                const uint32_t scroll_id = nt_ui_id("dialogue/content_scroll");
                nt_ui_scroll_begin(ctx, NULL, scroll_id, &scroll_style,
                                   &(Clay_ElementDeclaration){
                                       .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                                  .padding = {.left = 0, .right = 10, .top = 0, .bottom = 2}},
                                       .cornerRadius = CLAY_CORNER_RADIUS(4)});
                CLAY({.id = CLAY_ID("dialogue/content_inner"),
                      .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                 .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                 .childGap = portrait ? 8 : 9,
                                 .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
                    quest_header_ui(ctx, node->quest_name, portrait, &quest_header_style);
                    CLAY({.id = CLAY_ID("dialogue/body_text"),
                          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                     .padding = {.left = 2, .right = 8, .top = 4, .bottom = 5}}}) {
                        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), node->text, &body_style);
                    }

                    dialogue_grant_ui(ctx, preview, portrait, &reward_section_style, &reward_icon_style, &reward_amount_style);

                    const char *objective = preview && preview->goal ? preview->goal : game_dialogue_current_objective(w);
                    task_section_ui(ctx, objective, portrait, &reward_section_style, &objective_style);
                    rewards_preview_ui(ctx, preview, portrait, &reward_section_style, &reward_icon_style, &reward_amount_style);
                }
                nt_ui_scroll_end(ctx);

                CLAY({.id = CLAY_ID("dialogue/choice_tray"),
                      .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                 .padding = {.left = portrait ? 10 : 12, .right = portrait ? 10 : 12, .top = portrait ? 10 : 8, .bottom = portrait ? 10 : 8},
                                 .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                 .childGap = portrait ? 8 : 8,
                                 .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}},
                      .backgroundColor = {45.0F, 31.0F, 24.0F, 235.0F},
                      .cornerRadius = CLAY_CORNER_RADIUS(4),
                      .border = {.color = {214.0F, 169.0F, 103.0F, 255.0F}, .width = {2, 1, 1, 1, 0}},
                      .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                    CLAY({.id = CLAY_ID("dialogue/choice_tray_header"),
                          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 24.0F : 22.0F)},
                                     .padding = {.left = 10, .right = 10, .top = 3, .bottom = 3},
                                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}},
                          .backgroundColor = {91.0F, 35.0F, 24.0F, 245.0F},
                          .cornerRadius = CLAY_CORNER_RADIUS(3),
                          .border = {.color = {178.0F, 126.0F, 66.0F, 235.0F}, .width = {1, 1, 1, 1, 0}},
                          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "Ответы", &answer_zone_label);
                    }
                    (void)dialogue_choices_ui(ctx, w, node, portrait, &topic_label, &primary_label, &topic_button, &primary_button);
                }
            }
        }
    }
    reward_detail_modal_ui(ctx, portrait);
    nt_ui_modal_end(ctx);
}
