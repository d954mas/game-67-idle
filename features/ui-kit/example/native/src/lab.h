#ifndef UI_LAB_H
#define UI_LAB_H

#include "atlas/nt_atlas.h"
#include "ui/nt_ui.h"

#include <stdbool.h>

// The polygon: a list of scenes, one of them current, and the demo numbers
// they all read. Nothing here is a game system; a scene is a recipe a game
// copies and feeds from its own state.

typedef enum {
    LAB_SCENE_HUD = 0,
    LAB_SCENE_UPGRADE,
    LAB_SCENE_RESULT,
    LAB_SCENE_SETTINGS,
    LAB_SCENE_COMPONENTS,
    LAB_SCENE_THEMES,
    LAB_SCENE_COUNT
} lab_scene_id_t;

typedef struct {
    const char *id;    // stable: CLI, docs, ui.tree ids
    const char *title; // the nav label
    void (*build)(nt_ui_context_t *ctx);
    // Scene-owned transient view state (a scroll offset, a modal tween) is
    // dropped on leave so the state pool never fills across scene loops.
    void (*enter)(nt_ui_context_t *ctx);
    void (*leave)(nt_ui_context_t *ctx);
} lab_scene_t;

#define LAB_UPGRADE_COUNT 5
typedef struct {
    const char *name;
    int decimals; // how the value prints: 20 or 2.5
    float base;
    float step;
    int price;      // level 0 price; grows with level
    int level;
    int max_level;
    int unlock_level; // player level that unlocks it; 0 = always
    nt_atlas_region_ref_t *icon;
} lab_upgrade_t;

typedef struct {
    int coins;
    int nuts;
    int level;
    int xp;
    int xp_next;
    int ability;         // selected ability slot
    float cooldown_left; // seconds left on the second ability
    bool music;
    bool sound;
    bool vibration;
    float volume_master;
    float volume_music;
    float volume_sfx;
    int language;
    lab_upgrade_t upgrades[LAB_UPGRADE_COUNT];
    bool sheet_open; // the overlay scene's modal; the engine clears it on close
} lab_state_t;

extern lab_state_t g_lab;

void lab_init(void);
// Applies a pending scene switch and advances demo timers. Call before build.
void lab_update(nt_ui_context_t *ctx, float dt);
void lab_build(nt_ui_context_t *ctx);

// A switch requested during a build lands on the next update, so the tree
// being declared stays balanced and leave() never clears a live state cell.
void lab_goto(lab_scene_id_t id);
bool lab_goto_id(const char *id);
// A theme change is deferred the same way: styles the engine already holds
// pointers to for this frame are not rewritten under it.
void lab_request_theme(int index);
lab_scene_id_t lab_scene(void);
const lab_scene_t *lab_scene_desc(lab_scene_id_t id);

int lab_upgrade_price(const lab_upgrade_t *u);
float lab_upgrade_value(const lab_upgrade_t *u, int level);
bool lab_upgrade_unlocked(const lab_upgrade_t *u);
bool lab_try_buy(int index);
void lab_claim_result(void);

// Thousands separated with a space, the way a counter reads at a glance.
void lab_format_amount(char *out, size_t cap, int amount);

#endif /* UI_LAB_H */
