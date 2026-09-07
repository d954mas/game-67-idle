#include "features/remote-image/remote_image.h"

#include "http/nt_http.h"
#include "time/nt_time.h"

#include "stb_image.h"

#include <stdlib.h>

/* The engine behind the table: nt_http for the bytes, stb_image for the
 * pixels, nt_gfx for the texture, nt_time for the retry clock. Nothing here
 * decides policy; the table does. */

static uint32_t engine_request(const char *url, void *ud) {
    (void)ud;
    return nt_http_request(url).id;
}

static remote_image_fetch_state_t engine_state(uint32_t request, void *ud) {
    (void)ud;
    switch (nt_http_state((nt_http_request_t){request})) {
    case NT_HTTP_STATE_DONE:
        return REMOTE_IMAGE_FETCH_DONE;
    case NT_HTTP_STATE_FAILED:
        return REMOTE_IMAGE_FETCH_FAILED;
    default:
        return REMOTE_IMAGE_FETCH_PENDING;
    }
}

static uint16_t engine_status(uint32_t request, void *ud) {
    (void)ud;
    return nt_http_status((nt_http_request_t){request});
}

static uint8_t *engine_take_body(uint32_t request, uint32_t *size, void *ud) {
    (void)ud;
    return nt_http_take_data((nt_http_request_t){request}, size);
}

static void engine_release(uint32_t request, void *ud) {
    (void)ud;
    nt_http_free((nt_http_request_t){request});
}

static uint8_t *engine_decode(const uint8_t *bytes, uint32_t size, uint16_t max_dim,
                              uint16_t *width, uint16_t *height, void *ud) {
    (void)ud;
    int w = 0, h = 0, channels = 0;
    if (!stbi_info_from_memory(bytes, (int)size, &w, &h, &channels) || w <= 0 || h <= 0) {
        return NULL;
    }
    /* A few kilobytes of PNG can describe a gigantic solid image; a source far
     * larger than anything shown at max_dim is a decode bomb, not a picture. */
    const int source_cap = 32 * (int)max_dim;
    if (w > source_cap || h > source_cap) {
        return NULL;
    }
    stbi_uc *pixels = stbi_load_from_memory(bytes, (int)size, &w, &h, &channels, 4);
    if (pixels == NULL) {
        return NULL;
    }
    uint8_t *fitted = remote_image_fit_rgba(pixels, (uint16_t)w, (uint16_t)h, max_dim, width, height);
    stbi_image_free(pixels);
    return fitted;
}

static uint32_t engine_make_texture(uint16_t width, uint16_t height, const uint8_t *rgba, void *ud) {
    (void)ud;
    const nt_texture_desc_t desc = {
        .width = width,
        .height = height,
        .data = rgba,
        .format = NT_TEXTURE_FORMAT_RGBA8,
        .min_filter = NT_FILTER_LINEAR,
        .mag_filter = NT_FILTER_LINEAR,
        .wrap_u = NT_WRAP_CLAMP_TO_EDGE,
        .wrap_v = NT_WRAP_CLAMP_TO_EDGE,
        .label = "remote_image",
    };
    return nt_gfx_make_texture(&desc).id;
}

static void engine_destroy_texture(uint32_t texture, void *ud) {
    (void)ud;
    nt_gfx_destroy_texture((nt_texture_t){texture});
}

static double engine_monotonic_now(void *ud) {
    (void)ud;
    return nt_time_now();
}

static const remote_image_backend_t k_engine_backend = {
    .request = engine_request,
    .state = engine_state,
    .status = engine_status,
    .take_body = engine_take_body,
    .release = engine_release,
    .decode = engine_decode,
    .make_texture = engine_make_texture,
    .destroy_texture = engine_destroy_texture,
    .monotonic_now = engine_monotonic_now,
    .userdata = NULL,
};

const remote_image_backend_t *remote_image_engine_backend(void) {
    return &k_engine_backend;
}
