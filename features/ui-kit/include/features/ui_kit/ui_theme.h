#ifndef FEATURE_UI_KIT_THEME_H
#define FEATURE_UI_KIT_THEME_H

#include "atlas/nt_atlas.h"
#include "ui/nt_ui_button.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_progress.h"
#include "ui/nt_ui_slider.h"

#include "features/ui_kit/ui_tokens.h"

// The tokens as engine styles. All styling lives here, separate from the logic
// that uses it: a screen picks a role (`button_danger`, `hint`) and never a
// colour or a size.
//
// SIZES IN THIS STRUCT ARE CSS PIXELS, not UI units. A label style's font_size
// is the ramp step from the tokens, and ui_kit converts it for the frame.
// Passing one straight to an engine widget renders CSS numbers as UI units,
// which is a phone-sized interface on a monitor and the reverse.

// The atlas regions the kit draws with. The CONSUMER resolves these, because it
// owns `build_packs.c` and the generated asset ids; the feature never writes to
// a pack and never includes a generated header. A zeroed region simply does not
// draw, so a game that ships only part of the kit still runs.
typedef struct {
    nt_atlas_region_ref_t panel;
    nt_atlas_region_ref_t button;
    nt_atlas_region_ref_t tile;
    nt_atlas_region_ref_t slider_track;
    nt_atlas_region_ref_t slider_fill;
    nt_atlas_region_ref_t slider_track_sm; // design-size copies for the engine
    nt_atlas_region_ref_t slider_fill_sm;  // slider, which bakes borders 1:1
    nt_atlas_region_ref_t thumb;
    nt_atlas_region_ref_t icon_play;
} ui_theme_art_t;

typedef struct {
    const ui_tokens_t *tokens;
    ui_theme_art_t art;
    nt_ui_image_style_t plate_img; // slice9 image style shared by every plate

    nt_ui_button_style_t button;         // neutral action: the light tile surface
    nt_ui_button_style_t button_confirm; // the standard action in a pair
    nt_ui_button_style_t button_info;    // a secondary or rewarded action; never green
    nt_ui_button_style_t button_danger;  // destructive action
    nt_ui_slider_style_t slider;         // track + fill + thumb
    nt_ui_progress_style_t progress;     // meter drawn from the slider art

    // Label styles. font_size is a CSS-pixel ramp step; see the note above.
    nt_ui_label_style_t title;               // modal / screen title
    nt_ui_label_style_t heading;             // section heading inside a plate
    nt_ui_label_style_t label;               // body text on the panel
    nt_ui_label_style_t button_label;        // on the neutral tile button (dark ink)
    nt_ui_label_style_t button_label_action; // on go/info/danger fills (white)
    nt_ui_label_style_t hint;                // secondary line, soft on the panel
    nt_ui_label_style_t amount;              // a number the player counts
    // Rows on light tile art. Wrapping is off: a row is one fixed height, so a
    // wrapped name would climb over the line under it.
    nt_ui_label_style_t row_title;
    nt_ui_label_style_t row_sub;
} ui_theme_t;

// Builds every style above from `tokens` and binds `art`. `tokens` must outlive
// the process (a static, which is what ui_tokens_studio_default returns).
// Calling it again repaints: a game may swap themes at runtime.
void ui_theme_init(const ui_tokens_t *tokens, const ui_theme_art_t *art);

// Mutable on purpose: the engine resolves an atlas region lazily and MEMOIZES
// the index back into the style it was handed, so a const copy would re-resolve
// every frame.
extern ui_theme_t g_ui_theme;

// The tokens the frame is running on, defaulting to the studio theme before
// ui_theme_init. ui_metrics reads these.
const ui_tokens_t *ui_theme_tokens(void);

#endif /* FEATURE_UI_KIT_THEME_H */
