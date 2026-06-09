#include "game_state_actions.h"

#include <stdio.h>

static void set_error(char *error, int error_cap, const char *message) {
    if (error && error_cap > 0) {
        (void)snprintf(error, (size_t)error_cap, "%s", message);
    }
}

static bool validate_after_action(GameState *state, bool changed, char *error, int error_cap) {
    if (!game_state_validate(state, error, error_cap)) {
        return false;
    }
    if (changed && state == &g_game_state) {
        game_state_mark_dirty();
    }
    return true;
}

bool game_state_action_shape_next(GameState *state, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    int next = (state->shape_index + 1) % GAME_STATE_SHAPE_COUNT;
    bool changed = state->shape_index != next;
    state->shape_index = next;
    return validate_after_action(state, changed, error, error_cap);
}

bool game_state_action_shape_prev(GameState *state, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    int next = (state->shape_index + GAME_STATE_SHAPE_COUNT - 1) % GAME_STATE_SHAPE_COUNT;
    bool changed = state->shape_index != next;
    state->shape_index = next;
    return validate_after_action(state, changed, error, error_cap);
}

bool game_state_action_render_mode_next(GameState *state, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    int next = (state->render_mode_index + 1) % GAME_STATE_RENDER_MODE_COUNT;
    bool changed = state->render_mode_index != next;
    state->render_mode_index = next;
    return validate_after_action(state, changed, error, error_cap);
}

bool game_state_action_camera_zoom(GameState *state, float wheel_delta, float zoom_speed, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    float old = state->camera_distance;
    state->camera_distance += wheel_delta * zoom_speed;
    if (state->camera_distance < 2.5F) {
        state->camera_distance = 2.5F;
    }
    if (state->camera_distance > 10.0F) {
        state->camera_distance = 10.0F;
    }
    return validate_after_action(state, state->camera_distance != old, error, error_cap);
}

bool game_state_action_test_ui_click(GameState *state, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    if (state->test_ui_clicks >= 1000000) {
        set_error(error, error_cap, "test_ui_clicks limit reached");
        return false;
    }
    state->test_ui_clicks++;
    (void)snprintf(state->test_label_text, sizeof(state->test_label_text), "Label: clicked %d", state->test_ui_clicks);
    (void)snprintf(state->test_button_text, sizeof(state->test_button_text), "Clicked %d", state->test_ui_clicks);
    return validate_after_action(state, true, error, error_cap);
}

static float clamp01(float value) {
    if (value < 0.0F) {
        return 0.0F;
    }
    if (value > 1.0F) {
        return 1.0F;
    }
    return value;
}

bool game_state_action_set_master_volume(GameState *state, float volume, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    float next = clamp01(volume);
    bool changed = state->settings_master_volume != next;
    state->settings_master_volume = next;
    return validate_after_action(state, changed, error, error_cap);
}

bool game_state_action_set_sfx_volume(GameState *state, float volume, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    float next = clamp01(volume);
    bool changed = state->settings_sfx_volume != next;
    state->settings_sfx_volume = next;
    return validate_after_action(state, changed, error, error_cap);
}
