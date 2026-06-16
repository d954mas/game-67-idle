#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <windows.h>
#endif

#include "stb_image.h"
#include "stb_image_resize2.h"
#include "stb_image_write.h"

typedef struct Image {
    uint8_t *pixels;
    int width;
    int height;
} Image;

typedef struct AssetSpec {
    const char *id;
    const char *source;
    const char *output;
    int width;
    int height;
    int padding;
    int slot_index;
    int slot_count;
    int tint_r;
    int tint_g;
    int tint_b;
} AssetSpec;

typedef struct SourceCache {
    const char *path;
    Image image;
} SourceCache;

typedef struct BBox {
    int left;
    int top;
    int right;
    int bottom;
} BBox;

static const char *SOURCE_DIR = "gamedesign/projects/critter-corral/art/generated/T0070";

static const AssetSpec ASSETS[] = {
    {"generated_upgrade_card", "generated-card-horizontal-source-v2.png", "card.png", 256, 128, 6, -1, 0, -1, -1, -1},
    {"generated_critter_neutral", "generated-critter-source-v1.png", "critter.png", 112, 112, 7, -1, 0, -1, -1, -1},
    {"generated_critter_a", "generated-critter-source-v1.png", "critter_a.png", 112, 112, 7, -1, 0, 255, 116, 78},
    {"generated_critter_b", "generated-critter-source-v1.png", "critter_b.png", 112, 112, 7, -1, 0, 84, 166, 255},
    {"generated_pen", "generated-pen-source-v1.png", "pen.png", 256, 200, 8, -1, 0, -1, -1, -1},
    {"generated_icon_radius", "generated-upgrade-icons-source-v1.png", "icon_radius.png", 96, 96, 4, 0, 6, -1, -1, -1},
    {"generated_icon_pull", "generated-upgrade-icons-source-v1.png", "icon_pull.png", 96, 96, 4, 1, 6, -1, -1, -1},
    {"generated_icon_second_lure", "generated-upgrade-icons-source-v1.png", "icon_second_lure.png", 96, 96, 4, 2, 6, -1, -1, -1},
    {"generated_icon_gate", "generated-upgrade-icons-source-v1.png", "icon_gate.png", 96, 96, 4, 3, 6, -1, -1, -1},
    {"generated_icon_calm", "generated-upgrade-icons-source-v1.png", "icon_calm.png", 96, 96, 4, 4, 6, -1, -1, -1},
    {"generated_icon_chain", "generated-upgrade-icons-source-v1.png", "icon_chain.png", 96, 96, 4, 5, 6, -1, -1, -1},
};

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

static int clamp_int(int value, int low, int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int abs_int(int value) {
    return value < 0 ? -value : value;
}

static bool exact_key_like(int red, int green, int blue, int tolerance) {
    int dr = abs_int(red - 255);
    int dg = abs_int(green);
    int db = abs_int(blue - 255);
    int max = dr > dg ? dr : dg;
    max = max > db ? max : db;
    return max <= tolerance;
}

static bool fringe_key_like(int red, int green, int blue) {
    return red > 170 && blue > 170 && green < 90 && red + blue > green * 4 + 260;
}

static bool purple_halo_like(int red, int green, int blue) {
    int min_rb = red < blue ? red : blue;
    return red > 75 && blue > 75 && green < 120 && min_rb - green > 20 && red + blue > green * 2 + 80;
}

static bool dark_purple_halo_like(int red, int green, int blue) {
    int min_rb = red < blue ? red : blue;
    return red >= 32 && blue >= 32 && (green < (min_rb * 55) / 100 || green <= 12) && abs_int(red - blue) < 64 && red + blue > green * 3 + 38;
}

static bool magenta_edge_spill_like(int red, int green, int blue) {
    return red > 80 && blue > 45 && green < 120 && red > green + 32 && blue > green + 6;
}

static bool dark_magenta_edge_spill_like(int red, int green, int blue) {
    return red > 44 && blue > 34 && green < 42 && red > green + 24 && blue > green + 14 && red + blue > green * 2 + 48;
}

static bool key_fringe_like(int red, int green, int blue) {
    return red > 115 && blue > 120 && green < 145 && red + blue > 300 && red + blue > green * 3;
}

static bool green_spill_like(int red, int green, int blue) {
    int max_rb = red > blue ? red : blue;
    return green > 100 && green * 100 > red * 135 && green * 100 > blue * 135 && green - max_rb > 28;
}

static bool bad_edge_like(int red, int green, int blue) {
    return key_fringe_like(red, green, blue) || purple_halo_like(red, green, blue) || dark_purple_halo_like(red, green, blue) ||
           magenta_edge_spill_like(red, green, blue) || dark_magenta_edge_spill_like(red, green, blue) || green_spill_like(red, green, blue) ||
           exact_key_like(red, green, blue, 36);
}

static bool touches_transparent(const Image *image, int x, int y, int radius) {
    int x0 = clamp_int(x - radius, 0, image->width - 1);
    int x1 = clamp_int(x + radius, 0, image->width - 1);
    int y0 = clamp_int(y - radius, 0, image->height - 1);
    int y1 = clamp_int(y + radius, 0, image->height - 1);
    for (int yy = y0; yy <= y1; ++yy) {
        for (int xx = x0; xx <= x1; ++xx) {
            const uint8_t *p = image->pixels + ((size_t)yy * (size_t)image->width + (size_t)xx) * 4u;
            if (p[3] <= 12) return true;
        }
    }
    return false;
}

static void free_image(Image *image) {
    free(image->pixels);
    image->pixels = NULL;
    image->width = 0;
    image->height = 0;
}

static Image clone_region(const Image *source, int x, int y, int width, int height) {
    Image out = {0};
    out.width = width;
    out.height = height;
    out.pixels = (uint8_t *)calloc((size_t)width * (size_t)height * 4u, 1);
    if (!out.pixels) return out;
    for (int row = 0; row < height; ++row) {
        const uint8_t *src = source->pixels + ((size_t)(y + row) * (size_t)source->width + (size_t)x) * 4u;
        uint8_t *dst = out.pixels + (size_t)row * (size_t)width * 4u;
        memcpy(dst, src, (size_t)width * 4u);
    }
    return out;
}

static bool alpha_bbox(const Image *image, BBox *bbox) {
    bbox->left = image->width;
    bbox->top = image->height;
    bbox->right = 0;
    bbox->bottom = 0;
    for (int y = 0; y < image->height; ++y) {
        for (int x = 0; x < image->width; ++x) {
            const uint8_t *p = image->pixels + ((size_t)y * (size_t)image->width + (size_t)x) * 4u;
            if (p[3] > 10) {
                if (x < bbox->left) bbox->left = x;
                if (y < bbox->top) bbox->top = y;
                if (x + 1 > bbox->right) bbox->right = x + 1;
                if (y + 1 > bbox->bottom) bbox->bottom = y + 1;
            }
        }
    }
    return bbox->right > bbox->left && bbox->bottom > bbox->top;
}

static void fast_key_to_alpha(Image *image) {
    for (int y = 0; y < image->height; ++y) {
        for (int x = 0; x < image->width; ++x) {
            uint8_t *p = image->pixels + ((size_t)y * (size_t)image->width + (size_t)x) * 4u;
            int red = p[0];
            int green = p[1];
            int blue = p[2];
            if (exact_key_like(red, green, blue, 34) || fringe_key_like(red, green, blue)) {
                p[0] = 0;
                p[1] = 0;
                p[2] = 0;
                p[3] = 0;
            }
        }
    }
}

static Image load_keyed_source(const char *source_name) {
    char path[1024];
    int written = snprintf(path, sizeof(path), "%s/%s", SOURCE_DIR, source_name);
    if (written <= 0 || written >= (int)sizeof(path)) return (Image){0};
    int width = 0;
    int height = 0;
    int channels = 0;
    uint8_t *loaded = stbi_load(path, &width, &height, &channels, 4);
    if (!loaded) return (Image){0};
    Image image = {loaded, width, height};
    fast_key_to_alpha(&image);
    return image;
}

static const Image *cached_source(SourceCache *cache, int *cache_count, const char *source_name) {
    for (int i = 0; i < *cache_count; ++i) {
        if (strcmp(cache[i].path, source_name) == 0) return &cache[i].image;
    }
    Image image = load_keyed_source(source_name);
    if (!image.pixels) return NULL;
    cache[*cache_count].path = source_name;
    cache[*cache_count].image = image;
    *cache_count += 1;
    return &cache[*cache_count - 1].image;
}

static void strict_cleanup(Image *image) {
    for (int pass = 0; pass < 3; ++pass) {
        bool changed = false;
        for (int y = 0; y < image->height; ++y) {
            for (int x = 0; x < image->width; ++x) {
                uint8_t *p = image->pixels + ((size_t)y * (size_t)image->width + (size_t)x) * 4u;
                if (p[3] <= 12) continue;
                if (bad_edge_like(p[0], p[1], p[2]) && touches_transparent(image, x, y, 6)) {
                    p[3] = 0;
                    changed = true;
                }
            }
        }
        if (!changed) break;
    }
    for (int y = 0; y < image->height; ++y) {
        for (int x = 0; x < image->width; ++x) {
            uint8_t *p = image->pixels + ((size_t)y * (size_t)image->width + (size_t)x) * 4u;
            if (p[3] <= 12) {
                p[0] = 0;
                p[1] = 0;
                p[2] = 0;
                p[3] = 0;
            }
        }
    }
}

static Image fit_to_canvas(const Image *region, int target_width, int target_height, int padding) {
    BBox bbox;
    if (!alpha_bbox(region, &bbox)) return (Image){0};
    int crop_w = bbox.right - bbox.left;
    int crop_h = bbox.bottom - bbox.top;
    Image crop = clone_region(region, bbox.left, bbox.top, crop_w, crop_h);
    if (!crop.pixels) return (Image){0};

    int max_w = target_width - padding * 2;
    int max_h = target_height - padding * 2;
    double scale_x = (double)max_w / (double)crop_w;
    double scale_y = (double)max_h / (double)crop_h;
    double scale = scale_x < scale_y ? scale_x : scale_y;
    int resized_w = clamp_int((int)((double)crop_w * scale + 0.5), 1, target_width);
    int resized_h = clamp_int((int)((double)crop_h * scale + 0.5), 1, target_height);
    uint8_t *resized = (uint8_t *)calloc((size_t)resized_w * (size_t)resized_h * 4u, 1);
    if (!resized) {
        free_image(&crop);
        return (Image){0};
    }
    (void)stbir_resize_uint8_srgb(crop.pixels, crop.width, crop.height, 0, resized, resized_w, resized_h, 0, STBIR_RGBA);
    free_image(&crop);

    Image canvas = {0};
    canvas.width = target_width;
    canvas.height = target_height;
    canvas.pixels = (uint8_t *)calloc((size_t)target_width * (size_t)target_height * 4u, 1);
    if (!canvas.pixels) {
        free(resized);
        return (Image){0};
    }
    int offset_x = (target_width - resized_w) / 2;
    int offset_y = (target_height - resized_h) / 2;
    for (int y = 0; y < resized_h; ++y) {
        uint8_t *dst = canvas.pixels + ((size_t)(offset_y + y) * (size_t)target_width + (size_t)offset_x) * 4u;
        const uint8_t *src = resized + (size_t)y * (size_t)resized_w * 4u;
        memcpy(dst, src, (size_t)resized_w * 4u);
    }
    free(resized);
    strict_cleanup(&canvas);
    return canvas;
}

static void apply_tint(Image *image, int tint_r, int tint_g, int tint_b) {
    if (tint_r < 0) return;
    for (int y = 0; y < image->height; ++y) {
        for (int x = 0; x < image->width; ++x) {
            uint8_t *p = image->pixels + ((size_t)y * (size_t)image->width + (size_t)x) * 4u;
            if (p[3] <= 12) continue;
            int luma = (p[0] * 77 + p[1] * 150 + p[2] * 29) >> 8;
            if (luma > 92) {
                p[0] = (uint8_t)((p[0] * 35 + tint_r * 65) / 100);
                p[1] = (uint8_t)((p[1] * 35 + tint_g * 65) / 100);
                p[2] = (uint8_t)((p[2] * 35 + tint_b * 65) / 100);
            }
        }
    }
    strict_cleanup(image);
}

static bool write_png_atomic(const char *path, const Image *image) {
    char tmp_path[1024];
    int written = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    if (written <= 0 || written >= (int)sizeof(tmp_path)) return false;
    if (!stbi_write_png(tmp_path, image->width, image->height, 4, image->pixels, image->width * 4)) return false;
    (void)remove(path);
    return rename(tmp_path, path) == 0;
}

static bool process_asset(const AssetSpec *spec, SourceCache *cache, int *cache_count, const char *output_dir, bool no_write) {
    const Image *source = cached_source(cache, cache_count, spec->source);
    if (!source) {
        fprintf(stderr, "missing source %s\n", spec->source);
        return false;
    }
    int x = 0;
    int y = 0;
    int width = source->width;
    int height = source->height;
    if (spec->slot_count > 0) {
        double cell_w = (double)source->width / (double)spec->slot_count;
        x = (int)(cell_w * (double)spec->slot_index + 0.5);
        int x1 = (int)(cell_w * (double)(spec->slot_index + 1) + 0.5);
        width = x1 - x;
    }
    Image region = clone_region(source, x, y, width, height);
    if (!region.pixels) return false;
    Image canvas = fit_to_canvas(&region, spec->width, spec->height, spec->padding);
    free_image(&region);
    if (!canvas.pixels) return false;
    apply_tint(&canvas, spec->tint_r, spec->tint_g, spec->tint_b);
    if (!no_write) {
        char output_path[1024];
        int written = snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, spec->output);
        if (written <= 0 || written >= (int)sizeof(output_path) || !write_png_atomic(output_path, &canvas)) {
            free_image(&canvas);
            return false;
        }
    }
    free_image(&canvas);
    return true;
}

static void usage(void) {
    fprintf(stderr, "usage: import_generated_core_assets_native [--output-dir <dir>] [--iterations N] [--no-write]\n");
}

int main(int argc, char **argv) {
    const char *output_dir = "tmp";
    int iterations = 1;
    bool no_write = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc) {
            output_dir = argv[++i];
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
#ifdef _WIN32
    (void)_mkdir(output_dir);
#endif
    double best_total = 0.0;
    for (int iteration = 0; iteration < iterations; ++iteration) {
        double started = now_seconds();
        SourceCache cache[8] = {0};
        int cache_count = 0;
        bool ok = true;
        for (size_t i = 0; i < sizeof(ASSETS) / sizeof(ASSETS[0]); ++i) {
            if (!process_asset(&ASSETS[i], cache, &cache_count, output_dir, no_write)) {
                ok = false;
                break;
            }
        }
        for (int i = 0; i < cache_count; ++i) {
            stbi_image_free(cache[i].image.pixels);
        }
        double total = now_seconds() - started;
        if (!ok) return 1;
        if (iteration == 0 || total < best_total) best_total = total;
        printf("iteration=%d assets=%zu no_write=%d total=%.6f\n", iteration + 1, sizeof(ASSETS) / sizeof(ASSETS[0]), no_write ? 1 : 0, total);
    }
    printf("best_total=%.6f\n", best_total);
    return 0;
}
