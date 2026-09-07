#ifndef FEATURES_REMOTE_IMAGE_H
#define FEATURES_REMOTE_IMAGE_H

#include "graphics/nt_gfx.h"

#include <stdbool.h>
#include <stdint.h>

/* A small cache that turns a URL into a texture and forgets it again.
 *
 * A fixed table keyed by URL. Asking about a URL is a touch; a URL nobody
 * asked about this frame is the only thing eviction may drop, least recently
 * touched first. A full table whose every entry is frame-hot admits nothing
 * new until a frame passes without a touch on one of them, so the capacity
 * is the size of one screen with headroom, not a memory budget.
 *
 * Failures are cached negatively and retried on one of two schedules: a
 * transport error or a server error is transient, a 4xx or a body that is
 * not an image is not, and asking again soon will not change the answer. */

typedef enum {
    REMOTE_IMAGE_PENDING = 0, /* queued or in flight: show a spinner */
    REMOTE_IMAGE_READY,       /* remote_image_get answers a texture */
    REMOTE_IMAGE_FAILED,      /* cached failure, retried after its delay: show a fallback */
} remote_image_state_t;

typedef enum {
    REMOTE_IMAGE_FETCH_PENDING = 0,
    REMOTE_IMAGE_FETCH_DONE,   /* a response arrived, any HTTP status */
    REMOTE_IMAGE_FETCH_FAILED, /* transport error, no usable body */
} remote_image_fetch_state_t;

/* I/O, decoding, GPU upload and the clock behind the table. NULL in the
 * config selects the engine (`nt_http`, `stb_image`, `nt_gfx`, `nt_time`);
 * tests hand in a canned one. */
typedef struct {
    uint32_t (*request)(const char *url, void *userdata); /* 0 when it could not start */
    remote_image_fetch_state_t (*state)(uint32_t request, void *userdata);
    uint16_t (*status)(uint32_t request, void *userdata); /* HTTP status; 0 when unknown */
    /* The body, malloc'd for the caller; NULL when empty or not done. */
    uint8_t *(*take_body)(uint32_t request, uint32_t *size, void *userdata);
    void (*release)(uint32_t request, void *userdata);
    /* RGBA8 pixels, malloc'd for the caller, both sides at most `max_dim`;
     * NULL when the bytes are not an image. */
    uint8_t *(*decode)(const uint8_t *bytes, uint32_t size, uint16_t max_dim,
                       uint16_t *width, uint16_t *height, void *userdata);
    uint32_t (*make_texture)(uint16_t width, uint16_t height, const uint8_t *rgba, void *userdata); /* 0 on failure */
    void (*destroy_texture)(uint32_t texture, void *userdata);
    double (*monotonic_now)(void *userdata); /* seconds; drives the retry delays */
    void *userdata;
} remote_image_backend_t;

/* Every limit arrives here; the pack ships no default for any of them. */
typedef struct {
    int capacity;            /* table entries, > 0; one screen of images plus headroom */
    uint32_t max_bytes;      /* a larger body is not an image */
    uint16_t max_dim;        /* decoded images are scaled down to fit, > 0 */
    double retry_delay_s;    /* after a transport error or a 5xx */
    double missing_delay_s;  /* after a 4xx or a body that does not decode */
    int max_in_flight;       /* concurrent fetches, > 0; the http slots are shared with other systems */
    const remote_image_backend_t *backend; /* NULL: the engine */
} remote_image_config_t;

void remote_image_init(const remote_image_config_t *cfg);
/* A touch: an unknown URL enters the table (evicting if it must) and starts
 * PENDING; a FAILED URL whose delay has passed is queued again. */
remote_image_state_t remote_image_state(const char *url);
/* Also a touch. INVALID unless READY. */
nt_texture_t remote_image_get(const char *url);
/* Once per frame: starts queued fetches up to the cap, collects finished
 * ones, decodes and uploads. Closes the frame for the eviction rule. */
void remote_image_update(void);
void remote_image_shutdown(void);

/* Area-average downscale of RGBA8 so that both sides fit `max_dim`; a
 * malloc'd copy the caller frees, NULL on allocation failure. Returns a copy
 * even when nothing had to shrink so ownership is the same on both paths. */
uint8_t *remote_image_fit_rgba(const uint8_t *rgba, uint16_t width, uint16_t height, uint16_t max_dim,
                               uint16_t *out_width, uint16_t *out_height);

#endif /* FEATURES_REMOTE_IMAGE_H */
