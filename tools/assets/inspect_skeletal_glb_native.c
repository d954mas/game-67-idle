#include "cgltf.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct PoseTransform {
    int has_matrix;
    cgltf_float translation[3];
    cgltf_float rotation[4];
    cgltf_float scale[3];
    cgltf_float matrix[16];
} PoseTransform;

typedef struct CliArgs {
    const char *path;
    const char *animation_name;
    const char *trace_csv_path;
    cgltf_float sample_time;
    cgltf_float fps;
    int frames;
} CliArgs;

static const char *name_or_fallback(const char *name, const char *fallback) {
    return name != NULL ? name : fallback;
}

static int sample_joint_matrices(const cgltf_data *data, const cgltf_skin *skin, const cgltf_float *worlds);

static void mat4_identity(cgltf_float *out) {
    memset(out, 0, sizeof(cgltf_float) * 16);
    out[0] = 1.0F;
    out[5] = 1.0F;
    out[10] = 1.0F;
    out[15] = 1.0F;
}

static void mat4_multiply(const cgltf_float *a, const cgltf_float *b, cgltf_float *out) {
    cgltf_float r[16];
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            r[col * 4 + row] = a[0 * 4 + row] * b[col * 4 + 0] + a[1 * 4 + row] * b[col * 4 + 1] + a[2 * 4 + row] * b[col * 4 + 2] + a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
    memcpy(out, r, sizeof(r));
}

static void quat_normalize(cgltf_float *q) {
    cgltf_float len = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (len <= 0.0F) {
        q[0] = 0.0F;
        q[1] = 0.0F;
        q[2] = 0.0F;
        q[3] = 1.0F;
        return;
    }
    q[0] /= len;
    q[1] /= len;
    q[2] /= len;
    q[3] /= len;
}

static void trs_to_mat4(const PoseTransform *tr, cgltf_float *out) {
    if (tr->has_matrix) {
        memcpy(out, tr->matrix, sizeof(cgltf_float) * 16);
        return;
    }

    cgltf_float q[4] = {tr->rotation[0], tr->rotation[1], tr->rotation[2], tr->rotation[3]};
    quat_normalize(q);

    const cgltf_float qx = q[0];
    const cgltf_float qy = q[1];
    const cgltf_float qz = q[2];
    const cgltf_float qw = q[3];
    const cgltf_float sx = tr->scale[0];
    const cgltf_float sy = tr->scale[1];
    const cgltf_float sz = tr->scale[2];

    out[0] = (1.0F - 2.0F * qy * qy - 2.0F * qz * qz) * sx;
    out[1] = (2.0F * qx * qy + 2.0F * qz * qw) * sx;
    out[2] = (2.0F * qx * qz - 2.0F * qy * qw) * sx;
    out[3] = 0.0F;

    out[4] = (2.0F * qx * qy - 2.0F * qz * qw) * sy;
    out[5] = (1.0F - 2.0F * qx * qx - 2.0F * qz * qz) * sy;
    out[6] = (2.0F * qy * qz + 2.0F * qx * qw) * sy;
    out[7] = 0.0F;

    out[8] = (2.0F * qx * qz + 2.0F * qy * qw) * sz;
    out[9] = (2.0F * qy * qz - 2.0F * qx * qw) * sz;
    out[10] = (1.0F - 2.0F * qx * qx - 2.0F * qy * qy) * sz;
    out[11] = 0.0F;

    out[12] = tr->translation[0];
    out[13] = tr->translation[1];
    out[14] = tr->translation[2];
    out[15] = 1.0F;
}

static void pose_transform_from_node(const cgltf_node *node, PoseTransform *out) {
    memset(out, 0, sizeof(*out));
    out->translation[0] = node->has_translation ? node->translation[0] : 0.0F;
    out->translation[1] = node->has_translation ? node->translation[1] : 0.0F;
    out->translation[2] = node->has_translation ? node->translation[2] : 0.0F;
    out->rotation[0] = node->has_rotation ? node->rotation[0] : 0.0F;
    out->rotation[1] = node->has_rotation ? node->rotation[1] : 0.0F;
    out->rotation[2] = node->has_rotation ? node->rotation[2] : 0.0F;
    out->rotation[3] = node->has_rotation ? node->rotation[3] : 1.0F;
    out->scale[0] = node->has_scale ? node->scale[0] : 1.0F;
    out->scale[1] = node->has_scale ? node->scale[1] : 1.0F;
    out->scale[2] = node->has_scale ? node->scale[2] : 1.0F;
    out->has_matrix = node->has_matrix ? 1 : 0;
    if (node->has_matrix) {
        memcpy(out->matrix, node->matrix, sizeof(cgltf_float) * 16);
    } else {
        mat4_identity(out->matrix);
    }
}

static cgltf_size node_index(const cgltf_data *data, const cgltf_node *node) {
    return (cgltf_size)(node - data->nodes);
}

static int read_scalar_key(const cgltf_accessor *accessor, cgltf_size index, cgltf_float *out) {
    cgltf_float tmp[1] = {0.0F};
    if (!cgltf_accessor_read_float(accessor, index, tmp, 1)) {
        return 0;
    }
    *out = tmp[0];
    return 1;
}

static int read_vec_key(const cgltf_accessor *accessor, cgltf_size index, cgltf_float *out, cgltf_size width) {
    return cgltf_accessor_read_float(accessor, index, out, width) != 0;
}

static void lerp_vec(const cgltf_float *a, const cgltf_float *b, cgltf_float alpha, cgltf_float *out, cgltf_size width) {
    for (cgltf_size i = 0; i < width; i++) {
        out[i] = a[i] + (b[i] - a[i]) * alpha;
    }
}

static void nlerp_quat(const cgltf_float *a, const cgltf_float *b, cgltf_float alpha, cgltf_float *out) {
    cgltf_float bx = b[0];
    cgltf_float by = b[1];
    cgltf_float bz = b[2];
    cgltf_float bw = b[3];
    const cgltf_float dot = a[0] * bx + a[1] * by + a[2] * bz + a[3] * bw;
    if (dot < 0.0F) {
        bx = -bx;
        by = -by;
        bz = -bz;
        bw = -bw;
    }
    out[0] = a[0] + (bx - a[0]) * alpha;
    out[1] = a[1] + (by - a[1]) * alpha;
    out[2] = a[2] + (bz - a[2]) * alpha;
    out[3] = a[3] + (bw - a[3]) * alpha;
    quat_normalize(out);
}

static int sample_channel(const cgltf_animation_channel *channel, cgltf_float sample_time, PoseTransform *transforms, cgltf_size target_index) {
    const cgltf_animation_sampler *sampler = channel->sampler;
    if (sampler == NULL || sampler->input == NULL || sampler->output == NULL) {
        fprintf(stderr, "animation channel has missing sampler/input/output\n");
        return 0;
    }
    if (sampler->interpolation == cgltf_interpolation_type_cubic_spline) {
        fprintf(stderr, "CUBICSPLINE animation is not supported by this probe yet\n");
        return 0;
    }

    const cgltf_size key_count = sampler->input->count;
    if (key_count == 0 || sampler->output->count < key_count) {
        fprintf(stderr, "animation sampler has invalid key counts\n");
        return 0;
    }

    cgltf_size key = 0;
    cgltf_float first_time = 0.0F;
    cgltf_float last_time = 0.0F;
    if (!read_scalar_key(sampler->input, 0, &first_time) || !read_scalar_key(sampler->input, key_count - 1, &last_time)) {
        fprintf(stderr, "failed to read animation input time\n");
        return 0;
    }

    if (sample_time <= first_time) {
        key = 0;
    } else if (sample_time >= last_time) {
        key = key_count - 1;
    } else {
        while (key < key_count - 2) {
            cgltf_float next_time = 0.0F;
            if (!read_scalar_key(sampler->input, key + 1, &next_time)) {
                fprintf(stderr, "failed to read animation input time\n");
                return 0;
            }
            if (next_time >= sample_time) {
                break;
            }
            key++;
        }
    }

    cgltf_float alpha = 0.0F;
    cgltf_size right_key = key;
    if (key < key_count - 1 && sample_time > first_time && sample_time < last_time && sampler->interpolation != cgltf_interpolation_type_step) {
        cgltf_float left_time = 0.0F;
        cgltf_float right_time = 0.0F;
        right_key = key + 1;
        if (!read_scalar_key(sampler->input, key, &left_time) || !read_scalar_key(sampler->input, right_key, &right_time)) {
            fprintf(stderr, "failed to read animation interpolation times\n");
            return 0;
        }
        const cgltf_float span = right_time - left_time;
        alpha = span == 0.0F ? 0.0F : (sample_time - left_time) / span;
    }

    cgltf_float left[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    cgltf_float right[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    cgltf_float sampled[4] = {0.0F, 0.0F, 0.0F, 1.0F};

    if (channel->target_path == cgltf_animation_path_type_translation) {
        if (!read_vec_key(sampler->output, key, left, 3)) {
            return 0;
        }
        if (right_key != key) {
            if (!read_vec_key(sampler->output, right_key, right, 3)) {
                return 0;
            }
            lerp_vec(left, right, alpha, sampled, 3);
        } else {
            memcpy(sampled, left, sizeof(cgltf_float) * 3);
        }
        transforms[target_index].has_matrix = 0;
        memcpy(transforms[target_index].translation, sampled, sizeof(cgltf_float) * 3);
        return 1;
    }

    if (channel->target_path == cgltf_animation_path_type_rotation) {
        if (!read_vec_key(sampler->output, key, left, 4)) {
            return 0;
        }
        if (right_key != key) {
            if (!read_vec_key(sampler->output, right_key, right, 4)) {
                return 0;
            }
            nlerp_quat(left, right, alpha, sampled);
        } else {
            memcpy(sampled, left, sizeof(cgltf_float) * 4);
            quat_normalize(sampled);
        }
        transforms[target_index].has_matrix = 0;
        memcpy(transforms[target_index].rotation, sampled, sizeof(cgltf_float) * 4);
        return 1;
    }

    if (channel->target_path == cgltf_animation_path_type_scale) {
        if (!read_vec_key(sampler->output, key, left, 3)) {
            return 0;
        }
        if (right_key != key) {
            if (!read_vec_key(sampler->output, right_key, right, 3)) {
                return 0;
            }
            lerp_vec(left, right, alpha, sampled, 3);
        } else {
            memcpy(sampled, left, sizeof(cgltf_float) * 3);
        }
        transforms[target_index].has_matrix = 0;
        memcpy(transforms[target_index].scale, sampled, sizeof(cgltf_float) * 3);
        return 1;
    }

    if (channel->target_path == cgltf_animation_path_type_weights) {
        return 1;
    }

    fprintf(stderr, "unsupported animation channel path\n");
    return 0;
}

static int compute_world_recursive(const cgltf_data *data, cgltf_size index, const PoseTransform *transforms, cgltf_float *worlds, int *visited) {
    if (visited[index]) {
        return 1;
    }

    cgltf_float local[16];
    trs_to_mat4(&transforms[index], local);

    const cgltf_node *node = &data->nodes[index];
    if (node->parent != NULL) {
        const cgltf_size parent_index = node_index(data, node->parent);
        if (parent_index >= data->nodes_count || !compute_world_recursive(data, parent_index, transforms, worlds, visited)) {
            return 0;
        }
        mat4_multiply(&worlds[parent_index * 16], local, &worlds[index * 16]);
    } else {
        memcpy(&worlds[index * 16], local, sizeof(local));
    }

    visited[index] = 1;
    return 1;
}

static int sample_pose(const cgltf_data *data, const cgltf_animation *animation, cgltf_float sample_time, PoseTransform *transforms, cgltf_float *worlds) {
    for (cgltf_size i = 0; i < data->nodes_count; i++) {
        pose_transform_from_node(&data->nodes[i], &transforms[i]);
        mat4_identity(&worlds[i * 16]);
    }

    for (cgltf_size i = 0; i < animation->channels_count; i++) {
        const cgltf_animation_channel *channel = &animation->channels[i];
        if (channel->target_node == NULL) {
            continue;
        }
        const cgltf_size target_index = node_index(data, channel->target_node);
        if (target_index >= data->nodes_count) {
            fprintf(stderr, "animation target node is out of range\n");
            return 0;
        }
        if (!sample_channel(channel, sample_time, transforms, target_index)) {
            return 0;
        }
    }

    int *visited = (int *)calloc(data->nodes_count > 0 ? data->nodes_count : 1, sizeof(int));
    if (visited == NULL) {
        fprintf(stderr, "failed to allocate pose visited buffer\n");
        return 0;
    }
    for (cgltf_size i = 0; i < data->nodes_count; i++) {
        if (!compute_world_recursive(data, i, transforms, worlds, visited)) {
            free(visited);
            return 0;
        }
    }
    free(visited);
    return 1;
}

static const cgltf_node *find_node(const cgltf_data *data, const char *name) {
    for (cgltf_size i = 0; i < data->nodes_count; i++) {
        if (data->nodes[i].name != NULL && strcmp(data->nodes[i].name, name) == 0) {
            return &data->nodes[i];
        }
    }
    return NULL;
}

static const cgltf_animation *find_animation(const cgltf_data *data, const char *name, cgltf_size *out_index) {
    if (name == NULL) {
        *out_index = 0;
        return data->animations_count > 0 ? &data->animations[0] : NULL;
    }

    for (cgltf_size i = 0; i < data->animations_count; i++) {
        if (data->animations[i].name != NULL && strcmp(data->animations[i].name, name) == 0) {
            *out_index = i;
            return &data->animations[i];
        }
    }
    return NULL;
}

static int animation_time_range(const cgltf_animation *animation, cgltf_float *out_min, cgltf_float *out_max) {
    int found = 0;
    cgltf_float min_time = 0.0F;
    cgltf_float max_time = 0.0F;

    for (cgltf_size i = 0; i < animation->samplers_count; i++) {
        const cgltf_animation_sampler *sampler = &animation->samplers[i];
        if (sampler->input == NULL || sampler->input->count == 0) {
            continue;
        }

        cgltf_float first = 0.0F;
        cgltf_float last = 0.0F;
        if (!read_scalar_key(sampler->input, 0, &first) || !read_scalar_key(sampler->input, sampler->input->count - 1, &last)) {
            return 0;
        }

        if (!found) {
            min_time = first;
            max_time = last;
            found = 1;
        } else {
            if (first < min_time) {
                min_time = first;
            }
            if (last > max_time) {
                max_time = last;
            }
        }
    }

    if (!found) {
        return 0;
    }

    *out_min = min_time;
    *out_max = max_time;
    return 1;
}

static void print_animation_names(const cgltf_data *data) {
    fprintf(stderr, "available animations:");
    for (cgltf_size i = 0; i < data->animations_count; i++) {
        fprintf(stderr, "%s%s", i == 0 ? " " : ", ", name_or_fallback(data->animations[i].name, "unnamed"));
    }
    fprintf(stderr, "\n");
}

static void attachment_position(const cgltf_data *data, const cgltf_float *worlds, const char *name, cgltf_float *out, int *found) {
    const cgltf_node *node = find_node(data, name);
    if (node == NULL) {
        out[0] = 0.0F;
        out[1] = 0.0F;
        out[2] = 0.0F;
        *found = 0;
        return;
    }

    const cgltf_size index = node_index(data, node);
    const cgltf_float *world = &worlds[index * 16];
    out[0] = world[12];
    out[1] = world[13];
    out[2] = world[14];
    *found = 1;
}

static void write_csv_value(FILE *file, const cgltf_float *value, int found) {
    if (!found) {
        fprintf(file, ",,,");
        return;
    }
    fprintf(file, ",%.5f,%.5f,%.5f", (double)value[0], (double)value[1], (double)value[2]);
}

static int run_playback_trace(const cgltf_data *data,
                              const cgltf_animation *animation,
                              const cgltf_skin *skin,
                              cgltf_float clip_min,
                              cgltf_float clip_max,
                              const CliArgs *args,
                              PoseTransform *transforms,
                              cgltf_float *worlds) {
    FILE *csv = NULL;
    if (args->trace_csv_path != NULL) {
        csv = fopen(args->trace_csv_path, "wb");
        if (csv == NULL) {
            fprintf(stderr, "failed to open trace csv: %s\n", args->trace_csv_path);
            return 0;
        }
        fprintf(csv, "frame,time,head_x,head_y,head_z,handslot_l_x,handslot_l_y,handslot_l_z,handslot_r_x,handslot_r_y,handslot_r_z,joint_matrices\n");
    }

    const int frames = args->frames > 0 ? args->frames : 1;
    printf("playback_trace: frames=%d fps=%.3f clip=[%.5f, %.5f]\n", frames, (double)args->fps, (double)clip_min, (double)clip_max);
    for (int frame = 0; frame < frames; frame++) {
        cgltf_float time = args->sample_time;
        if (args->frames > 1) {
            if (args->fps > 0.0F) {
                time = clip_min + (cgltf_float)frame / args->fps;
                while (time > clip_max && clip_max > clip_min) {
                    time -= clip_max - clip_min;
                }
            } else {
                const cgltf_float alpha = (cgltf_float)frame / (cgltf_float)(args->frames - 1);
                time = clip_min + (clip_max - clip_min) * alpha;
            }
        }

        if (!sample_pose(data, animation, time, transforms, worlds)) {
            if (csv != NULL) {
                fclose(csv);
            }
            return 0;
        }

        cgltf_float head[3];
        cgltf_float hand_l[3];
        cgltf_float hand_r[3];
        int has_head = 0;
        int has_hand_l = 0;
        int has_hand_r = 0;
        attachment_position(data, worlds, "head", head, &has_head);
        attachment_position(data, worlds, "handslot.l", hand_l, &has_hand_l);
        attachment_position(data, worlds, "handslot.r", hand_r, &has_hand_r);
        const int has_joint_matrices = sample_joint_matrices(data, skin, worlds);

        printf("frame %03d t=%.5f head=[%.5f, %.5f, %.5f] handslot.l=[%.5f, %.5f, %.5f] handslot.r=[%.5f, %.5f, %.5f] joint_matrices=%s\n",
               frame,
               (double)time,
               (double)head[0],
               (double)head[1],
               (double)head[2],
               (double)hand_l[0],
               (double)hand_l[1],
               (double)hand_l[2],
               (double)hand_r[0],
               (double)hand_r[1],
               (double)hand_r[2],
               has_joint_matrices ? "yes" : "no");

        if (csv != NULL) {
            fprintf(csv, "%d,%.5f", frame, (double)time);
            write_csv_value(csv, head, has_head);
            write_csv_value(csv, hand_l, has_hand_l);
            write_csv_value(csv, hand_r, has_hand_r);
            fprintf(csv, ",%s\n", has_joint_matrices ? "yes" : "no");
        }
    }

    if (csv != NULL) {
        fclose(csv);
        printf("trace_csv: %s\n", args->trace_csv_path);
    }
    return 1;
}

static int sample_joint_matrices(const cgltf_data *data, const cgltf_skin *skin, const cgltf_float *worlds) {
    if (skin->inverse_bind_matrices == NULL || skin->inverse_bind_matrices->count < skin->joints_count) {
        return 0;
    }

    cgltf_float inverse_bind[16];
    cgltf_float joint_matrix[16];
    for (cgltf_size i = 0; i < skin->joints_count; i++) {
        const cgltf_node *joint = skin->joints[i];
        const cgltf_size joint_index = node_index(data, joint);
        if (joint_index >= data->nodes_count || !cgltf_accessor_read_float(skin->inverse_bind_matrices, i, inverse_bind, 16)) {
            return 0;
        }
        mat4_multiply(&worlds[joint_index * 16], inverse_bind, joint_matrix);
    }
    return 1;
}

static int parse_args(int argc, char **argv, CliArgs *args) {
    memset(args, 0, sizeof(*args));
    args->path = "gamedesign/projects/mine-cards/visual/skeletal_spike/minecards_skeletal_miner_probe.glb";
    args->sample_time = 0.5F;
    args->fps = 0.0F;
    args->frames = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--time") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--time requires seconds\n");
                return 0;
            }
            args->sample_time = (cgltf_float)strtod(argv[i + 1], NULL);
            i++;
            continue;
        }
        if (strcmp(argv[i], "--animation") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--animation requires a clip name\n");
                return 0;
            }
            args->animation_name = argv[i + 1];
            i++;
            continue;
        }
        if (strcmp(argv[i], "--frames") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--frames requires a positive integer\n");
                return 0;
            }
            args->frames = (int)strtol(argv[i + 1], NULL, 10);
            if (args->frames <= 0) {
                fprintf(stderr, "--frames requires a positive integer\n");
                return 0;
            }
            i++;
            continue;
        }
        if (strcmp(argv[i], "--fps") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--fps requires a positive number\n");
                return 0;
            }
            args->fps = (cgltf_float)strtod(argv[i + 1], NULL);
            if (args->fps <= 0.0F) {
                fprintf(stderr, "--fps requires a positive number\n");
                return 0;
            }
            i++;
            continue;
        }
        if (strcmp(argv[i], "--trace-csv") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--trace-csv requires a path\n");
                return 0;
            }
            args->trace_csv_path = argv[i + 1];
            i++;
            continue;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: mine_cards_skeletal_glb_probe [file.glb] [--time seconds] [--animation name] [--frames count] [--fps fps] [--trace-csv path]\n");
            return 0;
        }
        args->path = argv[i];
    }
    return 1;
}

int main(int argc, char **argv) {
    CliArgs args;
    if (!parse_args(argc, argv, &args)) {
        return 1;
    }

    cgltf_options options;
    memset(&options, 0, sizeof(options));

    cgltf_data *data = NULL;
    cgltf_result result = cgltf_parse_file(&options, args.path, &data);
    if (result != cgltf_result_success) {
        fprintf(stderr, "failed to parse glTF: %s (cgltf error %d)\n", args.path, (int)result);
        return 1;
    }

    result = cgltf_load_buffers(&options, data, args.path);
    if (result != cgltf_result_success) {
        fprintf(stderr, "failed to load glTF buffers: %s (cgltf error %d)\n", args.path, (int)result);
        cgltf_free(data);
        return 1;
    }

    result = cgltf_validate(data);
    if (result != cgltf_result_success) {
        fprintf(stderr, "glTF validation warning/error: %s (cgltf error %d)\n", args.path, (int)result);
    }

    printf("file: %s\n", args.path);
    printf("scene: %zu nodes, %zu meshes, %zu skin(s), %zu animation(s)\n",
           data->nodes_count,
           data->meshes_count,
           data->skins_count,
           data->animations_count);

    if (data->skins_count == 0 || data->animations_count == 0) {
        fprintf(stderr, "expected at least one skin and one animation\n");
        cgltf_free(data);
        return 1;
    }

    const cgltf_skin *skin = &data->skins[0];
    printf("skeleton: %s, %zu joints, inverse_bind_matrices=%s\n",
           name_or_fallback(skin->name, "skin_0"),
           skin->joints_count,
           skin->inverse_bind_matrices != NULL ? "yes" : "no");
    printf("joints:");
    for (cgltf_size i = 0; i < skin->joints_count; i++) {
        const cgltf_node *joint = skin->joints[i];
        printf("%s%s", i == 0 ? " " : ", ", name_or_fallback(joint->name, "unnamed"));
    }
    printf("\n");

    cgltf_size animation_index = 0;
    const cgltf_animation *animation = find_animation(data, args.animation_name, &animation_index);
    if (animation == NULL) {
        fprintf(stderr, "animation not found: %s\n", args.animation_name != NULL ? args.animation_name : "animation_0");
        print_animation_names(data);
        cgltf_free(data);
        return 1;
    }
    printf("animation: %s (#%zu), %zu channels, %zu samplers\n",
           name_or_fallback(animation->name, "animation_0"),
           animation_index,
           animation->channels_count,
           animation->samplers_count);

    PoseTransform *transforms = (PoseTransform *)calloc(data->nodes_count > 0 ? data->nodes_count : 1, sizeof(PoseTransform));
    cgltf_float *worlds = (cgltf_float *)calloc((data->nodes_count > 0 ? data->nodes_count : 1) * 16, sizeof(cgltf_float));
    if (transforms == NULL || worlds == NULL) {
        fprintf(stderr, "failed to allocate pose buffers\n");
        free(transforms);
        free(worlds);
        cgltf_free(data);
        return 1;
    }

    cgltf_float clip_min = 0.0F;
    cgltf_float clip_max = 0.0F;
    if (!animation_time_range(animation, &clip_min, &clip_max)) {
        fprintf(stderr, "failed to read animation time range\n");
        free(transforms);
        free(worlds);
        cgltf_free(data);
        return 1;
    }
    printf("clip_time: %.5fs..%.5fs\n", (double)clip_min, (double)clip_max);

    if (args.frames > 0) {
        const int ok = run_playback_trace(data, animation, skin, clip_min, clip_max, &args, transforms, worlds);
        free(transforms);
        free(worlds);
        cgltf_free(data);
        return ok ? 0 : 1;
    }

    if (!sample_pose(data, animation, args.sample_time, transforms, worlds)) {
        free(transforms);
        free(worlds);
        cgltf_free(data);
        return 1;
    }

    printf("pose: sampled at %.5fs\n", (double)args.sample_time);
    const char *attachment_names[] = {"head", "right_arm", "pickaxe", "handslot.l", "handslot.r"};
    for (size_t i = 0; i < sizeof(attachment_names) / sizeof(attachment_names[0]); i++) {
        const cgltf_node *node = find_node(data, attachment_names[i]);
        if (node == NULL) {
            continue;
        }
        const cgltf_size index = node_index(data, node);
        const cgltf_float *world = &worlds[index * 16];
        printf("attachment.%s: node=%zu world_position=[%.5f, %.5f, %.5f]\n",
               attachment_names[i],
               index,
               (double)world[12],
               (double)world[13],
               (double)world[14]);
    }

    printf("sampled_joint_matrices: %s (%zu)\n", sample_joint_matrices(data, skin, worlds) ? "yes" : "no", skin->joints_count);

    free(transforms);
    free(worlds);
    cgltf_free(data);
    return 0;
}
