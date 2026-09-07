#include "features/remote-image/remote_image.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#if !defined(REMOTE_IMAGE_TESTING)
const remote_image_backend_t *remote_image_engine_backend(void);
#endif

typedef enum {
    ENTRY_QUEUED = 0, /* wanted, waiting for a fetch slot */
    ENTRY_IN_FLIGHT,
    ENTRY_READY,
    ENTRY_FAILED,
} entry_state_t;

typedef struct {
    char *url; /* NULL: free slot */
    entry_state_t state;
    uint32_t request; /* IN_FLIGHT */
    uint32_t texture; /* READY */
    double retry_at;  /* FAILED */
    uint64_t touch_seq;
    uint64_t touch_frame;
} entry_t;

static struct {
    bool initialised;
    remote_image_config_t cfg;
    const remote_image_backend_t *be;
    entry_t *entries;
    uint64_t frame; /* the frame a touch stamps; update closes it */
    uint64_t seq;   /* touch order across frames */
    double now;     /* read once per update so every decision in a frame agrees */
    int in_flight;
} g;

/* ---- table ---- */

static char *dup_string(const char *s) {
    size_t n = strlen(s) + 1;
    char *copy = (char *)malloc(n);
    if (copy != NULL) {
        memcpy(copy, s, n);
    }
    return copy;
}

static void drop_entry(entry_t *e) {
    if (e->state == ENTRY_IN_FLIGHT) {
        g.be->release(e->request, g.be->userdata);
        g.in_flight--;
    } else if (e->state == ENTRY_READY) {
        g.be->destroy_texture(e->texture, g.be->userdata);
    }
    free(e->url);
    memset(e, 0, sizeof *e);
}

static void stamp(entry_t *e) {
    e->touch_seq = ++g.seq;
    e->touch_frame = g.frame;
}

static entry_t *find_entry(const char *url) {
    for (int i = 0; i < g.cfg.capacity; i++) {
        entry_t *e = &g.entries[i];
        if (e->url != NULL && strcmp(e->url, url) == 0) {
            return e;
        }
    }
    return NULL;
}

/* A free slot, else the least recently touched entry nobody asked about this
 * frame. NULL when every entry is frame-hot: dropping one of those would
 * only make the same frame fetch it again. */
static entry_t *claim_slot(void) {
    entry_t *victim = NULL;
    for (int i = 0; i < g.cfg.capacity; i++) {
        entry_t *e = &g.entries[i];
        if (e->url == NULL) {
            return e;
        }
        if (e->touch_frame == g.frame) {
            continue;
        }
        if (victim == NULL || e->touch_seq < victim->touch_seq) {
            victim = e;
        }
    }
    if (victim != NULL) {
        drop_entry(victim);
    }
    return victim;
}

static entry_t *touch(const char *url) {
    entry_t *e = find_entry(url);
    if (e == NULL) {
        e = claim_slot();
        if (e == NULL) {
            return NULL;
        }
        e->url = dup_string(url);
        if (e->url == NULL) {
            return NULL;
        }
        e->state = ENTRY_QUEUED;
    }
    stamp(e);
    if (e->state == ENTRY_FAILED && g.now >= e->retry_at) {
        e->state = ENTRY_QUEUED;
    }
    return e;
}

static void fail_entry(entry_t *e, double delay_s) {
    e->state = ENTRY_FAILED;
    e->retry_at = g.now + delay_s;
}

/* ---- per-frame work ---- */

static void finish_fetch(entry_t *e) {
    const remote_image_backend_t *be = g.be;
    uint16_t status = be->status(e->request, be->userdata);
    uint32_t size = 0;
    uint8_t *body = be->take_body(e->request, &size, be->userdata);
    be->release(e->request, be->userdata);
    g.in_flight--;
    e->request = 0;

    if (status >= 500 || status == 429) {
        free(body);
        fail_entry(e, g.cfg.retry_delay_s);
        return;
    }
    if (status >= 400 || body == NULL || size > g.cfg.max_bytes) {
        free(body);
        fail_entry(e, g.cfg.missing_delay_s);
        return;
    }
    uint16_t width = 0, height = 0;
    uint8_t *pixels = be->decode(body, size, g.cfg.max_dim, &width, &height, be->userdata);
    free(body);
    if (pixels == NULL || width == 0 || height == 0) {
        free(pixels);
        fail_entry(e, g.cfg.missing_delay_s);
        return;
    }
    uint32_t texture = be->make_texture(width, height, pixels, be->userdata);
    free(pixels);
    if (texture == 0) {
        fail_entry(e, g.cfg.retry_delay_s);
        return;
    }
    e->state = ENTRY_READY;
    e->texture = texture;
}

static void collect_finished(void) {
    for (int i = 0; i < g.cfg.capacity; i++) {
        entry_t *e = &g.entries[i];
        if (e->url == NULL || e->state != ENTRY_IN_FLIGHT) {
            continue;
        }
        switch (g.be->state(e->request, g.be->userdata)) {
        case REMOTE_IMAGE_FETCH_DONE:
            finish_fetch(e);
            break;
        case REMOTE_IMAGE_FETCH_FAILED:
            g.be->release(e->request, g.be->userdata);
            g.in_flight--;
            e->request = 0;
            fail_entry(e, g.cfg.retry_delay_s);
            break;
        default:
            break;
        }
    }
}

/* Earliest touch first: rows are touched in draw order, so the top of the
 * screen loads before the bottom. */
static entry_t *next_queued(void) {
    entry_t *best = NULL;
    for (int i = 0; i < g.cfg.capacity; i++) {
        entry_t *e = &g.entries[i];
        if (e->url != NULL && e->state == ENTRY_QUEUED && (best == NULL || e->touch_seq < best->touch_seq)) {
            best = e;
        }
    }
    return best;
}

static void start_queued(void) {
    while (g.in_flight < g.cfg.max_in_flight) {
        entry_t *e = next_queued();
        if (e == NULL) {
            return;
        }
        uint32_t request = g.be->request(e->url, g.be->userdata);
        if (request == 0) {
            return; /* every http slot is busy elsewhere; the queue waits a frame */
        }
        e->state = ENTRY_IN_FLIGHT;
        e->request = request;
        g.in_flight++;
    }
}

/* ---- public ---- */

static bool config_valid(const remote_image_config_t *cfg) {
    return cfg != NULL && cfg->capacity > 0 && cfg->max_dim > 0 && cfg->max_in_flight > 0 &&
           cfg->retry_delay_s >= 0.0 && cfg->missing_delay_s >= 0.0;
}

void remote_image_init(const remote_image_config_t *cfg) {
    remote_image_shutdown();
    assert(config_valid(cfg));
    if (!config_valid(cfg)) {
        return;
    }
    g.cfg = *cfg;
    g.be = cfg->backend;
#if !defined(REMOTE_IMAGE_TESTING)
    if (g.be == NULL) {
        g.be = remote_image_engine_backend();
    }
#endif
    assert(g.be != NULL);
    if (g.be == NULL) {
        return;
    }
    g.entries = (entry_t *)calloc((size_t)cfg->capacity, sizeof *g.entries);
    if (g.entries == NULL) {
        return;
    }
    g.frame = 1;
    g.seq = 0;
    g.now = g.be->monotonic_now(g.be->userdata);
    g.in_flight = 0;
    g.initialised = true;
}

remote_image_state_t remote_image_state(const char *url) {
    if (!g.initialised || url == NULL || url[0] == '\0') {
        return REMOTE_IMAGE_FAILED;
    }
    entry_t *e = touch(url);
    if (e == NULL) {
        return REMOTE_IMAGE_PENDING;
    }
    switch (e->state) {
    case ENTRY_READY:
        return REMOTE_IMAGE_READY;
    case ENTRY_FAILED:
        return REMOTE_IMAGE_FAILED;
    default:
        return REMOTE_IMAGE_PENDING;
    }
}

nt_texture_t remote_image_get(const char *url) {
    if (!g.initialised || url == NULL || url[0] == '\0') {
        return (nt_texture_t){0};
    }
    entry_t *e = touch(url);
    if (e == NULL || e->state != ENTRY_READY) {
        return (nt_texture_t){0};
    }
    return (nt_texture_t){e->texture};
}

void remote_image_update(void) {
    if (!g.initialised) {
        return;
    }
    g.now = g.be->monotonic_now(g.be->userdata);
    collect_finished();
    start_queued();
    g.frame++;
}

void remote_image_shutdown(void) {
    if (!g.initialised) {
        return;
    }
    for (int i = 0; i < g.cfg.capacity; i++) {
        if (g.entries[i].url != NULL) {
            drop_entry(&g.entries[i]);
        }
    }
    free(g.entries);
    memset(&g, 0, sizeof g);
}

/* ---- resample ---- */

uint8_t *remote_image_fit_rgba(const uint8_t *rgba, uint16_t width, uint16_t height, uint16_t max_dim,
                               uint16_t *out_width, uint16_t *out_height) {
    if (rgba == NULL || width == 0 || height == 0 || max_dim == 0) {
        return NULL;
    }
    const uint32_t src_w = width, src_h = height, limit = max_dim;
    uint32_t dst_w = src_w, dst_h = src_h;
    uint32_t longest = src_w > src_h ? src_w : src_h;
    if (longest > limit) {
        /* Integer math on the longest side keeps both results at most max_dim. */
        dst_w = (src_w * limit) / longest;
        dst_h = (src_h * limit) / longest;
        if (dst_w == 0) dst_w = 1;
        if (dst_h == 0) dst_h = 1;
    }
    uint8_t *out = (uint8_t *)malloc((size_t)dst_w * dst_h * 4);
    if (out == NULL) {
        return NULL;
    }
    if (dst_w == src_w && dst_h == src_h) {
        memcpy(out, rgba, (size_t)src_w * src_h * 4);
    } else {
        for (uint32_t y = 0; y < dst_h; y++) {
            uint32_t sy0 = (y * src_h) / dst_h;
            uint32_t sy1 = ((y + 1) * src_h) / dst_h;
            if (sy1 <= sy0) sy1 = sy0 + 1;
            for (uint32_t x = 0; x < dst_w; x++) {
                uint32_t sx0 = (x * src_w) / dst_w;
                uint32_t sx1 = ((x + 1) * src_w) / dst_w;
                if (sx1 <= sx0) sx1 = sx0 + 1;
                uint32_t sum[4] = {0, 0, 0, 0};
                for (uint32_t sy = sy0; sy < sy1; sy++) {
                    const uint8_t *row = rgba + ((size_t)sy * src_w + sx0) * 4;
                    for (uint32_t sx = sx0; sx < sx1; sx++, row += 4) {
                        sum[0] += (uint32_t)row[0];
                        sum[1] += (uint32_t)row[1];
                        sum[2] += (uint32_t)row[2];
                        sum[3] += (uint32_t)row[3];
                    }
                }
                uint32_t count = (sx1 - sx0) * (sy1 - sy0);
                uint8_t *dst = out + ((size_t)y * dst_w + x) * 4;
                for (int c = 0; c < 4; c++) {
                    dst[c] = (uint8_t)((sum[c] + count / 2) / count);
                }
            }
        }
    }
    *out_width = (uint16_t)dst_w;
    *out_height = (uint16_t)dst_h;
    return out;
}
