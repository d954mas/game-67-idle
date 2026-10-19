#include "features/graphics_quality/graphics_quality.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#define GPU_NAME_MAX 256U

#define WORLD_SCALE_LOW_MOBILE 0.5F
#define WORLD_SCALE_LOW_DESKTOP (2.0F / 3.0F)

static const char *const k_names[GRAPHICS_QUALITY_COUNT] = {"high", "low"};

#ifdef __EMSCRIPTEN__
/* clang-format off */
EM_JS(int, graphics_quality_page_device, (char *gpu, int gpu_size), {
    var name = "";
    try {
        var canvas = Module["canvas"] || document.getElementById("canvas");
        var gl = canvas ? (canvas.getContext("webgl2") || canvas.getContext("webgl")) : null;
        if (gl) {
            var info = gl.getExtension("WEBGL_debug_renderer_info");
            name = String(gl.getParameter(info ? info["UNMASKED_RENDERER_WEBGL"] : gl.RENDERER) || "");
        }
    } catch (e) {
        name = "";
    }
    stringToUTF8(name, gpu, gpu_size);
    var agent = navigator["userAgentData"];
    if (agent && typeof agent["mobile"] === "boolean" && agent["mobile"]) { return 1; }
    var ua = String(navigator.userAgent || "");
    // iPadOS reports a desktop Safari agent; its touch points give it away.
    return /Android|iPhone|iPad|iPod|Mobi/i.test(ua) || (/Macintosh/.test(ua) && navigator.maxTouchPoints > 1) ? 1 : 0;
})
/* clang-format on */
#endif

void graphics_device_query(graphics_device_t *out) {
    out->gpu[0] = '\0';
#ifdef __EMSCRIPTEN__
    out->mobile = graphics_quality_page_device(out->gpu, (int)sizeof out->gpu) != 0;
#else
    out->mobile = false;
#endif
}

// The number right after `prefix` in `lower`, or -1 when the prefix is absent or no digit follows.
static long number_after(const char *lower, const char *prefix) {
    const char *at = strstr(lower, prefix);
    if (at == NULL) { return -1L; }
    at += strlen(prefix);
    return isdigit((unsigned char)*at) ? strtol(at, NULL, 10) : -1L;
}

// A phone-class GPU strong enough to draw the world at full resolution.
static bool flagship(const char *lower) {
    if (strstr(lower, "apple") != NULL || strstr(lower, "immortalis") != NULL || strstr(lower, "xclipse") != NULL) {
        return true;
    }
    // Mali-G76, -G78 and -G610 read below 710 and stay Low; the G710 line and later are 2022
    // flagships, and Arm's 2025 parts are named Mali-G1-Ultra, -Premium and -Pro.
    if (number_after(lower, "mali-g") >= 710L || strstr(lower, "mali-g1-") != NULL) { return true; }
    return number_after(lower, "adreno (tm) ") >= 730L || number_after(lower, "adreno ") >= 730L;
}

graphics_quality_t graphics_quality_guess(bool mobile, const char *gpu) {
    char lower[GPU_NAME_MAX];
    size_t length = 0U;
    for (; gpu != NULL && gpu[length] != '\0' && length + 1U < sizeof lower; ++length) {
        lower[length] = (char)tolower((unsigned char)gpu[length]);
    }
    lower[length] = '\0';
    if (strstr(lower, "swiftshader") != NULL || strstr(lower, "llvmpipe") != NULL || strstr(lower, "softpipe") != NULL
        || strstr(lower, "software") != NULL || strstr(lower, "basic render") != NULL) {
        return GRAPHICS_QUALITY_LOW;
    }
    // A phone GPU in a laptop (a Chromebook) is judged as a phone's.
    const bool phone_gpu = strstr(lower, "mali") != NULL || strstr(lower, "adreno") != NULL || strstr(lower, "powervr") != NULL;
    if (mobile || phone_gpu) { return flagship(lower) ? GRAPHICS_QUALITY_HIGH : GRAPHICS_QUALITY_LOW; }
    // Intel's HD Graphics predate its UHD and Iris parts; at a laptop's resolution they are pixel-bound.
    const bool old_intel = strstr(lower, "intel") != NULL && strstr(lower, "hd graphics") != NULL && strstr(lower, "uhd") == NULL;
    return old_intel ? GRAPHICS_QUALITY_LOW : GRAPHICS_QUALITY_HIGH;
}

float graphics_world_scale(graphics_quality_t quality, bool mobile) {
    if (quality != GRAPHICS_QUALITY_LOW) { return 1.0F; }
    return mobile ? WORLD_SCALE_LOW_MOBILE : WORLD_SCALE_LOW_DESKTOP;
}

bool graphics_shadows_default(graphics_quality_t quality) { return quality != GRAPHICS_QUALITY_LOW; }

const char *graphics_quality_name(graphics_quality_t quality) {
    return quality < GRAPHICS_QUALITY_COUNT ? k_names[quality] : k_names[GRAPHICS_QUALITY_HIGH];
}

bool graphics_quality_parse(const char *name, graphics_quality_t *out) {
    for (int i = 0; name != NULL && i < GRAPHICS_QUALITY_COUNT; ++i) {
        if (strcmp(name, k_names[i]) == 0) {
            *out = (graphics_quality_t)i;
            return true;
        }
    }
    return false;
}
