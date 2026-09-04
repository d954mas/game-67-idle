#include "features/ui_kit/ui_tokens.h"

/* Mirrors tokens/studio_default.json. The JSON is the sheet a human reads and
   the art generator parses; this is the same sheet compiled in, so the runtime
   needs no parser and no file at startup. tests/tokens_parity.test.mjs fails if
   the two ever disagree. */
static const ui_tokens_t STUDIO_DEFAULT = {
    .shell = 0xFF99323EU,
    .panel = 0xFFE75C6BU,
    .inset = 0xFFC44655U,
    .inset_rim = 0xFFA83543U,
    .scrim = 0x8C3C1016U,

    .tile = 0xFFFAF0EFU,
    .tile_dim = 0xFFEAD3CFU,
    .tile_rim = 0xFFC9ABA6U,

    .ink = 0xFF50252AU,
    .ink_soft = 0xFF8C6F6BU,
    .on_panel = 0xFFFFFFFFU,
    .on_panel_soft = 0xFFFFC8CFU,

    .coin = 0xFF4DC9FFU,
    .go = 0xFF6FC435U,
    .info = 0xFFF77B4AU,
    .danger = 0xFF6B6BFFU,
    .off = 0xFFAB908BU,

    .t_display = 32.0F,
    .t_title = 24.0F,
    .t_body = 20.0F,
    .t_num = 21.0F,
    .t_badge = 16.0F,
    .t_row = 18.0F,
    .t_row_sub = 14.0F,

    .rim = 3.0F,
    .lift = 5.0F,
    .gap = 12.0F,
    .pad = 20.0F,
    .hit = 44.0F,

    .panel_min_w = 260.0F,
    .panel_max_w = 560.0F,

    .ref_short = 720.0F,
    .short_edge_max = 480.0F,

    /* The art ships at 4 source pixels per design unit (art.export_scale). */
    .slice9_scale = 0.25F,
};

const ui_tokens_t *ui_tokens_studio_default(void) { return &STUDIO_DEFAULT; }
