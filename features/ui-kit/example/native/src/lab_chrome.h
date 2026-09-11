#ifndef UI_LAB_CHROME_H
#define UI_LAB_CHROME_H

#include "ui/nt_ui.h"

#include <stdint.h>

// Lab-only furniture: what the catalogue and the theme picker need and no
// game screen does. Everything a scene shows to a player comes from the kit.

// A flat colour square with the theme's contour, for a palette preview.
void lab_swatch(nt_ui_context_t *ctx, uint32_t abgr, float size);

// A catalogue heading with the kit's rhythm above it.
void lab_section(nt_ui_context_t *ctx, const char *title);

// The ground under a catalogue: the recessed colour, full bleed, under the
// main tree (zIndex -1).
void lab_ground(nt_ui_context_t *ctx, const char *id);

#endif /* UI_LAB_CHROME_H */
