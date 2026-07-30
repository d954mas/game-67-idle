#ifndef GAME_INPUT_H
#define GAME_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#define GAME_INPUT_POINTER_CAPACITY 8

typedef struct game_pointer_input_t {
    uint32_t id;
    float x;
    float y;
    float dx;
    float dy;
    float wheel_x;
    float wheel_y;
    float pressure;
    uint8_t kind;
    bool active;
    bool left_down;
    bool left_pressed;
    bool left_released;
    bool right_down;
    bool right_pressed;
    bool right_released;
    bool middle_down;
    bool middle_pressed;
    bool middle_released;
} game_pointer_input_t;

/* Immutable per-frame snapshot. Only game_input_capture reads engine input. */
typedef struct game_input_frame_t {
    bool any_gesture;
    bool escape_pressed;
    bool move_left;
    bool move_right;
    bool move_up;
    bool move_down;
    game_pointer_input_t pointers[GAME_INPUT_POINTER_CAPACITY];
} game_input_frame_t;

void game_input_capture(game_input_frame_t *out);

#endif /* GAME_INPUT_H */
