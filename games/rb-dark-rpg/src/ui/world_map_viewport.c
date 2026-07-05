#include "ui/world_map_viewport.h"

static float wm_clamp(float value, float lo, float hi) {
  if (value < lo) {
    return lo;
  }
  if (value > hi) {
    return hi;
  }
  return value;
}

world_map_viewport_desc_t world_map_viewport_compute(float panel_w,
                                                     float panel_h,
                                                     bool portrait) {
  const float viewport_w =
      wm_clamp(panel_w - (portrait ? 24.0F : 28.0F), 280.0F,
               portrait ? 420.0F : 1120.0F);
  const float viewport_h =
      wm_clamp(panel_h - (portrait ? 122.0F : 116.0F), portrait ? 350.0F : 320.0F,
               portrait ? 560.0F : 620.0F);
  float content_w = portrait ? 960.0F : 1280.0F;
  const float min_content_w = viewport_w * (portrait ? 2.15F : 1.35F);
  if (content_w < min_content_w) {
    content_w = min_content_w;
  }
  float content_h = content_w * 0.5625F;
  const float min_content_h = viewport_h * 1.12F;
  if (content_h < min_content_h) {
    content_h = min_content_h;
    content_w = content_h / 0.5625F;
  }
  return (world_map_viewport_desc_t){
      .viewport_w = viewport_w,
      .viewport_h = viewport_h,
      .content_w = content_w,
      .content_h = content_h,
  };
}

world_map_point_t world_map_viewport_location_point(float norm_x, float norm_y,
                                                    float content_w,
                                                    float content_h) {
  const float x = 24.0F + wm_clamp(norm_x, 0.0F, 1.0F) * (content_w - 48.0F);
  const float ui_y = 1.0F - wm_clamp(norm_y, 0.0F, 1.0F);
  const float y = 24.0F + ui_y * (content_h - 74.0F);
  return (world_map_point_t){.x = x, .y = y};
}

world_map_point_t world_map_viewport_center_offset(float point_x, float point_y,
                                                   float viewport_w,
                                                   float viewport_h,
                                                   float content_w,
                                                   float content_h) {
  float x = viewport_w * 0.5F - point_x;
  float y = viewport_h * 0.5F - point_y;
  if (content_w <= viewport_w) {
    x = (viewport_w - content_w) * 0.5F;
  } else {
    x = wm_clamp(x, viewport_w - content_w, 0.0F);
  }
  if (content_h <= viewport_h) {
    y = (viewport_h - content_h) * 0.5F;
  } else {
    y = wm_clamp(y, viewport_h - content_h, 0.0F);
  }
  return (world_map_point_t){.x = x, .y = y};
}
