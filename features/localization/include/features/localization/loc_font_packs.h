#ifndef FEATURES_LOCALIZATION_LOC_FONT_PACKS_H
#define FEATURES_LOCALIZATION_LOC_FONT_PACKS_H

#include <stdbool.h>

#include "font/nt_font.h"

/* Languages with too many letters for the first download share one face in a
   pack the game streams to every player, so a switch never waits once it is in.
   The rule is one: while that face is not here the fallback language shows, and
   when it lands the screen switches. The choice itself is kept from the moment
   it is made, so it can be saved at once. See README.md, "Languages in a second
   font pack". */

typedef struct {
    const char *const *languages; /* corpus codes drawn from the face; borrowed for the feature's lifetime */
    int count;
    const char *font_resource;    /* the font resource the second pack carries */
    const char *fallback;         /* the language shown while the face is away */
    nt_font_t common_font;        /* every other language draws with this */
    nt_resource_t common_face;    /* the second source of the pack languages' font */
    nt_font_create_desc_t font;   /* the size of the pack languages' font */
} loc_font_packs_desc_t;

/* After loc_init and nt_font_init, before the first set_language. */
void loc_font_packs_init(const loc_font_packs_desc_t *desc);
void loc_font_packs_shutdown(void);

/* Takes a corpus language; false for a code the corpus does not have. */
bool loc_font_packs_set_language(const char *code);
/* The language chosen, whether or not the screen shows it yet. */
const char *loc_font_packs_language(void);
/* Once a frame, before text is laid out: switches when the face lands. */
void loc_font_packs_step(void);
/* The font to draw with this frame; changes when a language switches. */
nt_font_t loc_font_packs_font(void);

#endif /* FEATURES_LOCALIZATION_LOC_FONT_PACKS_H */
