/* The CRT headers go before unity: its C11 `noreturn` macro breaks the
   __declspec form this CRT's stdlib.h still uses. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "features/remote-image/remote_image.h"

/* The table against a canned transport, a canned decoder, a counting GPU and
   a fake clock: touch order, eviction, the frame-hot rule, both negative
   cache delays, the concurrency cap, and what shutdown releases. Every limit
   here is a fixture, not the game's. */

/* ---- fake backend ---- */

#define REQ_MAX 16

typedef struct {
    bool active;
    char url[128];
    remote_image_fetch_state_t state;
    uint16_t status;
    uint8_t *body;
    uint32_t size;
} fake_req_t;

static fake_req_t g_req[REQ_MAX]; /* id = index + 1 */
static int g_request_calls;
static int g_release_calls;
static int g_textures_made;
static int g_textures_destroyed;
static uint32_t g_last_destroyed;
static double g_clock;
static bool g_request_refuses;
static bool g_gpu_fails;

static uint32_t fake_request(const char *url, void *ud) {
    (void)ud;
    if (g_request_refuses) {
        return 0;
    }
    for (int i = 0; i < REQ_MAX; i++) {
        if (!g_req[i].active) {
            memset(&g_req[i], 0, sizeof g_req[i]);
            g_req[i].active = true;
            snprintf(g_req[i].url, sizeof g_req[i].url, "%s", url);
            g_request_calls++;
            return (uint32_t)i + 1;
        }
    }
    TEST_FAIL_MESSAGE("fake transport out of slots");
    return 0;
}

static remote_image_fetch_state_t fake_state(uint32_t request, void *ud) {
    (void)ud;
    return g_req[request - 1].state;
}

static uint16_t fake_status(uint32_t request, void *ud) {
    (void)ud;
    return g_req[request - 1].status;
}

static uint8_t *fake_take_body(uint32_t request, uint32_t *size, void *ud) {
    (void)ud;
    fake_req_t *r = &g_req[request - 1];
    uint8_t *body = r->body;
    *size = r->size;
    r->body = NULL;
    r->size = 0;
    return body;
}

static void fake_release(uint32_t request, void *ud) {
    (void)ud;
    fake_req_t *r = &g_req[request - 1];
    TEST_ASSERT_TRUE_MESSAGE(r->active, "release of a request that is not live");
    free(r->body);
    memset(r, 0, sizeof *r);
    g_release_calls++;
}

/* A body is "an image" when it starts with IMG; it decodes to a 2x2 square
   of the fourth byte, then fits max_dim like the real decoder would. */
static uint8_t *fake_decode(const uint8_t *bytes, uint32_t size, uint16_t max_dim,
                            uint16_t *width, uint16_t *height, void *ud) {
    (void)ud;
    if (size < 4 || memcmp(bytes, "IMG", 3) != 0) {
        return NULL;
    }
    uint8_t square[2 * 2 * 4];
    memset(square, bytes[3], sizeof square);
    return remote_image_fit_rgba(square, 2, 2, max_dim, width, height);
}

static uint32_t fake_make_texture(uint16_t width, uint16_t height, const uint8_t *rgba, void *ud) {
    (void)ud;
    TEST_ASSERT_NOT_NULL(rgba);
    TEST_ASSERT_TRUE(width > 0 && height > 0);
    if (g_gpu_fails) {
        return 0;
    }
    return (uint32_t)++g_textures_made;
}

static void fake_destroy_texture(uint32_t texture, void *ud) {
    (void)ud;
    g_textures_destroyed++;
    g_last_destroyed = texture;
}

static double fake_now(void *ud) {
    (void)ud;
    return g_clock;
}

static const remote_image_backend_t k_fake = {
    .request = fake_request,
    .state = fake_state,
    .status = fake_status,
    .take_body = fake_take_body,
    .release = fake_release,
    .decode = fake_decode,
    .make_texture = fake_make_texture,
    .destroy_texture = fake_destroy_texture,
    .monotonic_now = fake_now,
};

/* ---- fixture ---- */

#define RETRY_DELAY 5.0
#define MISSING_DELAY 60.0
#define MAX_BYTES 64u

static void boot(int capacity, int max_in_flight) {
    const remote_image_config_t cfg = {
        .capacity = capacity,
        .max_bytes = MAX_BYTES,
        .max_dim = 8,
        .retry_delay_s = RETRY_DELAY,
        .missing_delay_s = MISSING_DELAY,
        .max_in_flight = max_in_flight,
        .backend = &k_fake,
    };
    remote_image_init(&cfg);
}

void setUp(void) {
    memset(g_req, 0, sizeof g_req);
    g_request_calls = g_release_calls = g_textures_made = g_textures_destroyed = 0;
    g_last_destroyed = 0;
    g_clock = 0.0;
    g_request_refuses = false;
    g_gpu_fails = false;
}

void tearDown(void) {
    remote_image_shutdown();
    for (int i = 0; i < REQ_MAX; i++) {
        free(g_req[i].body);
        g_req[i].body = NULL;
    }
}

static fake_req_t *live_request(const char *url) {
    for (int i = 0; i < REQ_MAX; i++) {
        if (g_req[i].active && strcmp(g_req[i].url, url) == 0) {
            return &g_req[i];
        }
    }
    return NULL;
}

static bool in_flight(const char *url) {
    return live_request(url) != NULL;
}

static void respond(const char *url, uint16_t status, const char *body) {
    fake_req_t *r = live_request(url);
    TEST_ASSERT_NOT_NULL_MESSAGE(r, url);
    r->state = REMOTE_IMAGE_FETCH_DONE;
    r->status = status;
    r->size = (uint32_t)strlen(body);
    r->body = r->size > 0 ? (uint8_t *)malloc(r->size) : NULL;
    if (r->body != NULL) {
        memcpy(r->body, body, r->size);
    }
}

static void drop_connection(const char *url) {
    fake_req_t *r = live_request(url);
    TEST_ASSERT_NOT_NULL_MESSAGE(r, url);
    r->state = REMOTE_IMAGE_FETCH_FAILED;
}

/* Touch, fetch, answer, collect: the URL is READY afterwards. */
static uint32_t make_ready(const char *url) {
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_PENDING, remote_image_state(url));
    remote_image_update();
    respond(url, 200, "IMG\x80");
    remote_image_update();
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_READY, remote_image_state(url));
    uint32_t texture = remote_image_get(url).id;
    TEST_ASSERT_NOT_EQUAL(0, texture);
    return texture;
}

/* ---- states ---- */

void test_unknown_url_is_pending_and_fetches_on_update(void) {
    boot(2, 2);
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_PENDING, remote_image_state("a"));
    TEST_ASSERT_EQUAL(0, remote_image_get("a").id);
    TEST_ASSERT_EQUAL_INT(0, g_request_calls);
    remote_image_update();
    TEST_ASSERT_EQUAL_INT(1, g_request_calls);
    TEST_ASSERT_TRUE(in_flight("a"));
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_PENDING, remote_image_state("a"));
}

void test_decoded_body_becomes_a_texture(void) {
    boot(2, 2);
    uint32_t texture = make_ready("a");
    TEST_ASSERT_EQUAL_INT(1, g_textures_made);
    TEST_ASSERT_EQUAL_INT(1, g_release_calls);
    TEST_ASSERT_EQUAL_UINT32(texture, remote_image_get("a").id);
    TEST_ASSERT_EQUAL_INT(1, g_request_calls); /* READY is never re-fetched */
    remote_image_update();
    TEST_ASSERT_EQUAL_INT(1, g_request_calls);
}

void test_before_init_and_empty_url_are_failed(void) {
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_FAILED, remote_image_state("a"));
    TEST_ASSERT_EQUAL(0, remote_image_get("a").id);
    boot(2, 2);
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_FAILED, remote_image_state(NULL));
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_FAILED, remote_image_state(""));
    TEST_ASSERT_EQUAL_INT(0, g_request_calls);
}

/* ---- eviction ---- */

void test_least_recently_touched_is_evicted(void) {
    boot(2, 2);
    uint32_t tex_a = make_ready("a");
    uint32_t tex_b = make_ready("b");
    /* b was touched after a; touching a again makes b the older one */
    remote_image_get("a");
    remote_image_update();
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_PENDING, remote_image_state("c"));
    TEST_ASSERT_EQUAL_INT(1, g_textures_destroyed);
    TEST_ASSERT_EQUAL_UINT32(tex_b, g_last_destroyed);
    TEST_ASSERT_EQUAL_UINT32(tex_a, remote_image_get("a").id);
}

void test_frame_hot_entry_survives(void) {
    boot(2, 2);
    uint32_t tex_a = make_ready("a");
    uint32_t tex_b = make_ready("b");
    remote_image_update();
    /* a is the older entry, but it was touched this frame: b goes instead */
    remote_image_get("a");
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_PENDING, remote_image_state("c"));
    TEST_ASSERT_EQUAL_UINT32(tex_b, g_last_destroyed);
    TEST_ASSERT_EQUAL_UINT32(tex_a, remote_image_get("a").id);
}

void test_full_table_of_hot_entries_admits_nothing(void) {
    boot(2, 2);
    make_ready("a");
    make_ready("b");
    remote_image_update();
    remote_image_get("a");
    remote_image_get("b");
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_PENDING, remote_image_state("c"));
    TEST_ASSERT_EQUAL_INT(0, g_textures_destroyed);
    remote_image_update();
    TEST_ASSERT_EQUAL_INT(2, g_request_calls); /* c was not fetched */
    /* next frame both cool off; c evicts the one touched first */
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_PENDING, remote_image_state("c"));
    TEST_ASSERT_EQUAL_INT(1, g_textures_destroyed);
    TEST_ASSERT_EQUAL_UINT32(1, g_last_destroyed);
}

void test_evicting_a_fetch_in_flight_releases_it(void) {
    boot(1, 2);
    remote_image_state("a");
    remote_image_update();
    TEST_ASSERT_TRUE(in_flight("a"));
    remote_image_state("b");
    TEST_ASSERT_FALSE(in_flight("a"));
    TEST_ASSERT_EQUAL_INT(1, g_release_calls);
    remote_image_update();
    TEST_ASSERT_TRUE(in_flight("b"));
}

/* ---- negative cache ---- */

void test_dropped_connection_retries_after_the_retry_delay(void) {
    boot(2, 2);
    remote_image_state("a");
    remote_image_update();
    drop_connection("a");
    remote_image_update();
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_FAILED, remote_image_state("a"));
    TEST_ASSERT_EQUAL(0, remote_image_get("a").id);
    for (int frame = 0; frame < 10; frame++) {
        g_clock += RETRY_DELAY / 20.0;
        remote_image_update();
        TEST_ASSERT_EQUAL(REMOTE_IMAGE_FAILED, remote_image_state("a"));
    }
    TEST_ASSERT_EQUAL_INT(1, g_request_calls);
    g_clock = RETRY_DELAY;
    remote_image_update();
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_PENDING, remote_image_state("a"));
    remote_image_update();
    TEST_ASSERT_EQUAL_INT(2, g_request_calls);
}

void test_server_error_is_transient(void) {
    boot(2, 2);
    remote_image_state("a");
    remote_image_update();
    respond("a", 503, "");
    remote_image_update();
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_FAILED, remote_image_state("a"));
    g_clock = RETRY_DELAY;
    remote_image_update();
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_PENDING, remote_image_state("a"));
}

void test_missing_resource_waits_for_the_longer_delay(void) {
    boot(2, 2);
    remote_image_state("a");
    remote_image_update();
    respond("a", 404, "");
    remote_image_update();
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_FAILED, remote_image_state("a"));
    g_clock = RETRY_DELAY * 2.0;
    remote_image_update();
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_FAILED, remote_image_state("a"));
    remote_image_update();
    TEST_ASSERT_EQUAL_INT(1, g_request_calls);
    g_clock = MISSING_DELAY;
    remote_image_update();
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_PENDING, remote_image_state("a"));
    remote_image_update();
    TEST_ASSERT_EQUAL_INT(2, g_request_calls);
}

void test_body_that_is_not_an_image_counts_as_missing(void) {
    boot(2, 2);
    remote_image_state("a");
    remote_image_update();
    respond("a", 200, "<html>not an image</html>");
    remote_image_update();
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_FAILED, remote_image_state("a"));
    TEST_ASSERT_EQUAL_INT(0, g_textures_made);
    g_clock = RETRY_DELAY * 2.0;
    remote_image_update();
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_FAILED, remote_image_state("a"));
}

void test_oversized_body_counts_as_missing(void) {
    boot(2, 2);
    remote_image_state("a");
    remote_image_update();
    char big[MAX_BYTES + 2];
    memset(big, 'x', sizeof big);
    memcpy(big, "IMG", 3);
    big[sizeof big - 1] = '\0';
    respond("a", 200, big);
    remote_image_update();
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_FAILED, remote_image_state("a"));
    TEST_ASSERT_EQUAL_INT(0, g_textures_made);
    g_clock = RETRY_DELAY * 2.0;
    remote_image_update();
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_FAILED, remote_image_state("a"));
}

void test_gpu_refusal_is_transient(void) {
    boot(2, 2);
    g_gpu_fails = true;
    remote_image_state("a");
    remote_image_update();
    respond("a", 200, "IMG\x80");
    remote_image_update();
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_FAILED, remote_image_state("a"));
    g_gpu_fails = false;
    g_clock = RETRY_DELAY;
    remote_image_update();
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_PENDING, remote_image_state("a"));
    remote_image_update();
    respond("a", 200, "IMG\x80");
    remote_image_update();
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_READY, remote_image_state("a"));
}

/* ---- concurrency ---- */

void test_concurrency_cap_holds_and_queue_drains_in_touch_order(void) {
    boot(4, 2);
    remote_image_state("a");
    remote_image_state("b");
    remote_image_state("c");
    remote_image_state("d");
    remote_image_update();
    TEST_ASSERT_EQUAL_INT(2, g_request_calls);
    TEST_ASSERT_TRUE(in_flight("a"));
    TEST_ASSERT_TRUE(in_flight("b"));
    TEST_ASSERT_FALSE(in_flight("c"));
    remote_image_update();
    TEST_ASSERT_EQUAL_INT(2, g_request_calls);
    respond("a", 200, "IMG\x80");
    remote_image_update();
    TEST_ASSERT_EQUAL_INT(3, g_request_calls);
    TEST_ASSERT_TRUE(in_flight("c"));
    TEST_ASSERT_FALSE(in_flight("d"));
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_PENDING, remote_image_state("d"));
}

void test_refused_request_stays_queued(void) {
    boot(2, 2);
    g_request_refuses = true;
    remote_image_state("a");
    remote_image_update();
    TEST_ASSERT_EQUAL_INT(0, g_request_calls);
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_PENDING, remote_image_state("a"));
    g_request_refuses = false;
    remote_image_update();
    TEST_ASSERT_EQUAL_INT(1, g_request_calls);
    TEST_ASSERT_TRUE(in_flight("a"));
}

/* ---- lifecycle ---- */

void test_shutdown_releases_flights_and_textures(void) {
    boot(3, 2);
    make_ready("a");
    remote_image_state("b");
    remote_image_update();
    int releases_before = g_release_calls;
    remote_image_shutdown();
    TEST_ASSERT_EQUAL_INT(1, g_textures_destroyed);
    TEST_ASSERT_EQUAL_INT(releases_before + 1, g_release_calls);
    TEST_ASSERT_FALSE(in_flight("b"));
    TEST_ASSERT_EQUAL(REMOTE_IMAGE_FAILED, remote_image_state("a"));
    remote_image_shutdown(); /* a second shutdown is inert */
}

/* ---- resample ---- */

void test_fit_keeps_small_images_and_copies_them(void) {
    uint8_t px[4] = {1, 2, 3, 4};
    uint16_t w = 0, h = 0;
    uint8_t *out = remote_image_fit_rgba(px, 1, 1, 8, &w, &h);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_TRUE(out != px);
    TEST_ASSERT_EQUAL_UINT16(1, w);
    TEST_ASSERT_EQUAL_UINT16(1, h);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(px, out, 4);
    free(out);
}

void test_fit_shrinks_to_max_dim_and_keeps_aspect(void) {
    enum { W = 300, H = 100 };
    uint8_t *px = (uint8_t *)malloc(W * H * 4);
    memset(px, 200, W * H * 4);
    uint16_t w = 0, h = 0;
    uint8_t *out = remote_image_fit_rgba(px, W, H, 128, &w, &h);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_UINT16(128, w);
    TEST_ASSERT_TRUE(h >= 1 && h <= 128);
    TEST_ASSERT_TRUE(h * 3 <= w && w <= h * 3 + 3); /* 3:1 within integer rounding */
    for (size_t i = 0; i < (size_t)w * (size_t)h * 4; i++) {
        TEST_ASSERT_EQUAL_UINT8(200, out[i]); /* a flat image stays flat */
    }
    free(out);
    free(px);
}

void test_fit_averages_the_covered_block(void) {
    uint8_t px[2 * 4] = {0, 0, 0, 0, 255, 255, 255, 255};
    uint16_t w = 0, h = 0;
    uint8_t *out = remote_image_fit_rgba(px, 2, 1, 1, &w, &h);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_UINT16(1, w);
    TEST_ASSERT_EQUAL_UINT16(1, h);
    TEST_ASSERT_TRUE(out[0] >= 127 && out[0] <= 128);
    free(out);
}

void test_fit_never_yields_a_zero_side(void) {
    enum { W = 1000, H = 1 };
    uint8_t *px = (uint8_t *)calloc(W * H * 4, 1);
    uint16_t w = 0, h = 0;
    uint8_t *out = remote_image_fit_rgba(px, W, H, 16, &w, &h);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_UINT16(16, w);
    TEST_ASSERT_EQUAL_UINT16(1, h);
    free(out);
    free(px);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unknown_url_is_pending_and_fetches_on_update);
    RUN_TEST(test_decoded_body_becomes_a_texture);
    RUN_TEST(test_before_init_and_empty_url_are_failed);
    RUN_TEST(test_least_recently_touched_is_evicted);
    RUN_TEST(test_frame_hot_entry_survives);
    RUN_TEST(test_full_table_of_hot_entries_admits_nothing);
    RUN_TEST(test_evicting_a_fetch_in_flight_releases_it);
    RUN_TEST(test_dropped_connection_retries_after_the_retry_delay);
    RUN_TEST(test_server_error_is_transient);
    RUN_TEST(test_missing_resource_waits_for_the_longer_delay);
    RUN_TEST(test_body_that_is_not_an_image_counts_as_missing);
    RUN_TEST(test_oversized_body_counts_as_missing);
    RUN_TEST(test_gpu_refusal_is_transient);
    RUN_TEST(test_concurrency_cap_holds_and_queue_drains_in_touch_order);
    RUN_TEST(test_refused_request_stays_queued);
    RUN_TEST(test_shutdown_releases_flights_and_textures);
    RUN_TEST(test_fit_keeps_small_images_and_copies_them);
    RUN_TEST(test_fit_shrinks_to_max_dim_and_keeps_aspect);
    RUN_TEST(test_fit_averages_the_covered_block);
    RUN_TEST(test_fit_never_yields_a_zero_side);
    return UNITY_END();
}
