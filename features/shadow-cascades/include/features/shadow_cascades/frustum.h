#ifndef FEATURE_SHADOW_CASCADES_FRUSTUM_H
#define FEATURE_SHADOW_CASCADES_FRUSTUM_H

#include <stdbool.h>
#include <stdint.h>

// The six clip planes of a view-projection (column-major, GL clip space), each
// normalized so a plane test reads in world metres; inside is positive.
// Planes in order: left, right, bottom, top, near, far.
typedef struct frustum_t {
    float plane[6][4];
} frustum_t;

#define SHADOW_VOLUME_MAX_PLANES 14U
typedef struct shadow_volume_t {
    float plane[SHADOW_VOLUME_MAX_PLANES][4];
    uint32_t plane_count;
} shadow_volume_t;

void frustum_from_view_proj(frustum_t *out, const float view_proj[16]);

// The same planes for a light's depth pass, without the near one: a caster
// between the light and its depth window still casts, pancaked onto the near
// plane by the depth shaders, so only the sides and the far plane drop it.
void frustum_from_light_view_proj(frustum_t *out, const float view_proj[16]);

// False only when the box lies wholly outside one plane by more than slack
// metres. Everything such a box holds is clipped anyway, so dropping it leaves
// the frame unchanged.
bool frustum_sees_box(const frustum_t *frustum, const float min[3], const float max[3], float slack);

// The world box around a local box under an affine world matrix (column-major).
void frustum_box_to_world(const float world[16], const float min[3], const float max[3], float out_min[3], float out_max[3]);

// Builds the caster region by extruding a camera-frustum slice toward the sun.
// Receiver slices include the cascade's blend overlap; casters are unbounded
// along the light direction and later clipped to the finite scene bounds.
bool shadow_volume_from_camera_slice(shadow_volume_t *out, const frustum_t *camera_frustum,
    const float eye[3], const float target[3], float fov_y, float aspect, float slice_near, float slice_far,
    const float sun_dir[3]);

// False only when the box lies wholly outside one plane by more than slack metres.
bool shadow_volume_sees_box(const shadow_volume_t *volume, const float min[3], const float max[3], float slack);

// Bounds the unbounded shadow volume after intersecting it with a finite scene box.
bool shadow_volume_bounds_in_box(const shadow_volume_t *volume, const float clip_min[3], const float clip_max[3],
    float out_min[3], float out_max[3]);

#endif
