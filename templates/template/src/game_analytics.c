/* game_analytics.c -- local analytics writer over the per-frame event bus
   (event_system_design §4). A RECORD-phase recorder that renders EVERY frame event via the
   E3 renderer (game_event_render) into an NDJSON stream and buffers it: native appends to
   build/analytics/session-<wall>-<pid>.ndjson; web keeps an in-memory ring; the ctest
   injects a capture sink + clock. Append-only stream, NOT a game_storage save slot (native
   replace = O(n^2) on a growing log; web localStorage is a shared itch quota). Single
   thread; all state is file-static. Compiled only when GAME_ANALYTICS_ENABLED (CMake). */

#include "game_analytics.h"

#if FEATURE_GAME_ANALYTICS

#include <inttypes.h> /* PRId64 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> /* malloc (web export) */
#include <string.h> /* memcpy, memmove, memchr, strlen */

#include "cJSON.h"
#include "core/nt_assert.h"
#include "game_event_render.h" /* game_event_render (E3, reused) */
#include "game_events.h"       /* game_event_log */
#include "hash/nt_hash.h"      /* nt_hash64_str */
#include "log/nt_log.h"        /* nt_log_warn */

#if defined(__EMSCRIPTEN__)
#include <emscripten.h> /* EM_JS (web wall clock) */
#else
#include <errno.h> /* EEXIST (segmented mkdir) */
#if defined(_WIN32)
#include <direct.h>  /* _mkdir */
#include <process.h> /* _getpid */
#else
#include <sys/stat.h> /* mkdir */
#include <unistd.h>   /* getpid */
#endif
#if !defined(GAME_ANALYTICS_TESTING)
#include <time.h> /* time (native wall clock) */
#endif
#endif

/* GAME_STORAGE_APP_ID is a global compile-define for the game/test targets; a guarded
   default keeps this TU self-contained (mirror game_save.c). */
#ifndef GAME_STORAGE_APP_ID
#define GAME_STORAGE_APP_ID "template"
#endif

/* ---- config (#define with defaults; overridable per-game / by the ctest) ---- */
#ifndef GAME_ANALYTICS_LINE_MAX /* cap of one rendered line (== E3 tail entry) */
#define GAME_ANALYTICS_LINE_MAX 512
#endif
#ifndef GAME_ANALYTICS_BUF_BYTES /* accumulating line buffer */
#define GAME_ANALYTICS_BUF_BYTES (16u * 1024u)
#endif
#ifndef GAME_ANALYTICS_FLUSH_BYTES /* flush-to-sink threshold (< BUF_BYTES) */
#define GAME_ANALYTICS_FLUSH_BYTES (12u * 1024u)
#endif
#ifndef GAME_ANALYTICS_MAX_BYTES /* session byte cap (native file); 0 = uncapped */
#define GAME_ANALYTICS_MAX_BYTES (8u * 1024u * 1024u)
#endif
#ifndef GAME_ANALYTICS_WEB_RING_BYTES /* in-memory ring (web) */
#define GAME_ANALYTICS_WEB_RING_BYTES (256u * 1024u)
#endif
#ifndef GAME_ANALYTICS_DESC_REG_CAP
#define GAME_ANALYTICS_DESC_REG_CAP 64
#endif
#ifndef GAME_ANALYTICS_BUILD
#define GAME_ANALYTICS_BUILD "0"
#endif

/* ---- registry: type-hash -> descriptor (mirror of the E3 registry, ~15 lines; the E3
   copy is private to game_events_devapi.c and frozen) ---- */
typedef struct {
    uint64_t hash;
    const game_event_desc_t *desc;
} an_reg_entry_t;

static an_reg_entry_t s_reg[GAME_ANALYTICS_DESC_REG_CAP];
static int s_reg_count;

/* ---- writer state ---- */
static game_analytics_sink_fn s_sink; /* set in init if NULL; test injects before init */
static uint8_t s_buf[GAME_ANALYTICS_BUF_BYTES];
static size_t s_buf_len;
static uint64_t s_written_total; /* for the session byte cap (native) */
static uint64_t s_dropped;       /* health: REAL failures (line > buffer / write error) */
static uint64_t s_evicted;       /* routine web-ring eviction (NOT dropped) */
static bool s_open, s_capped, s_warned;
static int64_t s_started_at;
#if !defined(__EMSCRIPTEN__)
static FILE *s_file; /* native only (else -Wunused on web) */
#endif
#if defined(__EMSCRIPTEN__)
static char s_ring[GAME_ANALYTICS_WEB_RING_BYTES];
static size_t s_ring_len;
#endif

/* ---- wall clock (own tiny clock; game_save's is private + frozen; §0) ---- */
#if defined(GAME_ANALYTICS_TESTING)
static int64_t (*s_wall)(void);
static int64_t wall_now(void) { return s_wall ? s_wall() : 0; }
#elif defined(__EMSCRIPTEN__)
/* clang-format off */
EM_JS(double, game_analytics_web_now_ms, (void), { return Date.now(); }) /* precedent game_save.c:90 */
/* clang-format on */
static int64_t wall_now(void) { return (int64_t)game_analytics_web_now_ms(); }
#else
static int64_t wall_now(void) { return (int64_t)time(NULL) * 1000; }
#endif

/* ---- one-shot dev warn (single signature for every failure site) ---- */
static void warn_once(const char *msg) {
    if (s_warned) {
        return;
    }
    s_warned = true;
    nt_log_warn("game_analytics: %s", msg);
}

/* ---- platform default sink ---- */
#if defined(__EMSCRIPTEN__)
/* Append to the fixed-byte ring, evicting oldest WHOLE lines (invariant: the ring holds only
   complete '\n'-terminated lines -> export is always valid NDJSON). Routine eviction bumps
   s_evicted, never s_dropped (keep-newest, design §4 "web = memory"). */
static void web_ring_append(const char *bytes, size_t len) {
    if (len > sizeof s_ring) {
        s_dropped++;
        warn_once("web ring smaller than flush chunk");
        return;
    }
    while (s_ring_len + len > sizeof s_ring) {
        const char *nl = (const char *)memchr(s_ring, '\n', s_ring_len);
        const size_t drop = nl ? (size_t)(nl - s_ring) + 1u : s_ring_len;
        memmove(s_ring, s_ring + drop, s_ring_len - drop);
        s_ring_len -= drop;
        s_evicted++;
    }
    memcpy(s_ring + s_ring_len, bytes, len);
    s_ring_len += len;
}

static void default_platform_sink(const char *bytes, size_t len) { web_ring_append(bytes, len); }
#else
static bool an_make_dir(const char *path) {
#if defined(_WIN32)
    if (_mkdir(path) == 0) {
        return true;
    }
#else
    if (mkdir(path, 0755) == 0) {
        return true;
    }
#endif
    return errno == EEXIST;
}

/* Segmented mkdir of a directory path (mirror game_storage.c ensure_parent_dirs): analytics
   init runs BEFORE game_save_init, so build/ may not exist yet -> create build/, then
   build/analytics/ (a single _mkdir("build/analytics") would ENOENT and drop the stream). */
static bool an_ensure_dirs(const char *dir) {
    char temp[64];
    if (snprintf(temp, sizeof temp, "%s", dir) >= (int)sizeof temp) {
        return false;
    }
    for (char *p = temp; *p; ++p) {
        if (*p != '/' && *p != '\\') {
            continue;
        }
        if (p == temp || (*(p - 1) == ':')) {
            continue;
        }
        const char saved = *p;
        *p = '\0';
        if (!an_make_dir(temp)) {
            *p = saved;
            return false;
        }
        *p = saved;
    }
    return an_make_dir(temp); /* the final segment (dir carries no trailing slash) */
}

static int an_getpid(void) {
#if defined(_WIN32)
    return (int)_getpid();
#else
    return (int)getpid();
#endif
}

/* Lazily open the append-file on first flush (pid-suffix disambiguates two runs in the same
   wall second -- time(NULL) is second-resolution). fflush on every (rare, threshold-driven)
   flush bounds loss on a crash. */
static void native_file_sink(const char *bytes, size_t len) {
    if (!s_file) {
        if (!an_ensure_dirs("build/analytics")) {
            s_dropped++;
            warn_once("mkdir build/analytics failed");
            return;
        }
        char path[160];
        (void)snprintf(path, sizeof path, "build/analytics/session-%" PRId64 "-%d.ndjson",
                       s_started_at, an_getpid());
        s_file = fopen(path, "ab");
        if (!s_file) {
            s_dropped++;
            warn_once("open session file failed");
            return;
        }
    }
    (void)fwrite(bytes, 1, len, s_file);
    (void)fflush(s_file);
}

static void default_platform_sink(const char *bytes, size_t len) { native_file_sink(bytes, len); }
#endif

/* ---- registry ---- */
void game_analytics_register_descs(const game_event_desc_t *const *descs, int count) {
    for (int i = 0; i < count; ++i) {
        const game_event_desc_t *d = descs[i];
        if (!d || !d->name) {
            continue;
        }
        const uint64_t h = nt_hash64_str(d->name).value; /* == <frag>_ev_<evt>_type().value */
        for (int j = 0; j < s_reg_count; ++j) {
            NT_ASSERT(s_reg[j].hash != h && "duplicate event type hash in analytics registry");
        }
        if (s_reg_count >= GAME_ANALYTICS_DESC_REG_CAP) {
            nt_log_warn("game_analytics: descriptor registry full (%d) -> dropping %s",
                        GAME_ANALYTICS_DESC_REG_CAP, d->name);
            return;
        }
        s_reg[s_reg_count].hash = h;
        s_reg[s_reg_count].desc = d;
        s_reg_count++;
    }
}

static const game_event_desc_t *reg_find(nt_hash64_t type) {
    for (int i = 0; i < s_reg_count; ++i) {
        if (s_reg[i].hash == type.value) {
            return s_reg[i].desc;
        }
    }
    return NULL;
}

/* ---- lifecycle ---- */
void game_analytics_init(void) {
    if (s_open) {
        return;
    }
    if (!s_sink) {
        s_sink = default_platform_sink; /* native lazy-file / web ring; test already set it */
    }
    s_started_at = wall_now();
    s_buf_len = 0;
    s_written_total = 0;
    s_capped = false;
    s_warned = false;

    /* session header (first line) via cJSON -- correct escaping under -Werror. */
    cJSON *hdr = cJSON_CreateObject();
    if (hdr) {
        cJSON_AddStringToObject(hdr, "schema", "analytics.v1");
        cJSON_AddStringToObject(hdr, "kind", "header");
        cJSON_AddStringToObject(hdr, "app", GAME_STORAGE_APP_ID);
        cJSON_AddStringToObject(hdr, "build", GAME_ANALYTICS_BUILD);
        cJSON_AddNumberToObject(hdr, "started_at", (double)s_started_at);
        char *s = cJSON_PrintUnformatted(hdr);
        if (s) {
            const size_t hlen = strlen(s);
            if (hlen + 1u <= sizeof s_buf) {
                memcpy(s_buf + s_buf_len, s, hlen);
                s_buf_len += hlen;
                s_buf[s_buf_len++] = (uint8_t)'\n';
            } else {
                s_dropped++;
                warn_once("session header exceeds buffer");
            }
            cJSON_free(s);
        }
        cJSON_Delete(hdr);
    }
    s_open = true;
    game_analytics_flush();
}

/* One linear pass per frame -- the log is fresh each frame (no cursor); cascades already
   settled to the react fixpoint before the RECORD phase. */
void game_analytics_record(void) {
    if (!s_open || s_capped) {
        return;
    }
    int n = 0;
    const game_event_t *log = game_event_log(&n);
    for (int i = 0; i < n; ++i) {
        char line[GAME_ANALYTICS_LINE_MAX];
        const int len = game_event_render(&log[i], reg_find(log[i].type), line, (int)sizeof line);
        if (len <= 0) {
            continue;
        }
        const size_t need = (size_t)len + 1u; /* + '\n' */
        if (need > sizeof s_buf) {
            s_dropped++;
            warn_once("rendered line exceeds buffer");
            continue;
        }
        if (s_buf_len + need > sizeof s_buf) {
            game_analytics_flush(); /* free room */
        }
        memcpy(s_buf + s_buf_len, line, (size_t)len);
        s_buf_len += (size_t)len;
        s_buf[s_buf_len++] = (uint8_t)'\n';
    }
    if (s_buf_len >= GAME_ANALYTICS_FLUSH_BYTES) {
        game_analytics_flush();
    }
}

void game_analytics_flush(void) {
    if (s_buf_len == 0) {
        return;
    }
    /* Native byte cap = keep-oldest-stop (tail of the session is lost, s_capped). Web ring =
       keep-newest-roll (start is evicted). Opposite policies (documented); override with
       GAME_ANALYTICS_MAX_BYTES=0 for an uncapped native file. */
    if (GAME_ANALYTICS_MAX_BYTES != 0u && s_written_total >= (uint64_t)GAME_ANALYTICS_MAX_BYTES) {
        s_capped = true;
        warn_once("session byte cap hit");
        s_buf_len = 0;
        return;
    }
    s_sink((const char *)s_buf, s_buf_len);
    s_written_total += s_buf_len;
    s_buf_len = 0;
}

void game_analytics_shutdown(void) {
    game_analytics_flush();
#if !defined(__EMSCRIPTEN__)
    if (s_file) {
        fclose(s_file);
        s_file = NULL;
    }
#endif
#if defined(__EMSCRIPTEN__)
    s_ring_len = 0;
#endif
    /* Reset ALL statics (registry included) so a fresh init/setUp starts clean -- otherwise a
       second ctest case re-registers the descriptors and trips the duplicate-hash assert
       (registration runs BEFORE init, so init cannot reset the registry). */
    s_sink = NULL;
    s_open = false;
    s_capped = false;
    s_warned = false;
    s_buf_len = 0;
    s_written_total = 0;
    s_started_at = 0;
    s_dropped = 0;
    s_evicted = 0;
    s_reg_count = 0;
}

uint64_t game_analytics_dropped(void) { return s_dropped; }
uint64_t game_analytics_evicted(void) { return s_evicted; }

#if defined(__EMSCRIPTEN__)
char *game_analytics_export(void) {
    if (s_ring_len == 0) {
        return NULL;
    }
    char *out = (char *)malloc(s_ring_len + 1u);
    if (!out) {
        return NULL;
    }
    memcpy(out, s_ring, s_ring_len);
    out[s_ring_len] = '\0';
    return out;
}
#endif

#ifdef GAME_ANALYTICS_TESTING
void game_analytics__set_sink_for_test(game_analytics_sink_fn sink) { s_sink = sink; }
void game_analytics__set_clock_for_test(int64_t (*wall)(void)) { s_wall = wall; }
#endif

#endif /* FEATURE_GAME_ANALYTICS */
