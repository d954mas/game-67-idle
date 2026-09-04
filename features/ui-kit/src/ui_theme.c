#include "features/ui_kit/ui_theme.h"

ui_theme_t g_ui_theme;

// Hover lightens the tint ~12% per channel (saturating); pressed drops to the
// deep step so the whole button reads pushed into its lift shadow. The art is
// drawn grayscale at that same deep ratio, which is why one image serves every
// action colour.
static uint32_t lighten(uint32_t abgr) {
    uint32_t out = abgr & 0xFF000000U;
    for (int shift = 0; shift < 24; shift += 8) {
        uint32_t c = (abgr >> shift) & 0xFFU;
        c = c + (c >> 3);
        if (c > 0xFFU) {
            c = 0xFFU;
        }
        out |= c << shift;
    }
    return out;
}

static uint32_t deepen(uint32_t abgr) {
    uint32_t out = abgr & 0xFF000000U;
    for (int shift = 0; shift < 24; shift += 8) {
        const uint32_t c = (abgr >> shift) & 0xFFU;
        out |= (c * 3U / 4U) << shift;
    }
    return out;
}

static nt_ui_button_style_t action_button(nt_ui_button_style_t base, uint32_t tint) {
    base.idle.bg_tint = tint;
    base.hover.bg_tint = lighten(tint);
    base.pressed.bg_tint = deepen(tint);
    base.disabled.bg_tint = tint;
    return base;
}

static Clay_Color clay_color(uint32_t abgr) {
    return (Clay_Color){
        (float)(abgr & 0xFFU),
        (float)((abgr >> 8) & 0xFFU),
        (float)((abgr >> 16) & 0xFFU),
        (float)((abgr >> 24) & 0xFFU),
    };
}

static nt_ui_label_style_t label_style(float css_size, uint32_t color, uint8_t wrap) {
    return (nt_ui_label_style_t){.font_id = 0, .font_size = css_size, .color = clay_color(color), .wrap_mode = wrap};
}

const ui_tokens_t *ui_theme_tokens(void) {
    return g_ui_theme.tokens != NULL ? g_ui_theme.tokens : ui_tokens_studio_default();
}

void ui_theme_init(const ui_tokens_t *tokens, const ui_theme_art_t *art) {
    const ui_tokens_t *t = tokens != NULL ? tokens : ui_tokens_studio_default();
    g_ui_theme.tokens = t;
    if (art != NULL) {
        g_ui_theme.art = *art;
    }

    g_ui_theme.plate_img = nt_ui_image_style_defaults();
    g_ui_theme.plate_img.slice9_scale = t->slice9_scale;

    nt_ui_button_style_t base = {
        .idle = {.bg = g_ui_theme.art.button, .bg_tint = 0xFFFFFFFFU, .scale = 1.0F, .opacity = 1.0F},
        .hover = {.bg = g_ui_theme.art.button, .bg_tint = 0xFFFFFFFFU, .scale = 1.04F, .opacity = 1.0F},
        // Pressed sinks INTO the lift ledge the art already draws: the offset is
        // the whole press, the scale only keeps the corners from popping.
        .pressed = {.bg = g_ui_theme.art.button, .bg_tint = 0xFFFFFFFFU, .scale = 0.98F, .offset_y = 3.0F, .opacity = 1.0F},
        .disabled = {.bg = g_ui_theme.art.button, .bg_tint = 0xFFFFFFFFU, .scale = 1.0F, .opacity = 0.4F},
        .transition_speed = 12.0F,
        .hit_padding_lrtb = {8, 8, 8, 8},
        .slice9_scale = t->slice9_scale,
    };
    // Neutral button: the light tile surface with dark ink text. Its hover goes
    // to pure white rather than a lightened tile, because the tile is already
    // near white and a saturating step would be invisible.
    g_ui_theme.button = action_button(base, t->tile);
    g_ui_theme.button.hover.bg_tint = 0xFFFFFFFFU;
    g_ui_theme.button.pressed.bg_tint = t->tile_dim;

    g_ui_theme.button_confirm = action_button(base, t->go);
    g_ui_theme.button_info = action_button(base, t->info);
    g_ui_theme.button_danger = action_button(base, t->danger);

    // Slider sizes are CSS pixels; ui_kit_slider_style converts them per frame.
    nt_ui_slider_style_t s = nt_ui_slider_style_defaults();
    s.track_h = 18.0F;
    s.thumb_w = 26.0F;
    s.thumb_h = 26.0F;
    s.value_speed = 18.0F;
    s.hit_padding_lrtb[2] = 12;
    s.hit_padding_lrtb[3] = 12;
    s.states[NT_UI_SLIDER_IDLE].track = g_ui_theme.art.slider_track_sm;
    s.states[NT_UI_SLIDER_IDLE].fill = g_ui_theme.art.slider_fill_sm;
    s.states[NT_UI_SLIDER_IDLE].thumb = g_ui_theme.art.thumb;
    s.states[NT_UI_SLIDER_IDLE].fill_tint = t->info;
    g_ui_theme.slider = s;

    nt_ui_progress_style_t p = nt_ui_progress_style_defaults();
    p.track = g_ui_theme.art.slider_track;
    p.fill = g_ui_theme.art.slider_fill;
    p.fill_tint = t->go;
    p.track_h = 18.0F;
    p.fill_mode = NT_UI_FILL_STRETCH;
    g_ui_theme.progress = p;

    g_ui_theme.title = label_style(t->t_display, t->on_panel, CLAY_TEXT_WRAP_WORDS);
    g_ui_theme.heading = label_style(t->t_title, t->on_panel, CLAY_TEXT_WRAP_WORDS);
    g_ui_theme.label = label_style(t->t_body, t->on_panel, CLAY_TEXT_WRAP_WORDS);
    g_ui_theme.button_label = label_style(t->t_body, t->ink, CLAY_TEXT_WRAP_WORDS);
    g_ui_theme.button_label_action = label_style(t->t_body, t->on_panel, CLAY_TEXT_WRAP_WORDS);
    g_ui_theme.hint = label_style(t->t_badge, t->on_panel_soft, CLAY_TEXT_WRAP_WORDS);
    g_ui_theme.amount = label_style(t->t_num, t->coin, CLAY_TEXT_WRAP_NONE);
    g_ui_theme.row_title = label_style(t->t_row, t->ink, CLAY_TEXT_WRAP_NONE);
    g_ui_theme.row_sub = label_style(t->t_row_sub, t->ink_soft, CLAY_TEXT_WRAP_NONE);
}
