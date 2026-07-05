#ifndef RB_DARK_RPG_UI_GAME_MODAL_H
#define RB_DARK_RPG_UI_GAME_MODAL_H

#include "atlas/nt_atlas.h"
#include "ui/nt_ui.h"
#include "ui/nt_ui_button.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_modal.h"
#include "ui/nt_ui_scroll.h"

#include <stdbool.h>

typedef enum game_modal_art_t {
    GAME_MODAL_ART_OUTER_FRAME = 0,
    GAME_MODAL_ART_BODY_PANEL,
    GAME_MODAL_ART_HEADER_PLAQUE,
    GAME_MODAL_ART_OBJECTIVE_PANEL,
    GAME_MODAL_ART_ANSWER_NORMAL,
    GAME_MODAL_ART_ANSWER_PRIMARY,
    GAME_MODAL_ART_REWARD_CELL,
    GAME_MODAL_ART_WHITE,
} game_modal_art_t;

nt_atlas_region_ref_t *game_modal_art(game_modal_art_t art);
nt_ui_modal_style_t game_modal_style(nt_ui_layer_t layer, bool dismissible);
bool game_modal_visible(nt_ui_context_t *ctx, uint32_t id,
                        const nt_ui_modal_style_t *style, bool *open,
                        bool ignore_close_request);
void game_modal_clear_state(nt_ui_context_t *ctx, uint32_t id);
nt_ui_image_style_t game_modal_panel_image(bool portrait);
nt_ui_image_style_t game_modal_body_image(bool portrait);
nt_ui_image_style_t game_modal_header_image(bool portrait);
nt_ui_image_style_t game_modal_small_panel_image(bool portrait);
nt_ui_button_style_t game_modal_button_style(bool primary);
nt_ui_button_style_t game_modal_close_button_style(void);
nt_ui_scroll_style_t game_modal_scroll_style(void);
nt_ui_label_style_t game_modal_label(float size, float r, float g, float b, float a);
bool game_modal_close_button(nt_ui_context_t *ctx, nt_ui_layer_t image_layer,
                             nt_ui_layer_t text_layer, const char *id_text,
                             bool portrait);

#endif /* RB_DARK_RPG_UI_GAME_MODAL_H */
