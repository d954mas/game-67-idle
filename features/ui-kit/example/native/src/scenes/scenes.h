#ifndef UI_LAB_SCENES_H
#define UI_LAB_SCENES_H

#include "ui/nt_ui.h"

// One file per scene; lab.c owns the registry.

void scene_hud_build(nt_ui_context_t *ctx);
// The HUD is also the ground the overlay scenes stand on: they draw it, then
// their sheet on top.
void scene_hud_build_world(nt_ui_context_t *ctx);
void scene_hud_build_overlay(nt_ui_context_t *ctx);

void scene_upgrade_build(nt_ui_context_t *ctx);
void scene_upgrade_enter(nt_ui_context_t *ctx);
void scene_upgrade_leave(nt_ui_context_t *ctx);

void scene_result_build(nt_ui_context_t *ctx);
void scene_result_enter(nt_ui_context_t *ctx);
void scene_result_leave(nt_ui_context_t *ctx);

void scene_settings_build(nt_ui_context_t *ctx);
void scene_settings_enter(nt_ui_context_t *ctx);
void scene_settings_leave(nt_ui_context_t *ctx);

void scene_components_build(nt_ui_context_t *ctx);
void scene_components_leave(nt_ui_context_t *ctx);

void scene_themes_build(nt_ui_context_t *ctx);
void scene_themes_leave(nt_ui_context_t *ctx);

#endif /* UI_LAB_SCENES_H */
