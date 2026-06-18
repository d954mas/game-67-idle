#ifndef BACKROOMS_PORTAL_SCENE_H
#define BACKROOMS_PORTAL_SCENE_H

#include <stdbool.h>
#include <stdint.h>

#define BACKROOMS_PORTAL_MAX_ROOMS 16
#define BACKROOMS_PORTAL_MAX_PORTALS 32

typedef enum BackroomsPortalFlags {
    BACKROOMS_PORTAL_NONE = 0,
    BACKROOMS_PORTAL_VISIBLE_WHEN_MARKED = 1 << 0,
    BACKROOMS_PORTAL_COPIES_MARKS = 1 << 1,
    BACKROOMS_PORTAL_HAS_NESTED_APERTURE = 1 << 2,
} BackroomsPortalFlags;

typedef struct BackroomsPortalRoom {
    float half_width;
    float half_depth;
    float height;
    float light_strength;
    uint32_t material_id;
} BackroomsPortalRoom;

typedef struct BackroomsPortalMaterial {
    float wall_panel_scale;
    float carpet_tile_scale;
    float grime_strength;
    float wetness_strength;
    float fluorescent_width;
    float fluorescent_intensity;
    float corner_shadow_strength;
    float baseboard_strength;
    float trim_strength;
    float fixture_spacing;
    float ceiling_panel_scale;
    float shadow_spill_strength;
} BackroomsPortalMaterial;

typedef struct BackroomsPortalConstruction {
    float jamb_depth;
    float threshold_lip;
    float conduit_strength;
    float landmark_column_strength;
} BackroomsPortalConstruction;

typedef struct BackroomsPortal {
    uint8_t from_room;
    uint8_t to_room;
    uint32_t flags;
    float wall_x;
    float center_z;
    float half_z;
    float min_y;
    float max_y;
    float inner_scale_z;
    float nested_half_z;
} BackroomsPortal;

typedef struct BackroomsPortalScene {
    BackroomsPortalRoom rooms[BACKROOMS_PORTAL_MAX_ROOMS];
    BackroomsPortalMaterial materials[BACKROOMS_PORTAL_MAX_ROOMS];
    BackroomsPortalConstruction construction[BACKROOMS_PORTAL_MAX_ROOMS];
    BackroomsPortal portals[BACKROOMS_PORTAL_MAX_PORTALS];
    uint8_t room_count;
    uint8_t portal_count;
} BackroomsPortalScene;

typedef struct BackroomsPortalGpuParams {
    float entry[4]; /* wall_x, center_z, half_z, enabled */
    float shape[4]; /* inner_depth, inner_half_width, inner_height, nested_enabled */
    float style[4]; /* inner_scale_z, nested_half_z, light_strength, copies_marks */
    float bounds[4]; /* min_y, max_y, outer_wall_start_x, outer_wall_end_x */
    float material[4]; /* wall_panel_scale, carpet_tile_scale, grime_strength, wetness_strength */
    float light[4]; /* fluorescent_width, fluorescent_intensity, corner_shadow_strength, baseboard_strength */
    float finish[4]; /* trim_strength, fixture_spacing, ceiling_panel_scale, shadow_spill_strength */
    float construction[4]; /* jamb_depth, threshold_lip, conduit_strength, landmark_column_strength */
} BackroomsPortalGpuParams;

void backrooms_portal_scene_clear(BackroomsPortalScene *scene);
bool backrooms_portal_scene_add_room(BackroomsPortalScene *scene,
                                     const BackroomsPortalRoom *room,
                                     const BackroomsPortalMaterial *material,
                                     const BackroomsPortalConstruction *construction,
                                     uint8_t *out_index);
bool backrooms_portal_scene_add_portal(BackroomsPortalScene *scene, const BackroomsPortal *portal);
bool backrooms_portal_scene_validate(const BackroomsPortalScene *scene);
void backrooms_portal_scene_build_t0010(BackroomsPortalScene *scene);
BackroomsPortalGpuParams backrooms_portal_scene_gpu_params(const BackroomsPortalScene *scene, uint8_t portal_index, bool visible);

#endif
