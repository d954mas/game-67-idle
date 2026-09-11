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

    .ref_short = 390.0F,

    /* The art ships at 4 source pixels per design unit (art.export_scale). */
    .slice9_scale = 0.25F,

    .on_action = 0xFFFFFFFFU,
};

/* Mirrors tokens/studio_b.json. Studio B is opt-in: existing consumers keep
   STUDIO_DEFAULT until their game-owned theme binding selects this sheet. */
static const ui_tokens_t STUDIO_B = {
    .shell = 0xFF382517U,
    .panel = 0xFFF9FDFFU,
    .inset = 0xFFF5EEE8U,
    .inset_rim = 0xFF756152U,
    .scrim = 0x8C382517U,

    .tile = 0xFFF9FDFFU,
    .tile_dim = 0xFFF5EEE8U,
    .tile_rim = 0xFF756152U,

    .ink = 0xFF382517U,
    .ink_soft = 0xFF756152U,
    .on_panel = 0xFF382517U,
    .on_panel_soft = 0xFF756152U,

    .coin = 0xFF32BFF5U,
    .go = 0xFF559D1FU,
    .info = 0xFFA47B08U,
    .danger = 0xFF1823B4U,
    .off = 0xFFA49387U,

    .t_display = 32.0F,
    .t_title = 24.0F,
    .t_body = 20.0F,
    .t_num = 21.0F,
    .t_badge = 16.0F,
    .t_row = 18.0F,
    .t_row_sub = 14.0F,

    .rim = 3.0F,
    .lift = 4.0F,
    .gap = 12.0F,
    .pad = 20.0F,
    .hit = 44.0F,

    .panel_min_w = 260.0F,
    .panel_max_w = 560.0F,

    .ref_short = 390.0F,
    .slice9_scale = 0.25F,

    .on_action = 0xFFFFFFFFU,
};

const ui_tokens_t *ui_tokens_studio_default(void) { return &STUDIO_DEFAULT; }
const ui_tokens_t *ui_tokens_studio_b(void) { return &STUDIO_B; }
