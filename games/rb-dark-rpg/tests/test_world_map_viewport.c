#include "ui/world_map_viewport.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void expect_near(float actual, float expected, const char *label) {
  if (fabsf(actual - expected) > 0.01F) {
    fprintf(stderr, "%s: expected %.3f, got %.3f\n", label, (double)expected,
            (double)actual);
    exit(1);
  }
}

static void test_portrait_keeps_scrollable_content(void) {
  world_map_viewport_desc_t v = world_map_viewport_compute(390.0F, 720.0F, true);
  if (!(v.content_w > v.viewport_w * 2.0F)) {
    fprintf(stderr, "portrait content should be substantially wider than viewport\n");
    exit(1);
  }
  if (!(v.content_h > v.viewport_h)) {
    fprintf(stderr, "portrait content should remain vertically pannable\n");
    exit(1);
  }
}

static void test_location_point_matches_map_y_up_contract(void) {
  world_map_point_t p =
      world_map_viewport_location_point(0.25F, 0.75F, 1000.0F, 600.0F);
  expect_near(p.x, 262.0F, "location x");
  expect_near(p.y, 155.5F, "location y");
}

static void test_center_offset_clamps_to_scroll_bounds(void) {
  world_map_point_t mid =
      world_map_viewport_center_offset(700.0F, 380.0F, 400.0F, 260.0F,
                                       1000.0F, 600.0F);
  expect_near(mid.x, -500.0F, "center offset x");
  expect_near(mid.y, -250.0F, "center offset y");

  world_map_point_t edge =
      world_map_viewport_center_offset(40.0F, 40.0F, 400.0F, 260.0F, 1000.0F,
                                       600.0F);
  expect_near(edge.x, 0.0F, "edge offset x");
  expect_near(edge.y, 0.0F, "edge offset y");
}

int main(void) {
  test_portrait_keeps_scrollable_content();
  test_location_point_matches_map_y_up_contract();
  test_center_offset_clamps_to_scroll_bounds();
  return 0;
}
