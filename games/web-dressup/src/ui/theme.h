#ifndef GAME_UI_THEME_H
#define GAME_UI_THEME_H

#include "atlas/nt_atlas.h"
#include "resource/nt_resource.h"
#include "ui/nt_ui_button.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_slider.h"

/* Fashion magazine chrome — styles live only here. */
typedef struct {
    nt_atlas_region_ref_t panel_region;       /* cream glass */
    nt_atlas_region_ref_t panel_dark_region;  /* rose glass rail/stage */
    nt_atlas_region_ref_t panel_brown_region;
    nt_atlas_region_ref_t checkmark_region;
    nt_ui_image_style_t panel_img;
    nt_ui_button_style_t button;          /* cream pill */
    nt_ui_button_style_t button_success;  /* rose-gold CTA */
    nt_ui_button_style_t button_danger;   /* soft wine */
    nt_ui_button_style_t button_selected; /* selected chip */
    nt_ui_slider_style_t slider;
    nt_ui_label_style_t title;
    nt_ui_label_style_t label;
    nt_ui_label_style_t button_label;
    nt_ui_label_style_t button_label_light;
    nt_ui_label_style_t hint;
} ui_theme_t;

extern ui_theme_t g_theme;

void theme_init(nt_resource_t atlas);

#endif /* GAME_UI_THEME_H */
