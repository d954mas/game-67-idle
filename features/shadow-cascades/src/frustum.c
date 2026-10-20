#include "features/shadow_cascades/frustum.h"

#include "math/nt_math.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// Rejects a vector too short or nonfinite to give a direction, so a degenerate
// camera or sun builds no volume instead of one with NaN planes.
static bool normalize3(float value[3]) {
    const float length = glm_vec3_norm(value);
    if (!(length > 1.0e-6F) || !isfinite(length)) { return false; }
    glm_vec3_divs(value, length, value);
    return true;
}

typedef struct projected_corner {
    float x, y;
    uint32_t corner;
} projected_corner;

static int compare_projected_corners(const void *left, const void *right) {
    const projected_corner *a = left;
    const projected_corner *b = right;
    if (a->x != b->x) { return (a->x > b->x) - (a->x < b->x); }
    if (a->y != b->y) { return (a->y > b->y) - (a->y < b->y); }
    return (a->corner > b->corner) - (a->corner < b->corner);
}

static float projected_cross(const projected_corner *origin, const projected_corner *a, const projected_corner *b) {
    return (a->x - origin->x) * (b->y - origin->y) - (a->y - origin->y) * (b->x - origin->x);
}

static bool shadow_volume_add_plane(shadow_volume_t *volume, const float plane[4], const float inside[3]) {
    if (volume->plane_count >= SHADOW_VOLUME_MAX_PLANES) { return false; }
    float *out = volume->plane[volume->plane_count];
    for (int c = 0; c < 4; ++c) { out[c] = plane[c]; }
    if (out[0] * inside[0] + out[1] * inside[1] + out[2] * inside[2] + out[3] < 0.0F) {
        for (int c = 0; c < 4; ++c) { out[c] = -out[c]; }
    }
    volume->plane_count += 1U;
    return true;
}

void frustum_from_view_proj(frustum_t *out, const float m[16]) {
    glm_frustum_planes((vec4 *)m, out->plane);
}

// A zero plane passes every box.
void frustum_from_light_view_proj(frustum_t *out, const float m[16]) {
    frustum_from_view_proj(out, m);
    for (int c = 0; c < 4; ++c) { out->plane[4][c] = 0.0F; }
}

bool frustum_sees_box(const frustum_t *frustum, const float min[3], const float max[3], float slack) {
    for (int k = 0; k < 6; ++k) {
        const float *p = frustum->plane[k];
        // The box corner furthest along the plane normal.
        const float x = p[0] >= 0.0F ? max[0] : min[0];
        const float y = p[1] >= 0.0F ? max[1] : min[1];
        const float z = p[2] >= 0.0F ? max[2] : min[2];
        if (p[0] * x + p[1] * y + p[2] * z + p[3] < -slack) { return false; }
    }
    return true;
}

void frustum_box_to_world(const float m[16], const float min[3], const float max[3], float out_min[3], float out_max[3]) {
    for (int row = 0; row < 3; ++row) {
        float lo = m[12 + row];
        float hi = lo;
        for (int col = 0; col < 3; ++col) {
            const float a = m[4 * col + row] * min[col];
            const float b = m[4 * col + row] * max[col];
            lo += fminf(a, b);
            hi += fmaxf(a, b);
        }
        out_min[row] = lo;
        out_max[row] = hi;
    }
}

bool shadow_volume_from_camera_slice(shadow_volume_t *out, const frustum_t *camera_frustum,
    const float eye[3], const float target[3], float fov_y, float aspect, float slice_near, float slice_far,
    const float sun_dir[3]) {
    if (out == NULL || camera_frustum == NULL || eye == NULL || target == NULL || sun_dir == NULL ||
        !(fov_y > 0.0F && fov_y < 3.13F) || !(aspect > 0.0F) || !(slice_near >= 0.0F) || !(slice_far > slice_near)) {
        return false;
    }
    memset(out, 0, sizeof *out);

    float forward[3] = {target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]};
    float toward_sun[3] = {sun_dir[0], sun_dir[1], sun_dir[2]};
    if (!normalize3(forward) || !normalize3(toward_sun)) { return false; }
    float world_up[3] = {0.0F, 1.0F, 0.0F};
    float right[3];
    glm_vec3_cross(forward, world_up, right);
    if (!normalize3(right)) {
        float fallback_up[3] = {0.0F, 0.0F, 1.0F};
        glm_vec3_cross(forward, fallback_up, right);
        if (!normalize3(right)) { return false; }
    }
    float up[3];
    glm_vec3_cross(right, forward, up);
    if (!normalize3(up)) { return false; }

    const float half_tan = tanf(0.5F * fov_y);
    float corners[8][3];
    for (uint32_t ring = 0U; ring < 2U; ++ring) {
        const float depth = ring == 0U ? slice_near : slice_far;
        const float half_height = depth * half_tan;
        const float half_width = half_height * aspect;
        float center[3];
        for (int axis = 0; axis < 3; ++axis) { center[axis] = eye[axis] + forward[axis] * depth; }
        for (uint32_t corner = 0U; corner < 4U; ++corner) {
            const float side = (corner & 1U) != 0U ? 1.0F : -1.0F;
            const float vertical = (corner & 2U) != 0U ? 1.0F : -1.0F;
            float *point = corners[ring * 4U + corner];
            for (int axis = 0; axis < 3; ++axis) {
                point[axis] = center[axis] + side * half_width * right[axis] + vertical * half_height * up[axis];
            }
        }
    }

    float origin[3] = {eye[0], eye[1], eye[2]};
    const float near_plane[4] = {forward[0], forward[1], forward[2], -glm_vec3_dot(forward, origin) - slice_near};
    const float far_plane[4] = {-forward[0], -forward[1], -forward[2], glm_vec3_dot(forward, origin) + slice_far};
    float slice_planes[6][4];
    for (uint32_t i = 0U; i < 4U; ++i) {
        for (int c = 0; c < 4; ++c) { slice_planes[i][c] = camera_frustum->plane[i][c]; }
    }
    memcpy(slice_planes[4], near_plane, sizeof near_plane);
    memcpy(slice_planes[5], far_plane, sizeof far_plane);

    float inside[3];
    for (int axis = 0; axis < 3; ++axis) { inside[axis] = eye[axis] + forward[axis] * (0.5F * (slice_near + slice_far)); }
    for (uint32_t i = 0U; i < 6U; ++i) {
        const float along_light = slice_planes[i][0] * toward_sun[0] + slice_planes[i][1] * toward_sun[1] + slice_planes[i][2] * toward_sun[2];
        if (along_light >= -1.0e-5F && !shadow_volume_add_plane(out, slice_planes[i], inside)) { return false; }
    }

    float reference[3] = {0.0F, fabsf(toward_sun[1]) < 0.9F ? 1.0F : 0.0F, fabsf(toward_sun[1]) < 0.9F ? 0.0F : 1.0F};
    float basis_x[3];
    glm_vec3_cross(reference, toward_sun, basis_x);
    if (!normalize3(basis_x)) { return false; }
    float basis_y[3];
    glm_vec3_cross(toward_sun, basis_x, basis_y);
    if (!normalize3(basis_y)) { return false; }

    projected_corner projected[8];
    for (uint32_t i = 0U; i < 8U; ++i) {
        projected[i] = (projected_corner){.x = glm_vec3_dot(corners[i], basis_x), .y = glm_vec3_dot(corners[i], basis_y), .corner = i};
    }
    qsort(projected, 8U, sizeof projected[0], compare_projected_corners);
    projected_corner unique[8];
    uint32_t unique_count = 0U;
    for (uint32_t i = 0U; i < 8U; ++i) {
        if (unique_count > 0U && fabsf(projected[i].x - unique[unique_count - 1U].x) < 1.0e-5F &&
            fabsf(projected[i].y - unique[unique_count - 1U].y) < 1.0e-5F) {
            continue;
        }
        unique[unique_count++] = projected[i];
    }
    if (unique_count < 3U) { return false; }

    uint32_t hull[16];
    uint32_t hull_count = 0U;
    for (uint32_t i = 0U; i < unique_count; ++i) {
        while (hull_count >= 2U && projected_cross(&unique[hull[hull_count - 2U]], &unique[hull[hull_count - 1U]], &unique[i]) <= 1.0e-6F) {
            hull_count -= 1U;
        }
        hull[hull_count++] = i;
    }
    const uint32_t lower_count = hull_count;
    for (int32_t i = (int32_t)unique_count - 2; i >= 0; --i) {
        while (hull_count > lower_count && projected_cross(&unique[hull[hull_count - 2U]], &unique[hull[hull_count - 1U]], &unique[(uint32_t)i]) <= 1.0e-6F) {
            hull_count -= 1U;
        }
        hull[hull_count++] = (uint32_t)i;
    }
    if (hull_count <= 3U) { return false; }
    hull_count -= 1U;

    for (uint32_t i = 0U; i < hull_count; ++i) {
        const uint32_t a_index = unique[hull[i]].corner;
        const uint32_t b_index = unique[hull[(i + 1U) % hull_count]].corner;
        float edge[3] = {corners[b_index][0] - corners[a_index][0], corners[b_index][1] - corners[a_index][1], corners[b_index][2] - corners[a_index][2]};
        float normal[3];
        glm_vec3_cross(edge, toward_sun, normal);
        if (!normalize3(normal)) { return false; }
        const float plane[4] = {normal[0], normal[1], normal[2], -glm_vec3_dot(normal, corners[a_index])};
        if (!shadow_volume_add_plane(out, plane, inside)) { return false; }
    }
    return out->plane_count >= 4U;
}

bool shadow_volume_sees_box(const shadow_volume_t *volume, const float min[3], const float max[3], float slack) {
    if (volume == NULL) { return true; }
    for (uint32_t i = 0U; i < volume->plane_count; ++i) {
        const float *p = volume->plane[i];
        const float x = p[0] >= 0.0F ? max[0] : min[0];
        const float y = p[1] >= 0.0F ? max[1] : min[1];
        const float z = p[2] >= 0.0F ? max[2] : min[2];
        if (p[0] * x + p[1] * y + p[2] * z + p[3] < -slack) { return false; }
    }
    return true;
}

static bool plane_triple_intersection(const float a[4], const float b[4], const float c[4], float out[3]) {
    float na[3] = {a[0], a[1], a[2]};
    float nb[3] = {b[0], b[1], b[2]};
    float nc[3] = {c[0], c[1], c[2]};
    float bc[3], ca[3], ab[3];
    glm_vec3_cross(nb, nc, bc);
    glm_vec3_cross(nc, na, ca);
    glm_vec3_cross(na, nb, ab);
    const float determinant = glm_vec3_dot(na, bc);
    if (fabsf(determinant) < 1.0e-6F) { return false; }
    const float wa = -a[3], wb = -b[3], wc = -c[3];
    for (int axis = 0; axis < 3; ++axis) { out[axis] = (wa * bc[axis] + wb * ca[axis] + wc * ab[axis]) / determinant; }
    return isfinite(out[0]) && isfinite(out[1]) && isfinite(out[2]);
}

bool shadow_volume_bounds_in_box(const shadow_volume_t *volume, const float clip_min[3], const float clip_max[3],
    float out_min[3], float out_max[3]) {
    if (volume == NULL || clip_min == NULL || clip_max == NULL || out_min == NULL || out_max == NULL ||
        volume->plane_count > SHADOW_VOLUME_MAX_PLANES) { return false; }
    float planes[SHADOW_VOLUME_MAX_PLANES + 6U][4];
    const uint32_t plane_count = volume->plane_count + 6U;
    for (uint32_t i = 0U; i < volume->plane_count; ++i) { memcpy(planes[i], volume->plane[i], sizeof planes[i]); }
    const float box_planes[6][4] = {
        {1.0F, 0.0F, 0.0F, -clip_min[0]}, {-1.0F, 0.0F, 0.0F, clip_max[0]},
        {0.0F, 1.0F, 0.0F, -clip_min[1]}, {0.0F, -1.0F, 0.0F, clip_max[1]},
        {0.0F, 0.0F, 1.0F, -clip_min[2]}, {0.0F, 0.0F, -1.0F, clip_max[2]},
    };
    for (uint32_t i = 0U; i < 6U; ++i) { memcpy(planes[volume->plane_count + i], box_planes[i], sizeof box_planes[i]); }
    for (int axis = 0; axis < 3; ++axis) { out_min[axis] = INFINITY; out_max[axis] = -INFINITY; }
    bool found = false;
    for (uint32_t i = 0U; i + 2U < plane_count; ++i) {
        for (uint32_t j = i + 1U; j + 1U < plane_count; ++j) {
            for (uint32_t k = j + 1U; k < plane_count; ++k) {
                float point[3];
                if (!plane_triple_intersection(planes[i], planes[j], planes[k], point)) { continue; }
                bool inside = true;
                for (uint32_t p = 0U; p < plane_count; ++p) {
                    const float *plane = planes[p];
                    if (plane[0] * point[0] + plane[1] * point[1] + plane[2] * point[2] + plane[3] < -0.01F) {
                        inside = false;
                        break;
                    }
                }
                if (!inside) { continue; }
                found = true;
                for (int axis = 0; axis < 3; ++axis) {
                    out_min[axis] = fminf(out_min[axis], point[axis]);
                    out_max[axis] = fmaxf(out_max[axis], point[axis]);
                }
            }
        }
    }
    if (!found) { return false; }
    for (int axis = 0; axis < 3; ++axis) {
        out_min[axis] = fmaxf(clip_min[axis], out_min[axis] - 0.05F);
        out_max[axis] = fminf(clip_max[axis], out_max[axis] + 0.05F);
    }
    return true;
}
