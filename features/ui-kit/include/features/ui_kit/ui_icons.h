#ifndef FEATURE_UI_KIT_ICONS_H
#define FEATURE_UI_KIT_ICONS_H

// The kit's glyph set: white masks under `features/ui-kit/assets/icons/<name>.png`,
// tinted at runtime so one file serves every theme. One list feeds the enum,
// the names a pack builder packs under its prefix, and the theme's
// bind-by-name, so the three cannot drift. Header-only: a pack builder tool
// includes it without the engine.
#define UI_KIT_ICONS(X)         \
    X(CLOSE, "close")           \
    X(GEAR, "gear")             \
    X(CHECK, "check")           \
    X(LOCK, "lock")             \
    X(UNLOCK, "unlock")         \
    X(PLUS, "plus")             \
    X(MINUS, "minus")           \
    X(BACK, "back")             \
    X(ARROW_LEFT, "arrow_left")       \
    X(ARROW_RIGHT, "arrow_right")     \
    X(ARROW_UP, "arrow_up")           \
    X(ARROW_DOWN, "arrow_down")       \
    X(STAR, "star")             \
    X(COIN, "coin")             \
    X(SOUND_ON, "sound_on")     \
    X(SOUND_OFF, "sound_off")   \
    X(MUSIC_ON, "music_on")     \
    X(MUSIC_OFF, "music_off")   \
    X(INFO, "info")             \
    X(QUESTION, "question")     \
    X(WARNING, "warning")       \
    X(HOME, "home")             \
    X(PAUSE, "pause")           \
    X(TROPHY, "trophy")         \
    X(CART, "cart")             \
    X(TRASH, "trash")

typedef enum {
#define UI_ICON_ENUM(id, name) UI_ICON_##id,
    UI_KIT_ICONS(UI_ICON_ENUM)
#undef UI_ICON_ENUM
        UI_ICON_COUNT
} ui_icon_t;

// The file stem and the region suffix of a glyph.
static inline const char *ui_icon_name(ui_icon_t icon) {
    static const char *const names[] = {
#define UI_ICON_NAME(id, name) name,
        UI_KIT_ICONS(UI_ICON_NAME)
#undef UI_ICON_NAME
    };
    return (icon >= 0 && icon < UI_ICON_COUNT) ? names[icon] : "";
}

#endif /* FEATURE_UI_KIT_ICONS_H */
