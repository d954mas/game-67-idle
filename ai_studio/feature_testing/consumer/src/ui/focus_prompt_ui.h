#ifndef GAME_UI_FOCUS_PROMPT_UI_H
#define GAME_UI_FOCUS_PROMPT_UI_H

#include "game_input.h"
#include "ui/nt_ui.h"

#include <stdbool.h>

void focus_prompt_ui_update(const game_input_frame_t *input);
bool focus_prompt_ui_visible(void);
void focus_prompt_ui_build(nt_ui_context_t *ctx);

#endif /* GAME_UI_FOCUS_PROMPT_UI_H */
