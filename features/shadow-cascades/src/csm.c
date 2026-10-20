#include "features/shadow_cascades/csm.h"

#include "math/nt_math.h"

#include <math.h>
#include <string.h>

/* Sphere radii round up to this step, metres, so float noise can never flip
   the texel size of a cascade. */
#define CSM_RADIUS_STEP 0.5F

bool csm_config_valid(const csm_config_t *config) {
    if (config == NULL || config->cascades < 1U || config->cascades > CSM_MAX_CASCADES) return false;
    if (config->resolution < 16U || (config->resolution & (config->resolution - 1U)) != 0U) return false;
    if (!(config->depth_range > 0.0F) || !(config->margin_texels >= 0.0F) || !isfinite(config->margin_texels)) return false;
    float previous = 0.0F;
    for (uint32_t i = 0; i < config->cascades; ++i) {
        if (!(config->split_far[i] > previous)) return false;
        previous = config->split_far[i];
    }
    float sun[3] = {config->sun_dir[0], config->sun_dir[1], config->sun_dir[2]};
    const float length = glm_vec3_norm(sun);
    return length > 0.5F && length < 1.5F;
}

static uint32_t grid_x_of(const csm_config_t *config) { return config->cascades == 1U ? 1U : 2U; }
static uint32_t grid_y_of(const csm_config_t *config) { return config->cascades <= 2U ? 1U : 2U; }

uint32_t csm_atlas_width(const csm_config_t *config) { return grid_x_of(config) * config->resolution; }
uint32_t csm_atlas_height(const csm_config_t *config) { return grid_y_of(config) * config->resolution; }

void csm_slice_sphere(float near_z, float far_z, float fov_y, float aspect, float *centre_z, float *radius) {
    /* A corner at depth z sits z * k off the axis. The sphere through the near
       and far corner rings centres at (n + f)(1 + k^2) / 2, unless the far ring
       alone is wider, when it centres on the far plane. */
    const float t = tanf(0.5F * fov_y);
    const float k2 = t * t * (1.0F + aspect * aspect);
    float c = 0.5F * (near_z + far_z) * (1.0F + k2);
    if (c > far_z) c = far_z;
    const float along = far_z - c;
    *centre_z = c;
    *radius = sqrtf(along * along + far_z * far_z * k2);
}

void csm_compute(const csm_config_t *config, const csm_camera_t *camera, csm_cascades_t *out) {
    memset(out, 0, sizeof *out);
    out->count = config->cascades;
    out->grid_x = grid_x_of(config);
    out->grid_y = grid_y_of(config);

    float forward[3] = {camera->target[0] - camera->eye[0], camera->target[1] - camera->eye[1], camera->target[2] - camera->eye[2]};
    glm_vec3_normalize(forward);

    /* Light basis: z toward the sun; fixed for a fixed sun, so snapping in it holds. */
    float lz[3] = {config->sun_dir[0], config->sun_dir[1], config->sun_dir[2]};
    glm_vec3_normalize(lz);
    float reference[3] = {0.0F, fabsf(lz[1]) > 0.99F ? 0.0F : 1.0F, fabsf(lz[1]) > 0.99F ? 1.0F : 0.0F};
    float lx[3], ly[3];
    glm_vec3_cross(reference, lz, lx);
    glm_vec3_normalize(lx);
    glm_vec3_cross(lz, lx, ly);
    float light_view[16] = {
        lx[0], ly[0], lz[0], 0.0F,
        lx[1], ly[1], lz[1], 0.0F,
        lx[2], ly[2], lz[2], 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F,
    };
    memcpy(out->light_view, light_view, sizeof out->light_view);

    float slice_near = camera->near_plane;
    for (uint32_t i = 0; i < config->cascades; ++i) {
        const float slice_far = config->split_far[i];
        out->split_far[i] = slice_far;
        float centre_z, radius;
        csm_slice_sphere(slice_near, slice_far, camera->fov_y, camera->aspect, &centre_z, &radius);
        /* A texel is 2r / resolution, so each margin texel widens the radius by 2r / resolution. */
        radius = ceilf(radius * (1.0F + 2.0F * config->margin_texels / (float)config->resolution) / CSM_RADIUS_STEP) * CSM_RADIUS_STEP;
        float centre[3] = {camera->eye[0] + forward[0] * centre_z, camera->eye[1] + forward[1] * centre_z,
                                 camera->eye[2] + forward[2] * centre_z};

        const float texel = 2.0F * radius / (float)config->resolution;
        const float cx = floorf(glm_vec3_dot(lx, centre) / texel) * texel;
        const float cy = floorf(glm_vec3_dot(ly, centre) / texel) * texel;
        const float cz = glm_vec3_dot(lz, centre);
        /* The window holds every receiver in the sphere; casters beyond it toward
           the sun are pancaked onto the near plane by the depth shaders. */
        const float half_depth = fmaxf(0.5F * config->depth_range, radius);
        out->half_depth[i] = half_depth;
        /* Nearer the sun is nearer the light: light-space z above the centre maps toward -1. */
        float ortho[16] = {
            1.0F / radius, 0.0F, 0.0F, 0.0F,
            0.0F, 1.0F / radius, 0.0F, 0.0F,
            0.0F, 0.0F, -1.0F / half_depth, 0.0F,
            -cx / radius, -cy / radius, cz / half_depth, 1.0F,
        };
        glm_mat4_mul((vec4 *)ortho, (vec4 *)light_view, (vec4 *)out->render_vp[i]);

        const float sx = 0.5F / (float)out->grid_x;
        const float sy = 0.5F / (float)out->grid_y;
        const float ox = (float)(i % out->grid_x) / (float)out->grid_x;
        const float oy = (float)(i / out->grid_x) / (float)out->grid_y;
        float tile[16] = {
            sx, 0.0F, 0.0F, 0.0F,
            0.0F, sy, 0.0F, 0.0F,
            0.0F, 0.0F, 0.5F, 0.0F,
            sx + ox, sy + oy, 0.5F, 1.0F,
        };
        // tile * ortho only scales and offsets each axis: its diagonal and translation.
        float window[16];
        glm_mat4_mul((vec4 *)tile, (vec4 *)ortho, (vec4 *)window);
        for (int axis = 0; axis < 3; ++axis) {
            out->lookup_scale[i][axis] = window[axis * 5];
            out->lookup_offset[i][axis] = window[12 + axis];
        }
        out->tile[i][0] = ox;
        out->tile[i][1] = oy;
        out->tile[i][2] = ox + 2.0F * sx;
        out->tile[i][3] = oy + 2.0F * sy;
        out->texel_world[i] = texel;
        out->viewport[i][0] = (int)((i % out->grid_x) * config->resolution);
        out->viewport[i][1] = (int)((i / out->grid_x) * config->resolution);
        out->viewport[i][2] = (int)config->resolution;
        out->viewport[i][3] = (int)config->resolution;
        slice_near = slice_far;
    }
}
