#include "game_state_actions.h"

#include <stdio.h>
#include <string.h>

#define RUNE_HP_MAX 20
#define RUNE_MANA_BASE_MAX 10
#define RUNE_MANA_WARD1_MAX 12
#define RUNE_MANA_WARD2_MAX 14
#define RUNE_WISP_HP 10
#define RUNE_WISP_DAMAGE 2
#define RUNE_REED_RAIDER_HP 12
#define RUNE_REED_RAIDER_DAMAGE 3
#define RUNE_FEN_SHADE_HP 14
#define RUNE_FEN_SHADE_DAMAGE 4
#define RUNE_BRIAR_STALKER_HP 18
#define RUNE_BRIAR_STALKER_DAMAGE 5
#define RUNE_MOONWELL_SENTINEL_HP 16
#define RUNE_MOONWELL_SENTINEL_DAMAGE 4
#define RUNE_STRIKE_DAMAGE 3
#define RUNE_SPARK_BASE_DAMAGE 5
#define RUNE_SPARK_UPGRADE_BONUS 2
#define RUNE_SPARK_WARD2_BONUS 4
#define RUNE_SPARK_COST 3
#define RUNE_SCOUT_SILVER 6
#define RUNE_SCOUT_XP 4
#define RUNE_EAST_ROAD_SILVER 8
#define RUNE_EAST_ROAD_XP 6
#define RUNE_GREENFEN_SILVER 10
#define RUNE_GREENFEN_XP 8
#define RUNE_BRIAR_GATE_SILVER 12
#define RUNE_BRIAR_GATE_XP 10
#define RUNE_MOONWELL_SILVER 6
#define RUNE_MOONWELL_XP 6
#define RUNE_ASHEN_CAIRN_XP 4
#define RUNE_STARFALL_GROTTO_XP 4
#define RUNE_LEVEL2_XP 20
#define RUNE_LEVEL_HP_BONUS 4
#define RUNE_REST_COST 3
#define RUNE_SPARK_WARD_COST_SILVER 12
#define RUNE_SPARK_WARD_COST_SPARKS 1
#define RUNE_BELL_ROPE_SILVER_REWARD 6

static void set_text(char *dst, size_t dst_cap, const char *text) {
    (void)snprintf(dst, dst_cap, "%s", text);
}

static void set_reward(GameState *state, const char *text) {
    set_text(state->rune_reward_text, sizeof(state->rune_reward_text), text);
}

static int rune_mana_max(const GameState *state) {
    if (state->rune_spell_level >= 2) {
        return RUNE_MANA_WARD2_MAX;
    }
    return state->rune_spell_level > 0 ? RUNE_MANA_WARD1_MAX : RUNE_MANA_BASE_MAX;
}

int game_rune_hp_max(const GameState *state) {
    return RUNE_HP_MAX + (state->rune_player_level > 1 ? RUNE_LEVEL_HP_BONUS : 0);
}

static bool rune_check_level_up(GameState *state) {
    if (state->rune_player_level < 2 && state->rune_xp >= RUNE_LEVEL2_XP) {
        state->rune_player_level = 2;
        state->rune_hp = game_rune_hp_max(state);
        state->rune_ward_rank += 1;
        if (state->rune_ward_rank > 99) {
            state->rune_ward_rank = 99;
        }
        return true;
    }
    return false;
}

static void sync_compatibility(GameState *state) {
    state->wallet_soft = state->rune_silver;
    set_text(state->test_button_text, sizeof(state->test_button_text), "Scout");
    (void)snprintf(
        state->test_label_text,
        sizeof(state->test_label_text),
        "Rune Marches: %s",
        state->rune_combat_log);
}

static void mark_rune_dirty(GameState *state) {
    sync_compatibility(state);
    game_state_mark_dirty();
}

static bool rune_in_combat(const GameState *state) {
    return state->rune_encounter != GAME_STATE_RUNE_ENCOUNTER_NONE;
}

static int rune_enemy_damage(const GameState *state) {
    if (state->rune_encounter == GAME_STATE_RUNE_ENCOUNTER_BRIAR_STALKER) {
        return RUNE_BRIAR_STALKER_DAMAGE;
    }
    if (state->rune_encounter == GAME_STATE_RUNE_ENCOUNTER_MOONWELL_SENTINEL) {
        return RUNE_MOONWELL_SENTINEL_DAMAGE;
    }
    if (state->rune_encounter == GAME_STATE_RUNE_ENCOUNTER_FEN_SHADE) {
        return RUNE_FEN_SHADE_DAMAGE;
    }
    return state->rune_encounter == GAME_STATE_RUNE_ENCOUNTER_REED_RAIDER ? RUNE_REED_RAIDER_DAMAGE : RUNE_WISP_DAMAGE;
}

int game_rune_enemy_max_hp(const GameState *state) {
    if (state->rune_encounter == GAME_STATE_RUNE_ENCOUNTER_BRIAR_STALKER) {
        return RUNE_BRIAR_STALKER_HP;
    }
    if (state->rune_encounter == GAME_STATE_RUNE_ENCOUNTER_MOONWELL_SENTINEL) {
        return RUNE_MOONWELL_SENTINEL_HP;
    }
    if (state->rune_encounter == GAME_STATE_RUNE_ENCOUNTER_FEN_SHADE) {
        return RUNE_FEN_SHADE_HP;
    }
    return state->rune_encounter == GAME_STATE_RUNE_ENCOUNTER_REED_RAIDER ? RUNE_REED_RAIDER_HP : RUNE_WISP_HP;
}

static void rune_soft_loss(GameState *state) {
    state->rune_location = GAME_STATE_RUNE_LOCATION_MIREGATE;
    state->rune_encounter = GAME_STATE_RUNE_ENCOUNTER_NONE;
    state->rune_enemy_hp = 0;
    state->rune_hp = 1;
    state->rune_xp += 1;
    set_text(state->rune_objective, sizeof(state->rune_objective), "Rest at Miregate");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "You retreat to Miregate");
}

static void rune_win_encounter(GameState *state) {
    if (state->rune_encounter == GAME_STATE_RUNE_ENCOUNTER_BRIAR_STALKER) {
        state->rune_location = GAME_STATE_RUNE_LOCATION_BRIAR_GATE;
        state->rune_encounter = GAME_STATE_RUNE_ENCOUNTER_NONE;
        state->rune_enemy_hp = 0;
        state->rune_silver += RUNE_BRIAR_GATE_SILVER;
        state->rune_xp += RUNE_BRIAR_GATE_XP;
        const bool leveled = rune_check_level_up(state);
        state->rune_briar_gate_safety += 1;
        if (state->rune_main_quest_step < 16) {
            state->rune_main_quest_step = 16;
        }
        set_text(state->rune_objective, sizeof(state->rune_objective), "Briar Gate safer");
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), leveled ? "Briar stalker cleared; Rank II" : "Briar stalker cleared");
        set_reward(state, leveled ? "+12 SILVER  +10 XP  BRIAR  RANK II" : "+12 SILVER  +10 XP  BRIAR SAFE");
        return;
    }

    if (state->rune_encounter == GAME_STATE_RUNE_ENCOUNTER_MOONWELL_SENTINEL) {
        state->rune_location = GAME_STATE_RUNE_LOCATION_MOONWELL;
        state->rune_encounter = GAME_STATE_RUNE_ENCOUNTER_NONE;
        state->rune_enemy_hp = 0;
        state->rune_silver += RUNE_MOONWELL_SILVER;
        state->rune_xp += RUNE_MOONWELL_XP;
        const bool leveled = rune_check_level_up(state);
        state->rune_moonwell_safety += 1;
        state->rune_moonwell_blessing += 1;
        if (state->rune_moonwell_blessing > 9) {
            state->rune_moonwell_blessing = 9;
        }
        state->rune_mana = rune_mana_max(state);
        if (state->rune_main_quest_step < 16) {
            state->rune_main_quest_step = 16;
        }
        set_text(state->rune_objective, sizeof(state->rune_objective), "Moonwell oath sealed");
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), leveled ? "Moonwell sentinel calmed; Rank II" : "Moonwell sentinel calmed");
        set_reward(state, leveled ? "+1 MOONWELL  +6 XP  MP FULL  RANK II" : "+1 MOONWELL  +6 XP  MP FULL");
        return;
    }

    if (state->rune_encounter == GAME_STATE_RUNE_ENCOUNTER_FEN_SHADE) {
        state->rune_location = GAME_STATE_RUNE_LOCATION_GREENFEN_CAUSEWAY;
        state->rune_encounter = GAME_STATE_RUNE_ENCOUNTER_NONE;
        state->rune_enemy_hp = 0;
        state->rune_silver += RUNE_GREENFEN_SILVER;
        state->rune_xp += RUNE_GREENFEN_XP;
        const bool leveled = rune_check_level_up(state);
        state->rune_greenfen_safety += 1;
        state->rune_rune_lore += 1;
        if (state->rune_main_quest_step < 12) {
            state->rune_main_quest_step = 12;
        }
        set_text(state->rune_objective, sizeof(state->rune_objective), "Study rune lore");
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), leveled ? "Fen shade: +10 +8 XP; Rank II" : "Fen shade: +10 silver +8 XP");
        set_reward(state, leveled ? "+10 SILVER  +8 XP  LORE  RANK II" : "+10 SILVER  +8 XP  LORE");
        return;
    }

    if (state->rune_encounter == GAME_STATE_RUNE_ENCOUNTER_REED_RAIDER) {
        state->rune_location = GAME_STATE_RUNE_LOCATION_REEDMERE_CROSSING;
        state->rune_encounter = GAME_STATE_RUNE_ENCOUNTER_NONE;
        state->rune_enemy_hp = 0;
        state->rune_silver += RUNE_EAST_ROAD_SILVER;
        state->rune_xp += RUNE_EAST_ROAD_XP;
        const bool leveled = rune_check_level_up(state);
        state->rune_east_road_safety += 1;
        if (state->rune_main_quest_step < 8) {
            state->rune_main_quest_step = 8;
        }
        set_text(state->rune_objective, sizeof(state->rune_objective), state->rune_kindness_reputation > 0 ? "Light Moss Shrine" : "Reedmere is open");
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), leveled ? "Reed raider: +8 +6 XP; Rank II" : "Reed raider: +8 silver +6 XP");
        set_reward(state, leveled ? "+8 SILVER  +6 XP  RANK II" : "+8 SILVER  +6 XP  EAST SAFE");
        return;
    }

    const bool first_spark = state->rune_sparks == 0 && state->rune_main_quest_step < 2;
    const bool found_bell_rope = state->rune_road_safety >= 1 && state->rune_side_quest_step == 1 && !state->rune_bell_rope_charm;
    state->rune_location = GAME_STATE_RUNE_LOCATION_WISPFEN_ROAD;
    state->rune_encounter = GAME_STATE_RUNE_ENCOUNTER_NONE;
    state->rune_enemy_hp = 0;
    state->rune_silver += RUNE_SCOUT_SILVER;
    state->rune_xp += RUNE_SCOUT_XP;
    const bool leveled = rune_check_level_up(state);
    state->rune_road_safety += 1;
    state->rune_ward_rank += 1;
    if (first_spark) {
        state->rune_sparks += 1;
    }
    if (state->rune_main_quest_step < 2) {
        state->rune_main_quest_step = 2;
    }
    if (found_bell_rope) {
        state->rune_bell_rope_charm = true;
        state->rune_side_quest_step = 2;
        set_text(state->rune_objective, sizeof(state->rune_objective), "Return bell rope?");
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Found bell rope charm");
        set_reward(state, "+6 SILVER  +4 XP  BELL ROPE");
    } else {
        set_text(state->rune_objective, sizeof(state->rune_objective), "Buy Spark Ward");
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), leveled ? "Wisp: +6 +4 XP; Rank II" : "Wisp defeated: +6 silver +4 XP");
        set_reward(state, first_spark ? "+6 SILVER  +4 XP  +SPARK" : (leveled ? "+6 SILVER  +4 XP  RANK II" : "+6 SILVER  +4 XP"));
    }
}

static void rune_enemy_turn(GameState *state, int damage) {
    if (!rune_in_combat(state) || state->rune_enemy_hp <= 0) {
        return;
    }
    state->rune_hp -= damage;
    if (state->rune_hp <= 0) {
        rune_soft_loss(state);
    }
}

static void rune_damage_enemy(GameState *state, int damage, const char *log, int return_damage) {
    if (!rune_in_combat(state)) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Scout first");
        return;
    }
    state->rune_enemy_hp -= damage;
    if (state->rune_enemy_hp <= 0) {
        rune_win_encounter(state);
        return;
    }
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), log);
    rune_enemy_turn(state, return_damage);
}

void game_seed_reset_playtest(GameState *state) {
    game_rune_reset_playtest(state);
}

void game_seed_cycle(GameState *state) {
    state->shape_index = (state->shape_index + 1) % GAME_STATE_SHAPE_COUNT;
    game_rune_scout(state);
}

const char *game_seed_shape_label(const GameState *state) {
    return game_state_shape_name(state->shape_index);
}

void game_rune_reset_playtest(GameState *state) {
    game_state_init_defaults(state);
    state->rune_hp = game_rune_hp_max(state);
    state->rune_mana = RUNE_MANA_BASE_MAX;
    set_text(state->rune_objective, sizeof(state->rune_objective), "Scout the road");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Scout Wispfen Road");
    set_reward(state, "");
    sync_compatibility(state);
    game_state_mark_dirty();
}

void game_rune_scout(GameState *state) {
    state->test_ui_clicks += 1;
    state->tutorial_done = true;
    if (rune_in_combat(state)) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Enemy blocks the road");
        mark_rune_dirty(state);
        return;
    }
    state->rune_location = GAME_STATE_RUNE_LOCATION_WISPFEN_ROAD;
    state->rune_encounter = GAME_STATE_RUNE_ENCOUNTER_MIRE_WISP;
    state->rune_enemy_hp = RUNE_WISP_HP;
    if (state->rune_side_quest_step == 0) {
        state->rune_side_quest_step = 1;
    }
    if (state->rune_main_quest_step < 1) {
        state->rune_main_quest_step = 1;
    }
    set_text(state->rune_objective, sizeof(state->rune_objective), "Defeat Mire Wisp");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "A Mire Wisp appears");
    mark_rune_dirty(state);
}

void game_rune_scout_east(GameState *state) {
    state->test_ui_clicks += 1;
    state->tutorial_done = true;
    if (!state->rune_east_road_unlocked) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Listen at tower first");
        mark_rune_dirty(state);
        return;
    }
    if (rune_in_combat(state)) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Enemy blocks the road");
        mark_rune_dirty(state);
        return;
    }
    state->rune_location = GAME_STATE_RUNE_LOCATION_REEDMERE_CROSSING;
    state->rune_encounter = GAME_STATE_RUNE_ENCOUNTER_REED_RAIDER;
    state->rune_enemy_hp = RUNE_REED_RAIDER_HP;
    if (state->rune_main_quest_step < 7) {
        state->rune_main_quest_step = 7;
    }
    set_text(state->rune_objective, sizeof(state->rune_objective), "Clear Reedmere");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Reed raider blocks crossing");
    mark_rune_dirty(state);
}

bool game_rune_can_scout_greenfen(const GameState *state) {
    return state->rune_causeway_unlocked && state->rune_route_choice == GAME_STATE_RUNE_ROUTE_CHOICE_NONE && !rune_in_combat(state);
}

void game_rune_scout_greenfen(GameState *state) {
    state->test_ui_clicks += 1;
    state->tutorial_done = true;
    if (!state->rune_causeway_unlocked) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Open causeway first");
        mark_rune_dirty(state);
        return;
    }
    if (rune_in_combat(state)) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Enemy blocks the road");
        mark_rune_dirty(state);
        return;
    }
    state->rune_location = GAME_STATE_RUNE_LOCATION_GREENFEN_CAUSEWAY;
    state->rune_encounter = GAME_STATE_RUNE_ENCOUNTER_FEN_SHADE;
    state->rune_enemy_hp = RUNE_FEN_SHADE_HP;
    if (state->rune_main_quest_step < 11) {
        state->rune_main_quest_step = 11;
    }
    set_text(state->rune_objective, sizeof(state->rune_objective), "Clear Fen Shade");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Fen shade rises");
    mark_rune_dirty(state);
}

void game_rune_primary_action(GameState *state) {
    if (state->rune_tower_unlocked && !state->rune_tower_inspected) {
        game_rune_inspect_tower(state);
        return;
    }
    if (game_rune_can_choose_next_route(state)) {
        game_rune_choose_briar_gate(state);
        return;
    }
    if (game_rune_can_scout_briar_gate(state)) {
        game_rune_scout_briar_gate(state);
        return;
    }
    if (game_rune_can_scout_moonwell(state)) {
        game_rune_scout_moonwell(state);
        return;
    }
    if (game_rune_can_discover_ashen_cairn(state)) {
        game_rune_discover_ashen_cairn(state);
        return;
    }
    if (game_rune_can_discover_starfall_grotto(state)) {
        game_rune_discover_starfall_grotto(state);
        return;
    }
    if (state->rune_route_choice == GAME_STATE_RUNE_ROUTE_CHOICE_BRIAR_GATE) {
        state->rune_location = state->rune_ashen_cairn_unlocked ? GAME_STATE_RUNE_LOCATION_ASHEN_CAIRN : GAME_STATE_RUNE_LOCATION_BRIAR_GATE;
        set_text(state->rune_objective, sizeof(state->rune_objective), state->rune_ashen_cairn_unlocked ? "Ashen Cairn found" : "Briar Gate safer");
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), state->rune_ashen_cairn_unlocked ? "Ashen Cairn waits" : "Briar Gate patrol holds");
        mark_rune_dirty(state);
        return;
    }
    if (state->rune_route_choice == GAME_STATE_RUNE_ROUTE_CHOICE_MOONWELL) {
        state->rune_location = state->rune_starfall_grotto_unlocked ? GAME_STATE_RUNE_LOCATION_STARFALL_GROTTO : GAME_STATE_RUNE_LOCATION_MOONWELL;
        set_text(state->rune_objective, sizeof(state->rune_objective), state->rune_starfall_grotto_unlocked ? "Starfall Grotto found" : "Moonwell oath sealed");
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), state->rune_starfall_grotto_unlocked ? "Starfall Grotto glows" : "Moonwell blessing hums");
        mark_rune_dirty(state);
        return;
    }
    if (game_rune_can_scout_greenfen(state)) {
        game_rune_scout_greenfen(state);
        return;
    }
    if (game_rune_can_open_causeway(state) || state->rune_causeway_unlocked) {
        game_rune_open_causeway(state);
        return;
    }
    if (state->rune_east_road_unlocked) {
        game_rune_scout_east(state);
        return;
    }
    game_rune_scout(state);
}

void game_rune_strike(GameState *state) {
    rune_damage_enemy(state, RUNE_STRIKE_DAMAGE, "Strike hits; foe strikes", rune_enemy_damage(state));
    mark_rune_dirty(state);
}

void game_rune_spark(GameState *state) {
    if (!rune_in_combat(state)) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Scout first");
        mark_rune_dirty(state);
        return;
    }
    if (state->rune_mana < RUNE_SPARK_COST) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Need mana: rest");
        mark_rune_dirty(state);
        return;
    }
    state->rune_mana -= RUNE_SPARK_COST;
    rune_damage_enemy(state, game_rune_spark_damage(state), "Spark burns; foe strikes", rune_enemy_damage(state));
    mark_rune_dirty(state);
}

void game_rune_guard(GameState *state) {
    if (!rune_in_combat(state)) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Scout first");
        mark_rune_dirty(state);
        return;
    }
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Guard: damage reduced");
    rune_enemy_turn(state, 1);
    mark_rune_dirty(state);
}

void game_rune_retreat(GameState *state) {
    if (!rune_in_combat(state)) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "No danger nearby");
        mark_rune_dirty(state);
        return;
    }
    state->rune_location = GAME_STATE_RUNE_LOCATION_MIREGATE;
    state->rune_encounter = GAME_STATE_RUNE_ENCOUNTER_NONE;
    state->rune_enemy_hp = 0;
    if (state->rune_hp > 1) {
        state->rune_hp -= 1;
    }
    set_text(state->rune_objective, sizeof(state->rune_objective), "Rest or scout again");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Retreated to Miregate");
    mark_rune_dirty(state);
}

void game_rune_rest(GameState *state) {
    const int mana_max = rune_mana_max(state);
    const int hp_max = game_rune_hp_max(state);
    if (state->rune_hp >= hp_max && state->rune_mana >= mana_max) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Already rested");
        mark_rune_dirty(state);
        return;
    }
    if (state->rune_free_rest_used && state->rune_silver < RUNE_REST_COST) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Need 3 silver to rest");
        mark_rune_dirty(state);
        return;
    }
    if (state->rune_free_rest_used) {
        state->rune_silver -= RUNE_REST_COST;
    } else {
        state->rune_free_rest_used = true;
    }
    state->rune_hp = hp_max;
    state->rune_mana = mana_max;
    set_text(state->rune_objective, sizeof(state->rune_objective), "Scout the road");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Rested at Miregate");
    set_reward(state, "RESTED  HP AND MP FULL");
    mark_rune_dirty(state);
}

void game_rune_buy_spark_ward(GameState *state) {
    if (state->rune_spell_level > 0) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Spark Ward is active");
        mark_rune_dirty(state);
        return;
    }
    if (state->rune_silver < RUNE_SPARK_WARD_COST_SILVER || state->rune_sparks < RUNE_SPARK_WARD_COST_SPARKS) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Need 12 silver and 1 spark");
        mark_rune_dirty(state);
        return;
    }
    state->rune_silver -= RUNE_SPARK_WARD_COST_SILVER;
    state->rune_sparks -= RUNE_SPARK_WARD_COST_SPARKS;
    state->rune_spell_level = 1;
    state->rune_mana = RUNE_MANA_WARD1_MAX;
    state->rune_tower_unlocked = true;
    state->rune_main_quest_step = 4;
    state->rune_location = GAME_STATE_RUNE_LOCATION_OLD_BELL_TOWER;
    set_text(state->rune_objective, sizeof(state->rune_objective), "Inspect Old Bell Tower");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Spark Ward unlocks tower");
    set_reward(state, "WARD I  DMG 7  MP 12");
    mark_rune_dirty(state);
}

bool game_rune_can_study_rune_lore(const GameState *state) {
    return state->rune_greenfen_safety > 0 && state->rune_rune_lore > 0 && state->rune_spell_level == 1 && !rune_in_combat(state);
}

void game_rune_study_rune_lore(GameState *state) {
    if (!game_rune_can_study_rune_lore(state)) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), state->rune_spell_level >= 2 ? "Spark Ward II is active" : "Need Greenfen rune lore");
        mark_rune_dirty(state);
        return;
    }
    state->rune_rune_lore -= 1;
    state->rune_spell_level = 2;
    state->rune_mana = RUNE_MANA_WARD2_MAX;
    if (state->rune_main_quest_step < 13) {
        state->rune_main_quest_step = 13;
    }
    set_text(state->rune_objective, sizeof(state->rune_objective), "Spark Ward II ready");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Rune lore studied: Spark Ward II");
    set_reward(state, "WARD II  DMG 9  MP 14");
    mark_rune_dirty(state);
}

void game_rune_choose_bell_rope_silver(GameState *state) {
    if (state->rune_side_quest_step != 2 || !state->rune_bell_rope_charm) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "No bell rope charm");
        mark_rune_dirty(state);
        return;
    }
    state->rune_bell_rope_charm = false;
    state->rune_side_quest_step = 3;
    state->rune_silver += RUNE_BELL_ROPE_SILVER_REWARD;
    set_text(state->rune_objective, sizeof(state->rune_objective), "Buy Spark Ward");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Rope traded: +6 silver");
    set_reward(state, "+6 SILVER");
    mark_rune_dirty(state);
}

void game_rune_choose_bell_rope_kindness(GameState *state) {
    if (state->rune_side_quest_step != 2 || !state->rune_bell_rope_charm) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "No bell rope charm");
        mark_rune_dirty(state);
        return;
    }
    state->rune_bell_rope_charm = false;
    state->rune_side_quest_step = 3;
    state->rune_kindness_reputation += 1;
    if (state->rune_kindness_reputation > 99) {
        state->rune_kindness_reputation = 99;
    }
    set_text(state->rune_objective, sizeof(state->rune_objective), "Buy Spark Ward");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Rope returned: +kindness");
    set_reward(state, "+1 KINDNESS");
    mark_rune_dirty(state);
}

void game_rune_inspect_tower(GameState *state) {
    if (!state->rune_tower_unlocked) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Tower still locked");
        mark_rune_dirty(state);
        return;
    }
    state->rune_location = GAME_STATE_RUNE_LOCATION_OLD_BELL_TOWER;
    state->rune_tower_inspected = true;
    state->rune_east_road_unlocked = true;
    state->rune_tower_echoes += 1;
    if (state->rune_tower_echoes > 9) {
        state->rune_tower_echoes = 9;
    }
    if (state->rune_main_quest_step < 6) {
        state->rune_main_quest_step = 6;
    }
    state->rune_xp += 2;
    const bool leveled = rune_check_level_up(state);
    set_text(state->rune_objective, sizeof(state->rune_objective), "Scout Reedmere");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), leveled ? "Bell echoes east; Rank II" : "Bell echoes east");
    set_reward(state, leveled ? "+2 XP  REEDMERE OPEN  RANK II" : "+2 XP  REEDMERE OPEN");
    mark_rune_dirty(state);
}

bool game_rune_can_light_moss_shrine(const GameState *state) {
    return state->rune_kindness_reputation > 0 && state->rune_east_road_safety > 0 && !state->rune_moss_shrine_lit && !rune_in_combat(state);
}

void game_rune_light_moss_shrine(GameState *state) {
    if (!game_rune_can_light_moss_shrine(state)) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), state->rune_moss_shrine_lit ? "Moss shrine already lit" : "Need kindness at Reedmere");
        mark_rune_dirty(state);
        return;
    }
    state->rune_location = GAME_STATE_RUNE_LOCATION_REEDMERE_CROSSING;
    state->rune_moss_shrine_lit = true;
    state->rune_spirit_favor += 1;
    if (state->rune_spirit_favor > 99) {
        state->rune_spirit_favor = 99;
    }
    state->rune_ward_rank += 1;
    if (state->rune_ward_rank > 99) {
        state->rune_ward_rank = 99;
    }
    state->rune_mana = rune_mana_max(state);
    set_text(state->rune_objective, sizeof(state->rune_objective), "Open Greenfen Causeway");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Moss shrine lit: +favor +ward");
    set_reward(state, "+1 FAVOR  +WARD  MP FULL");
    mark_rune_dirty(state);
}

bool game_rune_can_open_causeway(const GameState *state) {
    return state->rune_spirit_favor > 0 && state->rune_moss_shrine_lit && !state->rune_causeway_unlocked && !rune_in_combat(state);
}

bool game_rune_can_choose_next_route(const GameState *state) {
    return state->rune_spell_level >= 2 && state->rune_route_choice == GAME_STATE_RUNE_ROUTE_CHOICE_NONE && !rune_in_combat(state);
}

void game_rune_open_causeway(GameState *state) {
    if (rune_in_combat(state)) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Enemy blocks the road");
        mark_rune_dirty(state);
        return;
    }
    if (state->rune_causeway_unlocked) {
        state->rune_location = GAME_STATE_RUNE_LOCATION_GREENFEN_CAUSEWAY;
        set_text(state->rune_objective, sizeof(state->rune_objective), "Scout Greenfen");
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Causeway open");
        mark_rune_dirty(state);
        return;
    }
    if (!game_rune_can_open_causeway(state)) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Need favor from shrine");
        mark_rune_dirty(state);
        return;
    }
    state->rune_location = GAME_STATE_RUNE_LOCATION_GREENFEN_CAUSEWAY;
    state->rune_causeway_unlocked = true;
    state->rune_causeway_safety = 1;
    if (state->rune_main_quest_step < 10) {
        state->rune_main_quest_step = 10;
    }
    set_text(state->rune_objective, sizeof(state->rune_objective), "Scout Greenfen");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Greenfen Causeway opened");
    set_reward(state, "GREENFEN ROUTE OPEN");
    mark_rune_dirty(state);
}

void game_rune_choose_briar_gate(GameState *state) {
    if (!game_rune_can_choose_next_route(state)) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), state->rune_spell_level < 2 ? "Study rune lore first" : "Route already chosen");
        mark_rune_dirty(state);
        return;
    }
    state->rune_route_choice = GAME_STATE_RUNE_ROUTE_CHOICE_BRIAR_GATE;
    state->rune_briar_gate_unlocked = true;
    state->rune_location = GAME_STATE_RUNE_LOCATION_BRIAR_GATE;
    if (state->rune_main_quest_step < 14) {
        state->rune_main_quest_step = 14;
    }
    set_text(state->rune_objective, sizeof(state->rune_objective), "Scout Briar Gate");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Briar Gate route marked");
    set_reward(state, "BRIAR GATE OPEN");
    mark_rune_dirty(state);
}

void game_rune_choose_moonwell(GameState *state) {
    if (!game_rune_can_choose_next_route(state)) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), state->rune_spell_level < 2 ? "Study rune lore first" : "Route already chosen");
        mark_rune_dirty(state);
        return;
    }
    state->rune_route_choice = GAME_STATE_RUNE_ROUTE_CHOICE_MOONWELL;
    state->rune_moonwell_unlocked = true;
    state->rune_moonwell_blessing += 1;
    if (state->rune_moonwell_blessing > 9) {
        state->rune_moonwell_blessing = 9;
    }
    state->rune_location = GAME_STATE_RUNE_LOCATION_MOONWELL;
    if (state->rune_main_quest_step < 14) {
        state->rune_main_quest_step = 14;
    }
    set_text(state->rune_objective, sizeof(state->rune_objective), "Face Moonwell trial");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Moonwell side path marked");
    set_reward(state, "+1 MOONWELL BLESSING");
    mark_rune_dirty(state);
}

bool game_rune_can_scout_briar_gate(const GameState *state) {
    return state->rune_route_choice == GAME_STATE_RUNE_ROUTE_CHOICE_BRIAR_GATE && state->rune_briar_gate_unlocked && state->rune_briar_gate_safety == 0 && !rune_in_combat(state);
}

void game_rune_scout_briar_gate(GameState *state) {
    state->test_ui_clicks += 1;
    state->tutorial_done = true;
    if (!game_rune_can_scout_briar_gate(state)) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), state->rune_briar_gate_safety > 0 ? "Briar Gate already safe" : "Choose Briar Gate first");
        mark_rune_dirty(state);
        return;
    }
    state->rune_location = GAME_STATE_RUNE_LOCATION_BRIAR_GATE;
    state->rune_encounter = GAME_STATE_RUNE_ENCOUNTER_BRIAR_STALKER;
    state->rune_enemy_hp = RUNE_BRIAR_STALKER_HP;
    if (state->rune_main_quest_step < 15) {
        state->rune_main_quest_step = 15;
    }
    set_text(state->rune_objective, sizeof(state->rune_objective), "Clear Briar Stalker");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Briar stalker prowls");
    mark_rune_dirty(state);
}

bool game_rune_can_scout_moonwell(const GameState *state) {
    return state->rune_route_choice == GAME_STATE_RUNE_ROUTE_CHOICE_MOONWELL && state->rune_moonwell_unlocked && state->rune_moonwell_safety == 0 && !rune_in_combat(state);
}

void game_rune_scout_moonwell(GameState *state) {
    state->test_ui_clicks += 1;
    state->tutorial_done = true;
    if (!game_rune_can_scout_moonwell(state)) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), state->rune_moonwell_safety > 0 ? "Moonwell already calm" : "Choose Moonwell first");
        mark_rune_dirty(state);
        return;
    }
    state->rune_location = GAME_STATE_RUNE_LOCATION_MOONWELL;
    state->rune_encounter = GAME_STATE_RUNE_ENCOUNTER_MOONWELL_SENTINEL;
    state->rune_enemy_hp = RUNE_MOONWELL_SENTINEL_HP;
    if (state->rune_main_quest_step < 15) {
        state->rune_main_quest_step = 15;
    }
    set_text(state->rune_objective, sizeof(state->rune_objective), "Calm Moonwell Sentinel");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Moonwell sentinel wakes");
    mark_rune_dirty(state);
}

bool game_rune_can_discover_ashen_cairn(const GameState *state) {
    return state->rune_route_choice == GAME_STATE_RUNE_ROUTE_CHOICE_BRIAR_GATE && state->rune_briar_gate_safety > 0 && !state->rune_ashen_cairn_unlocked && !rune_in_combat(state);
}

void game_rune_discover_ashen_cairn(GameState *state) {
    state->test_ui_clicks += 1;
    state->tutorial_done = true;
    if (!game_rune_can_discover_ashen_cairn(state)) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), state->rune_ashen_cairn_unlocked ? "Ashen Cairn already mapped" : "Clear Briar Gate first");
        mark_rune_dirty(state);
        return;
    }
    state->rune_location = GAME_STATE_RUNE_LOCATION_ASHEN_CAIRN;
    state->rune_ashen_cairn_unlocked = true;
    state->rune_xp += RUNE_ASHEN_CAIRN_XP;
    (void)rune_check_level_up(state);
    if (state->rune_main_quest_step < 18) {
        state->rune_main_quest_step = 18;
    }
    set_text(state->rune_objective, sizeof(state->rune_objective), "Ashen Cairn found");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Ashen Cairn mapped");
    set_reward(state, "+4 XP  ASHEN CAIRN MAP");
    mark_rune_dirty(state);
}

bool game_rune_can_discover_starfall_grotto(const GameState *state) {
    return state->rune_route_choice == GAME_STATE_RUNE_ROUTE_CHOICE_MOONWELL && state->rune_moonwell_safety > 0 && !state->rune_starfall_grotto_unlocked && !rune_in_combat(state);
}

void game_rune_discover_starfall_grotto(GameState *state) {
    state->test_ui_clicks += 1;
    state->tutorial_done = true;
    if (!game_rune_can_discover_starfall_grotto(state)) {
        set_text(state->rune_combat_log, sizeof(state->rune_combat_log), state->rune_starfall_grotto_unlocked ? "Starfall Grotto already mapped" : "Calm Moonwell first");
        mark_rune_dirty(state);
        return;
    }
    state->rune_location = GAME_STATE_RUNE_LOCATION_STARFALL_GROTTO;
    state->rune_starfall_grotto_unlocked = true;
    state->rune_xp += RUNE_STARFALL_GROTTO_XP;
    state->rune_rune_lore += 1;
    if (state->rune_rune_lore > 99) {
        state->rune_rune_lore = 99;
    }
    (void)rune_check_level_up(state);
    if (state->rune_main_quest_step < 18) {
        state->rune_main_quest_step = 18;
    }
    set_text(state->rune_objective, sizeof(state->rune_objective), "Starfall Grotto found");
    set_text(state->rune_combat_log, sizeof(state->rune_combat_log), "Starfall Grotto mapped");
    set_reward(state, "+4 XP  +1 LORE  STARFALL MAP");
    mark_rune_dirty(state);
}

int game_rune_spark_damage(const GameState *state) {
    if (state->rune_spell_level >= 2) {
        return RUNE_SPARK_BASE_DAMAGE + RUNE_SPARK_WARD2_BONUS;
    }
    return RUNE_SPARK_BASE_DAMAGE + (state->rune_spell_level > 0 ? RUNE_SPARK_UPGRADE_BONUS : 0);
}
