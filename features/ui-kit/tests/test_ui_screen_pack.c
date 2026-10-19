// Text over the world on the canvas: however many labels crowd one spot,
// every one that shows lies inside the bounds, clear of the HUD's boxes and of
// every other label, and a crowd spreads across instead of into one column.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "unity.h"

#include "features/ui_kit/ui_screen_pack.h"

void setUp(void) {}
void tearDown(void) {}

#define LABELS 48U
#define HUD_BOXES 3U

static const float k_offsets[][2] = {{0.0F, 0.0F}, {0.55F, 0.0F}, {-0.55F, 0.0F}, {0.0F, 1.0F}, {0.55F, 1.0F},
    {-0.55F, 1.0F}, {1.1F, 0.0F}, {-1.1F, 0.0F}, {0.0F, 2.0F}, {1.1F, 1.0F}, {-1.1F, 1.0F}, {0.0F, -1.6F}};
#define OFFSET_COUNT (sizeof k_offsets / sizeof k_offsets[0])

static uint32_t s_rng = 7U;
static float uniform(float lo, float hi) {
    s_rng = s_rng * 1664525U + 1013904223U;
    return lo + (hi - lo) * (float)(s_rng >> 8) / (float)(1U << 24);
}

typedef struct packed_t {
    ui_screen_box_t boxes[HUD_BOXES + LABELS];
    uint32_t count;
    uint32_t shown;
} packed_t;

static void pack_crowd(packed_t *out, float w, float h, float cx, float cy, float spread) {
    const ui_screen_box_t bounds = {8.0F, 8.0F, w - 8.0F, h - 8.0F};
    // A minimap, a skill row and an orb, as the HUD stands them.
    out->boxes[0] = (ui_screen_box_t){w - 120.0F, 10.0F, w - 10.0F, 120.0F};
    out->boxes[1] = (ui_screen_box_t){w * 0.4F, h - 90.0F, w - 10.0F, h - 10.0F};
    out->boxes[2] = (ui_screen_box_t){10.0F, h - 100.0F, 100.0F, h - 10.0F};
    out->count = HUD_BOXES;
    out->shown = 0U;
    const float gap = 2.0F;
    for (uint32_t i = 0U; i < LABELS; ++i) {
        const float lw = uniform(40.0F, 140.0F);
        const float lh = uniform(14.0F, 26.0F);
        const float x = cx + uniform(-spread, spread);
        const float y = cy + uniform(-spread, spread);
        float offsets[OFFSET_COUNT][2];
        for (uint32_t s = 0U; s < OFFSET_COUNT; ++s) {
            offsets[s][0] = k_offsets[s][0] * (lw + gap);
            offsets[s][1] = k_offsets[s][1] * (lh + gap);
        }
        const ui_screen_pack_t pack = {.bounds = bounds, .taken = out->boxes, .taken_count = out->count, .gap = gap};
        const int32_t found = ui_screen_pack_first_free(&pack, x, y, lw, lh, offsets, OFFSET_COUNT);
        if (found < 0) { continue; }
        TEST_ASSERT_TRUE(found < (int32_t)OFFSET_COUNT);
        out->boxes[out->count++] = ui_screen_box_at(x + offsets[found][0], y + offsets[found][1], lw, lh);
        out->shown += 1U;
    }
    for (uint32_t i = HUD_BOXES; i < out->count; ++i) {
        const ui_screen_box_t *b = &out->boxes[i];
        TEST_ASSERT_TRUE(b->l >= bounds.l && b->r <= bounds.r && b->t >= bounds.t && b->b <= bounds.b);
        for (uint32_t j = 0U; j < i; ++j) { TEST_ASSERT_FALSE(ui_screen_box_hits(b, &out->boxes[j], gap * 0.999F)); }
    }
}

static void test_a_crowd_never_overlaps_itself_or_the_hud(void) {
    const float canvases[][2] = {{1280.0F, 720.0F}, {390.0F, 844.0F}};
    for (uint32_t c = 0U; c < 2U; ++c) {
        const float w = canvases[c][0];
        const float h = canvases[c][1];
        // A pile on one spot, a scattered fight, and one by the skill row.
        const float scenes[][3] = {{w * 0.5F, h * 0.5F, 4.0F}, {w * 0.5F, h * 0.45F, w * 0.3F}, {w * 0.7F, h - 80.0F, 30.0F}};
        for (uint32_t s = 0U; s < 3U; ++s) {
            static packed_t packed;
            pack_crowd(&packed, w, h, scenes[s][0], scenes[s][1], scenes[s][2]);
            TEST_ASSERT_TRUE(packed.shown > 0U);
        }
    }
}

static void test_a_pile_spreads_across_not_into_a_column(void) {
    static packed_t packed;
    pack_crowd(&packed, 1280.0F, 720.0F, 640.0F, 360.0F, 2.0F);
    TEST_ASSERT_TRUE(packed.shown >= 3U);
    float min_x = packed.boxes[HUD_BOXES].l;
    float max_x = packed.boxes[HUD_BOXES].r;
    for (uint32_t i = HUD_BOXES; i < packed.count; ++i) {
        min_x = packed.boxes[i].l < min_x ? packed.boxes[i].l : min_x;
        max_x = packed.boxes[i].r > max_x ? packed.boxes[i].r : max_x;
    }
    const float widest = 140.0F;
    TEST_ASSERT_TRUE(max_x - min_x > widest);
}

static void test_no_free_spot_is_none(void) {
    const ui_screen_box_t wall = {0.0F, 0.0F, 100.0F, 100.0F};
    const ui_screen_pack_t pack = {.bounds = {0.0F, 0.0F, 100.0F, 100.0F}, .taken = &wall, .taken_count = 1U, .gap = 0.0F};
    TEST_ASSERT_EQUAL_INT32(-1, ui_screen_pack_first_free(&pack, 50.0F, 60.0F, 20.0F, 10.0F, k_offsets, OFFSET_COUNT));
    // Outside the bounds counts as taken.
    const ui_screen_pack_t open = {.bounds = {0.0F, 0.0F, 100.0F, 100.0F}, .taken = NULL, .taken_count = 0U, .gap = 0.0F};
    TEST_ASSERT_EQUAL_INT32(-1, ui_screen_pack_first_free(&open, 500.0F, 60.0F, 20.0F, 10.0F, k_offsets, 1U));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_a_crowd_never_overlaps_itself_or_the_hud);
    RUN_TEST(test_a_pile_spreads_across_not_into_a_column);
    RUN_TEST(test_no_free_spot_is_none);
    return UNITY_END();
}
