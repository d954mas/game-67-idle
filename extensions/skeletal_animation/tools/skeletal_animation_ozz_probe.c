#include "skeletal_animation/nt_skeletal_animation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct CliArgs {
    const char *skeleton_path;
    const char *animation_path;
    const char *trace_csv_path;
    int frames;
    float fps;
} CliArgs;

static void usage(void) {
    puts("Usage: skeletal_animation_ozz_probe [--skeleton path] [--animation path] [--frames count] [--fps fps] [--trace-csv path]");
}

static int parse_args(int argc, char **argv, CliArgs *args) {
    args->skeleton_path = "gamedesign/projects/mine-cards/visual/skeletal_spike/ozz_runtime/rig_medium_skeleton.ozz";
    args->animation_path = "gamedesign/projects/mine-cards/visual/skeletal_spike/ozz_runtime/rig_medium_pickaxing.ozz";
    args->trace_csv_path = NULL;
    args->frames = 8;
    args->fps = 4.0F;

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
            if (args->fps <= 0.0F) {
                fprintf(stderr, "--fps requires a positive number\n");
                return 0;
            }
        } else if (strcmp(argv[i], "--trace-csv") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "--trace-csv requires a path\n");
                return 0;
            }
            args->trace_csv_path = argv[i];
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            return 0;
        }
    }
    return 1;
}

static void print_attachment(const char *name, const nt_skeletal_anim_attachment_sample_t *sample) {
    printf(" %s=[%.5f, %.5f, %.5f]",
           name,
           (double)sample->model_position.x,
           (double)sample->model_position.y,
           (double)sample->model_position.z);
}

static void csv_attachment(FILE *csv, const nt_skeletal_anim_attachment_sample_t *sample) {
    fprintf(csv,
            ",%.5f,%.5f,%.5f",
            (double)sample->model_position.x,
            (double)sample->model_position.y,
            (double)sample->model_position.z);
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
        fprintf(csv,
                "frame,ratio,time,head_x,head_y,head_z,handslot_l_x,handslot_l_y,"
                "handslot_l_z,handslot_r_x,handslot_r_y,handslot_r_z\n");
    }

    printf("skeleton: joints=%d\n", nt_skeletal_anim_joint_count(clip));
    printf("animation: duration=%.5f tracks=%d\n", (double)nt_skeletal_anim_duration(clip), nt_skeletal_anim_track_count(clip));
    printf("attachments: head=%d handslot.l=%d handslot.r=%d\n",
           nt_skeletal_anim_find_joint(clip, "head"),
           nt_skeletal_anim_find_joint(clip, "handslot.l"),
           nt_skeletal_anim_find_joint(clip, "handslot.r"));

    for (int frame = 0; frame < args.frames; ++frame) {
        const float time = (float)frame / args.fps;
        nt_skeletal_anim_sample_info_t info;
        nt_skeletal_anim_attachment_sample_t head;
        nt_skeletal_anim_attachment_sample_t hand_l;
        nt_skeletal_anim_attachment_sample_t hand_r;

        if (!nt_skeletal_anim_sample(clip, time, &info, error, sizeof(error)) ||
            !nt_skeletal_anim_sample_attachment(clip, "head", time, &head, error, sizeof(error)) ||
            !nt_skeletal_anim_sample_attachment(clip, "handslot.l", time, &hand_l, error, sizeof(error)) ||
            !nt_skeletal_anim_sample_attachment(clip, "handslot.r", time, &hand_r, error, sizeof(error))) {
            fprintf(stderr, "%s\n", error);
            if (csv != NULL) {
                fclose(csv);
            }
            nt_skeletal_anim_destroy(clip);
            return 1;
        }

        printf("frame %03d ratio=%.5f t=%.5f", frame, (double)info.ratio, (double)info.time_seconds);
        print_attachment("head", &head);
        print_attachment("handslot.l", &hand_l);
        print_attachment("handslot.r", &hand_r);
        printf("\n");

        if (csv != NULL) {
            fprintf(csv, "%d,%.5f,%.5f", frame, (double)info.ratio, (double)info.time_seconds);
            csv_attachment(csv, &head);
            csv_attachment(csv, &hand_l);
            csv_attachment(csv, &hand_r);
            fprintf(csv, "\n");
        }
    }

    if (csv != NULL) {
        fclose(csv);
        printf("trace_csv: %s\n", args.trace_csv_path);
    }
    nt_skeletal_anim_destroy(clip);
    return 0;
}
