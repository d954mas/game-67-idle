#ifndef GAME_UI_THEME_H
#define GAME_UI_THEME_H

#include "resource/nt_resource.h"

#include "features/ui_kit/ui_theme.h"

// The game's half of the theme: the ui-kit feature owns every style, and this
// binds it to THIS game's atlas. The split is where the generated asset ids
// live — `build_packs.c` and `generated/game_assets.h` are game-owned, and the
// feature must not include either.
//
// A game that wants its own face passes its own token sheet here instead of the
// studio default, and regenerates its art from the same sheet
// (features/ui-kit/INSTALL.md).
void theme_init(nt_resource_t atlas);

#endif /* GAME_UI_THEME_H */
