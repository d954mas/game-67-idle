#include "game_input.h"

#include "input/nt_input.h"

#include <string.h>

_Static_assert(
    GAME_INPUT_POINTER_CAPACITY == NT_INPUT_MAX_POINTERS,
    "game input snapshot must cover every engine pointer slot");

void game_input_capture(game_input_frame_t *out) {
    memset(out, 0, sizeof *out);
    out->any_gesture = nt_input_any_key_pressed();
    out->escape_pressed = nt_input_key_is_pressed(NT_KEY_ESCAPE);
    out->move_left = nt_input_key_is_down(NT_KEY_A);
    out->move_right = nt_input_key_is_down(NT_KEY_D);
    out->move_up = nt_input_key_is_down(NT_KEY_W);
    out->move_down = nt_input_key_is_down(NT_KEY_S);

    for (int i = 0; i < GAME_INPUT_POINTER_CAPACITY; ++i) {
        const nt_pointer_t *source = &g_nt_input.pointers[i];
        game_pointer_input_t *target = &out->pointers[i];
        target->id = source->id;
        target->x = source->x;
        target->y = source->y;
        target->dx = source->dx;
        target->dy = source->dy;
        target->wheel_x = source->wheel_dx;
        target->wheel_y = source->wheel_dy;
        target->pressure = source->pressure;
        target->kind = source->type;
        target->active = source->active;
        target->left_down = source->buttons[NT_BUTTON_LEFT].is_down;
        target->left_pressed = source->buttons[NT_BUTTON_LEFT].is_pressed;
        target->left_released = source->buttons[NT_BUTTON_LEFT].is_released;
        target->right_down = source->buttons[NT_BUTTON_RIGHT].is_down;
        target->right_pressed = source->buttons[NT_BUTTON_RIGHT].is_pressed;
        target->right_released = source->buttons[NT_BUTTON_RIGHT].is_released;
        target->middle_down = source->buttons[NT_BUTTON_MIDDLE].is_down;
        target->middle_pressed = source->buttons[NT_BUTTON_MIDDLE].is_pressed;
        target->middle_released = source->buttons[NT_BUTTON_MIDDLE].is_released;
        if (source->active &&
            (source->wheel_dx != 0.0F || source->wheel_dy != 0.0F ||
             source->buttons[NT_BUTTON_LEFT].is_pressed ||
             source->buttons[NT_BUTTON_RIGHT].is_pressed ||
             source->buttons[NT_BUTTON_MIDDLE].is_pressed)) {
            out->any_gesture = true;
        }
    }
}
