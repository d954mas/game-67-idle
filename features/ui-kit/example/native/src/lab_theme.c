#include "lab_theme.h"

#include "hash/nt_hash.h"
#include "log/nt_log.h"

#include "features/ui_kit/ui_theme.h"

#include <stdio.h>
#include <string.h>

/* Each block mirrors themes/<id>.json, which draws that theme's art;
   tests/lab_themes_parity.test.mjs holds the two together. Geometry and the
   type ramp are Studio B's: a repaint changes colours, not layout. */
#define LAB_THEME_GEOMETRY                                                                                           \
    .t_display = 32.0F, .t_title = 24.0F, .t_body = 20.0F, .t_num = 21.0F, .t_badge = 16.0F, .t_row = 18.0F,        \
    .t_row_sub = 14.0F, .rim = 3.0F, .lift = 4.0F, .gap = 12.0F, .pad = 20.0F, .hit = 44.0F, .panel_min_w = 260.0F, \
    .panel_max_w = 560.0F, .ref_short = 390.0F, .slice9_scale = 0.25F

static const ui_tokens_t FOREST = {
    .shell = 0xFF2D3621U,
    .panel = 0xFFF3FDFFU,
    .inset = 0xFFDEECE7U,
    .inset_rim = 0xFF2D3621U,
    .scrim = 0x8C2D3621U,
    .tile = 0xFFF3FDFFU,
    .tile_dim = 0xFFDEECE7U,
    .tile_rim = 0xFF2D3621U,
    .ink = 0xFF2D3621U,
    .ink_soft = 0xFF536456U,
    .on_panel = 0xFF2D3621U,
    .on_panel_soft = 0xFF536456U,
    .coin = 0xFF5EC4EAU,
    .go = 0xFF4F7222U,
    .info = 0xFFC97F1EU,
    .danger = 0xFF2A3AA6U,
    .off = 0xFF89968AU,
    .header = 0xFF43543FU,
    LAB_THEME_GEOMETRY,
    .on_action = 0xFFFFFFFFU,
};

static const ui_tokens_t EMBER = {
    .shell = 0xFF222835U,
    .panel = 0xFFF5FBFFU,
    .inset = 0xFFDDE5F1U,
    .inset_rim = 0xFF222835U,
    .scrim = 0x8C222835U,
    .tile = 0xFFF5FBFFU,
    .tile_dim = 0xFFDDE5F1U,
    .tile_rim = 0xFF222835U,
    .ink = 0xFF222835U,
    .ink_soft = 0xFF505B70U,
    .on_panel = 0xFF222835U,
    .on_panel_soft = 0xFF505B70U,
    .coin = 0xFF51C1F4U,
    .go = 0xFF1E42B8U,
    .info = 0xFFD47F2AU,
    .danger = 0xFF1F1F7EU,
    .off = 0xFF828B9CU,
    .header = 0xFF353C4BU,
    LAB_THEME_GEOMETRY,
    .on_action = 0xFFFFFFFFU,
};

static const ui_tokens_t NIGHT = {
    .shell = 0xFF20120BU,
    .panel = 0xFF36251BU,
    .inset = 0xFF271911U,
    .inset_rim = 0xFF20120BU,
    .scrim = 0x8C100805U,
    .tile = 0xFF473224U,
    .tile_dim = 0xFF36251BU,
    .tile_rim = 0xFF20120BU,
    .ink = 0xFFFBF6F2U,
    .ink_soft = 0xFFCBB7A9U,
    .on_panel = 0xFFFBF6F2U,
    .on_panel_soft = 0xFFCBB7A9U,
    .coin = 0xFF5AD2FFU,
    .go = 0xFF6BB335U,
    .info = 0xFFFF8C3DU,
    .danger = 0xFF5D5DFFU,
    .off = 0xFF846B5BU,
    .header = 0xFF523A2AU,
    LAB_THEME_GEOMETRY,
    .on_action = 0xFFFFFFFFU,
};

static const lab_theme_desc_t THEMES[] = {
    {"b", "Studio B", NULL}, // the kit's preset; resolved at bind, it is not a constant
    {"forest", "Forest", &FOREST},
    {"ember", "Ember", &EMBER},
    {"night", "Night", &NIGHT},
};
#define THEME_COUNT ((int)(sizeof THEMES / sizeof THEMES[0]))

static const char *const REGIONS[] = {"panel",           "button",         "tile",         "slider_track", "slider_fill",
                                      "slider_track_sm", "slider_fill_sm", "slider_thumb", "icon_play", "header"};
#define REGION_COUNT ((int)(sizeof REGIONS / sizeof REGIONS[0]))

lab_art_t g_lab_art;

static nt_resource_t s_ui_atlas;
static ui_theme_art_t s_art[THEME_COUNT];
static lab_theme_desc_t s_themes[THEME_COUNT];
static int s_current = -1;
static bool s_checked;
static bool s_regions_ok;

// A region is keyed by its own name inside the atlas; the atlas prefix in the
// generated header is a label, not part of the hash.
static uint64_t region_hash(const char *theme, const char *region) {
    char name[96];
    (void)snprintf(name, sizeof name, "%s/%s", theme, region);
    return nt_hash64_str(name).value;
}

static nt_atlas_region_ref_t region_ref(const char *theme, const char *region) {
    return nt_atlas_ref(s_ui_atlas, region_hash(theme, region));
}

void lab_theme_bind(nt_resource_t ui_atlas, nt_material_t radial) {
    s_ui_atlas = ui_atlas;
    memcpy(s_themes, THEMES, sizeof THEMES);
    s_themes[0].tokens = ui_tokens_studio_b();
    for (int i = 0; i < THEME_COUNT; ++i) {
        const char *id = s_themes[i].id;
        s_art[i] = (ui_theme_art_t){
            .panel = region_ref(id, "panel"),
            .button = region_ref(id, "button"),
            .tile = region_ref(id, "tile"),
            .slider_track = region_ref(id, "slider_track"),
            .slider_fill = region_ref(id, "slider_fill"),
            .slider_track_sm = region_ref(id, "slider_track_sm"),
            .slider_fill_sm = region_ref(id, "slider_fill_sm"),
            .thumb = region_ref(id, "slider_thumb"),
            .icon_play = region_ref(id, "icon_play"),
            .header = region_ref(id, "header"),
            .radial = radial,
        };
    }
    g_lab_art = (lab_art_t){
        .coin = nt_atlas_ref(ui_atlas, nt_hash64_str("icon/coin").value),
        .xp = nt_atlas_ref(ui_atlas, nt_hash64_str("icon/xp").value),
        .energy = nt_atlas_ref(ui_atlas, nt_hash64_str("icon/energy").value),
        .sword = nt_atlas_ref(ui_atlas, nt_hash64_str("icon/sword").value),
        .potion = nt_atlas_ref(ui_atlas, nt_hash64_str("icon/potion").value),
        .world = nt_atlas_ref(ui_atlas, nt_hash64_str("scene/world").value),
    };
    (void)lab_theme_apply(0);
}

bool lab_theme_regions_ready(void) {
    if (s_checked) {
        return s_regions_ok;
    }
    if (!nt_resource_is_ready(s_ui_atlas)) {
        return false;
    }
    s_checked = true;
    s_regions_ok = true;
    for (int t = 0; t < THEME_COUNT; ++t) {
        for (int r = 0; r < REGION_COUNT; ++r) {
            if (nt_atlas_find_region(s_ui_atlas, region_hash(s_themes[t].id, REGIONS[r])) == NT_ATLAS_INVALID_REGION) {
                nt_log_error("ui_lab: atlas has no region ui/%s/%s", s_themes[t].id, REGIONS[r]);
                s_regions_ok = false;
            }
        }
    }
    return s_regions_ok;
}

int lab_theme_count(void) { return THEME_COUNT; }

const lab_theme_desc_t *lab_theme_at(int index) {
    return (index >= 0 && index < THEME_COUNT) ? &s_themes[index] : NULL;
}

int lab_theme_index(void) { return s_current; }

bool lab_theme_apply(int index) {
    if (index < 0 || index >= THEME_COUNT) {
        return false;
    }
    s_current = index;
    ui_theme_init(s_themes[index].tokens, &s_art[index]);
    return true;
}

bool lab_theme_apply_id(const char *id) {
    for (int i = 0; i < THEME_COUNT; ++i) {
        if (strcmp(s_themes[i].id, id) == 0) {
            return lab_theme_apply(i);
        }
    }
    return false;
}
