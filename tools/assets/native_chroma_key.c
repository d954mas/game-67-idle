#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "stb_image.h"
#include "stb_image_write.h"
#include "tinycthread.h"

typedef struct ChromaOptions {
    uint8_t key_r;
    uint8_t key_g;
    uint8_t key_b;
    int exact_tolerance;
    int edge_tolerance;
    int threads;
} ChromaOptions;

typedef struct RowJob {
    uint8_t *pixels;
    const uint8_t *connected;
    int width;
    int height;
    int y0;
    int y1;
    ChromaOptions options;
} RowJob;

static double now_seconds(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#endif
}

static int abs_int(int value) {
    return value < 0 ? -value : value;
}

static bool exact_key_like(uint8_t red, uint8_t green, uint8_t blue, const ChromaOptions *options, int tolerance) {
    int dr = abs_int((int)red - (int)options->key_r);
    int dg = abs_int((int)green - (int)options->key_g);
    int db = abs_int((int)blue - (int)options->key_b);
    int max = dr > dg ? dr : dg;
    max = max > db ? max : db;
    return max <= tolerance;
}

static bool key_fringe_like(int red, int green, int blue) {
    return red > 115 && blue > 120 && green < 145 && red + blue > 300 && red + blue > green * 3;
}

static bool purple_halo_like(int red, int green, int blue) {
    return red > 75 && blue > 75 && green < 120 && (red < blue ? red : blue) - green > 20 && red + blue > green * 2 + 80;
}

static bool dark_purple_like(int red, int green, int blue) {
    int min_rb = red < blue ? red : blue;
    return red >= 32 && blue >= 32 && (green < (min_rb * 55) / 100 || green <= 12) && abs_int(red - blue) < 64 && red + blue > green * 3 + 38;
}

static bool magenta_spill_like(int red, int green, int blue) {
    return red > 80 && blue > 45 && green < 120 && red > green + 32 && blue > green + 6;
}

static bool dark_magenta_spill_like(int red, int green, int blue) {
    return red > 44 && blue > 34 && green < 42 && red > green + 24 && blue > green + 14 && red + blue > green * 2 + 48;
}

static bool green_spill_like(int red, int green, int blue) {
    int max_rb = red > blue ? red : blue;
    return green > 100 && green * 100 > red * 135 && green * 100 > blue * 135 && green - max_rb > 28;
}

static bool source_key_spill_like(int red, int green, int blue, const ChromaOptions *options) {
    if (options->key_g > 220 && options->key_r < 40 && options->key_b < 40) {
        int max_rb = red > blue ? red : blue;
        bool saturated = green > 90 && green * 100 > red * 125 && green * 100 > blue * 125 && green - max_rb > 22;
        bool muted = green >= 55 && blue <= 32 && green - blue >= 40 && green - red >= 18;
        return saturated || muted;
    }
    if (options->key_r > 220 && options->key_b > 220 && options->key_g < 40) {
        return exact_key_like((uint8_t)red, (uint8_t)green, (uint8_t)blue, options, 36);
    }
    return exact_key_like((uint8_t)red, (uint8_t)green, (uint8_t)blue, options, 36);
}

static bool bad_edge_like(int red, int green, int blue, const ChromaOptions *options) {
    return key_fringe_like(red, green, blue) || purple_halo_like(red, green, blue) || dark_purple_like(red, green, blue) ||
           magenta_spill_like(red, green, blue) || dark_magenta_spill_like(red, green, blue) || green_spill_like(red, green, blue) ||
           source_key_spill_like(red, green, blue, options);
}

static bool touches_connected(const uint8_t *connected, int width, int height, int x, int y, int radius) {
    int y0 = y - radius;
    int y1 = y + radius;
    int x0 = x - radius;
    int x1 = x + radius;
    if (y0 < 0) y0 = 0;
    if (x0 < 0) x0 = 0;
    if (y1 >= height) y1 = height - 1;
    if (x1 >= width) x1 = width - 1;
    for (int yy = y0; yy <= y1; ++yy) {
        const uint8_t *row = connected + (size_t)yy * (size_t)width;
        for (int xx = x0; xx <= x1; ++xx) {
            if (row[xx]) return true;
        }
    }
    return false;
}

static bool touches_transparent(const uint8_t *pixels, int width, int height, int x, int y, int radius) {
    int y0 = y - radius;
    int y1 = y + radius;
    int x0 = x - radius;
    int x1 = x + radius;
    if (y0 < 0) y0 = 0;
    if (x0 < 0) x0 = 0;
    if (y1 >= height) y1 = height - 1;
    if (x1 >= width) x1 = width - 1;
    for (int yy = y0; yy <= y1; ++yy) {
        for (int xx = x0; xx <= x1; ++xx) {
            const uint8_t *pixel = pixels + ((size_t)yy * (size_t)width + (size_t)xx) * 4u;
            if (pixel[3] <= 12) return true;
        }
    }
    return false;
}

static int clear_connected_worker(void *arg) {
    RowJob *job = (RowJob *)arg;
    for (int y = job->y0; y < job->y1; ++y) {
        for (int x = 0; x < job->width; ++x) {
            uint8_t *pixel = job->pixels + ((size_t)y * (size_t)job->width + (size_t)x) * 4u;
            if (job->connected[(size_t)y * (size_t)job->width + (size_t)x]) {
                pixel[3] = 0;
            } else if (touches_connected(job->connected, job->width, job->height, x, y, 2)) {
                if (exact_key_like(pixel[0], pixel[1], pixel[2], &job->options, job->options.edge_tolerance)) {
                    pixel[3] = 0;
                } else {
                    int red = pixel[0];
                    int green = pixel[1];
                    int blue = pixel[2];
                    int spill = (red < blue ? red : blue) - green;
                    if (spill > 28) {
                        int new_red = red - spill / 2;
                        int new_blue = blue - spill / 2;
                        pixel[0] = (uint8_t)(new_red > green ? new_red : green);
                        pixel[2] = (uint8_t)(new_blue > green ? new_blue : green);
                    }
                }
            }
        }
    }
    return 0;
}

static int edge_cleanup_worker(void *arg) {
    RowJob *job = (RowJob *)arg;
    for (int y = job->y0; y < job->y1; ++y) {
        for (int x = 0; x < job->width; ++x) {
            uint8_t *pixel = job->pixels + ((size_t)y * (size_t)job->width + (size_t)x) * 4u;
            if (pixel[3] <= 12) continue;
            int red = pixel[0];
            int green = pixel[1];
            int blue = pixel[2];
            if (bad_edge_like(red, green, blue, &job->options) && touches_transparent(job->pixels, job->width, job->height, x, y, 2)) {
                pixel[3] = 0;
            }
        }
    }
    return 0;
}

static int zero_worker(void *arg) {
    RowJob *job = (RowJob *)arg;
    for (int y = job->y0; y < job->y1; ++y) {
        for (int x = 0; x < job->width; ++x) {
            uint8_t *pixel = job->pixels + ((size_t)y * (size_t)job->width + (size_t)x) * 4u;
            if (pixel[3] == 0) {
                pixel[0] = 0;
                pixel[1] = 0;
                pixel[2] = 0;
            }
        }
    }
    return 0;
}

static bool run_rows(uint8_t *pixels, const uint8_t *connected, int width, int height, ChromaOptions options, int (*worker)(void *)) {
    int threads = options.threads;
    if (threads < 1) threads = 1;
    if (threads > height) threads = height;
    if (threads == 1) {
        RowJob job = {pixels, connected, width, height, 0, height, options};
        return worker(&job) == 0;
    }
    thrd_t *handles = (thrd_t *)calloc((size_t)threads, sizeof(thrd_t));
    RowJob *jobs = (RowJob *)calloc((size_t)threads, sizeof(RowJob));
    if (!handles || !jobs) {
        free(handles);
        free(jobs);
        return false;
    }
    for (int index = 0; index < threads; ++index) {
        int y0 = (height * index) / threads;
        int y1 = (height * (index + 1)) / threads;
        jobs[index] = (RowJob){pixels, connected, width, height, y0, y1, options};
        if (thrd_create(&handles[index], worker, &jobs[index]) != thrd_success) {
            free(handles);
            free(jobs);
            return false;
        }
    }
    for (int index = 0; index < threads; ++index) {
        (void)thrd_join(handles[index], NULL);
    }
    free(handles);
    free(jobs);
    return true;
}

static bool build_connected_mask(const uint8_t *pixels, int width, int height, const ChromaOptions *options, uint8_t *connected, int *out_count) {
    int total = width * height;
    int *queue = (int *)malloc((size_t)total * sizeof(int));
    if (!queue) return false;
    int head = 0;
    int tail = 0;

#define PUSH_IF_KEY(px, py)                                                                                 \
    do {                                                                                                    \
        int _idx = (py) * width + (px);                                                                     \
        if (!connected[_idx]) {                                                                             \
            const uint8_t *_pixel = pixels + (size_t)_idx * 4u;                                             \
            if (exact_key_like(_pixel[0], _pixel[1], _pixel[2], options, options->exact_tolerance)) {       \
                connected[_idx] = 1;                                                                        \
                queue[tail++] = _idx;                                                                       \
            }                                                                                               \
        }                                                                                                   \
    } while (0)

    for (int x = 0; x < width; ++x) {
        PUSH_IF_KEY(x, 0);
        PUSH_IF_KEY(x, height - 1);
    }
    for (int y = 0; y < height; ++y) {
        PUSH_IF_KEY(0, y);
        PUSH_IF_KEY(width - 1, y);
    }

    while (head < tail) {
        int idx = queue[head++];
        int x = idx % width;
        int y = idx / width;
        if (x + 1 < width) PUSH_IF_KEY(x + 1, y);
        if (x > 0) PUSH_IF_KEY(x - 1, y);
        if (y + 1 < height) PUSH_IF_KEY(x, y + 1);
        if (y > 0) PUSH_IF_KEY(x, y - 1);
    }

#undef PUSH_IF_KEY
    *out_count = tail;
    free(queue);
    return true;
}

static bool write_png_atomic(const char *path, const uint8_t *pixels, int width, int height) {
    char tmp_path[4096];
    int written = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    if (written <= 0 || written >= (int)sizeof(tmp_path)) return false;
    if (!stbi_write_png(tmp_path, width, height, 4, pixels, width * 4)) return false;
    (void)remove(path);
    return rename(tmp_path, path) == 0;
}

static bool parse_key(const char *text, ChromaOptions *options) {
    const char *hex = text[0] == '#' ? text + 1 : text;
    if (strlen(hex) != 6) return false;
    unsigned int red = 0;
    unsigned int green = 0;
    unsigned int blue = 0;
    if (sscanf(hex, "%02x%02x%02x", &red, &green, &blue) != 3) return false;
    options->key_r = (uint8_t)red;
    options->key_g = (uint8_t)green;
    options->key_b = (uint8_t)blue;
    return true;
}

static void usage(void) {
    fprintf(stderr, "usage: asset_chroma_key_native --input <png> [--output <png>] [--key #ff00ff] [--threads N] [--iterations N] [--no-write]\n");
}

int main(int argc, char **argv) {
    const char *input = NULL;
    const char *output = NULL;
    int iterations = 1;
    bool no_write = false;
    ChromaOptions options = {255, 0, 255, 10, 24, 1};

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            input = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output = argv[++i];
        } else if (strcmp(argv[i], "--key") == 0 && i + 1 < argc) {
            if (!parse_key(argv[++i], &options)) {
                fprintf(stderr, "invalid --key\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            options.threads = atoi(argv[++i]);
            if (options.threads < 1) options.threads = 1;
            if (options.threads > 64) options.threads = 64;
        } else if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            iterations = atoi(argv[++i]);
            if (iterations < 1) iterations = 1;
        } else if (strcmp(argv[i], "--no-write") == 0) {
            no_write = true;
        } else {
            usage();
            return 2;
        }
    }
    if (!input) {
        usage();
        return 2;
    }

    double best_total = 0.0;
    int width = 0;
    int height = 0;
    int connected_count = 0;
    for (int iteration = 0; iteration < iterations; ++iteration) {
        int channels = 0;
        double started = now_seconds();
        uint8_t *pixels = stbi_load(input, &width, &height, &channels, 4);
        double loaded = now_seconds();
        if (!pixels) {
            fprintf(stderr, "failed to load %s\n", input);
            return 1;
        }
        uint8_t *connected = (uint8_t *)calloc((size_t)width * (size_t)height, 1);
        if (!connected) {
            stbi_image_free(pixels);
            return 1;
        }
        if (!build_connected_mask(pixels, width, height, &options, connected, &connected_count)) {
            free(connected);
            stbi_image_free(pixels);
            return 1;
        }
        double flooded = now_seconds();
        if (!run_rows(pixels, connected, width, height, options, clear_connected_worker)) return 1;
        for (int pass = 0; pass < 3; ++pass) {
            if (!run_rows(pixels, connected, width, height, options, edge_cleanup_worker)) return 1;
        }
        if (!run_rows(pixels, connected, width, height, options, zero_worker)) return 1;
        double processed = now_seconds();
        if (!no_write && output) {
            if (!write_png_atomic(output, pixels, width, height)) {
                fprintf(stderr, "failed to write %s\n", output);
                free(connected);
                stbi_image_free(pixels);
                return 1;
            }
        }
        double ended = now_seconds();
        double total = ended - started;
        if (iteration == 0 || total < best_total) best_total = total;
        printf(
            "iteration=%d input=%s size=%dx%d threads=%d connected=%d load=%.6f flood=%.6f process=%.6f write=%.6f total=%.6f\n",
            iteration + 1,
            input,
            width,
            height,
            options.threads,
            connected_count,
            loaded - started,
            flooded - loaded,
            processed - flooded,
            ended - processed,
            total);
        free(connected);
        stbi_image_free(pixels);
    }
    double megapixels = ((double)width * (double)height) / 1000000.0;
    printf("best_total=%.6f best_megapixels_per_second=%.3f\n", best_total, best_total > 0.0 ? megapixels / best_total : 0.0);
    return 0;
}
