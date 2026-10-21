#include "features/ui_kit/ui_shape_pack.h"

#include "unity.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* The shader's own unpacking, in C: what ui_uber.frag reads back. Unity is
   built with UNITY_EXCLUDE_FLOAT, so floats are compared as exact integers. */
static uint32_t lo12(float v) { return (uint32_t)(v - floorf(v / 4096.0F) * 4096.0F); }
static uint32_t hi12(float v) { return (uint32_t)floorf(v / 4096.0F); }
static uint32_t byte_at(float v, int i) {
    const uint32_t n = (uint32_t)v;
    return (n >> (8 * i)) & 0xFFU;
}

static void assert_exact_integer_below_2_24(float v) {
    TEST_ASSERT_TRUE(v >= 0.0F && v < 16777216.0F);
    TEST_ASSERT_TRUE(floorf(v) == v);
}

static void test_panel_fields_round_trip(void) {
    const ui_kit_panel_style_t style = {.radius = {14.0F, 12.5F, 0.25F, 1023.75F}, .top_rgb = 0xA6FA55U,
        .bottom_rgb = 0x43BF1EU, .outline_w = 4.0F, .outline_rgb = 0x040D03U, .face_radius = 8.0F, .lip_h = 6.0F,
        .lip_rgb = 0x111118U, .press = 1.0F, .highlight_w = 2.5F, .highlight_a = 0.5F, .gloss_h = 12.0F,
        .gloss_inset = 8.0F, .gloss_top = 3.0F, .gloss_cap = 8.0F, .gloss_a = 0.35F, .checker_px = 7.0F,
        .checker_a = 0.14F, .overlay = true, .alpha = 1.0F};
    ui_kit_shape_block_t b;
    ui_kit_pack_panel(&style, &b);
    for (int i = 0; i < 4; ++i) {
        assert_exact_integer_below_2_24(b.shape[i]);
        assert_exact_integer_below_2_24(b.paint[i]);
        assert_exact_integer_below_2_24(b.fx[i]);
    }
    TEST_ASSERT_EQUAL_UINT32(UI_KIT_SHAPE_MODE_PANEL, hi12(b.shape[0]));
    TEST_ASSERT_EQUAL_UINT32(16U, lo12(b.shape[0]));
    TEST_ASSERT_EQUAL_UINT32(56U, lo12(b.shape[1]));
    TEST_ASSERT_EQUAL_UINT32(50U, hi12(b.shape[1]));
    TEST_ASSERT_EQUAL_UINT32(1U, lo12(b.shape[2]));
    TEST_ASSERT_EQUAL_UINT32(4095U, hi12(b.shape[2]));
    TEST_ASSERT_EQUAL_UINT32(32U, lo12(b.shape[3]));
    TEST_ASSERT_EQUAL_UINT32(24U, hi12(b.shape[3]));
    TEST_ASSERT_EQUAL_UINT32(0x43U, byte_at(b.paint[0], 0));
    TEST_ASSERT_EQUAL_UINT32(0xBFU, byte_at(b.paint[0], 1));
    TEST_ASSERT_EQUAL_UINT32(0x1EU, byte_at(b.paint[0], 2));
    TEST_ASSERT_EQUAL_UINT32(0x18U, byte_at(b.paint[2], 2));
    TEST_ASSERT_EQUAL_UINT32(89U, byte_at(b.paint[3], 0));
    TEST_ASSERT_EQUAL_UINT32(36U, byte_at(b.paint[3], 1));
    TEST_ASSERT_EQUAL_UINT32(128U, byte_at(b.paint[3], 2));
    TEST_ASSERT_EQUAL_UINT32(32U, lo12(b.fx[0]));
    TEST_ASSERT_EQUAL_UINT32(12U, hi12(b.fx[0]));
    TEST_ASSERT_EQUAL_UINT32(48U, lo12(b.fx[1]));
    TEST_ASSERT_EQUAL_UINT32(32U, hi12(b.fx[1]));
    TEST_ASSERT_EQUAL_UINT32(28U, lo12(b.fx[2]));
    TEST_ASSERT_EQUAL_UINT32(10U, hi12(b.fx[2]));
    TEST_ASSERT_EQUAL_UINT32(255U, lo12(b.fx[3]));
    TEST_ASSERT_EQUAL_UINT32(1U, hi12(b.fx[3]));
}

/* Out-of-range lengths clamp instead of spilling into the neighbouring field. */
static void test_lengths_clamp_to_their_field(void) {
    const ui_kit_panel_style_t style = {.radius = {5000.0F, -3.0F, 0.0F, 0.0F}, .outline_w = 2000.0F, .alpha = 1.0F};
    ui_kit_shape_block_t b;
    ui_kit_pack_panel(&style, &b);
    TEST_ASSERT_EQUAL_UINT32(UI_KIT_SHAPE_MODE_PANEL, hi12(b.shape[0]));
    TEST_ASSERT_EQUAL_UINT32(4095U, lo12(b.shape[0]));
    TEST_ASSERT_EQUAL_UINT32(4095U, lo12(b.shape[1]));
    TEST_ASSERT_EQUAL_UINT32(0U, hi12(b.shape[1]));
}

static void test_radial_carries_angles_verbatim(void) {
    const ui_kit_radial_style_t style = {.angle_start = -1.25F, .angle_end = 4.5F, .inner_norm = 0.8F,
        .top_rgb = 0xFFFFFFU, .bottom_rgb = 0x0A84FFU, .outline_w = 4.0F, .lip_h = 3.5F, .gloss_a = 0.35F,
        .alpha = 1.0F};
    ui_kit_shape_block_t b;
    ui_kit_pack_radial(&style, &b);
    TEST_ASSERT_EQUAL_UINT32(UI_KIT_SHAPE_MODE_RADIAL, hi12(b.shape[0]));
    TEST_ASSERT_EQUAL_UINT32(16U, lo12(b.shape[0]));
    TEST_ASSERT_TRUE(b.shape[1] == -1.25F && b.shape[2] == 4.5F && b.shape[3] == 0.8F);
    TEST_ASSERT_EQUAL_UINT32(0xFFU, byte_at(b.paint[0], 2));
    TEST_ASSERT_EQUAL_UINT32(14U, lo12(b.fx[0]));
    TEST_ASSERT_EQUAL_UINT32(89U, byte_at(b.paint[3], 0));
}

/* The material's attr defaults are all zero, and zero is mode 0: a sprite. */
static void test_zero_style_is_not_a_sprite(void) {
    const ui_kit_panel_style_t style = {0};
    ui_kit_shape_block_t b;
    ui_kit_pack_panel(&style, &b);
    TEST_ASSERT_EQUAL_UINT32(UI_KIT_SHAPE_MODE_PANEL, hi12(b.shape[0]));
    TEST_ASSERT_EQUAL_HEX32(0x00563412U, ui_kit_shape_tint(0x123456U, 0.0F));
    TEST_ASSERT_EQUAL_HEX32(0xFF563412U, ui_kit_shape_tint(0x123456U, 1.0F));
}

/* The flags share one 12-bit field: overlay is bit 0, the inside lip bit 1. */
static void test_flags_pack_as_bits(void) {
    ui_kit_shape_block_t b;
    ui_kit_pack_panel(&(ui_kit_panel_style_t){.lip_inside = true, .alpha = 1.0F}, &b);
    TEST_ASSERT_EQUAL_UINT32(2U, hi12(b.fx[3]));
    ui_kit_pack_panel(&(ui_kit_panel_style_t){.lip_inside = true, .overlay = true, .alpha = 1.0F}, &b);
    TEST_ASSERT_EQUAL_UINT32(3U, hi12(b.fx[3]));
}

/* The engine reads an all-zero tint as "untinted" and draws it opaque white,
   so black faded to nothing must still pack to a non-zero, fully clear tint. */
static void test_clear_black_is_not_untinted(void) {
    const uint32_t tint = ui_kit_shape_tint(0x000000U, 0.0F);
    TEST_ASSERT_NOT_EQUAL_HEX32(0U, tint);
    TEST_ASSERT_EQUAL_UINT32(0U, tint >> 24U);
}

static bool same_look(const ui_kit_panel_style_t *a, const ui_kit_panel_style_t *b) {
    ui_kit_shape_block_t pa;
    ui_kit_shape_block_t pb;
    ui_kit_pack_panel(a, &pa);
    ui_kit_pack_panel(b, &pb);
    return memcmp(&pa, &pb, sizeof pa) == 0 && a->top_rgb == b->top_rgb && a->alpha == b->alpha;
}

/* A press has to show on every look a game can hand the button, and a
   disabled one is the off fill whatever colour it started in. */
static void test_look_states(void) {
    const uint32_t off = 0x8A8F96U;
    const ui_kit_panel_style_t looks[] = {
        {.radius = {12, 12, 12, 12}, .top_rgb = 0x66CC33U, .bottom_rgb = 0x449922U, .lip_h = 6.0F, .lip_rgb = 0x225511U,
         .alpha = 1.0F},
        {.radius = {16, 16, 16, 16}, .top_rgb = 0xFFFFFFU, .bottom_rgb = 0xFFFFFFU, .lip_h = 4.0F, .lip_inside = true,
         .outline_w = 3.0F, .alpha = 1.0F},
        {.radius = {8, 8, 8, 8}, .top_rgb = 0x3377FFU, .bottom_rgb = 0x3377FFU, .gloss_h = 6.0F, .gloss_a = 0.3F,
         .alpha = 1.0F},
    };
    for (size_t i = 0; i < sizeof looks / sizeof looks[0]; ++i) {
        const ui_kit_panel_style_t idle = ui_kit_look_state(&looks[i], UI_KIT_LOOK_IDLE, off);
        const ui_kit_panel_style_t hover = ui_kit_look_state(&looks[i], UI_KIT_LOOK_HOVER, off);
        const ui_kit_panel_style_t pressed = ui_kit_look_state(&looks[i], UI_KIT_LOOK_PRESSED, off);
        const ui_kit_panel_style_t disabled = ui_kit_look_state(&looks[i], UI_KIT_LOOK_DISABLED, off);
        TEST_ASSERT_TRUE(same_look(&idle, &looks[i]));
        TEST_ASSERT_TRUE(same_look(&hover, &idle));
        TEST_ASSERT_FALSE(same_look(&pressed, &idle));
        TEST_ASSERT_EQUAL_HEX32(off, disabled.top_rgb);
        TEST_ASSERT_EQUAL_HEX32(off, disabled.bottom_rgb);
        TEST_ASSERT_TRUE(disabled.press == 0.0F);
    }
    /* An outside lip presses as one shape: the face sinks, the colours stay. */
    const ui_kit_panel_style_t sunk = ui_kit_look_state(&looks[0], UI_KIT_LOOK_PRESSED, off);
    TEST_ASSERT_TRUE(sunk.press == 1.0F);
    TEST_ASSERT_EQUAL_HEX32(looks[0].bottom_rgb, sunk.bottom_rgb);
}

/* Content that sits inside the hysteresis band keeps whatever it had, so a
   body near the limit never flips its scissor from frame to frame. */
static void test_scroll_hysteresis(void) {
    const float room = 300.0F;
    const float band = 20.0F;
    TEST_ASSERT_FALSE(ui_kit_scroll_engaged(false, 299.0F, room, band));
    TEST_ASSERT_TRUE(ui_kit_scroll_engaged(false, 301.0F, room, band));
    TEST_ASSERT_TRUE(ui_kit_scroll_engaged(true, 290.0F, room, band));
    TEST_ASSERT_FALSE(ui_kit_scroll_engaged(true, 279.0F, room, band));
    bool on = false;
    for (int frame = 0; frame < 16; ++frame) {
        const float content = (frame % 2 == 0) ? 305.0F : 288.0F;
        const bool next = ui_kit_scroll_engaged(on, content, room, band);
        if (frame > 0) { TEST_ASSERT_TRUE(next); }
        on = next;
    }
}

/* Every cell keeps a positive width, the row spans the whole width, and no
   cell's padded touch zone reaches its neighbour's. */
static void test_choice_cells_do_not_overlap(void) {
    const float gaps[] = {0.0F, 3.0F, 8.0F, 15.0F, 40.0F};
    for (int count = 1; count <= 6; ++count) {
        for (size_t g = 0; g < sizeof gaps / sizeof gaps[0]; ++g) {
            ui_kit_cell_t cells[6];
            const float width = 360.0F;
            const float pad = ui_kit_choice_cells(count, width, gaps[g], 8.0F, cells);
            TEST_ASSERT_TRUE(pad >= 0.0F && pad <= 8.0F);
            for (int i = 0; i < count; ++i) {
                TEST_ASSERT_TRUE(cells[i].w > 0.0F);
                if (i + 1 < count) {
                    TEST_ASSERT_TRUE(cells[i].x + cells[i].w + pad <= cells[i + 1].x - pad + 0.001F);
                }
            }
            TEST_ASSERT_TRUE(fabsf(cells[count - 1].x + cells[count - 1].w - width) < 0.01F);
        }
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_panel_fields_round_trip);
    RUN_TEST(test_lengths_clamp_to_their_field);
    RUN_TEST(test_radial_carries_angles_verbatim);
    RUN_TEST(test_zero_style_is_not_a_sprite);
    RUN_TEST(test_clear_black_is_not_untinted);
    RUN_TEST(test_flags_pack_as_bits);
    RUN_TEST(test_look_states);
    RUN_TEST(test_scroll_hysteresis);
    RUN_TEST(test_choice_cells_do_not_overlap);
    return UNITY_END();
}
