#define NT_LOG_DOMAIN "loc"

#include "features/localization/loc.h"

#include <stdio.h>
#include <string.h>

#include "core/nt_assert.h"
#include "log/nt_log.h"
#include "memory/nt_mem_scratch.h"

/* Asserts are contracts, not error handling (engine AGENTS.md). The split
   below is deliberate:
     - a key index, an argument count, a language index, or an unbound table
       is a PROGRAMMER error. Generated code cannot produce any of them, so
       reaching one means the caller went around the codegen -- crash, do not
       paint a plausible string over the bug.
     - a missing translation is DATA. It falls back and logs, because a hole in
       a corpus is a thing that legitimately happens while a language is being
       written. */

/* LOC_MAX_ARGS and LOC_NUMBER_BUF live in loc.h: the generator asserts the
   corpus fits the first, and callers of loc_group_i64 size buffers by the
   second. */

static const loc_table_t *s_table;
static int s_lang;
static int s_fallback_log_count;
static int s_seen_count;

// #region table binding
void loc_bind_table(const loc_table_t *table) {
    NT_ASSERT(table != NULL && "loc_bind_table: table must not be NULL");
    NT_ASSERT(table->langs != NULL && table->lang_count > 0 && "loc_bind_table: table declares no languages");
    NT_ASSERT(table->strings != NULL && table->string_count > 0 && "loc_bind_table: table declares no strings");
    NT_ASSERT(table->fallback_lang >= 0 && table->fallback_lang < table->lang_count &&
              "loc_bind_table: fallback language index out of range");
    s_table = table;
    s_fallback_log_count = 0;
    s_lang = table->fallback_lang;
    if (table->fallback_logged && table->fallback_logged_bytes > 0) {
        memset(table->fallback_logged, 0, table->fallback_logged_bytes);
    }
    /* Every loc_table_t is generated (loc.py), so a coverage bitset that is
       missing or too short is a generator bug, not a caller's choice -- and it
       is the kind that lies QUIETLY: mark_seen bails on an out-of-range byte,
       so the keys past the end read back as "no surface ever showed this" for a
       string that is on screen every frame. That is exactly the claim the
       corpus report makes about the game, so it has to be load-bearing. */
    NT_ASSERT(table->seen != NULL && "loc_bind_table: table carries no coverage bitset");
    NT_ASSERT(table->seen_bytes >= ((size_t)table->string_count + 7u) / 8u &&
              "loc_bind_table: coverage bitset is too small for the key count");
    s_seen_count = 0;
    memset(table->seen, 0, table->seen_bytes);
}

void loc_set_lang_index(int lang) {
    NT_ASSERT(s_table != NULL && "loc_set_lang: call loc_init() first");
    NT_ASSERT(lang >= 0 && lang < s_table->lang_count && "loc_set_lang: language index out of range");
    s_lang = lang;
}

int loc_lang_index(void) {
    NT_ASSERT(s_table != NULL && "loc_lang_index: call loc_init() first");
    return s_lang;
}

int loc_lang_count(void) {
    NT_ASSERT(s_table != NULL && "loc_lang_count: call loc_init() first");
    return s_table->lang_count;
}

const char *loc_lang_code(int lang) {
    NT_ASSERT(s_table != NULL && "loc_lang_code: call loc_init() first");
    NT_ASSERT(lang >= 0 && lang < s_table->lang_count && "loc_lang_code: language index out of range");
    return s_table->langs[lang].code;
}

static size_t base_code_len(const char *code) {
    const char *dash = strchr(code, '-');
    return dash ? (size_t)(dash - code) : strlen(code);
}

int loc_lang_from_code(const char *code) {
    NT_ASSERT(s_table != NULL && "loc_lang_from_code: call loc_init() first");
    /* NOT an assert: the caller is autodetect, and an OS or browser locale is
       allowed to be absent. That is input, not a broken program. */
    if (!code || !*code) {
        return -1;
    }
    for (int i = 0; i < s_table->lang_count; ++i) {
        if (strcmp(s_table->langs[i].code, code) == 0) {
            return i;
        }
    }
    /* "en-US" -> "en": a base-language match is the whole point of autodetect. */
    const size_t want = base_code_len(code);
    for (int i = 0; i < s_table->lang_count; ++i) {
        const char *have = s_table->langs[i].code;
        if (base_code_len(have) == want && strncmp(have, code, want) == 0) {
            return i;
        }
    }
    return -1;
}

int loc_fallback_log_count(void) { return s_fallback_log_count; }
// #endregion

// #region fallback logging
/* One bit per key: the UI rebuilds every frame, so an undeduplicated warning
   would emit sixty lines a second for one missing translation. The bit is
   shared by both degradations below -- the contract is "at most one line per
   key", not "one line per kind of hole". */
static bool claim_fallback_slot(int key_index) {
    if (!s_table || !s_table->fallback_logged) {
        return false;
    }
    const size_t byte = (size_t)key_index / 8u;
    if (byte >= s_table->fallback_logged_bytes) {
        return false;
    }
    const uint8_t bit = (uint8_t)(1u << ((unsigned)key_index % 8u));
    if (s_table->fallback_logged[byte] & bit) {
        return false;
    }
    s_table->fallback_logged[byte] |= bit;
    s_fallback_log_count += 1;
    return true;
}

/* Marks a key as resolved for display. One byte-write per lookup on a table the
   generated TU already owns -- see loc_key_was_seen in loc.h for why coverage is
   worth that. Deliberately NOT conditional on NDEBUG: a capture run that only
   works in a debug build proves coverage of a build nobody ships. */
static void mark_seen(int key_index) {
    if (!s_table || !s_table->seen) {
        return;
    }
    const size_t byte = (size_t)key_index / 8u;
    if (byte >= s_table->seen_bytes) {
        return;
    }
    const uint8_t bit = (uint8_t)(1u << ((unsigned)key_index % 8u));
    if (s_table->seen[byte] & bit) {
        return;
    }
    s_table->seen[byte] |= bit;
    s_seen_count += 1;
}

/* The key carries no text in the active language. */
static void log_lang_fallback(int key_index, int from_lang) {
    if (!claim_fallback_slot(key_index)) {
        return;
    }
#ifndef NDEBUG
    NT_LOG_WARN("no '%s' for %s (using %s)", s_table->langs[from_lang].code,
                s_table->strings[key_index].key, s_table->langs[s_table->fallback_lang].code);
#else
    (void)from_lang;
#endif
}

/* The key carries text in this language, but not in the plural form the count
   selected -- so the player is reading a grammatically wrong sentence, which is
   exactly the kind of bug that survives a playthrough unnoticed. Generated data
   never reaches here (the generator requires every form its language's rule can
   select); a hand-written table can. */
static void log_form_fallback(int key_index, int lang, loc_plural_cat_t want) {
#ifndef NDEBUG
    /* Designated, not positional: this is the third place the category order
       appears (loc.h's enum, loc.py's PLURAL_CATS, here), and the other two are
       pinned against each other by _Static_asserts in the generated header.
       Indexing by the enumerator makes this one immune to a reorder instead of
       being a fourth thing to remember. */
    static const char *const k_cat_names[LOC_PLURAL_CAT_COUNT] = {
        [LOC_PLURAL_ZERO] = "zero",  [LOC_PLURAL_ONE] = "one",
        [LOC_PLURAL_TWO] = "two",    [LOC_PLURAL_FEW] = "few",
        [LOC_PLURAL_MANY] = "many",  [LOC_PLURAL_OTHER] = "other",
    };
#endif
    if (!claim_fallback_slot(key_index)) {
        return;
    }
#ifndef NDEBUG
    NT_LOG_WARN("no '%s' form of %s in %s (using another form of the same key)",
                k_cat_names[want], s_table->strings[key_index].key, s_table->langs[lang].code);
#else
    (void)lang;
    (void)want;
#endif
}
// #endregion

// #region plural selection
static bool in_range(uint64_t v, uint64_t lo, uint64_t hi) { return v >= lo && v <= hi; }

/* Transcribed one-for-one from the Defold i18n table (see loc.h). Every
 * partition here is verified against its Python twin by test_loc_e2e, which
 * walks the generated (rule, n) -> category array -- so an edit to one half
 * that the other does not get is a red test, not a wrong word on screen.
 *
 * No `default:`: -Wswitch is the only thing that makes adding a rule to the
 * enum without adding it here a compile error. */
loc_plural_cat_t loc_plural_category(loc_plural_rule_t rule, int64_t count) {
    /* Negative counts are nonsense in copy, but must not index out of the row.
       (-(v+1))+1 avoids the UB of negating INT64_MIN. */
    const uint64_t n = (count < 0) ? (uint64_t)(-(count + 1)) + 1u : (uint64_t)count;
    const uint64_t n10 = n % 10u;
    const uint64_t n100 = n % 100u;

    switch (rule) {
    case LOC_PLURAL_RULE_ONE_OTHER:
        return (n == 1u) ? LOC_PLURAL_ONE : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_ONE_ZERO_OTHER:
    case LOC_PLURAL_RULE_TACHELHIT:
        return (n <= 1u) ? LOC_PLURAL_ONE : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_ARABIC:
        if (n == 0u) { return LOC_PLURAL_ZERO; }
        if (n == 1u) { return LOC_PLURAL_ONE; }
        if (n == 2u) { return LOC_PLURAL_TWO; }
        if (in_range(n100, 3u, 10u)) { return LOC_PLURAL_FEW; }
        return in_range(n100, 11u, 99u) ? LOC_PLURAL_MANY : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_OTHER_ONLY:
        return LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_SLAVIC:
        if (n10 == 1u && n100 != 11u) { return LOC_PLURAL_ONE; }
        if (in_range(n10, 2u, 4u) && !in_range(n100, 12u, 14u)) { return LOC_PLURAL_FEW; }
        if (n10 == 0u || in_range(n10, 5u, 9u) || in_range(n100, 11u, 14u)) { return LOC_PLURAL_MANY; }
        return LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_BRETON:
        if (n10 == 1u && n100 != 11u && n100 != 71u && n100 != 91u) { return LOC_PLURAL_ONE; }
        if (n10 == 2u && n100 != 12u && n100 != 72u && n100 != 92u) { return LOC_PLURAL_TWO; }
        if ((n10 == 3u || n10 == 4u || n10 == 9u) && !in_range(n100, 10u, 19u) &&
            !in_range(n100, 70u, 79u) && !in_range(n100, 90u, 99u)) {
            return LOC_PLURAL_FEW;
        }
        return (n != 0u && n % 1000000u == 0u) ? LOC_PLURAL_MANY : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_CZECH:
        if (n == 1u) { return LOC_PLURAL_ONE; }
        return in_range(n, 2u, 4u) ? LOC_PLURAL_FEW : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_WELSH:
        if (n == 0u) { return LOC_PLURAL_ZERO; }
        if (n == 1u) { return LOC_PLURAL_ONE; }
        if (n == 2u) { return LOC_PLURAL_TWO; }
        if (n == 3u) { return LOC_PLURAL_FEW; }
        return (n == 6u) ? LOC_PLURAL_MANY : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_FRENCH:
        return (n < 2u) ? LOC_PLURAL_ONE : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_IRISH:
        if (n == 1u) { return LOC_PLURAL_ONE; }
        if (n == 2u) { return LOC_PLURAL_TWO; }
        if (in_range(n, 3u, 6u)) { return LOC_PLURAL_FEW; }
        return in_range(n, 7u, 10u) ? LOC_PLURAL_MANY : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_GAELIC:
        if (n == 1u || n == 11u) { return LOC_PLURAL_ONE; }
        if (n == 2u || n == 12u) { return LOC_PLURAL_TWO; }
        return (in_range(n, 3u, 10u) || in_range(n, 13u, 19u)) ? LOC_PLURAL_FEW : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_MANX:
        return (n10 == 1u || n10 == 2u || n % 20u == 0u) ? LOC_PLURAL_ONE : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_ONE_TWO_OTHER:
        if (n == 1u) { return LOC_PLURAL_ONE; }
        return (n == 2u) ? LOC_PLURAL_TWO : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_ZERO_ONE_OTHER:
    case LOC_PLURAL_RULE_LANGI:
        /* Langi differs only on fractions in the reference (0 < n < 2), which
           cannot reach this function. */
        if (n == 0u) { return LOC_PLURAL_ZERO; }
        return (n == 1u) ? LOC_PLURAL_ONE : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_LITHUANIAN:
        if (in_range(n100, 11u, 19u)) { return LOC_PLURAL_OTHER; }
        if (n10 == 1u) { return LOC_PLURAL_ONE; }
        return in_range(n10, 2u, 9u) ? LOC_PLURAL_FEW : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_LATVIAN:
        if (n == 0u) { return LOC_PLURAL_ZERO; }
        return (n10 == 1u && n100 != 11u) ? LOC_PLURAL_ONE : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_MACEDONIAN:
        return (n10 == 1u && n != 11u) ? LOC_PLURAL_ONE : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_ROMANIAN:
        if (n == 1u) { return LOC_PLURAL_ONE; }
        return (n == 0u || in_range(n100, 1u, 19u)) ? LOC_PLURAL_FEW : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_MALTESE:
        if (n == 1u) { return LOC_PLURAL_ONE; }
        if (n == 0u || in_range(n100, 2u, 10u)) { return LOC_PLURAL_FEW; }
        return in_range(n100, 11u, 19u) ? LOC_PLURAL_MANY : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_POLISH:
        if (n == 1u) { return LOC_PLURAL_ONE; }
        if (in_range(n10, 2u, 4u) && !in_range(n100, 12u, 14u)) { return LOC_PLURAL_FEW; }
        if (n10 == 0u || n10 == 1u || in_range(n10, 5u, 9u) || in_range(n100, 12u, 14u)) {
            return LOC_PLURAL_MANY;
        }
        return LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_SLOVENIAN:
        if (n100 == 1u) { return LOC_PLURAL_ONE; }
        if (n100 == 2u) { return LOC_PLURAL_TWO; }
        return (n100 == 3u || n100 == 4u) ? LOC_PLURAL_FEW : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_TAMAZIGHT:
        return (n <= 1u || in_range(n, 11u, 99u)) ? LOC_PLURAL_ONE : LOC_PLURAL_OTHER;

    case LOC_PLURAL_RULE_COUNT:
        break;
    }
    NT_ASSERT(false && "loc_plural_category: unknown plural rule");
    return LOC_PLURAL_OTHER;
}

/* `degraded` (optional) reports "this row had text, but not the form asked
   for" -- the one silent-wrong-word path in this file, so the callers turn it
   into a warning rather than letting it pass. It stays FALSE when the row is
   empty outright: that is a missing translation, which is logged separately. */
static const char *form_for(const loc_string_def_t *def, int lang, loc_plural_cat_t cat,
                            bool *degraded) {
    if (!def->is_plural) {
        return def->forms[lang];
    }
    const size_t row = (size_t)lang * (size_t)LOC_PLURAL_CAT_COUNT;
    const char *text = def->forms[row + (size_t)cat];
    if (text) {
        return text;
    }
    /* Last resort so a language that carries the key at all always renders
       something. `other` first, then anything in the row: Russian's rule
       selects one/few/many and never `other`, so `other` is not guaranteed to
       be present the way it is for English. Generated data never reaches this
       -- the generator requires every form its language's rule can select. */
    text = def->forms[row + (size_t)LOC_PLURAL_OTHER];
    for (size_t i = 0; !text && i < (size_t)LOC_PLURAL_CAT_COUNT; ++i) {
        text = def->forms[row + i];
    }
    if (text && degraded) {
        *degraded = true;
    }
    return text;
}
// #endregion

// #region argument rendering
/* Never truncates a number: a clipped price is a WRONG value on screen, not a
   cosmetic overflow. Separators are dropped before any digit is, and the
   result is empty only when even the bare digits do not fit. */
static void render_grouped(char *out, size_t cap, int64_t value, const loc_lang_def_t *lang) {
    if (cap == 0) {
        return;
    }
    char digits[24];
    const int written = snprintf(digits, sizeof digits, "%lld", (long long)value);
    if (written <= 0 || (size_t)written >= sizeof digits) {
        out[0] = '\0';
        return;
    }
    const size_t sign = (digits[0] == '-') ? 1u : 0u;
    const size_t count = (size_t)written - sign;
    const size_t sep_len = strlen(lang->group_separator);
    const size_t groups = (count >= 1u) ? (count - 1u) / 3u : 0u;
    const bool grouped = count >= (size_t)lang->group_min_digits && sep_len > 0 && groups > 0;
    const size_t needed = (size_t)written + (grouped ? groups * sep_len : 0u);
    if (needed + 1u > cap) {
        /* Grouping is the first thing sacrificed, the digits the last. */
        (void)snprintf(out, cap, "%s", ((size_t)written + 1u <= cap) ? digits : "");
        return;
    }
    size_t at = 0;
    if (sign) {
        out[at++] = '-';
    }
    for (size_t i = 0; i < count; ++i) {
        if (grouped && i > 0 && ((count - i) % 3u) == 0u) {
            memcpy(out + at, lang->group_separator, sep_len);
            at += sep_len;
        }
        out[at++] = digits[sign + i];
    }
    out[at] = '\0';
}

const char *loc_group_i64(int64_t value, char *out, size_t cap) {
    NT_ASSERT(s_table != NULL && "loc_group_i64: call loc_init() first");
    NT_ASSERT(out != NULL && cap > 0 && "loc_group_i64: needs a writable buffer");
    /* A cap too small to hold the grouped form is NOT an error: the contract
       says separators are dropped first, digits last. */
    render_grouped(out, cap, value, &s_table->langs[s_lang]);
    return out;
}

const char *loc_decimal_separator(void) {
    NT_ASSERT(s_table != NULL && "loc_decimal_separator: call loc_init() first");
    const char *sep = s_table->langs[s_lang].decimal_separator;
    /* The generator always emits one, but a hand-built table (the tests do
       this) may not. Returning NULL would push the check onto every caller,
       and the caller is a formatter that would then have its own idea of the
       default -- which is the duplication this function exists to remove. */
    return (sep != NULL) ? sep : ".";
}

LocStr loc_number(int64_t value) {
    NT_ASSERT(s_table != NULL && "loc_number: call loc_init() first");
    char buf[LOC_NUMBER_BUF];
    render_grouped(buf, sizeof buf, value, &s_table->langs[s_lang]);
    const size_t len = strlen(buf);
    char *out = (char *)nt_mem_scratch_alloc(len + 1u, 1u);
    memcpy(out, buf, len + 1u);
    return loc_raw(out);
}

/* The C library writes '.' whatever the language is, because this module never
 * touches setlocale -- that is process-global state, and a text module has no
 * business changing how the whole program parses numbers. So the mark is
 * substituted afterwards, in place.
 *
 * `snprintf` emits at most ONE '.', so this is a single splice, not a scan-and-
 * replace loop. A separator longer than one byte (a comma is one, but nothing
 * requires it) shifts the tail right; if the result would not fit, the ORIGINAL
 * text is left alone. Wrong mark beats a truncated number: `1.2` misreads as a
 * foreign convention, `1,` misreads as a different value. */
static void apply_decimal_separator(char *text, size_t cap, const loc_lang_def_t *lang) {
    const char *sep = lang->decimal_separator;
    /* EMPTY is rejected here, not just NULL and ".". An empty separator makes
       sep_len 0, the fit check passes trivially, and the memmove below shifts
       the tail LEFT over the dot -- 1.5 renders as 15, a different number. That
       is worse than the truncation the fit check exists to prevent. The schema
       (minLength 1) and loc.py keep it out of the data; a hand-built table, of
       which the tests are full, can still reach it. */
    if (sep == NULL || sep[0] == '\0' || (sep[0] == '.' && sep[1] == '\0')) {
        return;
    }
    char *dot = strchr(text, '.');
    if (dot == NULL) {
        return;
    }
    const size_t len = strlen(text);
    const size_t sep_len = strlen(sep);
    const size_t tail = len - (size_t)(dot - text) - 1u; /* bytes after the '.' */
    if (len - 1u + sep_len + 1u > cap) {
        return;
    }
    memmove(dot + sep_len, dot + 1, tail + 1u); /* +1 carries the NUL */
    memcpy(dot, sep, sep_len);
}

static const char *render_arg(const loc_arg_t *arg, const loc_lang_def_t *lang, char *scratch, size_t cap) {
    switch (arg->kind) {
    case LOC_ARG_STR:
        return arg->s ? arg->s : "";
    case LOC_ARG_INT:
        (void)snprintf(scratch, cap, "%lld", (long long)arg->i);
        return scratch;
    case LOC_ARG_INT_GROUP:
        render_grouped(scratch, cap, arg->i, lang);
        return scratch;
    case LOC_ARG_FLOAT1:
        (void)snprintf(scratch, cap, "%.1f", arg->f);
        apply_decimal_separator(scratch, cap, lang);
        return scratch;
    case LOC_ARG_FLOAT2:
        (void)snprintf(scratch, cap, "%.2f", arg->f);
        apply_decimal_separator(scratch, cap, lang);
        return scratch;
    case LOC_ARG_FLOAT_G:
    default:
        (void)snprintf(scratch, cap, "%g", arg->f);
        apply_decimal_separator(scratch, cap, lang);
        return scratch;
    }
}
// #endregion

// #region template expansion
/* Walks the generated template directly -- no printf, so `%` is literal and no
   format string ever originates at a call site. `out` NULL measures. */
static size_t expand(const char *tpl, const char *const *values, const size_t *lengths, int arg_count, char *out) {
    size_t used = 0;
    for (const char *p = tpl; *p != '\0';) {
        if (*p == '{') {
            if (p[1] == '{') {
                if (out) {
                    out[used] = '{';
                }
                used += 1;
                p += 2;
                continue;
            }
            const char *q = p + 1;
            int index = 0;
            /* Bounded on purpose: `{99999999999}` is out of range whatever it
               parses to, and must not overflow the accumulator on its way to
               being rejected. Past the cap the value stops moving and stays
               out of range. */
            while (*q >= '0' && *q <= '9') {
                if (index <= LOC_MAX_ARGS) {
                    index = index * 10 + (*q - '0');
                }
                ++q;
            }
            if (q > p + 1 && *q == '}' && index < arg_count) {
                if (out) {
                    memcpy(out + used, values[index], lengths[index]);
                }
                used += lengths[index];
                p = q + 1;
                continue;
            }
        } else if (*p == '}' && p[1] == '}') {
            if (out) {
                out[used] = '}';
            }
            used += 1;
            p += 2;
            continue;
        }
        if (out) {
            out[used] = *p;
        }
        used += 1;
        ++p;
    }
    return used;
}
// #endregion

// #region lookup
LocStr loc_get(int key_index) {
    NT_ASSERT(s_table != NULL && "loc_get: call loc_init() first");
    NT_ASSERT(key_index >= 0 && key_index < s_table->string_count && "loc_get: key index out of range");
    mark_seen(key_index);
    const loc_string_def_t *def = &s_table->strings[key_index];
    /* A zero-argument key cannot be plural (a plural key needs its count), so
       there is no form to degrade to and nothing to report. */
    const char *text = form_for(def, s_lang, LOC_PLURAL_OTHER, NULL);
    if (!text) {
        log_lang_fallback(key_index, s_lang);
        text = form_for(def, s_table->fallback_lang, LOC_PLURAL_OTHER, NULL);
    }
    /* The bare key is the emergency case: the generator guarantees the fallback
       language is complete, so reaching it means the table was hand-edited. */
    return loc_raw(text ? text : def->key);
}

bool loc_key_was_seen(int key_index) {
    if (!s_table || !s_table->seen || key_index < 0) {
        return false;
    }
    const size_t byte = (size_t)key_index / 8u;
    if (byte >= s_table->seen_bytes) {
        return false;
    }
    return (s_table->seen[byte] & (uint8_t)(1u << ((unsigned)key_index % 8u))) != 0;
}

int loc_seen_count(void) { return s_seen_count; }

void loc_seen_reset(void) {
    if (s_table && s_table->seen) {
        memset(s_table->seen, 0, s_table->seen_bytes);
    }
    s_seen_count = 0;
}

LocStr loc_format(int key_index, int plural_arg, const loc_arg_t *args, int arg_count) {
    NT_ASSERT(s_table != NULL && "loc_format: call loc_init() first");
    NT_ASSERT(key_index >= 0 && key_index < s_table->string_count && "loc_format: key index out of range");
    NT_ASSERT(arg_count >= 0 && arg_count <= LOC_MAX_ARGS && "loc_format: argument count out of range");
    NT_ASSERT((arg_count == 0 || args != NULL) && "loc_format: NULL args with a non-zero arg_count");
    NT_ASSERT(plural_arg >= -1 && plural_arg < arg_count && "loc_format: plural argument index out of range");
    mark_seen(key_index);
    const loc_string_def_t *def = &s_table->strings[key_index];

    int64_t plural_value = 0;
    bool fractional = false;
    if (plural_arg >= 0) {
        const loc_arg_t *selector = &args[plural_arg];
        if (selector->kind == LOC_ARG_INT || selector->kind == LOC_ARG_INT_GROUP) {
            plural_value = selector->i;
        } else {
            /* CLDR: a value with visible fraction digits selects `other`. */
            fractional = true;
        }
    }

    int lang = s_lang;
    loc_plural_cat_t cat = LOC_PLURAL_OTHER;
    if (def->is_plural && !fractional) {
        cat = loc_plural_category(s_table->langs[lang].plural_rule, plural_value);
    }
    bool degraded = false;
    const char *tpl = form_for(def, lang, cat, &degraded);
    if (!tpl) {
        log_lang_fallback(key_index, lang);
        lang = s_table->fallback_lang;
        cat = LOC_PLURAL_OTHER;
        if (def->is_plural && !fractional) {
            /* Categories are per language: ru's rule and en's disagree, so the
               form must be reselected under the language actually used. */
            cat = loc_plural_category(s_table->langs[lang].plural_rule, plural_value);
        }
        degraded = false;
        tpl = form_for(def, lang, cat, &degraded);
    }
    if (degraded) {
        log_form_fallback(key_index, lang, cat);
    }
    if (!tpl) {
        return loc_raw(def->key);
    }

    const loc_lang_def_t *lang_def = &s_table->langs[lang];
    char scratch[LOC_MAX_ARGS][LOC_NUMBER_BUF];
    const char *values[LOC_MAX_ARGS];
    size_t lengths[LOC_MAX_ARGS];
    for (int i = 0; i < arg_count; ++i) {
        values[i] = render_arg(&args[i], lang_def, scratch[i], sizeof scratch[i]);
        lengths[i] = strlen(values[i]);
    }

    const size_t needed = expand(tpl, values, lengths, arg_count, NULL);
    char *out = (char *)nt_mem_scratch_alloc(needed + 1u, 1u);
    (void)expand(tpl, values, lengths, arg_count, out);
    out[needed] = '\0';
    return loc_raw(out);
}
// #endregion
