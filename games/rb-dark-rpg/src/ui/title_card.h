#ifndef RB_DARK_RPG_TITLE_CARD_H
#define RB_DARK_RPG_TITLE_CARD_H

#include "ui/nt_ui.h"
#include "world/world.h"

/* Non-interactive title card shown once per session on the first screen: fades
 * in, holds, fades out, then never draws again (until the process restarts). */
void title_card_ui(nt_ui_context_t *ctx, World *w);

#endif /* RB_DARK_RPG_TITLE_CARD_H */
