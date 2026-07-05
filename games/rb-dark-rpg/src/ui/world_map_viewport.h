#ifndef RB_DARK_RPG_WORLD_MAP_VIEWPORT_H
#define RB_DARK_RPG_WORLD_MAP_VIEWPORT_H

#include <stdbool.h>

typedef struct world_map_viewport_desc_t {
  float viewport_w;
  float viewport_h;
  float content_w;
  float content_h;
} world_map_viewport_desc_t;

typedef struct world_map_point_t {
  float x;
  float y;
} world_map_point_t;

world_map_viewport_desc_t world_map_viewport_compute(float panel_w,
                                                     float panel_h,
                                                     bool portrait);
world_map_point_t world_map_viewport_location_point(float norm_x, float norm_y,
                                                    float content_w,
                                                    float content_h);
world_map_point_t world_map_viewport_center_offset(float point_x, float point_y,
                                                   float viewport_w,
                                                   float viewport_h,
                                                   float content_w,
                                                   float content_h);

#endif /* RB_DARK_RPG_WORLD_MAP_VIEWPORT_H */
