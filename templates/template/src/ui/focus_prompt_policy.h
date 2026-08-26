#ifndef GAME_UI_FOCUS_PROMPT_POLICY_H
#define GAME_UI_FOCUS_PROMPT_POLICY_H

#include <stdbool.h>

typedef struct FocusPromptGate {
    bool visible;
    bool awaiting_pointer_release;
} FocusPromptGate;

static inline bool focus_prompt_should_show(bool web, bool focused,
                                            bool fine_pointer) {
    return web && fine_pointer && !focused;
}

static inline FocusPromptGate focus_prompt_gate_next(FocusPromptGate gate,
                                                      bool focus_required,
                                                      bool pointer_down) {
    if (focus_required) {
        gate.visible = true;
        return gate;
    }
    if (gate.visible && pointer_down) {
        gate.awaiting_pointer_release = true;
        return gate;
    }
    gate.visible = false;
    gate.awaiting_pointer_release = false;
    return gate;
}

#endif /* GAME_UI_FOCUS_PROMPT_POLICY_H */
