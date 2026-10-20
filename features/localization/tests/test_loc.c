#include "unity.h"

#include "core/nt_assert.h"
#include "features/localization/loc.h"
#include "memory/nt_mem_scratch.h"

#include <setjmp.h>
#include <string.h>

/* Runtime contract for features/localization/src/loc.c. The fixture table is
   hand-written here on purpose: this proves the RUNTIME, so it must not depend
   on whatever the game's corpus happens to contain today. The shape is exactly
   what loc.py emits. */

enum {
    K_CLOSE = 0,   /* zero-arg, both languages */
    K_RU_ONLY,     /* zero-arg, ru only -- exercises the fallback chain */
    K_REQUIRES,    /* str + int, no plural */
    K_COINS,       /* int:group + plural */
    K_RATE,        /* float:2 */
    K_BRACES,      /* literal braces and a percent sign */
    K_HOLE,        /* plural, en missing -- fallback under a plural key */
    K_ESCAPED,     /* template with both escaped braces and a slot */
    K_GAPPY,       /* plural, ru carries one category -- in-row fallback */
    K_BIGIDX,      /* a slot index far past any argument count */
    K_COUNT
};

enum { LANG_RU = 0, LANG_EN = 1, LANG_COUNT = 2 };

/* Designated, not positional: this table is hand-built, so a new field in
   loc_lang_def_t shifts every value one slot right and only the type
   system caught it (it did -- adding decimal_separator broke this file at
   -Werror). Named fields make the next one a no-op instead. */
static const loc_lang_def_t k_langs[LANG_COUNT] = {
    {.code = "ru",
     .group_separator = " ",
     .decimal_separator = ",",
     .group_min_digits = 5,
     .plural_rule = LOC_PLURAL_RULE_SLAVIC},
    {.code = "en",
     .group_separator = ",",
     .decimal_separator = ".",
     .group_min_digits = 4,
     .plural_rule = LOC_PLURAL_RULE_ONE_OTHER},
};

static const char *const k_f_close[] = {"ЗАКРЫТЬ", "Close"};
static const char *const k_f_ru_only[] = {"ТОЛЬКО РУ", NULL};
static const char *const k_f_requires[] = {"Нужно {0} Ур {1}", "Requires {0} Lv {1}"};
static const char *const k_f_rate[] = {"Скорость {0}", "Rate {0}"};
/* A zero-argument key stores finished text: loc_get() hands the pointer over
   without expanding, so the generator resolves `{{`/`}}` at build time. */
static const char *const k_f_braces[] = {"100% {готово}", "100% {done}"};

/* zero, one, two, few, many, other -- per language, in loc_plural_cat_t order. */
static const char *const k_f_coins[] = {
    NULL, "нужна {0} монета", NULL, "нужно {0} монеты", "нужно {0} монет", "нужно {0} монеты",
    NULL, "need {0} coin", NULL, NULL, NULL, "need {0} coins",
};
static const char *const k_f_hole[] = {
    NULL, "{0} шаг", NULL, "{0} шага", "{0} шагов", "{0} шага",
    NULL, NULL, NULL, NULL, NULL, NULL,
};

static const char *const k_f_escaped[] = {"100% {{{0}}}", "100% {{{0}}}"};

/* ru carries `many` and nothing else: every other count the slavic rule
   selects has to reach into the row for something to render. A generated
   table never looks like this -- a hand-edited one can. */
static const char *const k_f_gappy[] = {
    NULL, NULL,        NULL, NULL, "{0} монет", NULL,
    NULL, "{0} step", NULL, NULL, NULL,        "{0} steps",
};

/* Not reachable from generated data (the generator emits {0}..{7}); here to
   pin that a runaway index is rejected rather than overflowing the parse. */
static const char *const k_f_bigidx[] = {"a{99999999999}b", "a{99999999999}b"};

static const loc_string_def_t k_strings[K_COUNT] = {
    {"common.close", k_f_close, false},
    {"common.ru_only", k_f_ru_only, false},
    {"upgrades.requires", k_f_requires, false},
    {"upgrades.coins_needed", k_f_coins, true},
    {"hud.rate", k_f_rate, false},
    {"hud.braces", k_f_braces, false},
    {"hud.steps", k_f_hole, true},
    {"hud.escaped", k_f_escaped, false},
    {"hud.gappy", k_f_gappy, true},
    {"hud.big_index", k_f_bigidx, false},
};

static uint8_t s_logged[(K_COUNT + 7) / 8];
/* Reading coverage: the module marks a bit per key it resolves. This test does
   not read the bits -- it just has to own the storage, like the generated TU
   does, so -Wmissing-field-initializers stays a useful alarm rather than
   something these tables have to be exempted from. */
static uint8_t s_seen[(K_COUNT + 7) / 8];

static const loc_table_t k_table = {
    k_langs, LANG_COUNT, k_strings, K_COUNT, LANG_RU, s_logged, sizeof s_logged,
    s_seen, sizeof s_seen,
};

void setUp(void) {
    nt_mem_scratch_init(64u * 1024u);
    loc_bind_table(&k_table);
}

void tearDown(void) { nt_mem_scratch_shutdown(); }

/* Contract violations are NT_ASSERT, so proving them needs the engine's own
   hookable handler (nt_assert.h: "Always defined so tests can link against it
   in any build mode"). Precedent: engine tests/unit/test_resource.c. */
#if NT_ASSERT_MODE == NT_ASSERT_FULL
static jmp_buf s_assert_jmp;

static void test_assert_trap(const char *expr, const char *file, int line) {
    (void)expr;
    (void)file;
    (void)line;
    longjmp(s_assert_jmp, 1);
}

#define EXPECT_ASSERT(code)                                     \
    do {                                                        \
        nt_assert_handler = test_assert_trap;                   \
        if (setjmp(s_assert_jmp) == 0) {                        \
            code;                                               \
            nt_assert_handler = NULL;                           \
            TEST_FAIL_MESSAGE("Expected NT_ASSERT to fire");    \
        }                                                       \
        nt_assert_handler = NULL;                               \
    } while (0)
#endif

static const char *coins(int64_t n) {
    const loc_arg_t args[1] = {{LOC_ARG_INT_GROUP, n, 0.0, NULL}};
    return loc_format(K_COINS, 0, args, 1).s;
}

/* --- language selection ------------------------------------------------- */

void test_binding_defaults_to_the_fallback_language(void) {
    TEST_ASSERT_EQUAL_INT(LANG_RU, loc_lang_index());
    TEST_ASSERT_EQUAL_INT(LANG_COUNT, loc_lang_count());
    TEST_ASSERT_EQUAL_STRING("en", loc_lang_code(LANG_EN));
}

void test_set_lang_switches_the_rendered_language(void) {
    TEST_ASSERT_EQUAL_STRING("ЗАКРЫТЬ", loc_get(K_CLOSE).s);
    loc_set_lang_index(LANG_EN);
    TEST_ASSERT_EQUAL_STRING("Close", loc_get(K_CLOSE).s);
}

void test_lang_from_code_matches_exactly_then_by_base(void) {
    TEST_ASSERT_EQUAL_INT(LANG_RU, loc_lang_from_code("ru"));
    TEST_ASSERT_EQUAL_INT(LANG_EN, loc_lang_from_code("en-US"));
    TEST_ASSERT_EQUAL_INT(LANG_RU, loc_lang_from_code("ru-RU"));
    TEST_ASSERT_EQUAL_INT(-1, loc_lang_from_code("de"));
    TEST_ASSERT_EQUAL_INT(-1, loc_lang_from_code(""));
    TEST_ASSERT_EQUAL_INT(-1, loc_lang_from_code(NULL));
}

/* --- fallback chain ----------------------------------------------------- */

void test_zero_arg_key_falls_back_to_the_authoring_language(void) {
    loc_set_lang_index(LANG_EN);
    TEST_ASSERT_EQUAL_STRING("ТОЛЬКО РУ", loc_get(K_RU_ONLY).s);
}

void test_zero_arg_key_returns_a_pointer_into_the_static_table(void) {
    TEST_ASSERT_EQUAL_PTR(k_f_close[LANG_RU], loc_get(K_CLOSE).s);
}

void test_plural_key_falls_back_and_reselects_under_the_fallback_rule(void) {
    loc_set_lang_index(LANG_EN);
    const loc_arg_t args[1] = {{LOC_ARG_INT, 3, 0.0, NULL}};
    /* 3 is `other` in English but `few` in Russian: the category must be
       recomputed under the language actually used, not carried over. */
    TEST_ASSERT_EQUAL_STRING("3 шага", loc_format(K_HOLE, 0, args, 1).s);
}

/* --- contracts ---------------------------------------------------------- */

#if NT_ASSERT_MODE == NT_ASSERT_FULL
void test_out_of_range_key_asserts(void) {
    /* Generated code cannot produce these, so reaching one means the caller
       went around the codegen. Painting a plausible string over that would
       hide the bug -- crash instead. */
    EXPECT_ASSERT(loc_get(-1));
    EXPECT_ASSERT(loc_get(K_COUNT));
    EXPECT_ASSERT(loc_format(K_COUNT, -1, NULL, 0));
}

void test_out_of_range_language_asserts(void) {
    EXPECT_ASSERT(loc_set_lang_index(LANG_COUNT));
    EXPECT_ASSERT(loc_set_lang_index(-1));
    EXPECT_ASSERT(loc_lang_code(LANG_COUNT));
}

void test_bad_argument_count_asserts(void) {
    loc_arg_t args[16];
    memset(args, 0, sizeof args);
    EXPECT_ASSERT(loc_format(K_REQUIRES, -1, args, 16));
    EXPECT_ASSERT(loc_format(K_REQUIRES, -1, args, -1));
    EXPECT_ASSERT(loc_format(K_REQUIRES, -1, NULL, 2));
}

void test_out_of_range_plural_argument_asserts(void) {
    const loc_arg_t args[1] = {{LOC_ARG_INT, 1, 0.0, NULL}};
    EXPECT_ASSERT(loc_format(K_COINS, 1, args, 1));
    EXPECT_ASSERT(loc_format(K_COINS, -2, args, 1));
}

void test_malformed_table_asserts_at_bind(void) {
    /* Checked once, here, so every accessor can trust the table afterwards. */
    EXPECT_ASSERT(loc_bind_table(NULL));
    static const loc_table_t no_langs = {k_langs,   0,          k_strings,    K_COUNT, 0,
                                         s_logged, sizeof s_logged, s_seen, sizeof s_seen};
    EXPECT_ASSERT(loc_bind_table(&no_langs));
    static const loc_table_t bad_fallback = {
        k_langs, LANG_COUNT, k_strings, K_COUNT, LANG_COUNT, s_logged, sizeof s_logged,
        s_seen, sizeof s_seen,
    };
    EXPECT_ASSERT(loc_bind_table(&bad_fallback));

    /* A coverage bitset that is missing or short is the one broken field that
       breaks NOTHING at runtime: mark_seen just skips the bits it cannot reach,
       and the corpus report then says "no surface ever showed this" about a
       string that is on screen every frame. Trapped at bind for the same reason
       as the rest -- so every reader of loc_key_was_seen can trust the answer. */
    static const loc_table_t no_seen = {
        k_langs, LANG_COUNT, k_strings, K_COUNT, LANG_RU, s_logged, sizeof s_logged,
        NULL, 0,
    };
    EXPECT_ASSERT(loc_bind_table(&no_seen));
    static const loc_table_t short_seen = {
        k_langs, LANG_COUNT, k_strings, K_COUNT, LANG_RU, s_logged, sizeof s_logged,
        s_seen, sizeof s_seen - 1u, /* one byte short: loses the tail keys only */
    };
    EXPECT_ASSERT(loc_bind_table(&short_seen));
}

void test_a_hole_in_the_data_never_asserts(void) {
    /* The other half of the split: a missing translation is data, not a broken
       program, so it falls back instead of trapping. */
    loc_set_lang_index(LANG_EN);
    TEST_ASSERT_EQUAL_STRING("ТОЛЬКО РУ", loc_get(K_RU_ONLY).s);
    TEST_ASSERT_EQUAL_INT(-1, loc_lang_from_code(NULL));
    TEST_ASSERT_EQUAL_INT(-1, loc_lang_from_code("de"));
}
#endif

/* --- fallback logging --------------------------------------------------- */

void test_fallback_logs_once_per_key_per_session(void) {
    loc_set_lang_index(LANG_EN);
    TEST_ASSERT_EQUAL_INT(0, loc_fallback_log_count());
    for (int i = 0; i < 60; ++i) {
        (void)loc_get(K_RU_ONLY);
    }
    TEST_ASSERT_EQUAL_INT(1, loc_fallback_log_count());

    const loc_arg_t args[1] = {{LOC_ARG_INT, 1, 0.0, NULL}};
    for (int i = 0; i < 60; ++i) {
        (void)loc_format(K_HOLE, 0, args, 1);
    }
    TEST_ASSERT_EQUAL_INT(2, loc_fallback_log_count()); /* a second key, still once each */
}

void test_rebinding_clears_the_dedup_bits(void) {
    loc_set_lang_index(LANG_EN);
    (void)loc_get(K_RU_ONLY);
    TEST_ASSERT_EQUAL_INT(1, loc_fallback_log_count());
    loc_bind_table(&k_table);
    TEST_ASSERT_EQUAL_INT(0, loc_fallback_log_count());
    TEST_ASSERT_EQUAL_INT(LANG_RU, loc_lang_index());
}

void test_a_complete_key_never_logs(void) {
    loc_set_lang_index(LANG_EN);
    (void)loc_get(K_CLOSE);
    TEST_ASSERT_EQUAL_INT(0, loc_fallback_log_count());
}

void test_a_missing_plural_form_falls_back_within_the_row_and_logs(void) {
    /* The key HAS Russian text, so the language fallback never fires -- but not
       in the form the count selects, so the player reads a grammatically wrong
       sentence -- the one degradation this module could do silently. */
    const loc_arg_t one[1] = {{LOC_ARG_INT, 1, 0.0, NULL}};
    TEST_ASSERT_EQUAL_STRING("1 монет", loc_format(K_GAPPY, 0, one, 1).s);
    for (int i = 0; i < 60; ++i) {
        (void)loc_format(K_GAPPY, 0, one, 1);
    }
    TEST_ASSERT_EQUAL_INT(1, loc_fallback_log_count()); /* deduped like any other */
}

void test_a_present_plural_form_never_logs(void) {
    const loc_arg_t many[1] = {{LOC_ARG_INT, 5, 0.0, NULL}}; /* ru: `many`, the one it carries */
    TEST_ASSERT_EQUAL_STRING("5 монет", loc_format(K_GAPPY, 0, many, 1).s);
    TEST_ASSERT_EQUAL_INT(0, loc_fallback_log_count());
}

/* --- plurals ------------------------------------------------------------ */

void test_russian_plural_categories(void) {
    TEST_ASSERT_EQUAL_STRING("нужна 1 монета", coins(1));
    TEST_ASSERT_EQUAL_STRING("нужно 2 монеты", coins(2));
    TEST_ASSERT_EQUAL_STRING("нужно 4 монеты", coins(4));
    TEST_ASSERT_EQUAL_STRING("нужно 5 монет", coins(5));
    TEST_ASSERT_EQUAL_STRING("нужно 0 монет", coins(0));
    TEST_ASSERT_EQUAL_STRING("нужно 11 монет", coins(11)); /* 11, not `one` */
    TEST_ASSERT_EQUAL_STRING("нужно 12 монет", coins(12)); /* 12-14, not `few` */
    TEST_ASSERT_EQUAL_STRING("нужно 14 монет", coins(14));
    TEST_ASSERT_EQUAL_STRING("нужна 21 монета", coins(21));
    TEST_ASSERT_EQUAL_STRING("нужно 22 монеты", coins(22));
    TEST_ASSERT_EQUAL_STRING("нужно 25 монет", coins(25));
    TEST_ASSERT_EQUAL_STRING("нужна 101 монета", coins(101));
    TEST_ASSERT_EQUAL_STRING("нужно 111 монет", coins(111));
}

void test_english_plural_categories(void) {
    loc_set_lang_index(LANG_EN);
    TEST_ASSERT_EQUAL_STRING("need 1 coin", coins(1));
    TEST_ASSERT_EQUAL_STRING("need 0 coins", coins(0));
    TEST_ASSERT_EQUAL_STRING("need 2 coins", coins(2));
    TEST_ASSERT_EQUAL_STRING("need 21 coins", coins(21));
}

void test_negative_plural_selector_does_not_index_out_of_the_row(void) {
    TEST_ASSERT_EQUAL_STRING("нужна -1 монета", coins(-1));
    TEST_ASSERT_EQUAL_STRING("нужно -5 монет", coins(-5));
}

void test_fractional_selector_always_takes_other(void) {
    /* CLDR: a value with visible fraction digits is `other` in ru and en.
       The comma is ru's decimal mark, not part of this assertion's point --
       see test_decimal_mark_is_language_data below. */
    const loc_arg_t args[1] = {{LOC_ARG_FLOAT1, 0, 1.5, NULL}};
    TEST_ASSERT_EQUAL_STRING("нужно 1,5 монеты", loc_format(K_COINS, 0, args, 1).s);
}

/* --- digit grouping ----------------------------------------------------- */

void test_russian_grouping_starts_at_five_digits(void) {
    TEST_ASSERT_EQUAL_STRING("нужно 1000 монет", coins(1000));
    TEST_ASSERT_EQUAL_STRING("нужно 9999 монет", coins(9999));
    TEST_ASSERT_EQUAL_STRING("нужно 10 000 монет", coins(10000));
    TEST_ASSERT_EQUAL_STRING("нужно 1 500 000 монет", coins(1500000));
}

void test_english_grouping_starts_at_four_digits(void) {
    loc_set_lang_index(LANG_EN);
    TEST_ASSERT_EQUAL_STRING("need 999 coins", coins(999));
    TEST_ASSERT_EQUAL_STRING("need 1,000 coins", coins(1000));
    TEST_ASSERT_EQUAL_STRING("need 1,500,000 coins", coins(1500000));
}

void test_grouping_keeps_the_sign_outside_the_groups(void) {
    loc_set_lang_index(LANG_EN);
    TEST_ASSERT_EQUAL_STRING("need -12,345 coins", coins(-12345));
}

void test_loc_number_groups_for_the_active_language(void) {
    /* The arena-allocated twin of loc_group_i64, for the call sites that hand a
       number straight to a widget. */
    TEST_ASSERT_EQUAL_STRING("1000", loc_number(1000).s); /* ru groups from 5 digits */
    TEST_ASSERT_EQUAL_STRING("10 000", loc_number(10000).s);
    loc_set_lang_index(LANG_EN);
    TEST_ASSERT_EQUAL_STRING("1,000", loc_number(1000).s);
    TEST_ASSERT_EQUAL_STRING("-12,345", loc_number(-12345).s);
}

void test_loc_number_is_exactly_sized_and_does_not_alias(void) {
    const size_t before = nt_mem_scratch_used();
    LocStr first = loc_number(10000);
    const size_t after = nt_mem_scratch_used();
    TEST_ASSERT_EQUAL_UINT((unsigned)(strlen(first.s) + 1u), (unsigned)(after - before));
    LocStr second = loc_number(7);
    TEST_ASSERT_EQUAL_STRING("10 000", first.s);
    TEST_ASSERT_EQUAL_STRING("7", second.s);
}

void test_plain_int_is_never_grouped(void) {
    const loc_arg_t args[2] = {
        {LOC_ARG_STR, 0, 0.0, "Руки"},
        {LOC_ARG_INT, 1500000, 0.0, NULL},
    };
    TEST_ASSERT_EQUAL_STRING("Нужно Руки Ур 1500000", loc_format(K_REQUIRES, -1, args, 2).s);
}

/* --- formatting --------------------------------------------------------- */

void test_str_argument_is_verbatim_and_null_is_empty(void) {
    const loc_arg_t args[2] = {
        {LOC_ARG_STR, 0, 0.0, NULL},
        {LOC_ARG_INT, 3, 0.0, NULL},
    };
    TEST_ASSERT_EQUAL_STRING("Нужно  Ур 3", loc_format(K_REQUIRES, -1, args, 2).s);
}

void test_float_precision(void) {
    const loc_arg_t two[1] = {{LOC_ARG_FLOAT2, 0, 1.25, NULL}};
    TEST_ASSERT_EQUAL_STRING("Скорость 1,25", loc_format(K_RATE, -1, two, 1).s);
    const loc_arg_t g[1] = {{LOC_ARG_FLOAT_G, 0, 2.5, NULL}};
    TEST_ASSERT_EQUAL_STRING("Скорость 2,5", loc_format(K_RATE, -1, g, 1).s);
}

/* The decimal MARK must not be hardcoded to the C locale, or every
   float:* argument printed '.' in every language, so the FALLBACK and authoring
   language rendered wrong. It is language data now, exactly like grouping.

   Both marks in one assertion on purpose: ru groups with a SPACE and separates
   with a COMMA, en does the opposite. If either rule reached into the other,
   one of these four strings would be wrong. */
void test_decimal_mark_is_language_data(void) {
    const loc_arg_t two[1] = {{LOC_ARG_FLOAT2, 0, 1.25, NULL}};
    const loc_arg_t g[1] = {{LOC_ARG_FLOAT_G, 0, 2.5, NULL}};
    TEST_ASSERT_EQUAL_STRING("Скорость 1,25", loc_format(K_RATE, -1, two, 1).s);
    TEST_ASSERT_EQUAL_STRING("нужно 1 500 000 монет", coins(1500000));
    loc_set_lang_index(LANG_EN); /* setUp rebinds the table, so no restore */
    TEST_ASSERT_EQUAL_STRING("Rate 1.25", loc_format(K_RATE, -1, two, 1).s);
    TEST_ASSERT_EQUAL_STRING("need 1,500,000 coins", coins(1500000));
    TEST_ASSERT_EQUAL_STRING("Rate 2.5", loc_format(K_RATE, -1, g, 1).s);
}

/* Game-side formatters (an abbreviated `1,2m`) must not depend on this module,
   so they take the mark as a parameter and the call site reads it here. */
void test_loc_decimal_separator_follows_the_active_language(void) {
    TEST_ASSERT_EQUAL_STRING(",", loc_decimal_separator());
    loc_set_lang_index(LANG_EN);
    TEST_ASSERT_EQUAL_STRING(".", loc_decimal_separator());
}

void test_literal_braces_and_percent_survive_expansion(void) {
    /* Zero-arg key: the generator already resolved the escapes. */
    TEST_ASSERT_EQUAL_STRING("100% {готово}", loc_get(K_BRACES).s);
    /* Formatted key: the runtime resolves them while expanding the template. */
    const loc_arg_t args[1] = {{LOC_ARG_STR, 0, 0.0, "Руки"}};
    TEST_ASSERT_EQUAL_STRING("100% {Руки}", loc_format(K_ESCAPED, -1, args, 1).s);
}

void test_a_runaway_slot_index_stays_literal(void) {
    /* The index parse is bounded: an index that could never name an argument
       stops accumulating instead of overflowing, and the braces read out as the
       text they are. */
    TEST_ASSERT_EQUAL_STRING("a{99999999999}b", loc_format(K_BIGIDX, -1, NULL, 0).s);
}

void test_formatted_result_is_exactly_sized(void) {
    const size_t before = nt_mem_scratch_used();
    const char *text = coins(5);
    const size_t after = nt_mem_scratch_used();
    TEST_ASSERT_EQUAL_UINT((unsigned)(strlen(text) + 1u), (unsigned)(after - before));
}

void test_two_formatted_strings_do_not_alias(void) {
    /* Both live until the next reset, so an earlier result must stay intact. */
    const char *first = coins(1);
    const char *second = coins(5);
    TEST_ASSERT_EQUAL_STRING("нужна 1 монета", first);
    TEST_ASSERT_EQUAL_STRING("нужно 5 монет", second);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_binding_defaults_to_the_fallback_language);
    RUN_TEST(test_set_lang_switches_the_rendered_language);
    RUN_TEST(test_lang_from_code_matches_exactly_then_by_base);
    RUN_TEST(test_zero_arg_key_falls_back_to_the_authoring_language);
    RUN_TEST(test_zero_arg_key_returns_a_pointer_into_the_static_table);
    RUN_TEST(test_plural_key_falls_back_and_reselects_under_the_fallback_rule);
#if NT_ASSERT_MODE == NT_ASSERT_FULL
    RUN_TEST(test_out_of_range_key_asserts);
    RUN_TEST(test_out_of_range_language_asserts);
    RUN_TEST(test_bad_argument_count_asserts);
    RUN_TEST(test_out_of_range_plural_argument_asserts);
    RUN_TEST(test_malformed_table_asserts_at_bind);
    RUN_TEST(test_a_hole_in_the_data_never_asserts);
#endif
    RUN_TEST(test_fallback_logs_once_per_key_per_session);
    RUN_TEST(test_rebinding_clears_the_dedup_bits);
    RUN_TEST(test_a_complete_key_never_logs);
    RUN_TEST(test_a_missing_plural_form_falls_back_within_the_row_and_logs);
    RUN_TEST(test_a_present_plural_form_never_logs);
    RUN_TEST(test_russian_plural_categories);
    RUN_TEST(test_english_plural_categories);
    RUN_TEST(test_negative_plural_selector_does_not_index_out_of_the_row);
    RUN_TEST(test_fractional_selector_always_takes_other);
    RUN_TEST(test_russian_grouping_starts_at_five_digits);
    RUN_TEST(test_english_grouping_starts_at_four_digits);
    RUN_TEST(test_grouping_keeps_the_sign_outside_the_groups);
    RUN_TEST(test_loc_number_groups_for_the_active_language);
    RUN_TEST(test_loc_number_is_exactly_sized_and_does_not_alias);
    RUN_TEST(test_plain_int_is_never_grouped);
    RUN_TEST(test_str_argument_is_verbatim_and_null_is_empty);
    RUN_TEST(test_float_precision);
    RUN_TEST(test_decimal_mark_is_language_data);
    RUN_TEST(test_loc_decimal_separator_follows_the_active_language);
    RUN_TEST(test_literal_braces_and_percent_survive_expansion);
    RUN_TEST(test_a_runaway_slot_index_stays_literal);
    RUN_TEST(test_formatted_result_is_exactly_sized);
    RUN_TEST(test_two_formatted_strings_do_not_alias);
    return UNITY_END();
}
