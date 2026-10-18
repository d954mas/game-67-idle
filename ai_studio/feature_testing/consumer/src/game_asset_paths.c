#include "game_asset_paths.h"

#include <stdio.h>
#include <string.h>

#if !defined(__EMSCRIPTEN__)
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <stdint.h>
#else
#include <unistd.h>
#endif

static bool executable_dir(char *out, size_t cap) {
    if (out == NULL || cap < 2U) return false;
    out[0] = '\0';
#if defined(_WIN32)
    // ANSI on purpose: the engine opens packs through fopen, so the resolved
    // path must live in the same code page the C runtime will parse.
    const DWORD written = GetModuleFileNameA(NULL, out, (DWORD)cap);
    if (written == 0U || (size_t)written >= cap) return false;
#elif defined(__APPLE__)
    uint32_t size = (uint32_t)cap;
    if (_NSGetExecutablePath(out, &size) != 0) return false;
    out[cap - 1U] = '\0';
#else
    const ssize_t written = readlink("/proc/self/exe", out, cap - 1U);
    if (written <= 0 || (size_t)written >= cap - 1U) return false;
    out[written] = '\0';
#endif
    char *separator = NULL;
    for (char *cursor = out; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') separator = cursor;
    }
    if (separator == NULL || separator == out) return false;
    *separator = '\0';
    return true;
}
#endif /* !__EMSCRIPTEN__ */

bool game_asset_paths_resolve(const char *relative, char *out, size_t cap) {
    if (relative == NULL || out == NULL || cap == 0U) return false;
#if defined(__EMSCRIPTEN__)
    const int written = snprintf(out, cap, "%s", relative);
#else
    char directory[GAME_ASSET_PATH_MAX];
    // Losing the executable path is not fatal: the working-directory-relative
    // form still works for a launch from the install folder.
    const int written = executable_dir(directory, sizeof directory)
                            ? snprintf(out, cap, "%s/%s", directory, relative)
                            : snprintf(out, cap, "%s", relative);
#endif
    return written > 0 && (size_t)written < cap;
}
