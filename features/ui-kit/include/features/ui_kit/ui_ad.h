#ifndef FEATURE_UI_KIT_AD_H
#define FEATURE_UI_KIT_AD_H

// Rewarded-ad buttons, ui_ad_policy.h's rule as widgets: the ad button is blue
// and carries the video icon, and the standard button shows with it, equal or
// larger, beside it or above it. Whether a screen offers a standard button at
// all is the game's decision; a screen that shows the ad button alone puts its
// own larger standard button above it.

#include "clay.h"
#include "features/ui_kit/ui_ad_policy.h"
#include "features/ui_kit/ui_kit.h"
#include "ui/nt_ui.h"
#include "ui/nt_ui_label.h"

#include <stdbool.h>

// Copy, styles and art are the consumer's.
typedef struct ui_ad_look_t {
    ui_kit_button_look_t button;  // its face must be blue (asserted)
    nt_atlas_region_ref_t *video; // the video icon the portal requires
    const nt_ui_label_style_t *reward_style;
    const char *tag; // the small "Ad"; NULL = none
    const nt_ui_label_style_t *tag_style;
    float video_size, reward_icon_size, gap;
} ui_ad_look_t;

// The ad button `width` x `height` named `id`: the video icon, `reward_icon`
// (NULL = none), `reward` and the tag. True when pressed. The look's lift stands
// inside `height`.
bool ui_kit_ad_button(nt_ui_context_t *ctx, const char *id, float width, float height, const ui_ad_look_t *look,
    nt_atlas_region_ref_t *reward_icon, const char *reward);

typedef enum ui_ad_pick_t { UI_AD_NONE, UI_AD_STANDARD, UI_AD_VIDEO } ui_ad_pick_t;

typedef struct ui_ad_choice_t {
    const ui_ad_look_t *ad;
    nt_atlas_region_ref_t *reward_icon; // NULL = none
    const char *reward;
    bool video; // a video can be asked for; false = the standard button alone, the row's width
    const ui_kit_button_look_t *standard_look; // never blue (asserted)
    const char *standard;
    nt_ui_label_style_t standard_style; // shrunk to fit, never below `standard_min_size`
    float standard_min_size;
    float pad;       // the standard label's padding each side
    float fit_inset; // what the fit keeps clear of the button's width
    float gap;       // between the two buttons
    float min_w;     // the least half-width that stays a row
} ui_ad_choice_t;

// One row `width` wide of `height` tall buttons, named `id`. The buttons take
// their ids from it with nt_ui_child_id ("standard", "ad"), so they keep no
// strings: a DevAPI bot finds them by box under the named row, not by name.
// Returns what was pressed.
ui_ad_pick_t ui_kit_ad_choice(nt_ui_context_t *ctx, const char *id, float width, float height, const ui_ad_choice_t *choice);

#endif /* FEATURE_UI_KIT_AD_H */
