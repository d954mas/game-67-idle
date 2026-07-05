#include "game_dialogue.h"

#include "game_audio.h"
#include "game_content.h"
#if FEATURE_GAME_STATE
#include "game_actions.h"
#endif
#include "world/world.h"

#include <stddef.h>
#include <string.h>

static const dialogue_node_t *find_node(const dialogue_definition_t *def,
                                        const char *node_id) {
  if (!def || !node_id) {
    return NULL;
  }
  for (int i = 0; i < def->node_count; ++i) {
    if (strcmp(def->nodes[i].id, node_id) == 0) {
      return &def->nodes[i];
    }
  }
  return NULL;
}

static const dialogue_choice_t *find_choice(const dialogue_node_t *node,
                                            const char *choice_id) {
  if (!node || !choice_id) {
    return NULL;
  }
  for (int i = 0; i < node->choice_count; ++i) {
    if (strcmp(node->choices[i].id, choice_id) == 0) {
      return &node->choices[i];
    }
  }
  return NULL;
}

void game_dialogue_init(World *w) {
  if (!w) {
    return;
  }
  w->dialogue.definition = NULL;
  w->dialogue.current_node = NULL;
  w->dialogue.open = false;
}

bool game_dialogue_open(World *w, const char *dialogue_id) {
  if (!w) {
    return false;
  }
  const dialogue_definition_t *def = game_content_find_dialogue(dialogue_id);
  if (!def) {
    return false;
  }
  w->dialogue.definition = def;
  w->dialogue.current_node = find_node(def, def->entry_node_id);
  w->dialogue.open = w->dialogue.current_node != NULL;
  if (w->dialogue.open) {
    game_audio_play(GAME_AUDIO_CUE_DIALOGUE_OPEN);
  }
  return w->dialogue.open;
}

bool game_dialogue_select_choice(World *w, const char *choice_id) {
  if (!w || !w->dialogue.open || !w->dialogue.current_node) {
    return false;
  }
  const dialogue_choice_t *choice =
      find_choice(w->dialogue.current_node, choice_id);
  if (!choice) {
    return false;
  }
  if (choice->kind == DIALOGUE_CHOICE_BRANCH) {
    if (!choice->next_node_id || choice->next_node_id[0] == '\0') {
      game_dialogue_close(w);
      game_audio_play(GAME_AUDIO_CUE_DIALOGUE_CHOICE);
      return true;
    }
    const dialogue_node_t *next =
        find_node(w->dialogue.definition, choice->next_node_id);
    if (!next) {
      return false;
    }
    w->dialogue.current_node = next;
    game_audio_play(GAME_AUDIO_CUE_DIALOGUE_CHOICE);
    return true;
  }
  if (choice->kind == DIALOGUE_CHOICE_PROGRESS) {
    if (!choice->quest_id || !choice->step_id) {
      return false;
    }
#if FEATURE_GAME_STATE
    if (!game_actions_apply_dialogue_choice(
            w->player_state, w->dialogue.definition->id, choice->id,
            choice->reward_id, choice->effects, choice->effect_count)) {
      return false;
    }
#endif
    bool has_grant_effect = false;
    for (int i = 0; i < choice->effect_count; ++i) {
      if (choice->effects[i].kind == DIALOGUE_EFFECT_GRANT_ITEM ||
          choice->effects[i].kind == DIALOGUE_EFFECT_GRANT_XP ||
          choice->effects[i].kind == DIALOGUE_EFFECT_GRANT_GOLD) {
        has_grant_effect = true;
        break;
      }
    }
    game_audio_play(has_grant_effect ? GAME_AUDIO_CUE_REWARD
                                     : GAME_AUDIO_CUE_DIALOGUE_CHOICE);
    const char *next_step_id =
        game_content_next_quest_step(choice->quest_id, choice->step_id);
    if (strcmp(choice->quest_id, "q001_gate_pass") == 0 &&
        strcmp(choice->step_id, "talk_gate_guard") == 0) {
      w->first_scene.active_quest_id = choice->quest_id;
      w->first_scene.active_quest_status = 1;
      w->first_scene.active_quest_current_step_id =
          next_step_id ? next_step_id : choice->step_id;
      w->first_scene.active_quest_completed_step_id = "talk_gate_guard";
      w->first_scene.active_quest_completed_talk_step = true;
      w->first_scene.active_quest_gate_guard_intro_seen = true;
      w->first_scene.objective_object_id = NULL;
      w->first_scene.tutorial_guard_talk_completed = true;
      w->first_scene.blacksmith_unlocked = false;
      w->first_scene.gate_locked = true;
      w->first_scene.contract_board_locked = true;
      w->first_scene.current_objective_text = "Надеть выданное снаряжение";
    }
    if (strcmp(choice->quest_id, "q001_gate_pass") == 0 &&
        strcmp(choice->step_id, "report_to_gate_guard") == 0) {
      // The guard starts the bread route; move the scene callout away from him.
      w->first_scene.objective_object_id = "hub_last_post.elder";
      w->first_scene.tutorial_guard_talk_completed = false;
      w->first_scene.current_objective_text =
          "Поговори со старостой";
    }
    game_dialogue_close(w);
    return true;
  }
  return false;
}

void game_dialogue_close(World *w) {
  if (!w) {
    return;
  }
  w->dialogue.definition = NULL;
  w->dialogue.current_node = NULL;
  w->dialogue.open = false;
}

const dialogue_runtime_t *game_dialogue_runtime(const World *w) {
  return w ? &w->dialogue : NULL;
}

const char *game_dialogue_current_objective(const World *w) {
  return w ? w->first_scene.current_objective_text : NULL;
}
