#include "unity.h"

#include "loc_plural_cases.gen.h"
#include "loc_strings.gen.h"
#include "memory/nt_mem_scratch.h"

#include <stdio.h>
#include <string.h>

/* End-to-end: e2e_strings.json -> loc.py -> THIS compiles against the real
 * generated header and calls the real generated accessors.
 *
 * test_loc.c proves loc.c against a hand-written table, and loc_test.py proves
 * the generator's output as text. Neither covers the seam between them: the
 * interpolation grammar is written by render_template() in Python and read by
 * expand() in C, and the plural category sets live in PLURAL_RULE_CATS while
 * the arithmetic that selects them lives in plural_select(). Both halves can
 * be individually correct and still disagree -- which is exactly how the
 * zero-argument brace-escape bug survived its first review.
 *
 * Everything asserted here therefore goes through generated code. */

void setUp(void) {
    nt_mem_scratch_init(64u * 1024u);
    loc_init();
}

void tearDown(void) { nt_mem_scratch_shutdown(); }

/* --- the table the generator built ------------------------------------- */

void test_generated_enums_match_the_corpus(void) {
    TEST_ASSERT_EQUAL_INT(7, LOC_LANG_COUNT); /* ru en ja pl cs fr + ar */
    TEST_ASSERT_EQUAL_INT(11, LOC_KEY_COUNT);
    /* Only the keys with no arguments are addressable as data. */
    TEST_ASSERT_EQUAL_INT(3, LOC0_COUNT);
    TEST_ASSERT_EQUAL_INT(LOC_LANG_RU, LOC_FALLBACK_LANG);
    TEST_ASSERT_EQUAL_INT(8, LOC_GENERATED_MAX_ARGS);
}

void test_init_starts_on_the_fallback_language(void) {
    TEST_ASSERT_EQUAL_INT(LOC_LANG_RU, loc_lang());
    TEST_ASSERT_EQUAL_STRING("ЗАКРЫТЬ", loc_e2e_plain().s);
    loc_set_lang(LOC_LANG_JA);
    TEST_ASSERT_EQUAL_STRING("閉じる", loc_e2e_plain().s);
}

void test_by_key_resolves_a_zero_argument_key(void) {
    TEST_ASSERT_EQUAL_STRING("ЗАКРЫТЬ", loc_by_key(LOC0_E2E_PLAIN).s);
    loc_set_lang(LOC_LANG_EN);
    TEST_ASSERT_EQUAL_STRING("CLOSE", loc_by_key(LOC0_E2E_PLAIN).s);
}

/* --- the interpolation grammar, both directions ------------------------- */

void test_escaped_braces_in_a_zero_argument_key(void) {
    /* Resolved by the generator: loc_get hands the pointer over untouched. */
    TEST_ASSERT_EQUAL_STRING("100% {готово}", loc_e2e_braces_bare().s);
    loc_set_lang(LOC_LANG_EN);
    TEST_ASSERT_EQUAL_STRING("100% {done}", loc_e2e_braces_bare().s);
}

void test_escaped_braces_around_a_slot(void) {
    /* Resolved by the runtime: the same source text, the other code path. */
    TEST_ASSERT_EQUAL_STRING("100% {Руки}", loc_e2e_braces_arg(loc_raw("Руки")).s);
}

void test_percent_never_reaches_a_format_string(void) {
    TEST_ASSERT_EQUAL_STRING("100% {готово}", loc_e2e_braces_bare().s);
}

void test_slots_are_indices_so_languages_may_reorder(void) {
    const LocStr a = loc_raw("A");
    const LocStr b = loc_raw("B");
    TEST_ASSERT_EQUAL_STRING("B затем A", loc_e2e_reordered(a, b).s);
    loc_set_lang(LOC_LANG_EN);
    TEST_ASSERT_EQUAL_STRING("A then B", loc_e2e_reordered(a, b).s);
    loc_set_lang(LOC_LANG_JA);
    TEST_ASSERT_EQUAL_STRING("A と B", loc_e2e_reordered(a, b).s);
}

void test_widest_key_renders_every_slot(void) {
    TEST_ASSERT_EQUAL_STRING("12345678", loc_e2e_widest(1, 2, 3, 4, 5, 6, 7, 8).s);
}

/* --- argument types ----------------------------------------------------- */

void test_number_types_render_per_language(void) {
    TEST_ASSERT_EQUAL_STRING("1500000|1 500 000|2.5|2.50|0.125",
                             loc_e2e_numbers(1500000, 1500000, 2.5F, 2.5F, 0.125F).s);
    loc_set_lang(LOC_LANG_EN);
    TEST_ASSERT_EQUAL_STRING("1500000|1,500,000|2.5|2.50|0.125",
                             loc_e2e_numbers(1500000, 1500000, 2.5F, 2.5F, 0.125F).s);
}

void test_grouping_threshold_comes_from_the_language_block(void) {
    /* ru groups from five digits, en from four -- same number, same key. */
    TEST_ASSERT_EQUAL_STRING("1000|1000|0.0|0.00|0", loc_e2e_numbers(1000, 1000, 0.0F, 0.0F, 0.0F).s);
    loc_set_lang(LOC_LANG_EN);
    TEST_ASSERT_EQUAL_STRING("1000|1,000|0.0|0.00|0", loc_e2e_numbers(1000, 1000, 0.0F, 0.0F, 0.0F).s);
}

void test_loc_group_i64_follows_the_active_language(void) {
    char buf[LOC_NUMBER_BUF];
    TEST_ASSERT_EQUAL_STRING("9999", loc_group_i64(9999, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("10 000", loc_group_i64(10000, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("-1 234 567", loc_group_i64(-1234567, buf, sizeof buf));
    loc_set_lang(LOC_LANG_EN);
    TEST_ASSERT_EQUAL_STRING("9,999", loc_group_i64(9999, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("-1,234,567", loc_group_i64(-1234567, buf, sizeof buf));
}

void test_loc_group_i64_drops_separators_before_digits(void) {
    /* A clipped price is a wrong value, not a cosmetic overflow. */
    char exact[8]; /* "123 456" + NUL fits exactly, so grouping survives */
    TEST_ASSERT_EQUAL_STRING("123 456", loc_group_i64(123456, exact, sizeof exact));
    char tight[7]; /* one byte short: the separator goes, the digits stay */
    TEST_ASSERT_EQUAL_STRING("123456", loc_group_i64(123456, tight, sizeof tight));
    char tiny[4];
    TEST_ASSERT_EQUAL_STRING("123", loc_group_i64(123, tiny, sizeof tiny));
    char hopeless[3];
    TEST_ASSERT_EQUAL_STRING("", loc_group_i64(123456, hopeless, sizeof hopeless));
}

void test_int64_extremes_survive_grouping(void) {
    char buf[LOC_NUMBER_BUF];
    TEST_ASSERT_EQUAL_STRING("9 223 372 036 854 775 807", loc_group_i64(INT64_MAX, buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("-9 223 372 036 854 775 808", loc_group_i64(INT64_MIN, buf, sizeof buf));
}

/* --- the two halves of the plural table, compared ----------------------- */

void test_every_plural_rule_matches_the_generator(void) {
    /* loc.py carries the 24 partitions in Python, loc.c carries them again in
       C. Neither is trusted: this walks every (rule, n) pair the Python table
       declares -- 24 rules x 423 values -- through the compiled selector. An
       edit to one half that the other did not get dies here. */
    TEST_ASSERT_EQUAL_INT(24 * 423, k_loc_plural_case_count);
    for (int i = 0; i < k_loc_plural_case_count; ++i) {
        const loc_plural_case_t *c = &k_loc_plural_cases[i];
        if (loc_plural_category(c->rule, c->n) != c->expected) {
            char message[128];
            (void)snprintf(message, sizeof message, "rule %d, n=%lld: expected cat %d, got %d",
                           (int)c->rule, (long long)c->n, (int)c->expected,
                           (int)loc_plural_category(c->rule, c->n));
            TEST_FAIL_MESSAGE(message);
        }
    }
}

void test_every_rule_is_reachable_from_the_case_table(void) {
    /* A rule added to the enum but not to loc.py would otherwise be untested. */
    bool seen[LOC_PLURAL_RULE_COUNT] = {false};
    for (int i = 0; i < k_loc_plural_case_count; ++i) {
        seen[k_loc_plural_cases[i].rule] = true;
    }
    for (int rule = 0; rule < LOC_PLURAL_RULE_COUNT; ++rule) {
        TEST_ASSERT_TRUE_MESSAGE(seen[rule], "a plural rule has no generated cases");
    }
}

/* --- plurals through the generated accessors ---------------------------- */

static const char *steps(int64_t n) { return loc_e2e_counted(n).s; }

void test_slavic_rule_selects_one_few_many(void) {
    TEST_ASSERT_EQUAL_STRING("1 шаг", steps(1));
    TEST_ASSERT_EQUAL_STRING("2 шага", steps(2));
    TEST_ASSERT_EQUAL_STRING("5 шагов", steps(5));
    TEST_ASSERT_EQUAL_STRING("11 шагов", steps(11));
    TEST_ASSERT_EQUAL_STRING("21 шаг", steps(21));
    TEST_ASSERT_EQUAL_STRING("22 шага", steps(22));
    TEST_ASSERT_EQUAL_STRING("111 шагов", steps(111));
    TEST_ASSERT_EQUAL_STRING("0 шагов", steps(0));
}

/* The zero and two slots. Slots 0 and 2 of every plural row this generator has
   ever emitted, and until this test NOTHING READ THEM: the fixture's six
   languages between them use only one/few/many/other, and no game corpus has a
   plural key at all.
   The 10152-case cross-check proves Python and C agree on which CATEGORY a
   number selects. It says nothing about whether the runtime can then render the
   slot it selected -- a row-offset error in form_for() would leave that check
   green and every Arabic or Welsh string wrong.
   Markers, not prose: the assertion is about WHICH SLOT was read. */
void test_arabic_rule_reads_every_slot_including_zero_and_two(void) {
    loc_set_lang(LOC_LANG_AR);
    TEST_ASSERT_EQUAL_STRING("ar-zero 0", loc_e2e_every_category(0).s);
    TEST_ASSERT_EQUAL_STRING("ar-one 1", loc_e2e_every_category(1).s);
    TEST_ASSERT_EQUAL_STRING("ar-two 2", loc_e2e_every_category(2).s);
    TEST_ASSERT_EQUAL_STRING("ar-few 3", loc_e2e_every_category(3).s);
    TEST_ASSERT_EQUAL_STRING("ar-few 10", loc_e2e_every_category(10).s);
    TEST_ASSERT_EQUAL_STRING("ar-many 11", loc_e2e_every_category(11).s);
    TEST_ASSERT_EQUAL_STRING("ar-many 99", loc_e2e_every_category(99).s);
    TEST_ASSERT_EQUAL_STRING("ar-other 100", loc_e2e_every_category(100).s);
    /* Same key, a rule that uses neither slot -- proves the row is indexed by
       category and not by position-within-the-forms-this-language-declares. */
    loc_set_lang(LOC_LANG_RU);
    TEST_ASSERT_EQUAL_STRING("ru-one 1", loc_e2e_every_category(1).s);
    TEST_ASSERT_EQUAL_STRING("ru-many 11", loc_e2e_every_category(11).s);
}

void test_one_other_rule(void) {
    loc_set_lang(LOC_LANG_EN);
    TEST_ASSERT_EQUAL_STRING("1 step", steps(1));
    TEST_ASSERT_EQUAL_STRING("0 steps", steps(0));
    TEST_ASSERT_EQUAL_STRING("21 steps", steps(21));
}

void test_other_only_rule(void) {
    loc_set_lang(LOC_LANG_JA);
    TEST_ASSERT_EQUAL_STRING("1 歩", steps(1));
    TEST_ASSERT_EQUAL_STRING("7 歩", steps(7));
}

void test_polish_rule(void) {
    loc_set_lang(LOC_LANG_PL);
    TEST_ASSERT_EQUAL_STRING("1 krok", steps(1));
    TEST_ASSERT_EQUAL_STRING("2 kroki", steps(2));
    TEST_ASSERT_EQUAL_STRING("5 krokow", steps(5));
    TEST_ASSERT_EQUAL_STRING("22 kroki", steps(22));
    /* Polish differs from Russian at exactly 21: `many`, not `one`. */
    TEST_ASSERT_EQUAL_STRING("21 krokow", steps(21));
}

void test_czech_rule(void) {
    loc_set_lang(LOC_LANG_CS);
    TEST_ASSERT_EQUAL_STRING("1 krok", steps(1));
    TEST_ASSERT_EQUAL_STRING("4 kroky", steps(4));
    TEST_ASSERT_EQUAL_STRING("5 kroku", steps(5));
}

void test_french_rule_covers_zero_with_one(void) {
    loc_set_lang(LOC_LANG_FR);
    TEST_ASSERT_EQUAL_STRING("0 pas", steps(0));
    TEST_ASSERT_EQUAL_STRING("1 pas", steps(1));
    TEST_ASSERT_EQUAL_STRING("2 pas x", steps(2));
}

void test_float_plural_always_takes_other(void) {
    TEST_ASSERT_EQUAL_STRING("1.5 метра", loc_e2e_fractional(1.5F).s);
    TEST_ASSERT_EQUAL_STRING("1.0 метра", loc_e2e_fractional(1.0F).s);
    loc_set_lang(LOC_LANG_EN);
    TEST_ASSERT_EQUAL_STRING("1.0 metres", loc_e2e_fractional(1.0F).s);
}

/* --- fallback ----------------------------------------------------------- */

void test_missing_language_falls_back_to_the_authoring_one(void) {
    loc_set_lang(LOC_LANG_EN);
    TEST_ASSERT_EQUAL_STRING("ТОЛЬКО РУ", loc_e2e_hole().s);
    TEST_ASSERT_EQUAL_INT(1, loc_fallback_log_count());
    for (int i = 0; i < 60; ++i) {
        (void)loc_e2e_hole();
    }
    TEST_ASSERT_EQUAL_INT(1, loc_fallback_log_count()); /* once per key, not per frame */
}

/* --- nesting ------------------------------------------------------------ */

void test_a_localized_string_nests_inside_another(void) {
    loc_set_lang(LOC_LANG_EN);
    const LocStr inner = loc_by_key(LOC0_E2E_PLAIN);
    TEST_ASSERT_EQUAL_STRING("Requires CLOSE Lv 3", loc_e2e_nested_outer(inner, 3).s);
}

void test_nested_inner_string_survives_the_outer_allocation(void) {
    /* The inner value is a table pointer, so the arena bump for the outer
       string cannot disturb it. */
    const LocStr inner = loc_by_key(LOC0_E2E_PLAIN);
    const char *outer = loc_e2e_nested_outer(inner, 1).s;
    TEST_ASSERT_EQUAL_STRING("ЗАКРЫТЬ", inner.s);
    TEST_ASSERT_EQUAL_STRING("Нужно ЗАКРЫТЬ Ур 1", outer);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_generated_enums_match_the_corpus);
    RUN_TEST(test_init_starts_on_the_fallback_language);
    RUN_TEST(test_by_key_resolves_a_zero_argument_key);
    RUN_TEST(test_escaped_braces_in_a_zero_argument_key);
    RUN_TEST(test_escaped_braces_around_a_slot);
    RUN_TEST(test_percent_never_reaches_a_format_string);
    RUN_TEST(test_slots_are_indices_so_languages_may_reorder);
    RUN_TEST(test_widest_key_renders_every_slot);
    RUN_TEST(test_number_types_render_per_language);
    RUN_TEST(test_grouping_threshold_comes_from_the_language_block);
    RUN_TEST(test_loc_group_i64_follows_the_active_language);
    RUN_TEST(test_loc_group_i64_drops_separators_before_digits);
    RUN_TEST(test_int64_extremes_survive_grouping);
    RUN_TEST(test_every_plural_rule_matches_the_generator);
    RUN_TEST(test_every_rule_is_reachable_from_the_case_table);
    RUN_TEST(test_slavic_rule_selects_one_few_many);
    RUN_TEST(test_arabic_rule_reads_every_slot_including_zero_and_two);
    RUN_TEST(test_one_other_rule);
    RUN_TEST(test_other_only_rule);
    RUN_TEST(test_polish_rule);
    RUN_TEST(test_czech_rule);
    RUN_TEST(test_french_rule_covers_zero_with_one);
    RUN_TEST(test_float_plural_always_takes_other);
    RUN_TEST(test_missing_language_falls_back_to_the_authoring_one);
    RUN_TEST(test_a_localized_string_nests_inside_another);
    RUN_TEST(test_nested_inner_string_survives_the_outer_allocation);
    return UNITY_END();
}
