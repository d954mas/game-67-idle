#ifndef GAME_SAVE_POLICY_H
#define GAME_SAVE_POLICY_H

#include <stdbool.h>

#include "game_save_sync.h"

/* Hero level and tutorial completion decide durable progression. Equal milestones
   may use positive persisted playtime; unknown or incomparable documents ask. */
game_save_choice_t game_save_policy_decide(
    const char *local_document, const char *remote_document);

/* Ignores envelope metadata regenerated during export. False also means that a
   document cannot safely participate in an automatic decision. */
bool game_save_policy_same_features(
    const char *first_document, const char *second_document);

void game_configure_save(void);

#endif /* GAME_SAVE_POLICY_H */
