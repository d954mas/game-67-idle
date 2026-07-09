#include "game_event_render.h"

#include <inttypes.h> /* PRIx64 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>  /* snprintf */
#include <string.h> /* memcpy, memchr, strlen */

#include "cJSON.h"
#include "hash/nt_hash.h"    /* nt_hash64_label */

/* Read of `width` bytes at `off` is legal only if the whole word fits in `size`.
   Written to avoid uint32 overflow in the offset+width sum (LOW-6). */
static bool render_in_bounds(uint32_t off, uint32_t width, uint32_t size) {
    return off <= size && (size - off) >= width;
}

/* Append up to GAME_EVENT_RENDER_HEX_MAX bytes of `data[0..len)` as "hex":"aabb..". */
static void render_add_hex(cJSON *obj, const uint8_t *data, uint32_t len) {
    const uint32_t nshow = len < (uint32_t)GAME_EVENT_RENDER_HEX_MAX ? len : (uint32_t)GAME_EVENT_RENDER_HEX_MAX;
    char hexbuf[(GAME_EVENT_RENDER_HEX_MAX * 2) + 1];
    for (uint32_t i = 0; i < nshow; ++i) {
        (void)snprintf(hexbuf + (i * 2u), 3, "%02x", (unsigned)data[i]);
    }
    hexbuf[nshow * 2u] = '\0';
    cJSON_AddStringToObject(obj, "hex", hexbuf);
}

/* { "size":N, "truncated":true } for a string whose inline bytes never NUL-terminate. */
static void render_add_truncated_string(cJSON *root, const char *name, uint32_t avail) {
    cJSON *t = cJSON_AddObjectToObject(root, name);
    if (!t) {
        return;
    }
    cJSON_AddNumberToObject(t, "size", (double)avail);
    cJSON_AddBoolToObject(t, "truncated", true);
}

static void render_add_field(cJSON *root, const game_event_field_t *f, const uint8_t *base, uint32_t size) {
    switch (f->type) {
        case GAME_EVENT_FT_BOOL: {
            if (!render_in_bounds(f->offset, 1u, size)) {
                return;
            }
            uint8_t v = 0;
            memcpy(&v, base + f->offset, 1);
            cJSON_AddBoolToObject(root, f->name, v != 0);
            return;
        }
        case GAME_EVENT_FT_INT: {
            if (!render_in_bounds(f->offset, 4u, size)) {
                return;
            }
            int32_t v = 0;
            memcpy(&v, base + f->offset, sizeof v);
            cJSON_AddNumberToObject(root, f->name, (double)v);
            return;
        }
        case GAME_EVENT_FT_I64: {
            if (!render_in_bounds(f->offset, 8u, size)) {
                return;
            }
            int64_t v = 0;
            memcpy(&v, base + f->offset, sizeof v);
            char b[24];
            (void)snprintf(b, sizeof b, "%lld", (long long)v); /* string, NOT double */
            cJSON_AddStringToObject(root, f->name, b);
            return;
        }
        case GAME_EVENT_FT_FLOAT: {
            if (!render_in_bounds(f->offset, 8u, size)) {
                return;
            }
            double v = 0.0;
            memcpy(&v, base + f->offset, sizeof v);
            cJSON_AddNumberToObject(root, f->name, v);
            return;
        }
        case GAME_EVENT_FT_STRING: {
            if (!render_in_bounds(f->offset, 4u, size)) {
                return;
            }
            uint32_t soff = 0;
            memcpy(&soff, base + f->offset, sizeof soff);
            if (soff >= size) {
                render_add_truncated_string(root, f->name, 0u);
                return;
            }
            const void *nul = memchr(base + soff, 0, (size_t)(size - soff));
            if (nul) {
                cJSON_AddStringToObject(root, f->name, (const char *)base + soff);
            } else {
                render_add_truncated_string(root, f->name, size - soff);
            }
            return;
        }
        case GAME_EVENT_FT_HASH: {
            if (!render_in_bounds(f->offset, 8u, size)) {
                return;
            }
            uint64_t hv = 0;
            memcpy(&hv, base + f->offset, sizeof hv);
            const nt_hash64_t h = {hv};
            const char *lbl = nt_hash64_label(h);
            if (lbl) {
                cJSON_AddStringToObject(root, f->name, lbl);
            } else {
                char hb[19];
                (void)snprintf(hb, sizeof hb, "0x%016" PRIx64, hv);
                cJSON_AddStringToObject(root, f->name, hb);
            }
            return;
        }
        case GAME_EVENT_FT_BYTES: {
            if (!render_in_bounds(f->offset, 4u, size) || !render_in_bounds(f->len_offset, 4u, size)) {
                return;
            }
            uint32_t boff = 0;
            uint32_t blen = 0;
            memcpy(&boff, base + f->offset, sizeof boff);
            memcpy(&blen, base + f->len_offset, sizeof blen);
            cJSON *b = cJSON_AddObjectToObject(root, f->name);
            if (!b) {
                return;
            }
            cJSON_AddNumberToObject(b, "size", (double)blen);
            if (render_in_bounds(boff, blen, size)) {
                render_add_hex(b, base + boff, blen); /* bounds ok -> size + hex */
            }
            return;
        }
        default:
            return; /* unknown field type: skip, never dereference */
    }
}

int game_event_render(const game_event_t *e, const game_event_desc_t *desc, char *out, int cap) {
    if (!out || cap <= 0) {
        return 0;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        (void)snprintf(out, (size_t)cap, "{}");
        return (cap > 2) ? 2 : (cap - 1);
    }

    cJSON_AddNumberToObject(root, "seq", (double)e->seq);
    cJSON_AddNumberToObject(root, "tick", (double)e->tick);

    char hexn[19];
    const char *tname = desc ? desc->name : nt_hash64_label(e->type);
    if (!tname) {
        (void)snprintf(hexn, sizeof hexn, "0x%016" PRIx64, e->type.value);
        tname = hexn;
    }
    cJSON_AddStringToObject(root, "type", tname);

    if (!desc) {
        cJSON_AddNumberToObject(root, "size", (double)e->size);
        cJSON_AddBoolToObject(root, "unknown", true);
        render_add_hex(root, (const uint8_t *)e->payload, e->size);
    } else {
        const uint8_t *base = (const uint8_t *)e->payload;
        for (int i = 0; i < desc->field_count; ++i) {
            render_add_field(root, &desc->fields[i], base, e->size);
        }
    }

    int written = 0;
    char *s = cJSON_PrintUnformatted(root);
    const int len = s ? (int)strlen(s) : -1;
    if (s && len < cap) {
        memcpy(out, s, (size_t)len + 1u);
        written = len;
    } else {
        /* Truncated fallback: a fresh { seq, tick, type, truncated:true } is guaranteed
           valid JSON (the ring/handler re-parses every slot, so it must parse). */
        cJSON *t = cJSON_CreateObject();
        char *ts = NULL;
        int tlen = -1;
        if (t) {
            cJSON_AddNumberToObject(t, "seq", (double)e->seq);
            cJSON_AddNumberToObject(t, "tick", (double)e->tick);
            cJSON_AddStringToObject(t, "type", tname);
            cJSON_AddBoolToObject(t, "truncated", true);
            ts = cJSON_PrintUnformatted(t);
            tlen = ts ? (int)strlen(ts) : -1;
        }
        if (ts && tlen < cap) {
            memcpy(out, ts, (size_t)tlen + 1u);
            written = tlen;
        } else {
            (void)snprintf(out, (size_t)cap, "{}");
            written = (cap > 2) ? 2 : (cap - 1);
        }
        if (ts) {
            cJSON_free(ts);
        }
        cJSON_Delete(t);
    }
    if (s) {
        cJSON_free(s);
    }
    cJSON_Delete(root);
    return written;
}
