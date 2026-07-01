#ifndef GAME_DEVAPI_UI_H
#define GAME_DEVAPI_UI_H

/* Game-owned DevAPI commands the engine deliberately does not ship: a per-frame
   UI-introspection tree (ui.tree/ui.element/ui.click) + entity.list, registered
   on the engine bus as group="game". Dev-only, gated with the rest of the layer. */

#if NT_DEVAPI_ENABLED

#include <stdbool.h>

/* Per-frame: clear, then register each UI node in the game's logical (input)
   coordinate space so a bot can introspect and click the UI. */
void game_devapi_ui_clear(void);
bool game_devapi_ui_register_node(const char *id, const char *parent_id, const char *role,
                                  const char *label, const char *text,
                                  float x, float y, float w, float h, bool visible, bool enabled);

/* Register the ui.* and entity.list commands on the engine bus (call once after
   nt_devapi_init). */
void game_devapi_ui_register(void);

#endif

#endif
