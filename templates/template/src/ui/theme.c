#include "ui/theme.h"

#include "features/ui_kit/ui_tokens.h"
#include "generated/game_assets.h"

/* The studio defaults already carry the pair this game lays out against: 400
   authored units across the short edge when the screen is in a hand, 720 when it
   is across a desk. Both are shares, not minimum sizes -- the interface keeps
   its proportion of any window -- and a game that wants to be read closer moves
   its own number here. */
static ui_tokens_t s_tokens;

void theme_init(nt_resource_t atlas) {
    // Region refs resolve lazily on first emit, once the atlas resource is
    // ready, so this may run before the pack has landed.
    const ui_theme_art_t art = {
        .panel = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_PANEL.value),
        .button = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_BUTTON.value),
        .tile = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_TILE.value),
        .slider_track = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_SLIDER_TRACK.value),
        .slider_fill = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_SLIDER_FILL.value),
        .slider_track_sm = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_SLIDER_TRACK_SM.value),
        .slider_fill_sm = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_SLIDER_FILL_SM.value),
        .thumb = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_SLIDER_THUMB.value),
        .icon_play = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_ICON_PLAY.value),
    };
    s_tokens = *ui_tokens_studio_default();
    ui_theme_init(&s_tokens, &art);
}
