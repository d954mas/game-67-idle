#ifndef GAME_UI_THEME_H
#define GAME_UI_THEME_H

#include "atlas/nt_atlas.h"
#include "resource/nt_resource.h"
#include "ui/nt_ui_button.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_slider.h"

// ALL UI styling lives here, separate from the logic that uses it (the rule:
// styles/theme in their own file). Colours, widget styles, and the atlas region
// refs the widgets render with. theme_init binds the refs to the atlas; the refs
// resolve lazily on first emit once the atlas resource is ready.
typedef struct {
    nt_atlas_region_ref_t panel_region; // slice9 panel background art
    nt_ui_image_style_t panel_img;
    nt_ui_button_style_t button;        // primary button (slice9 art)
    nt_ui_button_style_t button_danger; // reset button (red tint)
    nt_ui_slider_style_t slider;        // track + fill + thumb
    nt_ui_label_style_t title;
    nt_ui_label_style_t label;        // on panel (light text on dark panel)
    nt_ui_label_style_t button_label; // on light-centered button art (dark text)
    nt_ui_label_style_t hint;
} ui_theme_t;

extern ui_theme_t g_theme;

void theme_init(nt_resource_t atlas);

#endif /* GAME_UI_THEME_H */
