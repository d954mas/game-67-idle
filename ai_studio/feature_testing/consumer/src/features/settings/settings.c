#include "features/settings/settings.h"

#include "game_save.h"      /* game_save_mark_dirty */
#include "settings_state.h" /* generated: SettingsState + settings_state instance */

#include "features/localization/loc.h"
#include "features/platform_sdk/platform_sdk.h"
#include "loc_strings.gen.h" /* LOC_FALLBACK_LANG */

#include <stdlib.h>
#include <string.h>

static float clamp01(float v) { return v < 0.0F ? 0.0F : (v > 1.0F ? 1.0F : v); }

float settings_master(void) { return settings_state.master_volume; }
float settings_music(void)  { return settings_state.music_volume; }
float settings_sfx(void)    { return settings_state.sfx_volume; }

void settings_set_master(float value) {
    settings_state.master_volume = clamp01(value);
    game_save_mark_dirty();
    /* apply seam: a real game pushes master/sfx to its audio mixer here. */
}
void settings_set_music(float value) {
    settings_state.music_volume = clamp01(value);
    game_save_mark_dirty();
}
void settings_set_sfx(float value) {
    settings_state.sfx_volume = clamp01(value);
    game_save_mark_dirty();
}

/* Персистентный язык -> активный язык таблицы. Единственное место, где одно
   становится другим; зовётся после загрузки сейва и после каждой смены. */
void settings_apply_language(void) {
    const int chosen = loc_lang_from_code(settings_state_language_name(settings_state.language));
    loc_set_lang_index(chosen >= 0 ? chosen : (int)LOC_FALLBACK_LANG);
}

int settings_language(void) { return (int)settings_state.language; }

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>

/* The browser's language, for a portal that has none of its own (Poki, itch).
   Kept apart from the SDK read on purpose: a portal that watches for that read
   must never see the browser's answer stand in for its own. */
EM_JS(char *, settings_web_browser_locale, (void), {
    try {
        var locale = globalThis.navigator ? globalThis.navigator.language : null;
        if (!locale) { return 0; }
        var bytes = lengthBytesUTF8(locale) + 1;
        var ptr = _malloc(bytes);
        stringToUTF8(locale, ptr, bytes);
        return ptr;
    } catch (e) {
        return 0;
    }
})
#endif

/* The language whose code matches the tag's primary subtag ("ru-RU" -> "ru"),
   or -1 when the game does not ship it. Yandex serves Belarusian, Kazakh,
   Ukrainian and Uzbek players from its Russian catalogue and asks for Russian
   there, so those fall back to "ru" when the game has no table of their own. */
static int language_for_locale(const char *code) {
    if (code == NULL || code[0] == '\0') return -1;
    char primary[8] = {0};
    for (size_t i = 0; i < sizeof primary - 1 && code[i] != '\0' && code[i] != '-' && code[i] != '_'; ++i) {
        primary[i] = (char)((code[i] >= 'A' && code[i] <= 'Z') ? code[i] + ('a' - 'A') : code[i]);
    }
    int russian = -1;
    for (int i = 0; i < SETTINGS_STATE_LANGUAGE_COUNT; ++i) {
        const char *name = settings_state_language_name(i);
        if (name == NULL) continue;
        if (strcmp(name, primary) == 0) return i;
        if (strcmp(name, "ru") == 0) russian = i;
    }
    static const char *const k_russian_catalogue[] = {"be", "kk", "uk", "uz"};
    for (size_t i = 0; i < sizeof k_russian_catalogue / sizeof k_russian_catalogue[0]; ++i) {
        if (strcmp(primary, k_russian_catalogue[i]) == 0) return russian;
    }
    return -1;
}

void settings_choose_language(int language) {
    settings_set_language(language);
    settings_state.language_chosen = true;
    game_save_mark_dirty();
}

void settings_adopt_platform_language(void) {
    /* The portal is asked on every startup even when the answer changes
       nothing: Yandex reads the game as broken if the SDK's language is never
       queried while it loads. */
    int language = language_for_locale(platform_sdk_locale());
#ifdef __EMSCRIPTEN__
    if (language < 0) {
        char *code = settings_web_browser_locale();
        language = language_for_locale(code);
        if (code != NULL) free(code);
    }
#endif
    if (language >= 0 && !settings_state.language_chosen) settings_set_language(language);
}

void settings_set_language(int language) {
    if (language < 0 || language >= SETTINGS_STATE_LANGUAGE_COUNT) {
        return;
    }
    /* The generated field is int; the enum's underlying type is unsigned under
       some toolchains, so routing through it would change signedness. */
    settings_state.language = language;
    settings_apply_language();
    game_save_mark_dirty();
}
