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

// Whether the page runs on a phone or a tablet, from the user agent. Native
// builds are a computer.
bool graphics_device_mobile(void);

// The GPU's name as the engine's graphics context reports it (unmasked where the
// browser allows it); empty natively, which guesses High. Call it after the
// graphics context exists, or the name comes back empty.
const char *graphics_device_gpu(void);

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
