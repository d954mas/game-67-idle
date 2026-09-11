#ifndef UI_LAB_THEME_H
#define UI_LAB_THEME_H

#include "atlas/nt_atlas.h"
#include "material/nt_material.h"
#include "resource/nt_resource.h"

#include "features/ui_kit/ui_tokens.h"

#include <stdbool.h>

// The lab's theme table: one token sheet plus one folder of generated art per
// entry. Studio B is the kit's own preset; the other three are lab-owned copies
// of that sheet with new colours, which is exactly what a game does to repaint.
// Panel and tile colours are baked into the art, so a theme is always the pair
// (tokens, atlas folder) and never the tokens alone.
typedef struct {
    const char *id;    // atlas folder and CLI name
    const char *title; // what the Themes scene prints
    const ui_tokens_t *tokens;
} lab_theme_desc_t;

// Demo art shared by every theme: icons the scenes decorate with, and the
// world backdrop. Region refs resolve lazily and memoize, so they live here as
// mutable globals rather than being rebuilt every frame.
typedef struct {
    nt_atlas_region_ref_t coin;
    nt_atlas_region_ref_t xp;
    nt_atlas_region_ref_t energy;
    nt_atlas_region_ref_t sword;
    nt_atlas_region_ref_t potion;
    nt_atlas_region_ref_t world;
} lab_art_t;

extern lab_art_t g_lab_art;

// `radial` is the engine's radial SDF material (id 0 = none; the kit then
// draws its discs with the thumb art).
void lab_theme_bind(nt_resource_t ui_atlas, nt_material_t radial);

// True once the UI atlas is loaded and every region the table names exists.
// A missing region is logged once by name: a theme folder that was not packed
// fails here, not as a silently blank plate.
bool lab_theme_regions_ready(void);

int lab_theme_count(void);
const lab_theme_desc_t *lab_theme_at(int index);
int lab_theme_index(void);

// Repaints every kit style: ui_theme_init with this theme's tokens and art.
// Demo state is untouched; only the styles change.
bool lab_theme_apply(int index);
bool lab_theme_apply_id(const char *id);

#endif /* UI_LAB_THEME_H */
