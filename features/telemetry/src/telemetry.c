#include "features/telemetry/telemetry.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* One open batch and one in flight. Events land in `open`; a send moves it
 * whole into `pending` and the module waits for the answer before it sends
 * again, so at most one POST is ever out. Eight kilobytes is a hundred-odd
 * events: more than a flush interval produces, less than a page keeps. */
#define TELEMETRY_BATCH_BYTES 8192
#define TELEMETRY_ID_CHARS 32
#define TELEMETRY_RETRY_MAX 3
#define TELEMETRY_RETRY_BACKOFF_MAX_S 60.0F
#define TELEMETRY_FLUSH_DEFAULT_S 15.0F
#define TELEMETRY_HEARTBEAT_DEFAULT_S 30.0F

static struct {
    bool enabled;
    telemetry_config_t config;
    char url[512];
    char key[128];
    char game[64];
    char build[80];
    char platform[32];
    char player[TELEMETRY_ID_CHARS + 1];
    char session[TELEMETRY_ID_CHARS + 1];
    telemetry_transport_t transport;

    char open[TELEMETRY_BATCH_BYTES];
    uint32_t open_len;
    uint32_t open_events;
    bool in_event;
    bool event_has_field;
    uint32_t event_start; /* where the current event began, to roll it back */

    char body[TELEMETRY_BATCH_BYTES + 512];
    uint32_t body_len;
    void *handle;
    int retries;
    float retry_wait;
    float retry_timer;

    float since_flush;
    float clock;   /* seconds since init: the events' `t` */
    float play;    /* seconds the game called playing */
    float since_heartbeat;
    uint64_t dropped;
} s;

/* ---- ids ------------------------------------------------------------- */

static uint64_t s_rng;

static uint64_t rng_next(void) {
    /* xorshift64*: enough for an anonymous id that must only be unlikely to
     * collide across a playtest, never for anything a person is named by. */
    s_rng ^= s_rng >> 12;
    s_rng ^= s_rng << 25;
    s_rng ^= s_rng >> 27;
    return s_rng * 2685821657736338717ULL;
}

static void rng_seed(void) {
    if (s_rng != 0) return;
    uint64_t seed = (uint64_t)time(NULL) * 1000003ULL;
    seed ^= (uint64_t)clock() << 20;
    seed ^= (uint64_t)(uintptr_t)&s << 7;
    seed ^= (uint64_t)(uintptr_t)&seed;
    if (seed == 0) seed = 0x9E3779B97F4A7C15ULL;
    s_rng = seed;
    for (int i = 0; i < 8; ++i) (void)rng_next();
}

void telemetry_make_id(char out[33]) {
    static const char hex[] = "0123456789abcdef";
    rng_seed();
    for (int word = 0; word < 2; ++word) {
        uint64_t v = rng_next();
        for (int i = 0; i < 16; ++i) {
            out[word * 16 + i] = hex[v & 15U];
            v >>= 4;
        }
    }
    out[32] = '\0';
}

/* ---- JSON ------------------------------------------------------------ */

static void copy_str(char *dst, size_t cap, const char *src) {
    if (src == NULL) src = "";
    (void)snprintf(dst, cap, "%s", src);
}

static bool open_put(const char *bytes, uint32_t len) {
    if (s.open_len + len >= TELEMETRY_BATCH_BYTES) return false;
    memcpy(s.open + s.open_len, bytes, len);
    s.open_len += len;
    s.open[s.open_len] = '\0';
    return true;
}

static bool open_put_escaped(const char *text, uint32_t max_chars) {
    char buf[8];
    uint32_t count = 0;
    for (const char *p = text; *p != '\0' && count < max_chars; ++p, ++count) {
        const unsigned char c = (unsigned char)*p;
        int n;
        if (c == '"' || c == '\\') n = snprintf(buf, sizeof buf, "\\%c", c);
        else if (c < 0x20) n = snprintf(buf, sizeof buf, "\\u%04x", c);
        else { buf[0] = (char)c; n = 1; }
        if (n <= 0 || !open_put(buf, (uint32_t)n)) return false;
    }
    return true;
}

static void event_rollback(void) {
    s.open_len = s.event_start;
    s.open[s.open_len] = '\0';
    s.in_event = false;
    s.dropped += 1U;
}

void telemetry_event_begin(const char *name) {
    if (!s.enabled || name == NULL) return;
    if (s.in_event) telemetry_event_end();
    s.event_start = s.open_len;
    s.in_event = true;
    s.event_has_field = false;
    char head[96];
    const int n = snprintf(head, sizeof head, "%s{\"t\":%.2f,\"n\":\"", s.open_events > 0 ? "," : "", (double)s.clock);
    if (n <= 0 || !open_put(head, (uint32_t)n) || !open_put_escaped(name, 32) || !open_put("\"", 1)) {
        event_rollback();
    }
}

static bool field_head(const char *key) {
    if (!s.enabled || !s.in_event || key == NULL) return false;
    if (!open_put(",\"", 2) || !open_put_escaped(key, 32) || !open_put("\":", 2)) {
        event_rollback();
        return false;
    }
    return true;
}

void telemetry_int(const char *key, long long value) {
    if (!field_head(key)) return;
    char buf[32];
    const int n = snprintf(buf, sizeof buf, "%lld", value);
    if (n <= 0 || !open_put(buf, (uint32_t)n)) event_rollback();
}

void telemetry_float(const char *key, double value) {
    if (!field_head(key)) return;
    char buf[48];
    /* Two decimals: a second, a scale, a share. Anything finer is noise the
     * report never reads, and NaN is not JSON. */
    const int n = (value == value) ? snprintf(buf, sizeof buf, "%.2f", value) : snprintf(buf, sizeof buf, "0");
    if (n <= 0 || !open_put(buf, (uint32_t)n)) event_rollback();
}

void telemetry_str(const char *key, const char *value) {
    if (!field_head(key)) return;
    if (!open_put("\"", 1) || !open_put_escaped(value != NULL ? value : "", 64) || !open_put("\"", 1)) event_rollback();
}

void telemetry_event_end(void) {
    if (!s.enabled || !s.in_event) return;
    if (!open_put("}", 1)) {
        event_rollback();
        return;
    }
    s.in_event = false;
    s.open_events += 1U;
}

/* ---- sending --------------------------------------------------------- */

static void compose_body(void) {
    const long long sent_at = (long long)time(NULL) * 1000LL;
    s.body_len = (uint32_t)snprintf(s.body, sizeof s.body,
                                    "{\"v\":1,\"key\":\"%s\",\"game\":\"%s\",\"build\":\"%s\",\"platform\":\"%s\","
                                    "\"player\":\"%s\",\"session\":\"%s\",\"sent_at\":%lld,\"events\":[%s]}",
                                    s.key, s.game, s.build, s.platform, s.player, s.session, sent_at, s.open);
    if (s.body_len >= sizeof s.body) s.body_len = (uint32_t)sizeof s.body - 1U;
}

static void start_send(void) {
    if (s.in_event) telemetry_event_end();
    if (s.open_events == 0U || s.handle != NULL) return;
    compose_body();
    const uint32_t events = s.open_events;
    s.open_len = 0U;
    s.open_events = 0U;
    s.open[0] = '\0';
    s.retries = 0;
    s.retry_wait = 0.0F;
    s.since_flush = 0.0F;
    s.handle = s.transport.send(s.url, s.body, s.body_len, s.transport.userdata);
    if (s.handle == NULL) s.dropped += events;
}

static void retry_send(void) {
    s.handle = s.transport.send(s.url, s.body, s.body_len, s.transport.userdata);
    if (s.handle == NULL) s.retries = TELEMETRY_RETRY_MAX;
}

void telemetry_flush(void) {
    if (!s.enabled) return;
    start_send();
}

/* ---- lifecycle ------------------------------------------------------- */

void telemetry_init(const telemetry_config_t *config, const telemetry_transport_t *transport) {
    memset(&s, 0, sizeof s);
    if (config == NULL || config->url == NULL || config->url[0] == '\0') return;
    /* No transport is a dormant module: the core never names nt_http, so a
     * test or a tool links it without the engine. */
    if (transport == NULL || transport->send == NULL || transport->poll == NULL) return;
    s.transport = *transport;
    s.config = *config;
    copy_str(s.url, sizeof s.url, config->url);
    copy_str(s.key, sizeof s.key, config->key);
    copy_str(s.game, sizeof s.game, config->game);
    copy_str(s.build, sizeof s.build, config->build);
    copy_str(s.platform, sizeof s.platform, config->platform);
    copy_str(s.player, sizeof s.player, config->player);
    if (s.player[0] == '\0') telemetry_make_id(s.player);
    telemetry_make_id(s.session);
    s.config.flush_interval_s = config->flush_interval_s > 0.0F ? config->flush_interval_s : TELEMETRY_FLUSH_DEFAULT_S;
    s.config.heartbeat_s = config->heartbeat_s > 0.0F ? config->heartbeat_s : TELEMETRY_HEARTBEAT_DEFAULT_S;
    s.enabled = true;
}

void telemetry_shutdown(void) {
    if (s.enabled && s.handle != NULL && s.transport.release != NULL) {
        s.transport.release(s.handle, s.transport.userdata);
    }
    memset(&s, 0, sizeof s);
}

bool telemetry_enabled(void) { return s.enabled; }
uint64_t telemetry_dropped(void) { return s.dropped; }
const char *telemetry_session_id(void) { return s.session; }

void telemetry_update(float dt, bool playing) {
    if (!s.enabled || !(dt > 0.0F)) return;
    s.clock += dt;
    s.since_flush += dt;
    if (playing) {
        s.play += dt;
        s.since_heartbeat += dt;
        if (s.since_heartbeat >= s.config.heartbeat_s) {
            s.since_heartbeat -= s.config.heartbeat_s;
            telemetry_event_begin("heartbeat");
            telemetry_float("play", (double)s.play);
            telemetry_event_end();
        }
    }
    if (s.handle != NULL) {
        const int answer = s.transport.poll(s.handle, s.transport.userdata);
        if (answer == 0) return;
        if (s.transport.release != NULL) s.transport.release(s.handle, s.transport.userdata);
        s.handle = NULL;
        if (answer < 0) {
            /* The body stays composed for a retry: a collector that was down
             * for a moment loses nothing, one that is gone costs three tries. */
            s.retries += 1;
            if (s.retries >= TELEMETRY_RETRY_MAX) {
                s.dropped += 1U;
                s.body_len = 0U;
            } else {
                s.retry_wait = s.retry_wait > 0.0F ? s.retry_wait * 2.0F : s.config.flush_interval_s;
                if (s.retry_wait > TELEMETRY_RETRY_BACKOFF_MAX_S) s.retry_wait = TELEMETRY_RETRY_BACKOFF_MAX_S;
                s.retry_timer = 0.0F;
            }
        } else {
            s.body_len = 0U;
            s.retries = 0;
        }
        return;
    }
    if (s.body_len > 0U && s.retries > 0 && s.retries < TELEMETRY_RETRY_MAX) {
        s.retry_timer += dt;
        if (s.retry_timer >= s.retry_wait) retry_send();
        return;
    }
    if (s.since_flush >= s.config.flush_interval_s) start_send();
}
