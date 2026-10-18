#ifndef LOGIN_PROMPT_H
#define LOGIN_PROMPT_H

#include "features/ui_kit/ui_metrics.h"
#include "ui/nt_ui.h"

#include <stdbool.h>

/* The portal login offer, one shape everywhere it appears: the benefits the
 * player gets by logging in, then the button. The button opens the game's own
 * confirm modal, not the portal's dialog: a portal wants the benefits and a
 * way out shown before it takes over (Yandex 1.2.1), and its dialog, once
 * open, is a page the player can only close. */

/* Whether an offer belongs on screen: the portal has a login dialog and the
 * player is still anonymous. A portal without one never shows the row. */
bool login_prompt_wanted(void);

/* The offer inside a settings-style column: the hint, then a full-width
 * button. The ids are string literals unique per screen, so a bot finds both
 * by name; `source` is the screen the tap is reported from. */
void login_prompt_row(nt_ui_context_t *ctx, const ui_metrics_t *m, const char *row_id, const char *button_id,
                      const char *source, bool interactive);

/* The confirm modal's panel, drawn by its own scene above whichever screen
 * held the button. "Log in" opens the portal dialog; "Not now" only closes. */
void login_prompt_draw_confirm(nt_ui_context_t *ctx, bool interactive);

#endif /* LOGIN_PROMPT_H */
