#include "render/capture.h"

#include "window/nt_window.h"

#ifndef NT_PLATFORM_WEB
#include <glad/gl.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void capture_write_ppm(const char *path) {
    const uint32_t w = g_nt_window.fb_width;
    const uint32_t h = g_nt_window.fb_height;
    if (w == 0 || h == 0 || !path || path[0] == '\0') {
        return;
    }
    const size_t row = (size_t)w * 3U;
    uint8_t *pixels = (uint8_t *)malloc(row * (size_t)h);
    if (!pixels) {
        return;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, (GLsizei)w, (GLsizei)h, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    FILE *f = fopen(path, "wb");
    if (f) {
        (void)fprintf(f, "P6\n%u %u\n255\n", w, h);
        // GL readback is bottom-left; PPM rows are top-left -> flip.
        for (uint32_t y = 0; y < h; ++y) {
            (void)fwrite(pixels + (size_t)(h - 1U - y) * row, 1, row, f);
        }
        (void)fclose(f);
    }
    free(pixels);
}
#else
void capture_write_ppm(const char *path) { (void)path; }
#endif
