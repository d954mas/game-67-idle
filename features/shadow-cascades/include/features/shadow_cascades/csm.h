#ifndef FEATURE_SHADOW_CASCADES_CSM_H
#define FEATURE_SHADOW_CASCADES_CSM_H

// Cascaded shadow maps for a directional sun: the light matrices of each
// cascade and where it sits in the atlas. Pure math, no GPU: the renderer owns
// the depth atlas and the passes. Matrices are column-major, GL clip space.

#include <stdbool.h>
#include <stdint.h>

#define CSM_MAX_CASCADES 4U

typedef struct csm_config_t {
    uint32_t cascades;                        // 1..CSM_MAX_CASCADES
    uint32_t resolution;                      // texels per cascade side
    float split_far[CSM_MAX_CASCADES];    // view depth each cascade ends at, metres, increasing;
                                              // constant, so cascade edges hold still
    float depth_range;                        // minimum light-space depth a cascade spans, metres, centred
                                              // on its slice; never less than the slice's sphere
    float margin_texels;                      // texels added around each slice sphere: the renderer's PCF
                                              // reach plus one for centre snapping
    float sun_dir[3];                         // unit vector toward the sun
} csm_config_t;

typedef struct csm_camera_t {
    float eye[3], target[3];
    float fov_y, aspect, near_plane;
} csm_camera_t;

typedef struct csm_cascades_t {
    uint32_t count;
    uint32_t grid_x, grid_y;                     // cascade tiles across and up the atlas
    float split_far[CSM_MAX_CASCADES];       // view depth where each cascade ends
    float render_vp[CSM_MAX_CASCADES][16];   // world -> cascade clip space, for the depth pass
    // Receivers: atlas uv + depth in [0, 1] = light_view * world, scaled and offset per cascade.
    // Every cascade shares the light's rotation, so a receiver rotates once, not once a cascade.
    float light_view[16];                        // world -> light space, a rotation
    float lookup_scale[CSM_MAX_CASCADES][3];
    float lookup_offset[CSM_MAX_CASCADES][3];
    float tile[CSM_MAX_CASCADES][4];         // atlas uv rect of each cascade: min x, min y, max x, max y
    float texel_world[CSM_MAX_CASCADES];     // world size of one shadow texel
    float half_depth[CSM_MAX_CASCADES];      // light-space depth either side of the slice centre, metres
    int viewport[CSM_MAX_CASCADES][4];       // x, y, w, h of each cascade in the atlas, pixels
} csm_cascades_t;

bool csm_config_valid(const csm_config_t *config);
// Atlas size in texels: 1x1 tiles for one cascade, 2x1 for two, 2x2 for three or four.
uint32_t csm_atlas_width(const csm_config_t *config);
uint32_t csm_atlas_height(const csm_config_t *config);
// The sphere around the view slice [near_z, far_z], metres: centre distance
// along the view axis and radius. It depends only on the slice and the lens,
// so the texel size holds still while the camera walks and turns.
void csm_slice_sphere(float near_z, float far_z, float fov_y, float aspect, float *centre_z, float *radius);
// Fits each cascade to its slice sphere and snaps its centre to whole texels
// in a fixed light basis, so a moving or turning camera never shimmers edges.
void csm_compute(const csm_config_t *config, const csm_camera_t *camera, csm_cascades_t *out);

#endif
