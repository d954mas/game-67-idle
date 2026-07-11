#include "skeletal_animation/nt_skeletal_animation.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct CliArgs {
    const char *skeleton_path;
    const char *animation_path;
    const char *trace_csv_path;
    int frames;
    float fps;
    int use_known_wrap_time;
    float known_wrap_time;
} CliArgs;

static void usage(void) {
    puts("Usage: experimental_skeletal_animation_ozz_probe [--skeleton path] [--animation path] [--frames count] [--fps fps] [--trace-csv path] [--known-wrap-time seconds]");
}

static int parse_args(int argc, char **argv, CliArgs *args) {
    args->skeleton_path = NULL;
    args->animation_path = NULL;
    args->trace_csv_path = NULL;
    args->frames = 8;
    args->fps = 4.0F;
    args->use_known_wrap_time = 0;
    args->known_wrap_time = 0.0F;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        }
        if (strcmp(argv[i], "--skeleton") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "--skeleton requires a path\n");
                return 0;
            }
            args->skeleton_path = argv[i];
        } else if (strcmp(argv[i], "--animation") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "--animation requires a path\n");
                return 0;
            }
            args->animation_path = argv[i];
        } else if (strcmp(argv[i], "--frames") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "--frames requires a positive integer\n");
                return 0;
            }
            args->frames = atoi(argv[i]);
            if (args->frames <= 0) {
                fprintf(stderr, "--frames requires a positive integer\n");
                return 0;
            }
        } else if (strcmp(argv[i], "--fps") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "--fps requires a positive number\n");
                return 0;
            }
            args->fps = (float)atof(argv[i]);
            if (!isfinite(args->fps) || args->fps <= 0.0F) {
                fprintf(stderr, "--fps requires a finite positive number\n");
                return 0;
            }
        } else if (strcmp(argv[i], "--trace-csv") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "--trace-csv requires a path\n");
                return 0;
            }
            args->trace_csv_path = argv[i];
        } else if (strcmp(argv[i], "--known-wrap-time") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "--known-wrap-time requires a number\n");
                return 0;
            }
            args->known_wrap_time = (float)atof(argv[i]);
            args->use_known_wrap_time = 1;
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            return 0;
        }
    }
    if (args->skeleton_path == NULL || args->animation_path == NULL) {
        fprintf(stderr, "--skeleton and --animation are required\n");
        usage();
        return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    CliArgs args;
    if (!parse_args(argc, argv, &args)) {
        return 1;
    }

    char error[512];
    nt_skeletal_anim_clip_t *clip = NULL;
    if (!nt_skeletal_anim_load_ozz(args.skeleton_path, args.animation_path, &clip, error, sizeof(error))) {
        fprintf(stderr, "%s\n", error);
        return 1;
    }

    FILE *csv = NULL;
    if (args.trace_csv_path != NULL) {
        csv = fopen(args.trace_csv_path, "wb");
        if (csv == NULL) {
            fprintf(stderr, "failed to open trace csv: %s\n", args.trace_csv_path);
            nt_skeletal_anim_destroy(clip);
            return 1;
        }
        fprintf(csv, "frame,ratio,time\n");
    }

    printf("skeleton: joints=%d\n", nt_skeletal_anim_joint_count(clip));
    printf("animation: duration=%.5f tracks=%d\n", (double)nt_skeletal_anim_duration(clip), nt_skeletal_anim_track_count(clip));
    const int joint_count = nt_skeletal_anim_joint_count(clip);
    float *pre_sample_matrices = (float *)calloc((size_t)joint_count * 16U, sizeof(float));
    if (pre_sample_matrices == NULL ||
        nt_skeletal_anim_copy_model_matrices(clip, pre_sample_matrices, joint_count, error, sizeof(error)) != joint_count) {
        fprintf(stderr, "pre-sample matrix defect was not reproduced: %s\n", error);
        free(pre_sample_matrices);
        nt_skeletal_anim_destroy(clip);
        return 1;
    }
    free(pre_sample_matrices);
    puts("KNOWN_DEFECT reproduced: model matrices accepted before first sample");
    for (int frame = 0; frame < args.frames; ++frame) {
        const float time = args.use_known_wrap_time ? args.known_wrap_time : (float)frame / args.fps;
        nt_skeletal_anim_sample_info_t info;

        if (!nt_skeletal_anim_sample(clip, time, &info, error, sizeof(error))) {
            fprintf(stderr, "%s\n", error);
            if (csv != NULL) {
                fclose(csv);
            }
            nt_skeletal_anim_destroy(clip);
            return 1;
        }

        printf("frame %03d ratio=%.5f t=%.5f\n", frame, (double)info.ratio, (double)info.time_seconds);

        if (csv != NULL) {
            fprintf(csv, "%d,%.5f,%.5f\n", frame, (double)info.ratio, (double)info.time_seconds);
        }
    }

    if (csv != NULL) {
        fclose(csv);
        printf("trace_csv: %s\n", args.trace_csv_path);
    }
    nt_skeletal_anim_destroy(clip);
    return 0;
}
