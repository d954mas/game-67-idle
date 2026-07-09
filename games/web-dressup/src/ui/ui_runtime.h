#ifndef GAME_UI_RUNTIME_H
#define GAME_UI_RUNTIME_H

#include "font/nt_font.h"
#include "material/nt_material.h"
#include "resource/nt_resource.h"
#include "ui/nt_ui.h"

#include <stdbool.h>

// Owns the engine UI stack (sprite renderer + slice9 atlas + nt_ui context) so
// game systems only build widgets. main.c drives: init -> begin/build/end per
// frame -> teardown. Reuses main's text material + font (one glyph atlas).
//
//   if (ui_runtime_begin(dt)) { settings_draw_ui(ui_runtime_ctx(), &world); ui_runtime_end(); }
void ui_runtime_init(nt_material_t text_material, nt_font_t font, nt_resource_t font_resource);

// Binds atlas/font when ready, sets the UI projection, opens the nt_ui frame.
// Returns false (skip widget build) until the atlas + font + materials are ready.
bool ui_runtime_begin(float dt);
bool ui_runtime_ready(void);

// Closes the frame, walks the tree, flushes the sprite + text renderers.
void ui_runtime_end(void);

void ui_runtime_restore_gpu(void);
void ui_runtime_shutdown(void);
nt_ui_context_t *ui_runtime_ctx(void);

#endif /* GAME_UI_RUNTIME_H */
