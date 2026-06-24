#include "blockside_capture.h"

#include "core/nt_core.h"
#include "window/nt_window.h"

#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
#include <glad/gl.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool s_capture_pending;
static char s_capture_output[260];

bool blockside_capture_request(const char *output_path) {
    if (!output_path || output_path[0] == '\0' || strlen(output_path) >= sizeof(s_capture_output)) {
        return false;
    }

    (void)snprintf(s_capture_output, sizeof(s_capture_output), "%s", output_path);
    s_capture_pending = true;
    return true;
}

bool blockside_capture_write_pending(void) {
    if (!s_capture_pending) {
        return true;
    }
    s_capture_pending = false;

    const uint32_t width = g_nt_window.fb_width;
    const uint32_t height = g_nt_window.fb_height;
    if (width == 0 || height == 0 || s_capture_output[0] == '\0') {
        return false;
    }

    const size_t row_bytes = (size_t)width * 3U;
    const size_t byte_count = row_bytes * (size_t)height;
    uint8_t *pixels = (uint8_t *)malloc(byte_count);
    if (!pixels) {
        return false;
    }

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, (GLsizei)width, (GLsizei)height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    FILE *file = fopen(s_capture_output, "wb");
    if (!file) {
        free(pixels);
        return false;
    }

    (void)fprintf(file, "P6\n%u %u\n255\n", width, height);
    /* DevAPI capture boundary: GL readback is bottom-left, PPM/PNG rows are top-left. */
    for (uint32_t y = 0; y < height; ++y) {
        const uint32_t src_y = height - 1U - y;
        (void)fwrite(pixels + (size_t)src_y * row_bytes, 1, row_bytes, file);
    }
    (void)fclose(file);
    free(pixels);
    return true;
}
#else
bool blockside_capture_request(const char *output_path) {
    (void)output_path;
    return false;
}

bool blockside_capture_write_pending(void) {
    return true;
}
#endif
