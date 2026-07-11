#include "features/resource_panel/resource_panel.h"

#include "game_format.h" /* L0-abbrev, легальный вниз */

#include "app/nt_app.h" /* g_nt_app.dt */
#include "clay.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"
#include "ui/theme.h" /* g_theme */

#include <math.h>
#include <stdio.h>
#include <string.h>

/* НИ ОДНОГО другого features/<x> include (греп-гейт G12) -- entries + геттеры даёт игра. */

/* UCRT's stdlib.h (non-standard MS extension, gated by _CRT_INTERNAL_NONSTDC_NAMES)
   #defines max(a,b)/min(a,b) as function-like macros -- pulled in transitively
   through the engine headers above on this toolchain. The preprocessor then
   mangles every `entry->max(...)` call below (it matches the bare token `max`
   before `(`, blind to the preceding `->`). entry->max is a struct field name
   mandated by the public header -- undef defensively rather than rename. */
#undef max
#undef min

/* Not <stdlib.h>'s llabs -- avoids re-pulling the same macro collision in a
   header that happens to route through stdlib.h on some other toolchain.
   L-fix (deep-review): `-v` at INT64_MIN is UB (no positive int64
   counterpart) -- unreachable via the template's own entries today, but this
   is a general-purpose helper, so it must not carry a latent UB trap.
   Negating through uint64_t is well-defined for every int64_t input. */
static int64_t rp_i64_abs(int64_t v) { return v < 0 ? (int64_t)(0ULL - (uint64_t)v) : v; }

#define LAYER_BG 0
#define LAYER_FILL 1
#define LAYER_IMG 2
#define LAYER_TEXT_SHADOW 3
#define LAYER_TEXT 4

#define RP_ROW_H 40.0F
#define RP_ICON_SIZE 24.0F
#define RP_VALUE_CELL_W 90.0F
#define RP_BAR_W 200.0F
#define RP_BAR_H 24.0F
#define RP_BAR_INSET 3.0F
#define RP_PANEL_X 12.0F
#define RP_PANEL_Y 12.0F

#define RP_EASE_TAU 0.12F     /* count-up ease-out time constant */
#define RP_ACCENT_SECS 0.35F  /* accent decay window */
#define RP_SNAP_ABS 1000LL    /* counter snap-ref floor when max is absent, #10 */

static const Clay_Color RP_COLOR_ICON_FALLBACK = {70.0F, 70.0F, 78.0F, 220.0F};
static const Clay_Color RP_COLOR_TRACK_BG = {18.0F, 13.0F, 9.0F, 218.0F};
static const Clay_Color RP_COLOR_TRACK_BORDER = {105.0F, 76.0F, 43.0F, 182.0F};
static const Clay_Color RP_COLOR_FILL = {120.0F, 170.0F, 230.0F, 255.0F};
static const Clay_Color RP_COLOR_GAIN = {120.0F, 220.0F, 120.0F, 255.0F};  /* gain accent */
static const Clay_Color RP_COLOR_SPEND = {225.0F, 95.0F, 95.0F, 255.0F};   /* акцент spend */

/* ---- Транзиентное состояние (static, keyed by entry.id -- НЕ в сейве, не в World) ---- */

typedef struct {
    char id[32];
    bool seen;
    double displayed;
    int64_t last_logical;
    float accent;     /* 0..1 accent timer */
    bool accent_gain;  /* true=gain(green) false=spend(red); valid while accent>0 */
    float anchor_x, anchor_y;
    bool drawn;
} rp_slot_t;

#define RESOURCE_PANEL_MAX_SLOTS 16
static rp_slot_t s_slots[RESOURCE_PANEL_MAX_SLOTS];

/* free slot = empty id (id[0]=='\0'); find-or-alloc by entry.id. */
static rp_slot_t *slot_for(const char *id) {
    for (int i = 0; i < RESOURCE_PANEL_MAX_SLOTS; ++i) {
        if (s_slots[i].id[0] != '\0' && strcmp(s_slots[i].id, id) == 0) {
            return &s_slots[i];
        }
    }
    for (int i = 0; i < RESOURCE_PANEL_MAX_SLOTS; ++i) {
        if (s_slots[i].id[0] == '\0') {
            memset(&s_slots[i], 0, sizeof(s_slots[i]));
            /* Best-effort UI cache key, not save-critical data -- unlike
               progression's find_or_alloc_track, a truncated id here at worst
               loses count-up smoothing for one absurdly-long entry id, never
               data. No truncation reject needed. */
            (void)snprintf(s_slots[i].id, sizeof s_slots[i].id, "%s", id);
            return &s_slots[i];
        }
    }
    return NULL; /* RESOURCE_PANEL_MAX_SLOTS budget exhausted -- entry silently not drawn */
}

static float clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int64_t rp_max_i64(int64_t a, int64_t b) { return a > b ? a : b; }

/* ---- Dynamic Clay ids (precedent: rb-dark shop_screen.c semantic_clay_id) ----
   Clay retains a POINTER into the id string for introspection (devapi ui.tree
   id_string) -- MUST be file-static, never a stack-local, or the pointer
   dangles once this TU's frame returns. Ring-buffered so several ids stay
   alive across one resource_panel_ui() call without colliding. */
#define RP_ID_BUF_LEN 64
#define RP_ID_BUF_SLOTS 32
static char s_id_storage[RP_ID_BUF_SLOTS][RP_ID_BUF_LEN];
static int s_id_cursor;

static Clay_ElementId rp_clay_id(const char *entry_id, const char *suffix) {
    char *buf = s_id_storage[s_id_cursor % RP_ID_BUF_SLOTS];
    s_id_cursor += 1;
    (void)snprintf(buf, RP_ID_BUF_LEN, "resource_panel/%s%s", entry_id, suffix);
    return Clay_GetElementId((Clay_String){.isStaticallyAllocated = false, .length = (int32_t)strlen(buf), .chars = buf});
}

static Clay_Color rp_lerp_color(Clay_Color a, Clay_Color b, float t) {
    if (t <= 0.0F) {
        return a;
    }
    if (t >= 1.0F) {
        return b;
    }
    return (Clay_Color){
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.a + (b.a - a.a) * t,
    };
}

/* Ported locally from rb-dark first_screen_hud.c:82-97 (ui-kit has no built-in
   text shadow). `slot` is a plain int differentiator (Clay id collision guard
   across calls in one frame) -- distinct from rp_slot_t/RESOURCE_PANEL_MAX_SLOTS. */
static void rp_shadowed_label(nt_ui_context_t *ctx, int slot, const char *text, const nt_ui_label_style_t *style) {
    nt_ui_label_style_t shadow = *style;
    shadow.color = (Clay_Color){8.0F, 5.0F, 3.0F, 142.0F};

    CLAY({.id = CLAY_IDI("resource_panel/shadowed_label", slot),
          .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
        CLAY({.id = CLAY_IDI("resource_panel/shadowed_label_shadow", slot),
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP, .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                           .offset = {1.0F, 1.0F}},
              .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT_SHADOW), text, &shadow);
        }
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), text, style);
    }
}

/* Graceful no-art: icon==NULL -> flat rect placeholder, never
   requires an atlas. icon!=NULL -> nt_ui_image with a MUTABLE copy (resolve
   mutates the ref; entry->icon is game-owned and const from here). */
static void rp_draw_icon_or_fallback(nt_ui_context_t *ctx, const resource_panel_entry_t *entry) {
    if (entry->icon != NULL) {
        nt_atlas_region_ref_t region_copy = *entry->icon;
        nt_ui_image_style_t style = nt_ui_image_style_defaults();
        nt_ui_image(ctx, NT_UI_DATA_LAYER(LAYER_IMG), &region_copy, &style,
                    &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(RP_ICON_SIZE), CLAY_SIZING_FIXED(RP_ICON_SIZE)}}});
    } else {
        CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(RP_ICON_SIZE), CLAY_SIZING_FIXED(RP_ICON_SIZE)}},
              .backgroundColor = RP_COLOR_ICON_FALLBACK,
              .cornerRadius = CLAY_CORNER_RADIUS(4),
              .userData = NT_UI_CLAY_DATA(LAYER_IMG)}) {}
    }
}

/* Poll+diff+count-up+accent. Reads entry->value once (poll), never
   touches entry->max/level without a NULL-гард. `out_max_val` (nullable)
   hands the ALREADY-POLLED max reading back to the caller so rp_draw_bar does
   not call entry->max() a second time this frame -- poll-once means
   once per getter per frame, not once per call site (L-fix, deep-review). */
static void rp_step_slot(rp_slot_t *slot, const resource_panel_entry_t *entry, float dt, int64_t *out_max_val) {
    bool first_frame = !slot->seen;
    int64_t logical = entry->value(entry->ud); /* required callback */
    bool has_max = (entry->kind == RESOURCE_PANEL_BAR) && (entry->max != NULL);
    int64_t max_val = has_max ? entry->max(entry->ud) : 0;
    if (out_max_val != NULL) {
        *out_max_val = max_val;
    }

    /* #10: max may be NULL on a counter -- snap base falls back to |logical|,
       floored at RP_SNAP_ABS so a small counter (e.g. gold=5) doesn't get a
       twitchy ~1-unit snap threshold. */
    int64_t snap_ref = has_max ? rp_max_i64(1, max_val) : rp_max_i64(RP_SNAP_ABS, rp_i64_abs(logical));
    bool snap = first_frame || rp_i64_abs(logical - slot->last_logical) > snap_ref / 4;

    if (snap) {
        slot->displayed = (double)logical;
    } else {
        /* Ретаргет, не рестарт: цель ВСЕГДА текущий logical. */
        slot->displayed += ((double)logical - slot->displayed) * (double)(1.0F - expf(-dt / RP_EASE_TAU));
    }
    slot->seen = true;

    /* Skip the accent flash on this slot's first-ever frame -- the load-time
       value is a snap, not a player-caused "change" (same reasoning as the
       count-up snap above, applied to the accent). */
    if (!first_frame && logical != slot->last_logical) {
        slot->accent = 1.0F;
        slot->accent_gain = logical > slot->last_logical;
    }
    slot->accent -= dt / RP_ACCENT_SECS;
    if (slot->accent < 0.0F) {
        slot->accent = 0.0F;
    }
    slot->last_logical = logical;
}

static void rp_draw_counter(nt_ui_context_t *ctx, const resource_panel_entry_t *entry, rp_slot_t *slot, int index) {
    char value_buf[24];
    (void)game_format_i64_abbrev((int64_t)llround(slot->displayed), value_buf, sizeof value_buf);

    Clay_Color accent_tint = slot->accent_gain ? RP_COLOR_GAIN : RP_COLOR_SPEND;
    nt_ui_label_style_t value_style = g_theme.label;
    value_style.color = rp_lerp_color(g_theme.label.color, accent_tint, slot->accent);

    /* #14 punch: FIXED cell so the row never reflows; the glyph itself scales
       via a RENDER-TIME transform (no layout effect, nt_ui.h contract) --
       never via font_size (that re-measures Clay's FIT layout and shoves
       siblings, the exact thing #14 forbids). */
    nt_ui_transform_t punch = nt_ui_transform_defaults();
    float scale = 1.0F + 0.12F * slot->accent;
    punch.scale_x = scale;
    punch.scale_y = scale;

    CLAY({.id = rp_clay_id(entry->id, ""),
          .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(RP_ROW_H)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 8,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        rp_draw_icon_or_fallback(ctx, entry);
        if (entry->label != NULL) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), entry->label, &g_theme.label);
        }
        CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(RP_VALUE_CELL_W), CLAY_SIZING_FIXED(RP_ROW_H)},
                          .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            nt_ui_label(ctx, NT_UI_DATA_XFORM(LAYER_TEXT, &punch, 1.0F), value_buf, &value_style);
        }
    }

    slot->anchor_x = RP_PANEL_X + RP_ICON_SIZE * 0.5F;
    slot->anchor_y = RP_PANEL_Y + (float)index * RP_ROW_H + RP_ROW_H * 0.5F;
    slot->drawn = true;
}

/* max_val -- ALREADY POLLED once this frame by rp_step_slot (out-param); never
   re-call entry->max() here (L-fix, deep-review: was called twice/frame). */
static void rp_draw_bar(nt_ui_context_t *ctx, const resource_panel_entry_t *entry, rp_slot_t *slot, int index, int64_t max_val) {
    bool has_max = entry->max != NULL;
    bool has_level = entry->level != NULL;
    int64_t level_val = has_level ? entry->level(entry->ud) : 0;

    char value_buf[24];
    (void)game_format_i64_abbrev((int64_t)llround(slot->displayed), value_buf, sizeof value_buf);
    Clay_Color accent_tint = slot->accent_gain ? RP_COLOR_GAIN : RP_COLOR_SPEND;

    CLAY({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(RP_ROW_H)},
                      .layoutDirection = CLAY_LEFT_TO_RIGHT,
                      .childGap = 8,
                      .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
        rp_draw_icon_or_fallback(ctx, entry);

        if (!has_max) {
            /* NULL contract: bar без знаменателя -> счётчик-с-меткой,
               без заливки. */
            nt_ui_label_style_t style = g_theme.label;
            style.color = rp_lerp_color(g_theme.label.color, accent_tint, slot->accent);
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), value_buf, &style);
        } else {
            char caption[64];
            if (max_val > 0) {
                char max_buf[24];
                (void)game_format_i64_abbrev(max_val, max_buf, sizeof max_buf);
                if (has_level) {
                    (void)snprintf(caption, sizeof caption, "Lv %lld  %s/%s", (long long)level_val, value_buf, max_buf);
                } else {
                    (void)snprintf(caption, sizeof caption, "%s/%s", value_buf, max_buf);
                }
            } else if (has_level) {
                (void)snprintf(caption, sizeof caption, "Lv %lld  MAX", (long long)level_val);
            } else {
                (void)snprintf(caption, sizeof caption, "MAX");
            }

            /* displayed используется для fill (плавно ползёт), logical -- для diff/акцента. */
            float ratio = max_val > 0 ? clampf((float)(slot->displayed / (double)max_val), 0.0F, 1.0F) : 1.0F;

            CLAY({.id = rp_clay_id(entry->id, "/bar"),
                  .layout = {.sizing = {CLAY_SIZING_FIXED(RP_BAR_W), CLAY_SIZING_FIXED(RP_BAR_H)},
                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
                  .backgroundColor = RP_COLOR_TRACK_BG,
                  .cornerRadius = CLAY_CORNER_RADIUS(4),
                  .border = {.color = RP_COLOR_TRACK_BORDER, .width = {1, 1, 1, 1, 0}},
                  .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                float fill_w = (RP_BAR_W - RP_BAR_INSET * 2.0F) * ratio;
                if (fill_w > 1.0F) {
                    CLAY({.floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                                        .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_CENTER, .parent = CLAY_ATTACH_POINT_LEFT_CENTER},
                                        .offset = {RP_BAR_INSET, 0.0F}},
                          .layout = {.sizing = {CLAY_SIZING_FIXED(fill_w), CLAY_SIZING_FIXED(RP_BAR_H - RP_BAR_INSET * 2.0F)}},
                          .backgroundColor = rp_lerp_color(RP_COLOR_FILL, accent_tint, slot->accent),
                          .cornerRadius = CLAY_CORNER_RADIUS(2),
                          .userData = NT_UI_CLAY_DATA(LAYER_FILL)}) {}
                }
                rp_shadowed_label(ctx, index, caption, &g_theme.label);
            }
        }
    }

    slot->anchor_x = RP_PANEL_X + RP_BAR_W * 0.5F;
    slot->anchor_y = RP_PANEL_Y + (float)index * RP_ROW_H + RP_ROW_H * 0.5F;
    slot->drawn = true;
}

void resource_panel_ui(nt_ui_context_t *ctx, const resource_panel_entry_t *entries, int count) {
    if (ctx == NULL || entries == NULL || count <= 0) {
        return;
    }
    float dt = g_nt_app.dt;

    for (int i = 0; i < RESOURCE_PANEL_MAX_SLOTS; ++i) {
        s_slots[i].drawn = false; /* transient, recomputed every frame */
    }

    CLAY({.id = CLAY_ID("resource_panel/root"),
          /* M-fix (deep-review #1, z-order): settings' modal panel is ALSO an
             implicit-zIndex=0 floating root; the walker sorts GLOBALLY within
             a zIndex band, so this panel's text layers (3-4) painted over
             settings' art layers (max 2) whenever the modal was open --
             draw_ui CALL ORDER does not beat zIndex. zIndex=-1 sinks the whole
             HUD one band below the default (settings stays at 0), so it draws
             under ANY zIndex-0 overlay while staying above the 3D world
             (which isn't Clay-floated at all). */
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP, .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                       .offset = {RP_PANEL_X, RP_PANEL_Y},
                       .zIndex = -1},
          .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}, .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 6}}) {
        for (int i = 0; i < count; ++i) {
            const resource_panel_entry_t *entry = &entries[i];
            if (entry->id == NULL || entry->value == NULL) {
                continue; /* value is required; malformed entry -- defensive skip */
            }
            rp_slot_t *slot = slot_for(entry->id);
            if (slot == NULL) {
                continue; /* RESOURCE_PANEL_MAX_SLOTS budget exhausted */
            }
            int64_t max_val = 0;
            rp_step_slot(slot, entry, dt, &max_val);
            if (entry->kind == RESOURCE_PANEL_BAR) {
                rp_draw_bar(ctx, entry, slot, i, max_val);
            } else {
                rp_draw_counter(ctx, entry, slot, i);
            }
        }
    }
}

bool resource_panel_anchor(const char *entry_id, float *out_x, float *out_y) {
    if (entry_id == NULL) {
        return false;
    }
    for (int i = 0; i < RESOURCE_PANEL_MAX_SLOTS; ++i) {
        if (s_slots[i].id[0] != '\0' && strcmp(s_slots[i].id, entry_id) == 0) {
            if (!s_slots[i].drawn) {
                return false; /* not drawn this frame */
            }
            if (out_x != NULL) {
                *out_x = s_slots[i].anchor_x;
            }
            if (out_y != NULL) {
                *out_y = s_slots[i].anchor_y;
            }
            return true;
        }
    }
    return false;
}
