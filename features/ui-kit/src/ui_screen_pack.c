#include "features/ui_kit/ui_screen_pack.h"

ui_screen_box_t ui_screen_box_at(float x, float y, float w, float h) {
    return (ui_screen_box_t){.l = x - w * 0.5F, .t = y - h, .r = x + w * 0.5F, .b = y};
}

bool ui_screen_box_hits(const ui_screen_box_t *a, const ui_screen_box_t *b, float gap) {
    return a->l < b->r + gap && b->l < a->r + gap && a->t < b->b + gap && b->t < a->b + gap;
}

int32_t ui_screen_pack_first_free(const ui_screen_pack_t *pack, float x, float y, float w, float h,
    const float (*offsets)[2], uint32_t count) {
    for (uint32_t s = 0U; s < count; ++s) {
        const ui_screen_box_t box = ui_screen_box_at(x + offsets[s][0], y + offsets[s][1], w, h);
        if (box.l < pack->bounds.l || box.r > pack->bounds.r || box.t < pack->bounds.t || box.b > pack->bounds.b) {
            continue;
        }
        bool clear = true;
        for (uint32_t k = 0U; k < pack->taken_count && clear; ++k) {
            clear = !ui_screen_box_hits(&box, &pack->taken[k], pack->gap);
        }
        for (uint32_t k = 0U; k < pack->also_count && clear; ++k) {
            clear = !ui_screen_box_hits(&box, &pack->also[k], pack->gap);
        }
        if (clear) { return (int32_t)s; }
    }
    return -1;
}
