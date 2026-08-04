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
}

void loc_set_lang_index(int lang) {
    NT_ASSERT(s_table != NULL && "loc_set_lang: call loc_init() first");
    NT_ASSERT(lang >= 0 && lang < s_table->lang_count && "loc_set_lang: language index out of range");
    s_lang = lang;
}

int loc_lang_index(void) { return s_lang; }

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
   would emit sixty lines a second for one missing translation. */
static void log_fallback(int key_index, int from_lang) {
    if (!s_table || !s_table->fallback_logged) {
        return;
    }
    const size_t byte = (size_t)key_index / 8u;
    if (byte >= s_table->fallback_logged_bytes) {
        return;
    }
    const uint8_t bit = (uint8_t)(1u << ((unsigned)key_index % 8u));
    if (s_table->fallback_logged[byte] & bit) {
        return;
    }
    s_table->fallback_logged[byte] |= bit;
    s_fallback_log_count += 1;
#ifndef NDEBUG
    NT_LOG_WARN("no '%s' for %s (using %s)", s_table->langs[from_lang].code,
                s_table->strings[key_index].key, s_table->langs[s_table->fallback_lang].code);
#else
    (void)from_lang;
#endif
}
// #endregion

// #region plural selection
static loc_plural_cat_t plural_select(loc_plural_rule_t rule, int64_t value) {
    /* Negative counts are nonsense in copy, but must not index out of the row. */
    const uint64_t n = (value < 0) ? (uint64_t)(-(value + 1)) + 1u : (uint64_t)value;
    const uint64_t mod10 = n % 10u;
    const uint64_t mod100 = n % 100u;
    switch (rule) {
    case LOC_PLURAL_RULE_ONE_OTHER:
        return (n == 1u) ? LOC_PLURAL_ONE : LOC_PLURAL_OTHER;
    case LOC_PLURAL_RULE_FRENCH:
        return (n <= 1u) ? LOC_PLURAL_ONE : LOC_PLURAL_OTHER;
    case LOC_PLURAL_RULE_SLAVIC:
        if (mod10 == 1u && mod100 != 11u) {
            return LOC_PLURAL_ONE;
        }
        if (mod10 >= 2u && mod10 <= 4u && (mod100 < 12u || mod100 > 14u)) {
            return LOC_PLURAL_FEW;
        }
        return LOC_PLURAL_MANY;
    case LOC_PLURAL_RULE_POLISH:
        if (n == 1u) {
            return LOC_PLURAL_ONE;
        }
        if (mod10 >= 2u && mod10 <= 4u && (mod100 < 12u || mod100 > 14u)) {
            return LOC_PLURAL_FEW;
        }
        return LOC_PLURAL_MANY;
    case LOC_PLURAL_RULE_CZECH:
        if (n == 1u) {
            return LOC_PLURAL_ONE;
        }
        return (n >= 2u && n <= 4u) ? LOC_PLURAL_FEW : LOC_PLURAL_OTHER;
    case LOC_PLURAL_RULE_OTHER_ONLY:
    case LOC_PLURAL_RULE_COUNT:
    default:
        return LOC_PLURAL_OTHER;
    }
}

static const char *form_for(const loc_string_def_t *def, int lang, loc_plural_cat_t cat) {
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
        return scratch;
    case LOC_ARG_FLOAT2:
        (void)snprintf(scratch, cap, "%.2f", arg->f);
        return scratch;
    case LOC_ARG_FLOAT_G:
    default:
        (void)snprintf(scratch, cap, "%g", arg->f);
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
            while (*q >= '0' && *q <= '9') {
                index = index * 10 + (*q - '0');
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
    const loc_string_def_t *def = &s_table->strings[key_index];
    const char *text = form_for(def, s_lang, LOC_PLURAL_OTHER);
    if (!text) {
        log_fallback(key_index, s_lang);
        text = form_for(def, s_table->fallback_lang, LOC_PLURAL_OTHER);
    }
    /* The bare key is the emergency case: the generator guarantees the fallback
       language is complete, so reaching it means the table was hand-edited. */
    return loc_raw(text ? text : def->key);
}

LocStr loc_format(int key_index, int plural_arg, const loc_arg_t *args, int arg_count) {
    NT_ASSERT(s_table != NULL && "loc_format: call loc_init() first");
    NT_ASSERT(key_index >= 0 && key_index < s_table->string_count && "loc_format: key index out of range");
    NT_ASSERT(arg_count >= 0 && arg_count <= LOC_MAX_ARGS && "loc_format: argument count out of range");
    NT_ASSERT((arg_count == 0 || args != NULL) && "loc_format: NULL args with a non-zero arg_count");
    NT_ASSERT(plural_arg >= -1 && plural_arg < arg_count && "loc_format: plural argument index out of range");
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
        cat = plural_select(s_table->langs[lang].plural_rule, plural_value);
    }
    const char *tpl = form_for(def, lang, cat);
    if (!tpl) {
        log_fallback(key_index, lang);
        lang = s_table->fallback_lang;
        cat = LOC_PLURAL_OTHER;
        if (def->is_plural && !fractional) {
            /* Categories are per language: ru's rule and en's disagree, so the
               form must be reselected under the language actually used. */
            cat = plural_select(s_table->langs[lang].plural_rule, plural_value);
        }
        tpl = form_for(def, lang, cat);
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
