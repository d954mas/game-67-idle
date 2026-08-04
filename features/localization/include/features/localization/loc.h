#ifndef FEATURES_LOCALIZATION_LOC_H
#define FEATURES_LOCALIZATION_LOC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/nt_assert.h" /* the contracts below are asserts; generated code needs it too */

/* Universal localization runtime. Knows nothing about any game: the language
 * list, the key vocabulary, and every string live in the generated table this
 * module is handed through loc_bind_table(). Links against the engine
 * (nt_mem_scratch, nt_log, nt_assert) and generated code only.
 *
 * Game code calls the GENERATED accessors (loc_<key>()), never the entry
 * points below -- those are the codegen's ABI.
 *
 * CONTRACTS ARE ASSERTS. Every function here requires a bound table, and every
 * index must be in range; violating that is NT_ASSERT, not a return value,
 * because generated code cannot produce it -- reaching one means the caller
 * went around the codegen, and a plausible string painted over that bug is
 * worse than a crash. A MISSING TRANSLATION is the opposite: it is data, it
 * legitimately occurs while a language is being written, so it falls back and
 * logs. Same for an OS locale that is absent, and for a buffer too small to
 * hold digit grouping. */

/* Localized text. One-field wrapper so a raw literal cannot reach a text
 * widget by accident: passing `const char *` where LocStr is expected does not
 * compile. loc_raw() is the explicit, greppable escape hatch. */
typedef struct LocStr {
    const char *s;
} LocStr;

/* Widest key the runtime renders. The generated TU carries a _Static_assert
 * against this, so lowering it breaks the build instead of degrading a string
 * at runtime -- the cap lives here and nowhere else. */
#define LOC_MAX_ARGS 8

/* Buffer size that always holds one rendered number: int64 with a separator
 * every three digits, or %.2f of the widest float, plus sign and NUL. */
#define LOC_NUMBER_BUF 48

/* CLDR plural categories. Doubles as the slot order of a plural forms row. */
typedef enum loc_plural_cat {
    LOC_PLURAL_ZERO = 0,
    LOC_PLURAL_ONE,
    LOC_PLURAL_TWO,
    LOC_PLURAL_FEW,
    LOC_PLURAL_MANY,
    LOC_PLURAL_OTHER,
    LOC_PLURAL_CAT_COUNT
} loc_plural_cat_t;

/* Which CLDR rule selects a category. The generator maps a language code (or
 * an explicit `plural_rule` in the data) onto one of these; the arithmetic
 * lives here so adding a language never edits C. */
typedef enum loc_plural_rule {
    LOC_PLURAL_RULE_OTHER_ONLY = 0, /* ja zh ko th vi id ms */
    LOC_PLURAL_RULE_ONE_OTHER,      /* en de es it nl pt sv da no fi el he hu tr */
    LOC_PLURAL_RULE_FRENCH,         /* fr hy: `one` covers 0 and 1 */
    LOC_PLURAL_RULE_SLAVIC,         /* ru uk be hr sr bs */
    LOC_PLURAL_RULE_POLISH,         /* pl */
    LOC_PLURAL_RULE_CZECH,          /* cs sk */
    LOC_PLURAL_RULE_COUNT
} loc_plural_rule_t;

typedef struct loc_lang_def {
    const char *code;            /* "ru" */
    const char *group_separator; /* inserted every 3 digits by `int:group` */
    int group_min_digits;        /* grouping starts once the integer part is this long */
    loc_plural_rule_t plural_rule;
} loc_lang_def_t;

typedef struct loc_string_def {
    const char *key;
    /* Non-plural: lang_count entries, one per language.
     * Plural: lang_count * LOC_PLURAL_CAT_COUNT, row-major by language.
     * NULL means that language (or category) carries no text. */
    const char *const *forms;
    bool is_plural;
} loc_string_def_t;

typedef struct loc_table {
    const loc_lang_def_t *langs;
    int lang_count;
    const loc_string_def_t *strings;
    int string_count;
    int fallback_lang;
    /* One bit per key, set the first time that key falls back. Owned by the
     * generated TU so this module allocates nothing of its own. */
    uint8_t *fallback_logged;
    size_t fallback_logged_bytes;
} loc_table_t;

/* Argument kinds mirror the `args` types in strings.json. */
typedef enum loc_arg_kind {
    LOC_ARG_INT = 0,   /* "int"       plain digits */
    LOC_ARG_INT_GROUP, /* "int:group" digit grouping from the language block */
    LOC_ARG_FLOAT1,    /* "float:1" */
    LOC_ARG_FLOAT2,    /* "float:2" */
    LOC_ARG_FLOAT_G,   /* "float:g"   shortest round-trip */
    LOC_ARG_STR        /* "str"       a LocStr, verbatim */
} loc_arg_kind_t;

typedef struct loc_arg {
    loc_arg_kind_t kind;
    int64_t i;
    double f;
    const char *s;
} loc_arg_t;

/* --- codegen ABI ------------------------------------------------------- */

/* Called by the generated loc_init(). Resets the active language to the
 * table's fallback and clears the fallback-log bits. Asserts the table is
 * well-formed once here, so the accessors below do not re-check it. */
void loc_bind_table(const loc_table_t *table);

/* Zero-argument key: a pointer straight into the static table, no copy and no
 * length limit -- a long description never allocates. */
LocStr loc_get(int key_index);

/* Formatted key. Exact-size nt_mem_scratch_alloc; valid until the next arena
 * reset (start of the next frame). Copy it if it must outlive the frame.
 * plural_arg is the index into args of the argument selecting the form, or -1
 * for a non-plural key. */
LocStr loc_format(int key_index, int plural_arg, const loc_arg_t *args, int arg_count);

/* --- runtime API ------------------------------------------------------- */

/* The generated header wraps these as loc_set_lang(LocLang) / loc_lang(). */
void loc_set_lang_index(int lang);
int loc_lang_index(void);

int loc_lang_count(void);
const char *loc_lang_code(int lang);

/* Exact code match first, then a base-language match ("en-US" -> "en").
 * Returns -1 when nothing matches. */
int loc_lang_from_code(const char *code);

/* Digit grouping for the ACTIVE language, for a number that IS the whole
 * string and therefore has no key of its own -- a coin readout, a price. Those
 * cannot go through `int:group`, which only renders inside a key's text, and a
 * game-side thousands formatter would be one language's typography applied to
 * every language.
 *
 * Returns `out` so it chains at a call site. `cap` below LOC_NUMBER_BUF risks
 * losing the grouping (never a digit): the separators are dropped before any
 * digit is, and an empty string only when even the bare digits do not fit. */
const char *loc_group_i64(int64_t value, char *out, size_t cap);

/* Total deduplicated fallback events this session (one per key, at most). */
int loc_fallback_log_count(void);

/* Explicit escape hatch: text that is deliberately not localized. */
static inline LocStr loc_raw(const char *s) {
    LocStr out;
    out.s = s;
    return out;
}

#endif /* FEATURES_LOCALIZATION_LOC_H */
