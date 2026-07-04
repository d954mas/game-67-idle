#ifndef RB_DARK_RPG_GAME_DIALOGUE_H
#define RB_DARK_RPG_GAME_DIALOGUE_H

#include <stdbool.h>

typedef struct World World;

typedef enum dialogue_choice_kind_t {
    DIALOGUE_CHOICE_BRANCH = 0,
    DIALOGUE_CHOICE_PROGRESS = 1,
} dialogue_choice_kind_t;

typedef struct dialogue_choice_t {
    const char *id;
    const char *text;
    const char *next_node_id;
    dialogue_choice_kind_t kind;
    const char *quest_id;
    const char *step_id;
} dialogue_choice_t;

typedef enum dialogue_reward_kind_t {
    DIALOGUE_REWARD_ITEM = 0,
    DIALOGUE_REWARD_XP = 1,
    DIALOGUE_REWARD_UNLOCK = 2,
} dialogue_reward_kind_t;

typedef struct dialogue_reward_t {
    const char *id;
    const char *name;
    const char *icon_label;
    const char *summary;
    const char *detail;
    dialogue_reward_kind_t kind;
    int amount;
} dialogue_reward_t;

typedef struct dialogue_quest_preview_t {
    const char *goal;
    const dialogue_reward_t *immediate_rewards;
    int immediate_reward_count;
    const dialogue_reward_t *completion_rewards;
    int completion_reward_count;
} dialogue_quest_preview_t;

typedef struct dialogue_node_t {
    const char *id;
    const char *speaker_id;
    const char *speaker_name;
    const char *quest_name;
    const char *text;
    const dialogue_choice_t *choices;
    int choice_count;
} dialogue_node_t;

typedef struct dialogue_definition_t {
    const char *id;
    const char *title;
    const char *entry_node_id;
    const dialogue_node_t *nodes;
    int node_count;
    const dialogue_quest_preview_t *quest_preview;
} dialogue_definition_t;

typedef struct dialogue_runtime_t {
    const dialogue_definition_t *definition;
    const dialogue_node_t *current_node;
    bool open;
} dialogue_runtime_t;

void game_dialogue_init(World *w);
bool game_dialogue_open(World *w, const char *dialogue_id);
bool game_dialogue_select_choice(World *w, const char *choice_id);
void game_dialogue_close(World *w);
const dialogue_runtime_t *game_dialogue_runtime(const World *w);
const char *game_dialogue_current_objective(const World *w);

#endif
