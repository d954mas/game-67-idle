#include "game_state_actions.h"

#include <stdio.h>
#include <string.h>

#define FIRST_UPGRADE_COST 5
#define SECOND_UPGRADE_COST 8
#define THIRD_UPGRADE_COST 20
#define FOURTH_UPGRADE_COST 35
#define FIFTH_UPGRADE_COST 60
#define FIRST_JOB_DURATION_MS 6000
#define FIRST_JOB_REWARD 8
#define FIRST_JOB_ID "kiosk_memes"
#define SECOND_JOB_DURATION_MS 8000
#define SECOND_JOB_REWARD 30
#define SECOND_JOB_ID "sticker_run"
#define THIRD_JOB_DURATION_MS 10000
#define THIRD_JOB_REWARD 90
#define THIRD_JOB_ID "meme_stand_owner"
#define JOB_INACTIVE 0
#define JOB_RUNNING 1
#define JOB_DONE 2

static bool job_active(const GameState *state) { return state->active_job_id[0] != '\0'; }
static bool job_ready(const GameState *state) { return job_active(state) && state->active_job_elapsed_ms >= state->active_job_duration_ms; }
static bool second_job_unlocked(const GameState *state) { return state->third_upgrade_owned || state->status >= 6; }
static bool third_job_unlocked(const GameState *state) { return state->fifth_upgrade_owned || state->status >= 10; }

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

static float clamp_range(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static void set_feedback(GameState *state, int code) {
    state->feedback_code = code;
    state->feedback_pulse_ms = 900;
}

bool game_state_action_do67(GameState *state, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    state->seen_intro = true;
    state->meme_coins += state->click_power;
    state->coolness += 1;
    set_feedback(state, 1);
    return validate_after_action(state, true, error, error_cap);
}

bool game_state_action_buy_first_upgrade(GameState *state, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    if (state->first_upgrade_owned) {
        set_error(error, error_cap, "first upgrade already owned");
        return false;
    }
    if (state->meme_coins < FIRST_UPGRADE_COST) {
        set_error(error, error_cap, "not enough meme coins");
        return false;
    }
    state->meme_coins -= FIRST_UPGRADE_COST;
    state->first_upgrade_owned = true;
    state->status = 2;
    state->click_power = 2;
    state->hands_skill = 2;
    state->visual_stage = 2;
    set_feedback(state, 2);
    return validate_after_action(state, true, error, error_cap);
}

bool game_state_action_buy_second_upgrade(GameState *state, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    if (!state->first_upgrade_owned) {
        set_error(error, error_cap, "first upgrade required");
        return false;
    }
    if (state->second_upgrade_owned) {
        set_error(error, error_cap, "second upgrade already owned");
        return false;
    }
    if (state->meme_coins < SECOND_UPGRADE_COST) {
        set_error(error, error_cap, "not enough meme coins");
        return false;
    }
    state->meme_coins -= SECOND_UPGRADE_COST;
    state->second_upgrade_owned = true;
    if (state->status < 4) {
        state->status = 4;
    }
    state->click_power = 3;
    state->hands_skill = 3;
    if (state->visual_stage < 4) {
        state->visual_stage = 4;
    }
    set_feedback(state, 2);
    return validate_after_action(state, true, error, error_cap);
}

bool game_state_action_buy_third_upgrade(GameState *state, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    if (!state->second_upgrade_owned) {
        set_error(error, error_cap, "second upgrade required");
        return false;
    }
    if (state->third_upgrade_owned) {
        set_error(error, error_cap, "third upgrade already owned");
        return false;
    }
    if (state->meme_coins < THIRD_UPGRADE_COST) {
        set_error(error, error_cap, "not enough meme coins");
        return false;
    }
    state->meme_coins -= THIRD_UPGRADE_COST;
    state->third_upgrade_owned = true;
    if (state->status < 6) {
        state->status = 6;
    }
    state->click_power = 5;
    if (state->hands_skill < 4) {
        state->hands_skill = 4;
    }
    if (state->visual_stage < 6) {
        state->visual_stage = 6;
    }
    set_feedback(state, 2);
    return validate_after_action(state, true, error, error_cap);
}

bool game_state_action_buy_fourth_upgrade(GameState *state, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    if (!state->third_upgrade_owned) {
        set_error(error, error_cap, "third upgrade required");
        return false;
    }
    if (state->status < 7) {
        set_error(error, error_cap, "sticker job required");
        return false;
    }
    if (state->fourth_upgrade_owned) {
        set_error(error, error_cap, "fourth upgrade already owned");
        return false;
    }
    if (state->meme_coins < FOURTH_UPGRADE_COST) {
        set_error(error, error_cap, "not enough meme coins");
        return false;
    }
    state->meme_coins -= FOURTH_UPGRADE_COST;
    state->fourth_upgrade_owned = true;
    if (state->status < 9) {
        state->status = 9;
    }
    state->click_power = 8;
    if (state->income_per_second < 3) {
        state->income_per_second = 3;
    }
    if (state->hands_skill < 6) {
        state->hands_skill = 6;
    }
    if (state->visual_stage < 9) {
        state->visual_stage = 9;
    }
    set_feedback(state, 2);
    return validate_after_action(state, true, error, error_cap);
}

bool game_state_action_buy_fifth_upgrade(GameState *state, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    if (!state->fourth_upgrade_owned) {
        set_error(error, error_cap, "fourth upgrade required");
        return false;
    }
    if (state->status < 9) {
        set_error(error, error_cap, "status 9 required");
        return false;
    }
    if (state->fifth_upgrade_owned) {
        set_error(error, error_cap, "fifth upgrade already owned");
        return false;
    }
    if (state->meme_coins < FIFTH_UPGRADE_COST) {
        set_error(error, error_cap, "not enough meme coins");
        return false;
    }
    state->meme_coins -= FIFTH_UPGRADE_COST;
    state->fifth_upgrade_owned = true;
    if (state->status < 10) {
        state->status = 10;
    }
    if (state->click_power < 10) {
        state->click_power = 10;
    }
    if (state->income_per_second < 8) {
        state->income_per_second = 8;
    }
    if (state->hands_skill < 7) {
        state->hands_skill = 7;
    }
    if (state->visual_stage < 10) {
        state->visual_stage = 10;
    }
    set_feedback(state, 2);
    return validate_after_action(state, true, error, error_cap);
}

bool game_state_action_start_first_job(GameState *state, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    if (!state->first_upgrade_owned) {
        set_error(error, error_cap, "first upgrade required");
        return false;
    }
    if (job_active(state)) {
        set_error(error, error_cap, "job already active");
        return false;
    }
    const bool use_third_job = third_job_unlocked(state);
    const bool use_second_job = !use_third_job && second_job_unlocked(state);
    (void)snprintf(state->active_job_id, sizeof(state->active_job_id), "%s", use_third_job ? THIRD_JOB_ID : (use_second_job ? SECOND_JOB_ID : FIRST_JOB_ID));
    state->active_job_elapsed_ms = 0;
    state->active_job_duration_ms = use_third_job ? THIRD_JOB_DURATION_MS : (use_second_job ? SECOND_JOB_DURATION_MS : FIRST_JOB_DURATION_MS);
    set_feedback(state, 3);
    return validate_after_action(state, true, error, error_cap);
}

bool game_state_action_claim_first_job(GameState *state, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    if (!job_active(state)) {
        set_error(error, error_cap, "no first job active");
        return false;
    }
    if (!job_ready(state)) {
        set_error(error, error_cap, "first job is not done");
        return false;
    }
    bool third_job = strcmp(state->active_job_id, THIRD_JOB_ID) == 0;
    bool second_job = strcmp(state->active_job_id, SECOND_JOB_ID) == 0;
    state->meme_coins += third_job ? THIRD_JOB_REWARD : (second_job ? SECOND_JOB_REWARD : FIRST_JOB_REWARD);
    if (third_job) {
        if (state->income_per_second < 16) {
            state->income_per_second = 16;
        }
        if (state->status < 11) {
            state->status = 11;
        }
        if (state->hands_skill < 8) {
            state->hands_skill = 8;
        }
        if (state->visual_stage < 11) {
            state->visual_stage = 11;
        }
    } else if (second_job) {
        if (state->income_per_second < 2) {
            state->income_per_second = 2;
        }
        if (state->status < 7) {
            state->status = 7;
        }
        if (state->hands_skill < 5) {
            state->hands_skill = 5;
        }
        if (state->visual_stage < 7) {
            state->visual_stage = 7;
        }
    } else {
        state->income_per_second = 1;
        state->comfort = 2;
        if (state->status < 3) {
            state->status = 3;
        }
        if (state->visual_stage < 3) {
            state->visual_stage = 3;
        }
    }
    if (state->comfort < 2) {
        state->comfort = 2;
    }
    state->active_job_id[0] = '\0';
    state->active_job_elapsed_ms = 0;
    state->active_job_duration_ms = 0;
    set_feedback(state, third_job ? 8 : (second_job ? 6 : 4));
    return validate_after_action(state, true, error, error_cap);
}

bool game_state_action_tick(GameState *state, int delta_ms, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    if (delta_ms < 0) {
        delta_ms = 0;
    }
    bool changed = false;
    if (job_active(state) && state->active_job_elapsed_ms < state->active_job_duration_ms) {
        state->active_job_elapsed_ms += delta_ms;
        if (state->active_job_elapsed_ms > state->active_job_duration_ms) {
            state->active_job_elapsed_ms = state->active_job_duration_ms;
        }
        changed = true;
    }
    if (state->income_per_second > 0) {
        state->income_accum_ms += delta_ms;
        while (state->income_accum_ms >= 1000) {
            state->income_accum_ms -= 1000;
            state->meme_coins += state->income_per_second;
            changed = true;
        }
    }
    if (state->feedback_pulse_ms > 0) {
        state->feedback_pulse_ms -= delta_ms;
        if (state->feedback_pulse_ms < 0) {
            state->feedback_pulse_ms = 0;
        }
        changed = true;
    }
    return validate_after_action(state, changed, error, error_cap);
}

bool game_state_action_reset_playtest(GameState *state, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    game_state_init_defaults(state);
    set_feedback(state, 5);
    return validate_after_action(state, true, error, error_cap);
}

bool game_state_action_set_master_volume(GameState *state, float volume, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    float next = clamp_range(volume, GAME_STATE_SETTINGS_MASTER_VOLUME_MIN, GAME_STATE_SETTINGS_MASTER_VOLUME_MAX);
    bool changed = state->settings_master_volume != next;
    state->settings_master_volume = next;
    return validate_after_action(state, changed, error, error_cap);
}

bool game_state_action_set_sfx_volume(GameState *state, float volume, char *error, int error_cap) {
    if (!state) {
        set_error(error, error_cap, "state is null");
        return false;
    }
    float next = clamp_range(volume, GAME_STATE_SETTINGS_SFX_VOLUME_MIN, GAME_STATE_SETTINGS_SFX_VOLUME_MAX);
    bool changed = state->settings_sfx_volume != next;
    state->settings_sfx_volume = next;
    return validate_after_action(state, changed, error, error_cap);
}
