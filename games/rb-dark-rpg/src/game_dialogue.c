#include "game_dialogue.h"

#include "world/world.h"

#include <stddef.h>
#include <string.h>

static const dialogue_choice_t GATE_GUARD_START_CHOICES[] = {
    {.id = "ask_what_happened", .text = "Что случилось за воротами?", .next_node_id = "outside_lore", .kind = DIALOGUE_CHOICE_BRANCH},
    {.id = "ask_what_needed", .text = "Что за проверка?", .next_node_id = "explain_check", .kind = DIALOGUE_CHOICE_BRANCH},
    {.id = "accept", .text = "Беру снаряжение", .kind = DIALOGUE_CHOICE_PROGRESS, .quest_id = "q001_gate_pass", .step_id = "talk_gate_guard"},
};

static const dialogue_choice_t GATE_GUARD_LORE_CHOICES[] = {
    {.id = "ask_what_needed", .text = "Что за проверка?", .next_node_id = "explain_check", .kind = DIALOGUE_CHOICE_BRANCH},
    {.id = "accept", .text = "Беру снаряжение", .kind = DIALOGUE_CHOICE_PROGRESS, .quest_id = "q001_gate_pass", .step_id = "talk_gate_guard"},
};

static const dialogue_choice_t GATE_GUARD_EXPLAIN_CHOICES[] = {
    {.id = "ask_what_happened", .text = "Почему всё так плохо?", .next_node_id = "outside_lore", .kind = DIALOGUE_CHOICE_BRANCH},
    {.id = "accept", .text = "Беру снаряжение", .kind = DIALOGUE_CHOICE_PROGRESS, .quest_id = "q001_gate_pass", .step_id = "talk_gate_guard"},
};

static const dialogue_reward_t GATE_GUARD_IMMEDIATE_REWARDS[] = {
    {.id = "old_sword",
     .name = "Старый меч",
     .icon_label = "МЕЧ",
     .summary = "+4 урон оружия",
     .detail = "Оружие для первой проверки. Дает +4 к урону оружия и открывает первый бой.",
     .kind = DIALOGUE_REWARD_ITEM,
     .amount = 1},
    {.id = "padded_jacket",
     .name = "Стеганая куртка",
     .icon_label = "БР",
     .summary = "+6 живучесть, +15 защита",
     .detail = "Стартовая броня. Увеличивает запас здоровья и снижает входящий урон через защиту.",
     .kind = DIALOGUE_REWARD_ITEM,
     .amount = 1},
    {.id = "leather_greaves",
     .name = "Кожаные поножи",
     .icon_label = "ПН",
     .summary = "+3 живучесть, +8 защита",
     .detail = "Простая защита ног. Нужна, чтобы первый комплект выглядел как полный набор, а не один меч.",
     .kind = DIALOGUE_REWARD_ITEM,
     .amount = 1},
};

static const dialogue_reward_t GATE_GUARD_COMPLETION_REWARDS[] = {
    {.id = "seeker_token",
     .name = "Жетон искателя",
     .icon_label = "ЖЕ",
     .summary = "доступ за ворота",
     .detail = "Главный пропуск. После проверки страж выдаст жетон, и ворота перестанут быть закрытым экраном.",
     .kind = DIALOGUE_REWARD_ITEM,
     .amount = 1},
    {.id = "quest_xp_10",
     .name = "Опыт",
     .icon_label = "XP",
     .summary = "+10 опыта",
     .detail = "Награда за завершение проверки у ворот. Отдельна от добычи за сам бой.",
     .kind = DIALOGUE_REWARD_XP,
     .amount = 10},
    {.id = "map_unlock",
     .name = "Карта и контракты",
     .icon_label = "КР",
     .summary = "новые экраны",
     .detail = "Откроются карта Пепельной Границы, старая мельница и следующий контракт.",
     .kind = DIALOGUE_REWARD_UNLOCK,
     .amount = 1},
};

static const dialogue_quest_preview_t GATE_GUARD_PREVIEW = {
    .goal = "Надень меч, броню и поножи, убери падальщика у ворот и вернись к стражу.",
    .immediate_rewards = GATE_GUARD_IMMEDIATE_REWARDS,
    .immediate_reward_count = (int)(sizeof GATE_GUARD_IMMEDIATE_REWARDS / sizeof GATE_GUARD_IMMEDIATE_REWARDS[0]),
    .completion_rewards = GATE_GUARD_COMPLETION_REWARDS,
    .completion_reward_count = (int)(sizeof GATE_GUARD_COMPLETION_REWARDS / sizeof GATE_GUARD_COMPLETION_REWARDS[0]),
};

static const dialogue_node_t GATE_GUARD_NODES[] = {
    {.id = "start",
     .speaker_id = "gate_guard",
     .speaker_name = "Страж у ворот",
     .quest_name = "Допуск за ворота",
     .text = "За ворота без жетона и железа не выпускаю. Хочешь выйти - пройдешь проверку.",
     .choices = GATE_GUARD_START_CHOICES,
     .choice_count = (int)(sizeof GATE_GUARD_START_CHOICES / sizeof GATE_GUARD_START_CHOICES[0])},
    {.id = "outside_lore",
     .speaker_id = "gate_guard",
     .speaker_name = "Страж у ворот",
     .quest_name = "Допуск за ворота",
     .text = "После ухода Дракона дороги стали чужими. На трактах падальщики и люди с черным знаком. Совет платит тем, кто возвращается с новостями.",
     .choices = GATE_GUARD_LORE_CHOICES,
     .choice_count = (int)(sizeof GATE_GUARD_LORE_CHOICES / sizeof GATE_GUARD_LORE_CHOICES[0])},
    {.id = "explain_check",
     .speaker_id = "gate_guard",
     .speaker_name = "Страж у ворот",
     .quest_name = "Допуск за ворота",
     .text = "Я выдам старый меч, стеганую куртку и поножи прямо сейчас. Надень комплект, убери падальщика у ворот, потом вернешься ко мне за жетоном искателя.",
     .choices = GATE_GUARD_EXPLAIN_CHOICES,
     .choice_count = (int)(sizeof GATE_GUARD_EXPLAIN_CHOICES / sizeof GATE_GUARD_EXPLAIN_CHOICES[0])},
};

static const dialogue_definition_t DIALOGUES[] = {
    {.id = "dlg_gate_guard_intro",
     .title = "Страж: допуск за ворота",
     .entry_node_id = "start",
     .nodes = GATE_GUARD_NODES,
     .node_count = (int)(sizeof GATE_GUARD_NODES / sizeof GATE_GUARD_NODES[0]),
     .quest_preview = &GATE_GUARD_PREVIEW},
};

static const dialogue_definition_t *find_dialogue(const char *dialogue_id) {
    for (size_t i = 0; i < sizeof DIALOGUES / sizeof DIALOGUES[0]; ++i) {
        if (strcmp(DIALOGUES[i].id, dialogue_id) == 0) {
            return &DIALOGUES[i];
        }
    }
    return NULL;
}

static const dialogue_node_t *find_node(const dialogue_definition_t *def, const char *node_id) {
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

static const dialogue_choice_t *find_choice(const dialogue_node_t *node, const char *choice_id) {
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
    const dialogue_definition_t *def = find_dialogue(dialogue_id);
    if (!def) {
        return false;
    }
    w->dialogue.definition = def;
    w->dialogue.current_node = find_node(def, def->entry_node_id);
    w->dialogue.open = w->dialogue.current_node != NULL;
    return w->dialogue.open;
}

bool game_dialogue_select_choice(World *w, const char *choice_id) {
    if (!w || !w->dialogue.open || !w->dialogue.current_node) {
        return false;
    }
    const dialogue_choice_t *choice = find_choice(w->dialogue.current_node, choice_id);
    if (!choice) {
        return false;
    }
    if (choice->kind == DIALOGUE_CHOICE_BRANCH) {
        const dialogue_node_t *next = find_node(w->dialogue.definition, choice->next_node_id);
        if (!next) {
            return false;
        }
        w->dialogue.current_node = next;
        return true;
    }
    if (choice->kind == DIALOGUE_CHOICE_PROGRESS) {
        w->first_scene.active_quest_id = choice->quest_id;
        w->first_scene.active_quest_status = 1;
        w->first_scene.active_quest_current_step_id = "equip_old_sword";
        w->first_scene.active_quest_completed_step_id = "talk_gate_guard";
        w->first_scene.active_quest_completed_talk_step = true;
        w->first_scene.active_quest_gate_guard_intro_seen = true;
        w->first_scene.objective_object_id = SCENE_OBJECT_ID_NONE;
        w->first_scene.tutorial_guard_talk_completed = true;
        w->first_scene.blacksmith_unlocked = false;
        w->first_scene.gate_locked = true;
        w->first_scene.contract_board_locked = true;
        w->first_scene.current_objective_text = "Надеть выданное снаряжение";
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
