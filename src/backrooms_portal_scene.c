#include "backrooms_portal_scene.h"

#include <string.h>

void backrooms_portal_scene_clear(BackroomsPortalScene *scene) {
    if (scene == NULL) {
        return;
    }
    memset(scene, 0, sizeof(*scene));
}

static BackroomsPortalMaterial default_material(void) {
    return (BackroomsPortalMaterial){
        .wall_panel_scale = 0.42F,
        .carpet_tile_scale = 0.40F,
        .grime_strength = 0.64F,
        .wetness_strength = 0.55F,
        .fluorescent_width = 0.24F,
        .fluorescent_intensity = 1.86F,
        .corner_shadow_strength = 0.58F,
        .baseboard_strength = 0.82F,
        .trim_strength = 0.72F,
        .fixture_spacing = 3.2F,
        .ceiling_panel_scale = 0.48F,
        .shadow_spill_strength = 0.58F,
    };
}

bool backrooms_portal_scene_add_room(BackroomsPortalScene *scene, const BackroomsPortalRoom *room, const BackroomsPortalMaterial *material, uint8_t *out_index) {
    if (scene == NULL || room == NULL || scene->room_count >= BACKROOMS_PORTAL_MAX_ROOMS) {
        return false;
    }
    if (room->half_width <= 0.0F || room->half_depth <= 0.0F || room->height <= 0.0F) {
        return false;
    }
    const uint8_t index = scene->room_count++;
    scene->rooms[index] = *room;
    scene->materials[index] = material != NULL ? *material : default_material();
    if (out_index != NULL) {
        *out_index = index;
    }
    return true;
}

bool backrooms_portal_scene_add_portal(BackroomsPortalScene *scene, const BackroomsPortal *portal) {
    if (scene == NULL || portal == NULL || scene->portal_count >= BACKROOMS_PORTAL_MAX_PORTALS) {
        return false;
    }
    if (portal->from_room >= scene->room_count || portal->to_room >= scene->room_count) {
        return false;
    }
    if (portal->half_z <= 0.0F || portal->max_y <= portal->min_y || portal->inner_scale_z <= 0.0F) {
        return false;
    }
    scene->portals[scene->portal_count++] = *portal;
    return true;
}

bool backrooms_portal_scene_validate(const BackroomsPortalScene *scene) {
    if (scene == NULL || scene->room_count == 0U) {
        return false;
    }
    for (uint8_t i = 0; i < scene->portal_count; ++i) {
        const BackroomsPortal *portal = &scene->portals[i];
        if (portal->from_room >= scene->room_count || portal->to_room >= scene->room_count) {
            return false;
        }
        if (portal->half_z <= 0.0F || portal->max_y <= portal->min_y || portal->inner_scale_z <= 0.0F) {
            return false;
        }
    }
    return true;
}

void backrooms_portal_scene_build_t0010(BackroomsPortalScene *scene) {
    backrooms_portal_scene_clear(scene);

    uint8_t corridor = 0;
    uint8_t impossible_room = 0;
    (void)backrooms_portal_scene_add_room(scene,
                                          &(BackroomsPortalRoom){
                                              .half_width = 4.25F,
                                              .half_depth = 1.85F,
                                              .height = 2.55F,
                                              .light_strength = 1.0F,
                                              .material_id = 1U,
                                          },
                                          &(BackroomsPortalMaterial){
                                              .wall_panel_scale = 0.45F,
                                              .carpet_tile_scale = 0.42F,
                                              .grime_strength = 0.72F,
                                              .wetness_strength = 0.35F,
                                              .fluorescent_width = 0.30F,
                                              .fluorescent_intensity = 1.72F,
                                              .corner_shadow_strength = 0.56F,
                                              .baseboard_strength = 0.90F,
                                              .trim_strength = 0.68F,
                                              .fixture_spacing = 3.8F,
                                              .ceiling_panel_scale = 0.50F,
                                              .shadow_spill_strength = 0.54F,
                                          },
                                          &corridor);
    (void)backrooms_portal_scene_add_room(scene,
                                          &(BackroomsPortalRoom){
                                              .half_width = 7.55F,
                                              .half_depth = 11.8F,
                                              .height = 2.95F,
                                              .light_strength = 1.22F,
                                              .material_id = 1U,
                                          },
                                          &(BackroomsPortalMaterial){
                                              .wall_panel_scale = 0.36F,
                                              .carpet_tile_scale = 0.52F,
                                              .grime_strength = 0.86F,
                                              .wetness_strength = 0.72F,
                                              .fluorescent_width = 0.20F,
                                              .fluorescent_intensity = 2.12F,
                                              .corner_shadow_strength = 0.74F,
                                              .baseboard_strength = 0.96F,
                                              .trim_strength = 0.92F,
                                              .fixture_spacing = 2.9F,
                                              .ceiling_panel_scale = 0.66F,
                                              .shadow_spill_strength = 0.82F,
                                          },
                                          &impossible_room);
    (void)backrooms_portal_scene_add_portal(scene,
                                            &(BackroomsPortal){
                                                .from_room = corridor,
                                                .to_room = impossible_room,
                                                .flags = BACKROOMS_PORTAL_VISIBLE_WHEN_MARKED | BACKROOMS_PORTAL_COPIES_MARKS | BACKROOMS_PORTAL_HAS_NESTED_APERTURE,
                                                .wall_x = 3.86F,
                                                .center_z = 10.8F,
                                                .half_z = 1.10F,
                                                .min_y = 0.30F,
                                                .max_y = 1.78F,
                                                .inner_scale_z = 3.35F,
                                                .nested_half_z = 1.42F,
                                            });
}

BackroomsPortalGpuParams backrooms_portal_scene_gpu_params(const BackroomsPortalScene *scene, uint8_t portal_index, bool visible) {
    BackroomsPortalGpuParams params = {
        .entry = {3.86F, 10.8F, 1.10F, visible ? 1.0F : 0.0F},
        .shape = {11.8F, 7.55F, 2.95F, 1.0F},
        .style = {3.35F, 1.42F, 1.22F, 1.0F},
        .bounds = {0.30F, 1.78F, 3.36F, 4.12F},
        .material = {0.36F, 0.52F, 0.86F, 0.72F},
        .light = {0.20F, 2.12F, 0.74F, 0.96F},
        .finish = {0.92F, 2.9F, 0.66F, 0.82F},
    };
    if (scene == NULL || portal_index >= scene->portal_count) {
        return params;
    }

    const BackroomsPortal *portal = &scene->portals[portal_index];
    const BackroomsPortalRoom *target = &scene->rooms[portal->to_room];
    const BackroomsPortalMaterial *material = &scene->materials[portal->to_room];
    params.entry[0] = portal->wall_x;
    params.entry[1] = portal->center_z;
    params.entry[2] = portal->half_z;
    params.entry[3] = visible ? 1.0F : 0.0F;
    params.shape[0] = target->half_depth;
    params.shape[1] = target->half_width;
    params.shape[2] = target->height;
    params.shape[3] = (portal->flags & BACKROOMS_PORTAL_HAS_NESTED_APERTURE) != 0U ? 1.0F : 0.0F;
    params.style[0] = portal->inner_scale_z;
    params.style[1] = portal->nested_half_z;
    params.style[2] = target->light_strength;
    params.style[3] = (portal->flags & BACKROOMS_PORTAL_COPIES_MARKS) != 0U ? 1.0F : 0.0F;
    params.bounds[0] = portal->min_y;
    params.bounds[1] = portal->max_y;
    params.bounds[2] = portal->wall_x - 0.50F;
    params.bounds[3] = portal->wall_x + 0.26F;
    params.material[0] = material->wall_panel_scale;
    params.material[1] = material->carpet_tile_scale;
    params.material[2] = material->grime_strength;
    params.material[3] = material->wetness_strength;
    params.light[0] = material->fluorescent_width;
    params.light[1] = material->fluorescent_intensity;
    params.light[2] = material->corner_shadow_strength;
    params.light[3] = material->baseboard_strength;
    params.finish[0] = material->trim_strength;
    params.finish[1] = material->fixture_spacing;
    params.finish[2] = material->ceiling_panel_scale;
    params.finish[3] = material->shadow_spill_strength;
    return params;
}
