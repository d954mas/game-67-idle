#include "lab.h"

#include "lab_theme.h"
#include "scenes/scenes.h"

#include "features/ui_kit/ui_kit.h"

#include <stdio.h>
#include <string.h>

lab_state_t g_lab;

static const lab_scene_t SCENES[LAB_SCENE_COUNT] = {
    [LAB_SCENE_HUD] = {"hud", "Игра", scene_hud_build, NULL, NULL},
    [LAB_SCENE_UPGRADE] = {"upgrade", "Улучшения", scene_upgrade_build, scene_upgrade_enter, scene_upgrade_leave},
    [LAB_SCENE_RESULT] = {"result", "Результат", scene_result_build, scene_result_enter, scene_result_leave},
    [LAB_SCENE_SETTINGS] = {"settings", "Настройки", scene_settings_build, scene_settings_enter, scene_settings_leave},
    [LAB_SCENE_COMPONENTS] = {"components", "Компоненты", scene_components_build, NULL, scene_components_leave},
    [LAB_SCENE_THEMES] = {"themes", "Темы", scene_themes_build, NULL, scene_themes_leave},
};

static lab_scene_id_t s_current = LAB_SCENE_HUD;
static lab_scene_id_t s_pending = LAB_SCENE_COUNT; // COUNT = nothing pending
static int s_pending_theme = -1;
static bool s_entered;

void lab_request_theme(int index) { s_pending_theme = index; }

void lab_init(void) {
    g_lab = (lab_state_t){
        .coins = 1240,
        .nuts = 38,
        .level = 7,
        .xp = 640,
        .xp_next = 1000,
        .ability = 0,
        .cooldown_left = 3.0F,
        .music = true,
        .sound = true,
        .vibration = false,
        .volume_master = 0.8F,
        .volume_music = 0.7F,
        .volume_sfx = 0.9F,
        .language = 0,
    };
    // Demo content: what an upgrade card has to show, not a balance sheet.
    g_lab.upgrades[0] = (lab_upgrade_t){"Сила удара", 0, 20.0F, 5.0F, 120, 0, 9, 0, &g_lab_art.sword};
    g_lab.upgrades[1] = (lab_upgrade_t){"Радиус", 1, 2.0F, 0.5F, 1600, 0, 5, 0, &g_lab_art.energy};
    g_lab.upgrades[2] = (lab_upgrade_t){"Скорость", 1, 1.0F, 0.2F, 400, 0, 6, 0, &g_lab_art.potion};
    g_lab.upgrades[3] = (lab_upgrade_t){"Добыча", 0, 1.0F, 1.0F, 900, 0, 4, 0, &g_lab_art.coin};
    g_lab.upgrades[4] = (lab_upgrade_t){"Опыт", 0, 10.0F, 5.0F, 700, 0, 6, 9, &g_lab_art.xp};
}

int lab_upgrade_price(const lab_upgrade_t *u) { return u->price + (u->price * u->level) / 2; }

float lab_upgrade_value(const lab_upgrade_t *u, int level) { return u->base + u->step * (float)level; }

bool lab_upgrade_unlocked(const lab_upgrade_t *u) { return g_lab.level >= u->unlock_level; }

bool lab_try_buy(int index) {
    if (index < 0 || index >= LAB_UPGRADE_COUNT) {
        return false;
    }
    lab_upgrade_t *u = &g_lab.upgrades[index];
    const int price = lab_upgrade_price(u);
    if (!lab_upgrade_unlocked(u) || u->level >= u->max_level || g_lab.coins < price) {
        return false;
    }
    g_lab.coins -= price;
    u->level += 1;
    return true;
}

void lab_claim_result(void) {
    g_lab.coins += 240;
    g_lab.nuts += 15;
    g_lab.level += 1;
    g_lab.xp = 0;
}

void lab_format_amount(char *out, size_t cap, int amount) {
    char digits[24];
    (void)snprintf(digits, sizeof digits, "%lld", amount < 0 ? -(long long)amount : (long long)amount);
    const size_t n = strlen(digits);
    size_t o = 0;
    if (amount < 0 && o + 1 < cap) {
        out[o++] = '-';
    }
    for (size_t i = 0; i < n && o + 1 < cap; ++i) {
        if (i > 0 && (n - i) % 3 == 0 && o + 1 < cap) {
            out[o++] = ' ';
        }
        out[o++] = digits[i];
    }
    out[o] = '\0';
}

void lab_goto(lab_scene_id_t id) {
    if (id >= 0 && id < LAB_SCENE_COUNT) {
        s_pending = id;
    }
}

bool lab_goto_id(const char *id) {
    for (int i = 0; i < LAB_SCENE_COUNT; ++i) {
        if (strcmp(SCENES[i].id, id) == 0) {
            lab_goto((lab_scene_id_t)i);
            return true;
        }
    }
    return false;
}

lab_scene_id_t lab_scene(void) { return s_current; }

const lab_scene_t *lab_scene_desc(lab_scene_id_t id) { return (id >= 0 && id < LAB_SCENE_COUNT) ? &SCENES[id] : NULL; }

void lab_update(nt_ui_context_t *ctx, float dt) {
    if (!s_entered) {
        // The first scene is entered once the context exists, the same path a
        // switch takes.
        s_entered = true;
        if (SCENES[s_current].enter != NULL) {
            SCENES[s_current].enter(ctx);
        }
    }
    if (s_pending != LAB_SCENE_COUNT && s_pending != s_current) {
        if (SCENES[s_current].leave != NULL) {
            SCENES[s_current].leave(ctx);
        }
        s_current = s_pending;
        if (SCENES[s_current].enter != NULL) {
            SCENES[s_current].enter(ctx);
        }
    }
    s_pending = LAB_SCENE_COUNT;
    if (s_pending_theme >= 0) {
        (void)lab_theme_apply(s_pending_theme);
        s_pending_theme = -1;
    }

    // The second ability's cooldown loops so its disabled state is always on
    // screen for a while and then comes back.
    g_lab.cooldown_left -= dt;
    if (g_lab.cooldown_left < -2.0F) {
        g_lab.cooldown_left = 3.0F;
    }
}

// The scene list: compact chips sized to their labels, centred; one row
// across a desk, two rows of three in a hand. Lab chrome, so it borrows the
// plate and label roles at a smaller size rather than adding a role.
#define NAV_SCALE 0.8F
#define NAV_HEIGHT 0.75F
static bool nav_chip(nt_ui_context_t *ctx, const char *id, const char *label, bool active, const ui_metrics_t *m) {
    const ui_kit_button_kind_t kind = active ? UI_KIT_BUTTON_CONFIRM : UI_KIT_BUTTON_NEUTRAL;
    bool clicked = false;
    CLAY({.id = (Clay_ElementId){.id = nt_ui_id(id)},
          .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(m->hit * NAV_HEIGHT)}}}) {
        ui_kit_button_begin(ctx, nt_ui_child_id(nt_ui_id(id), "plate"), ui_kit_button_style(kind), true, NULL);
        CLAY({.layout = {.padding = {.left = (uint16_t)(m->gap * 0.8F), .right = (uint16_t)(m->gap * 0.8F)}}}) {
            ui_kit_label_scaled(ctx, label, ui_kit_button_label_style(kind), NAV_SCALE);
        }
        clicked = ui_kit_button_end(ctx);
    }
    return clicked;
}

static void build_nav(nt_ui_context_t *ctx) {
    const ui_metrics_t m = ui_metrics();
    const int per_row = m.portrait ? 3 : LAB_SCENE_COUNT;
    CLAY({.id = CLAY_ID("lab/nav"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .padding = {.left = (uint16_t)(m.margin + m.safe_l),
                                 .right = (uint16_t)(m.margin + m.safe_r),
                                 .top = (uint16_t)(m.margin * 0.5F + m.safe_t),
                                 .bottom = (uint16_t)(m.margin * 0.5F)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = (uint16_t)(m.gap * 0.4F),
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP}}}) {
        for (int row = 0; row * per_row < LAB_SCENE_COUNT; ++row) {
            CLAY({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = CLAY_LEFT_TO_RIGHT,
                             .childGap = (uint16_t)(m.gap * 0.4F)}}) {
                for (int i = row * per_row; i < LAB_SCENE_COUNT && i < (row + 1) * per_row; ++i) {
                    char id[48];
                    (void)snprintf(id, sizeof id, "lab/nav/%s", SCENES[i].id);
                    if (nav_chip(ctx, id, SCENES[i].title, i == (int)s_current, &m)) {
                        lab_goto((lab_scene_id_t)i);
                    }
                }
            }
        }
    }
}

// The scene list is lab chrome; a game has none. An overlay scene gets the
// whole canvas, the way its dialog does in a game, and returns to the HUD by
// its own close.
static bool nav_visible(lab_scene_id_t id) {
    return id == LAB_SCENE_HUD || id == LAB_SCENE_COMPONENTS || id == LAB_SCENE_THEMES;
}

void lab_build(nt_ui_context_t *ctx) {
    CLAY({.id = CLAY_ID("lab/root"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
        if (nav_visible(s_current)) {
            build_nav(ctx);
        }
        CLAY({.id = CLAY_ID("lab/scene"), .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {
            SCENES[s_current].build(ctx);
        }
    }
}
