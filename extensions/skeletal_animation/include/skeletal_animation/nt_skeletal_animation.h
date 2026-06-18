#ifndef NT_SKELETAL_ANIMATION_H
#define NT_SKELETAL_ANIMATION_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nt_skeletal_anim_clip nt_skeletal_anim_clip_t;

typedef struct nt_skeletal_anim_vec3 {
    float x;
    float y;
    float z;
} nt_skeletal_anim_vec3_t;

typedef struct nt_skeletal_anim_sample_info {
    float time_seconds;
    float ratio;
    float duration_seconds;
    int joint_count;
    int track_count;
} nt_skeletal_anim_sample_info_t;

typedef struct nt_skeletal_anim_attachment_sample {
    int joint_index;
    nt_skeletal_anim_vec3_t model_position;
} nt_skeletal_anim_attachment_sample_t;

int nt_skeletal_anim_load_ozz(const char *skeleton_path,
                              const char *animation_path,
                              nt_skeletal_anim_clip_t **out_clip,
                              char *error,
                              size_t error_cap);

void nt_skeletal_anim_destroy(nt_skeletal_anim_clip_t *clip);

int nt_skeletal_anim_joint_count(const nt_skeletal_anim_clip_t *clip);
int nt_skeletal_anim_track_count(const nt_skeletal_anim_clip_t *clip);
float nt_skeletal_anim_duration(const nt_skeletal_anim_clip_t *clip);

int nt_skeletal_anim_find_joint(const nt_skeletal_anim_clip_t *clip, const char *joint_name);

int nt_skeletal_anim_sample(nt_skeletal_anim_clip_t *clip,
                            float time_seconds,
                            nt_skeletal_anim_sample_info_t *out_info,
                            char *error,
                            size_t error_cap);

int nt_skeletal_anim_sample_attachment(nt_skeletal_anim_clip_t *clip,
                                       const char *joint_name,
                                       float time_seconds,
                                       nt_skeletal_anim_attachment_sample_t *out_sample,
                                       char *error,
                                       size_t error_cap);

int nt_skeletal_anim_copy_model_matrices(nt_skeletal_anim_clip_t *clip,
                                         float *out_column_major_matrices,
                                         int max_matrices,
                                         char *error,
                                         size_t error_cap);

#ifdef __cplusplus
}
#endif

#endif
