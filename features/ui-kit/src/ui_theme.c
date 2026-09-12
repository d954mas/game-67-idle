#include "features/ui_kit/ui_theme.h"

#include "hash/nt_hash.h"

#include <stdio.h>

ui_theme_t g_ui_theme;

void ui_theme_art_bind_icons(ui_theme_art_t *art, nt_resource_t atlas, const char *prefix) {
    for (int i = 0; i < UI_ICON_COUNT; ++i) {
        char name[96];
        (void)snprintf(name, sizeof name, "%s/%s", prefix, ui_icon_name((ui_icon_t)i));
        art->icons[i] = nt_atlas_ref(atlas, nt_hash64_str(name).value);
    }
}

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

/* A selection wash: the action colour folded a quarter of the way into the light
 * tile, so a picked row reads as chosen without shouting over its neighbours. */
static uint32_t wash(uint32_t color, uint32_t base) {
    uint32_t out = base & 0xFF000000U;
    for (int shift = 0; shift < 24; shift += 8) {
        const uint32_t c = (color >> shift) & 0xFFU;
        const uint32_t b = (base >> shift) & 0xFFU;
        out |= ((b * 3U + c) / 4U) << shift;
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
    return base;
}

static void switch_cells(nt_ui_cb_state_t cells[4], nt_atlas_region_ref_t box, nt_atlas_region_ref_t check,
                         uint32_t box_tint, uint32_t check_tint) {
    for (int i = 0; i < 4; ++i) {
        cells[i].box = box;
        cells[i].check = check;
        cells[i].box_tint = box_tint;
        cells[i].check_tint = check_tint;
    }
    cells[NT_UI_CB_HOVER].scale = 1.04F;
    cells[NT_UI_CB_PRESSED].scale = 0.96F;
    cells[NT_UI_CB_DISABLED].opacity = 0.4F;
}

/* One switch style: the unchecked and checked rows differ only in the body
   tint (or, for the radio, in nothing but the dot). */
static nt_ui_checkbox_style_t switch_style(const ui_tokens_t *t, nt_atlas_region_ref_t body, uint32_t off_tint,
                                           uint32_t on_tint, nt_atlas_region_ref_t knob, uint32_t knob_tint,
                                           float box_w, float box_h, float overlay, float thumb_pad) {
    nt_ui_checkbox_style_t s = nt_ui_checkbox_style_defaults();
    switch_cells(s.unchecked, body, knob, off_tint, knob_tint);
    switch_cells(s.checked, body, knob, on_tint, knob_tint);
    s.box_w = box_w;
    s.box_h = box_h;
    s.overlay_w = overlay;
    s.overlay_h = overlay;
    s.thumb_pad = thumb_pad;
    s.gap = t->gap * 0.6F;
    s.state_speed = 12.0F;
    s.value_speed = 16.0F;
    return s;
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
    const uint32_t action_text = t->on_action != 0U ? t->on_action : t->on_panel;
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
        .pressed = {.bg = g_ui_theme.art.button, .bg_tint = 0xFFFFFFFFU, .scale = 0.98F, .offset_y = t->lift * 0.75F, .opacity = 1.0F},
        // One disabled look for every role: the off fill, no dimming, so a
        // disabled buy never reads as a faded buy.
        .disabled = {.bg = g_ui_theme.art.button, .bg_tint = t->off, .scale = 1.0F, .opacity = 1.0F},
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
    g_ui_theme.button_ad = action_button(base, t->info);
    g_ui_theme.button_info = g_ui_theme.button_ad;
    g_ui_theme.button_danger = action_button(base, t->danger);

    // The round close: the thumb art (a circle with the contour rim) in the
    // neutral tile colour, because dismissing is the most frequent tap and red
    // is kept for what destroys; a circle has no ledge to sink into, so the
    // press is scale alone.
    nt_ui_button_style_t round = base;
    round.idle.bg = g_ui_theme.art.thumb;
    round.hover.bg = g_ui_theme.art.thumb;
    round.pressed.bg = g_ui_theme.art.thumb;
    round.disabled.bg = g_ui_theme.art.thumb;
    round.pressed.offset_y = 0.0F;
    round.pressed.scale = 0.92F;
    g_ui_theme.button_close = action_button(round, t->tile);
    g_ui_theme.button_close.hover.bg_tint = 0xFFFFFFFFU;
    g_ui_theme.button_close.pressed.bg_tint = t->tile_dim;

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
    s.states[NT_UI_SLIDER_IDLE].fill_tint = t->go;
    g_ui_theme.slider = s;

    // Pick-one control. The trigger wears the neutral button art so it reads as
    // the same family as the buttons beside it; the open list is a flat tile
    // panel whose rows only tint, because slice9 art per row would fight the
    // panel's own border. The game sets min_width to its own content box.
    nt_ui_dropdown_style_t dd = nt_ui_dropdown_style_defaults();
    dd.trigger_idle = (nt_ui_dd_state_t){.bg = g_ui_theme.art.button, .bg_tint = t->tile, .scale = 1.0F, .opacity = 1.0F};
    dd.trigger_hover = (nt_ui_dd_state_t){.bg = g_ui_theme.art.button, .bg_tint = 0xFFFFFFFFU, .scale = 1.02F, .opacity = 1.0F};
    dd.trigger_pressed = (nt_ui_dd_state_t){.bg = g_ui_theme.art.button, .bg_tint = t->tile_dim, .scale = 0.99F, .opacity = 1.0F};
    dd.row_idle = (nt_ui_dd_state_t){.fill = 0U, .bg_tint = 0xFFFFFFFFU, .scale = 1.0F, .opacity = 1.0F};
    dd.row_hover = (nt_ui_dd_state_t){.fill = t->tile_dim, .bg_tint = 0xFFFFFFFFU, .scale = 1.0F, .opacity = 1.0F};
    dd.row_pressed = (nt_ui_dd_state_t){.fill = deepen(t->tile_dim), .bg_tint = 0xFFFFFFFFU, .scale = 1.0F, .opacity = 1.0F};
    dd.row_selected = (nt_ui_dd_state_t){.fill = wash(t->go, t->tile), .bg_tint = 0xFFFFFFFFU, .scale = 1.0F, .opacity = 1.0F};
    // The open list is the tile plate, so its corners and rim are the kit's.
    dd.panel_bg = g_ui_theme.art.tile;
    dd.panel_tint = 0xFFFFFFFFU;
    dd.panel_fill = t->tile;
    dd.panel_corner_radius = 12U;
    dd.trigger_text = t->ink;
    dd.row_text = t->ink;
    dd.font_size = t->t_body;
    dd.slice9_scale = t->slice9_scale;
    dd.row_height = (uint16_t)t->hit;
    dd.min_width = (uint16_t)t->panel_min_w;
    dd.pad = (uint16_t)(t->gap * 0.6F);
    // The trigger's chevron is the kit's glyph in ink; a consumer that packed
    // no glyphs gets the plain trigger, and the open list is the affordance.
    dd.chevron = g_ui_theme.art.icons[UI_ICON_ARROW_DOWN];
    dd.chevron_tint = t->ink;
    dd.chevron_size = (uint16_t)(dd.chevron.atlas.id != 0U ? t->hit * 0.4F : 0.0F);
    dd.max_visible_rows = 6U; // a longer list scrolls instead of leaving the panel
    dd.state_speed = 12.0F;
    dd.open_ease_speed = 16.0F;
    g_ui_theme.dropdown = dd;

    // A scrolled list's bar: thin, the fill pill in soft ink, gone when the
    // list rests, so a long list never grows a hairline down its edge.
    nt_ui_scroll_style_t sc = nt_ui_scroll_style_defaults();
    sc.bar_visibility = NT_UI_SCROLLBAR_AUTO_HIDE;
    sc.bar_thickness = t->gap * 0.5F;
    sc.thumb_ref = g_ui_theme.art.slider_fill;
    sc.thumb_tint = t->ink_soft;
    g_ui_theme.scroll = sc;

    // The engine progress bar bakes its slice9 borders at source size, like the
    // slider, so it takes the design-size pair; the 4x pair belongs to
    // ui_kit_meter, which scales the borders itself.
    nt_ui_progress_style_t p = nt_ui_progress_style_defaults();
    p.track = g_ui_theme.art.slider_track_sm;
    p.fill = g_ui_theme.art.slider_fill_sm;
    p.fill_tint = t->go;
    p.track_h = 18.0F;
    p.fill_mode = NT_UI_FILL_STRETCH;
    g_ui_theme.progress = p;

    // The white fill pill takes a tint exactly, so the body is the action
    // colour when on; the radio is the knob art twice, a white ring around a
    // coloured dot.
    const nt_atlas_region_ref_t pill = g_ui_theme.art.slider_fill_sm;
    const nt_atlas_region_ref_t knob = g_ui_theme.art.thumb;
    g_ui_theme.toggle = switch_style(t, pill, t->off, t->go, knob, 0xFFFFFFFFU, t->hit * 1.18F, t->hit * 0.68F, t->hit * 0.55F, t->rim);
    g_ui_theme.checkbox = switch_style(t, pill, t->tile_dim, t->go, knob, 0xFFFFFFFFU, t->hit * 0.64F, t->hit * 0.64F, t->hit * 0.32F, 0.0F);
    g_ui_theme.radio = switch_style(t, knob, 0xFFFFFFFFU, 0xFFFFFFFFU, knob, t->go, t->hit * 0.64F, t->hit * 0.64F, t->hit * 0.32F, 0.0F);
    g_ui_theme.toggle.text_base = label_style(t->t_body, t->on_panel, CLAY_TEXT_WRAP_NONE);
    g_ui_theme.checkbox.text_base = g_ui_theme.toggle.text_base;
    g_ui_theme.radio.text_base = g_ui_theme.toggle.text_base;

    g_ui_theme.title = label_style(t->t_display, t->on_panel, CLAY_TEXT_WRAP_WORDS);
    g_ui_theme.heading = label_style(t->t_title, t->on_panel, CLAY_TEXT_WRAP_WORDS);
    g_ui_theme.label = label_style(t->t_body, t->on_panel, CLAY_TEXT_WRAP_WORDS);
    g_ui_theme.button_label = label_style(t->t_body, t->ink, CLAY_TEXT_WRAP_WORDS);
    g_ui_theme.button_label_action = label_style(t->t_body, action_text, CLAY_TEXT_WRAP_WORDS);
    // White on a colour reads at any size with the contour around the glyphs.
    g_ui_theme.button_label_action.outline_w = 0.09F;
    g_ui_theme.button_label_action.outline_color = t->shell;
    g_ui_theme.button_label_disabled = label_style(t->t_body, t->ink_soft, CLAY_TEXT_WRAP_WORDS);
    g_ui_theme.header_title = label_style(t->t_title, action_text, CLAY_TEXT_WRAP_NONE);
    g_ui_theme.header_title.outline_w = 0.09F;
    g_ui_theme.header_title.outline_color = t->shell;
    g_ui_theme.hint = label_style(t->t_badge, t->on_panel_soft, CLAY_TEXT_WRAP_WORDS);
    g_ui_theme.amount = label_style(t->t_num, t->coin, CLAY_TEXT_WRAP_NONE);
    g_ui_theme.row_title = label_style(t->t_row, t->ink, CLAY_TEXT_WRAP_NONE);
    g_ui_theme.row_sub = label_style(t->t_row_sub, t->ink_soft, CLAY_TEXT_WRAP_NONE);
    g_ui_theme.counter = label_style(t->t_num, t->ink, CLAY_TEXT_WRAP_NONE);
    g_ui_theme.on_world = label_style(t->t_title, 0xFFFFFFFFU, CLAY_TEXT_WRAP_WORDS);
    // The outline is in em so it keeps its share of the glyph at every scale.
    g_ui_theme.on_world.outline_w = 0.09F;
    g_ui_theme.on_world.outline_color = t->shell;
    g_ui_theme.generation++;
}
