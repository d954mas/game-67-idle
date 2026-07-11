#include "skeletal_animation/nt_skeletal_animation.h"

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/span.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

struct nt_skeletal_anim_clip {
    ozz::animation::Skeleton skeleton;
    ozz::animation::Animation animation;
    ozz::animation::SamplingJob::Context context;
    std::vector<ozz::math::SoaTransform> locals;
    std::vector<ozz::math::Float4x4> models;
    float last_time_seconds = 0.0f;
    float last_ratio = 0.0f;
};

static void set_error(char *error, size_t error_cap, const char *fmt, ...) {
    if (error == nullptr || error_cap == 0) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    (void)std::vsnprintf(error, error_cap, fmt, args);
    va_end(args);
    error[error_cap - 1] = '\0';
}

template <typename T>
static bool load_ozz_object(const char *path, T *out, char *error, size_t error_cap) {
    ozz::io::File file(path, "rb");
    if (!file.opened()) {
        set_error(error, error_cap, "failed to open %s", path);
        return false;
    }

    ozz::io::IArchive archive(&file);
    if (!archive.TestTag<T>()) {
        set_error(error, error_cap, "archive has unexpected object type: %s", path);
        return false;
    }

    archive >> *out;
    return true;
}

static float wrap_time(float time_seconds, float duration_seconds) {
    if (duration_seconds <= 0.0f) {
        return 0.0f;
    }
    while (time_seconds < 0.0f) {
        time_seconds += duration_seconds;
    }
    while (time_seconds > duration_seconds) {
        time_seconds -= duration_seconds;
    }
    return time_seconds;
}

static nt_skeletal_anim_vec3_t translation_from_model(const ozz::math::Float4x4 &matrix) {
    float values[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    ozz::math::StorePtrU(matrix.cols[3], values);
    return nt_skeletal_anim_vec3_t{values[0], values[1], values[2]};
}

int nt_skeletal_anim_load_ozz(const char *skeleton_path,
                              const char *animation_path,
                              nt_skeletal_anim_clip_t **out_clip,
                              char *error,
                              size_t error_cap) {
    if (out_clip == nullptr) {
        set_error(error, error_cap, "out_clip is null");
        return 0;
    }
    *out_clip = nullptr;
    if (skeleton_path == nullptr || animation_path == nullptr) {
        set_error(error, error_cap, "skeleton_path and animation_path are required");
        return 0;
    }

    nt_skeletal_anim_clip_t *clip = new (std::nothrow) nt_skeletal_anim_clip_t();
    if (clip == nullptr) {
        set_error(error, error_cap, "out of memory creating skeletal clip");
        return 0;
    }

    if (!load_ozz_object(skeleton_path, &clip->skeleton, error, error_cap) ||
        !load_ozz_object(animation_path, &clip->animation, error, error_cap)) {
        delete clip;
        return 0;
    }

    if (clip->skeleton.num_joints() != clip->animation.num_tracks()) {
        set_error(error,
                  error_cap,
                  "skeleton/animation mismatch: joints=%d tracks=%d",
                  clip->skeleton.num_joints(),
                  clip->animation.num_tracks());
        delete clip;
        return 0;
    }

    clip->context.Resize(clip->skeleton.num_joints());
    clip->locals.resize(static_cast<size_t>(clip->skeleton.num_soa_joints()));
    clip->models.resize(static_cast<size_t>(clip->skeleton.num_joints()));
    *out_clip = clip;
    return 1;
}

void nt_skeletal_anim_destroy(nt_skeletal_anim_clip_t *clip) {
    delete clip;
}

int nt_skeletal_anim_joint_count(const nt_skeletal_anim_clip_t *clip) {
    return clip != nullptr ? clip->skeleton.num_joints() : 0;
}

int nt_skeletal_anim_track_count(const nt_skeletal_anim_clip_t *clip) {
    return clip != nullptr ? clip->animation.num_tracks() : 0;
}

float nt_skeletal_anim_duration(const nt_skeletal_anim_clip_t *clip) {
    return clip != nullptr ? clip->animation.duration() : 0.0f;
}

int nt_skeletal_anim_find_joint(const nt_skeletal_anim_clip_t *clip, const char *joint_name) {
    if (clip == nullptr || joint_name == nullptr) {
        return -1;
    }
    const auto names = clip->skeleton.joint_names();
    for (int i = 0; i < clip->skeleton.num_joints(); ++i) {
        if (std::strcmp(names[static_cast<size_t>(i)], joint_name) == 0) {
            return i;
        }
    }
    return -1;
}

int nt_skeletal_anim_sample(nt_skeletal_anim_clip_t *clip,
                            float time_seconds,
                            nt_skeletal_anim_sample_info_t *out_info,
                            char *error,
                            size_t error_cap) {
    if (clip == nullptr) {
        set_error(error, error_cap, "clip is null");
        return 0;
    }

    const float duration = clip->animation.duration();
    const float wrapped_time = wrap_time(time_seconds, duration);
    const float ratio = duration > 0.0f ? wrapped_time / duration : 0.0f;

    ozz::animation::SamplingJob sampling_job;
    sampling_job.animation = &clip->animation;
    sampling_job.context = &clip->context;
    sampling_job.ratio = ratio;
    sampling_job.output = ozz::make_span(clip->locals);
    if (!sampling_job.Run()) {
        set_error(error, error_cap, "ozz SamplingJob failed");
        return 0;
    }

    ozz::animation::LocalToModelJob ltm_job;
    ltm_job.skeleton = &clip->skeleton;
    ltm_job.input = ozz::make_span(clip->locals);
    ltm_job.output = ozz::make_span(clip->models);
    if (!ltm_job.Run()) {
        set_error(error, error_cap, "ozz LocalToModelJob failed");
        return 0;
    }

    clip->last_time_seconds = wrapped_time;
    clip->last_ratio = ratio;
    if (out_info != nullptr) {
        out_info->time_seconds = wrapped_time;
        out_info->ratio = ratio;
        out_info->duration_seconds = duration;
        out_info->joint_count = clip->skeleton.num_joints();
        out_info->track_count = clip->animation.num_tracks();
    }
    return 1;
}

int nt_skeletal_anim_sample_attachment(nt_skeletal_anim_clip_t *clip,
                                       const char *joint_name,
                                       float time_seconds,
                                       nt_skeletal_anim_attachment_sample_t *out_sample,
                                       char *error,
                                       size_t error_cap) {
    if (out_sample == nullptr) {
        set_error(error, error_cap, "out_sample is null");
        return 0;
    }
    const int joint_index = nt_skeletal_anim_find_joint(clip, joint_name);
    if (joint_index < 0) {
        set_error(error, error_cap, "joint not found: %s", joint_name != nullptr ? joint_name : "<null>");
        return 0;
    }
    if (!nt_skeletal_anim_sample(clip, time_seconds, nullptr, error, error_cap)) {
        return 0;
    }
    out_sample->joint_index = joint_index;
    out_sample->model_position = translation_from_model(clip->models[static_cast<size_t>(joint_index)]);
    return 1;
}

int nt_skeletal_anim_copy_model_matrices(nt_skeletal_anim_clip_t *clip,
                                         float *out_column_major_matrices,
                                         int max_matrices,
                                         char *error,
                                         size_t error_cap) {
    if (clip == nullptr || out_column_major_matrices == nullptr) {
        set_error(error, error_cap, "clip and out_column_major_matrices are required");
        return 0;
    }
    if (max_matrices < clip->skeleton.num_joints()) {
        set_error(error,
                  error_cap,
                  "not enough matrix output capacity: need=%d got=%d",
                  clip->skeleton.num_joints(),
                  max_matrices);
        return 0;
    }
    for (int i = 0; i < clip->skeleton.num_joints(); ++i) {
        float *dst = out_column_major_matrices + static_cast<size_t>(i) * 16U;
        for (int col = 0; col < 4; ++col) {
            ozz::math::StorePtrU(clip->models[static_cast<size_t>(i)].cols[col], dst + col * 4);
        }
    }
    return clip->skeleton.num_joints();
}
