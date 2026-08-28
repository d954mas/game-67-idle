#ifndef POST_AA_EXAMPLE_POLICY_H
#define POST_AA_EXAMPLE_POLICY_H

typedef enum post_aa_mode_t {
  POST_AA_OFF = 0,
  POST_AA_FXAA = 1,
  POST_AA_SSAA = 2,
} post_aa_mode_t;

#define POST_AA_SSAA_MAX 1.5F
#define POST_AA_WORLD_DENSITY 1.875F

static inline float post_aa_world_scale(post_aa_mode_t mode,
                                        float device_pixel_ratio) {
  if (mode != POST_AA_SSAA) {
    return 1.0F;
  }
  if (!(device_pixel_ratio > 0.0F)) {
    device_pixel_ratio = 1.0F;
  }
  float factor = POST_AA_WORLD_DENSITY / device_pixel_ratio;
  if (factor < 1.0F) {
    factor = 1.0F;
  }
  if (factor > POST_AA_SSAA_MAX) {
    factor = POST_AA_SSAA_MAX;
  }
  return factor;
}

#endif
