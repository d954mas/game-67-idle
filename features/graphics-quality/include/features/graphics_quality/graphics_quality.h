#ifndef FEATURES_GRAPHICS_QUALITY_H
#define FEATURES_GRAPHICS_QUALITY_H
// feature-layer: L0

#include <stdbool.h>
#include <stddef.h>

// The player's Graphics setting. High draws the world at the window's
// resolution; Low draws it into a smaller target that the game stretches over
// the window, with the UI still drawn at the window's own resolution.
typedef enum graphics_quality_t {
    GRAPHICS_QUALITY_HIGH = 0,
    GRAPHICS_QUALITY_LOW,
    GRAPHICS_QUALITY_COUNT,
} graphics_quality_t;

// What the page says about the device: a phone (or tablet) and the GPU's name,
// unmasked where the browser allows it. Native builds report a computer with an
// empty name, which guesses High.
typedef struct graphics_device_t {
    bool mobile;
    char gpu[160];
} graphics_device_t;

// Web: reads the canvas's WebGL renderer and the user agent. Call it after the
// graphics context exists, or the GPU name comes back empty.
void graphics_device_query(graphics_device_t *out);

// The setting a device starts on until the player picks one. Phones get Low
// unless their GPU is a recent flagship; computers get High unless their GPU is
// a software renderer or an old integrated part.
graphics_quality_t graphics_quality_guess(bool mobile, const char *gpu);

// The world target's size against the window at this setting, 0..1 on each
// axis. A phone at the engine's DPR cap of 2 hides a half; a computer at DPR 1
// shows a half as blur, so it keeps two thirds.
float graphics_world_scale(graphics_quality_t quality, bool mobile);

// Whether shadows start on at this setting: a device that needs Low cannot
// afford the shadow pass and its lookup either.
bool graphics_shadows_default(graphics_quality_t quality);

// Stable names for saves and command lines ("high", "low").
const char *graphics_quality_name(graphics_quality_t quality);
bool graphics_quality_parse(const char *name, graphics_quality_t *out);

#endif /* FEATURES_GRAPHICS_QUALITY_H */
