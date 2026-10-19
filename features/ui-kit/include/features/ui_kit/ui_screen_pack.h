#ifndef FEATURES_UI_KIT_UI_SCREEN_PACK_H
#define FEATURES_UI_KIT_UI_SCREEN_PACK_H

#include <stdbool.h>
#include <stdint.h>

// Text standing over the world finds a free spot on the canvas (y down)
// among a fixed list of offsets around its anchor, or does not show. A fixed
// list, not a push, so a crowd spreads into a patch instead of a column.
// Pure arithmetic, so the no-overlap rule is testable.

typedef struct ui_screen_box_t {
    float l, t, r, b;
} ui_screen_box_t;

// The box of a w x h item whose bottom centre stands at (x, y).
ui_screen_box_t ui_screen_box_at(float x, float y, float w, float h);

// True when a and b come closer than `gap` on both axes.
bool ui_screen_box_hits(const ui_screen_box_t *a, const ui_screen_box_t *b, float gap);

typedef struct ui_screen_pack_t {
    ui_screen_box_t bounds;       // a box must lie wholly inside
    const ui_screen_box_t *taken; // boxes already standing, the HUD's among them
    uint32_t taken_count;
    // A second list, for boxes only some items keep clear of (the hero's figure).
    const ui_screen_box_t *also;
    uint32_t also_count;
    float gap;
} ui_screen_pack_t;

// The first of `count` offsets (dx, dy) whose w x h box, bottom-centred at
// (x + dx, y + dy), lies inside the bounds and clear of every box of both lists; -1
// when none is free.
int32_t ui_screen_pack_first_free(const ui_screen_pack_t *pack, float x, float y, float w, float h,
    const float (*offsets)[2], uint32_t count);

#endif /* FEATURES_UI_KIT_UI_SCREEN_PACK_H */
