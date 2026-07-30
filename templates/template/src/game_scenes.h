#ifndef GAME_SCENES_H
#define GAME_SCENES_H

#include "features/scenes/scene_manager.h"
#include "game_input.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct World World;
typedef struct nt_ui_context nt_ui_context_t;

#define GAME_SCENE_ROOT "game"
#define GAME_SCENE_SETTINGS "settings"

void game_scenes_init(World *world);
void game_scenes_step(uint64_t frame_index, float dt);
void game_scenes_update(float dt, const game_input_frame_t *input);
void game_scenes_build_ui(nt_ui_context_t *ui_context);
void game_scenes_build_input_gate(nt_ui_context_t *ui_context);
void game_scenes_shutdown(void);

scene_manager_t *game_scenes_manager(void);
bool game_scenes_is_in_history(const char *scene_id);
bool game_scenes_is_presented(const char *scene_id);
bool game_scenes_can_process_game_input(void);
bool game_scenes_should_render_world(void);
bool game_scenes_input_gated(void);
bool game_scenes_handle_escape(void);

scene_result_t game_scenes_show_settings(void);
/* Returns NOT_TOP when settings exists in history but is not the focused top. */
scene_result_t game_scenes_close_settings(void);

#endif
