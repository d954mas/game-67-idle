#ifndef UI_LAB_UI_RUNTIME_H
#define UI_LAB_UI_RUNTIME_H

#include "font/nt_font.h"
#include "input/nt_input.h"
#include "material/nt_material.h"
#include "resource/nt_resource.h"
#include "ui/nt_ui.h"

#include <stdbool.h>

// Owns the nt_ui context and the frame around it: the kit opens the canvas
// (ui_frame_begin), this feeds nt_ui the pointers and walks the tree. main owns
// the materials, the font and the resources and hands them in once.
//
//   if (lab_ui_runtime_begin(dt, pointers, count)) { lab_build(lab_ui_runtime_ctx()); lab_ui_runtime_end(); }
void lab_ui_runtime_init(nt_material_t sprite_material, nt_material_t text_material, nt_font_t font,
                         nt_resource_t font_resource, nt_resource_t ui_atlas);

// False until the atlas, its white region, the font and both programs are live.
bool lab_ui_runtime_ready(void);

// Pointers are raw device pixels; the context converts them through the
// viewport, so a synthetic pointer takes the same path as the mouse.
bool lab_ui_runtime_begin(float dt, const nt_pointer_t *pointers, uint32_t count);
void lab_ui_runtime_end(void);

void lab_ui_runtime_restore_gpu(void);
void lab_ui_runtime_shutdown(void);
nt_ui_context_t *lab_ui_runtime_ctx(void);

#endif /* UI_LAB_UI_RUNTIME_H */
