#ifndef FEATURES_LOCALIZATION_LOC_FONT_PACKS_H
#define FEATURES_LOCALIZATION_LOC_FONT_PACKS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "font/nt_font.h"
#include "resource/nt_resource.h"

/* A language whose letters are too many for the first download keeps them in a
   pack of its own, fetched when that language is chosen. The rule is one: while
   a language's font is not here the fallback language shows, and when it lands
   the screen switches. The choice itself is kept from the moment it is made, so
   it can be saved at once. See README.md, "Languages in their own font pack". */

#define LOC_FONT_PACKS_MAX 8

typedef struct {
    const char *code;          /* a corpus language code, as the corpus spells it */
    const char *pack;          /* its pack's name, hashed to the pack id */
    const char *font_resource; /* the font resource that pack carries */
} loc_font_pack_t;

/* Where the pack's file is; false fails the fetch for now and it is tried again. */
typedef bool (*loc_font_pack_path_fn)(const char *pack, char *out, size_t cap, void *user);

typedef struct {
    const loc_font_pack_t *packs; /* borrowed for the feature's lifetime, with its strings */
    int count;                    /* at most LOC_FONT_PACKS_MAX */
    const char *fallback;         /* the language shown while a pack is away */
    nt_font_t common_font;        /* every other language draws with this */
    nt_resource_t common_face;    /* the second source of a pack language's font */
    nt_font_create_desc_t font;   /* the size of a pack language's font */
    int16_t priority;             /* the packs' mount priority */
    loc_font_pack_path_fn path;
    void *user;
} loc_font_packs_desc_t;

/* After loc_init and nt_font_init, before the first set_language. */
void loc_font_packs_init(const loc_font_packs_desc_t *desc);
void loc_font_packs_shutdown(void);

/* Takes a corpus language; false for a code the corpus does not have. */
bool loc_font_packs_set_language(const char *code);
/* The language chosen, whether or not the screen shows it yet. */
const char *loc_font_packs_language(void);
/* Once a frame, before text is laid out: fetches, retries, and switches. */
void loc_font_packs_step(double now);
/* The font to draw with this frame; changes when a language switches. */
nt_font_t loc_font_packs_font(void);

#endif /* FEATURES_LOCALIZATION_LOC_FONT_PACKS_H */
