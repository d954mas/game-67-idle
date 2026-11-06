/* The rule of loc_font_packs against a fake engine: nothing here needs a real pack. */
#include "unity.h"

#include <string.h>

#include "features/localization/loc_font_packs.h"

static int s_lang;
static int s_fonts_created;
static bool s_ready;

int loc_lang_index(void) { return s_lang; }
void loc_set_lang_index(int lang) { s_lang = lang; }
const char *loc_lang_code(int lang) {
    static const char *const codes[] = {"en", "ru", "ja"};
    return codes[lang];
}
int loc_lang_from_code(const char *code) {
    for (int index = 0; index < 3; ++index) {
        if (strcmp(loc_lang_code(index), code) == 0) { return index; }
    }
    return -1;
}
nt_resource_t nt_resource_request(nt_hash64_t id, uint8_t type) {
    (void)id;
    (void)type;
    return (nt_resource_t){.id = 7U};
}
bool nt_resource_is_ready(nt_resource_t handle) { return handle.id == 7U && s_ready; }
nt_font_t nt_font_create(const nt_font_create_desc_t *desc) {
    (void)desc;
    ++s_fonts_created;
    return (nt_font_t){.id = 50U + (uint32_t)s_fonts_created};
}
void nt_font_add(nt_font_t font, nt_resource_t resource) {
    (void)font;
    (void)resource;
}
void nt_font_destroy(nt_font_t font) { (void)font; }

void setUp(void) {
    static const char *const languages[] = {"ja"};
    s_lang = 0;
    s_fonts_created = 0;
    s_ready = false;
    loc_font_packs_init(&(loc_font_packs_desc_t){.languages = languages, .count = 1, .font_resource = "game/font_cjk",
        .fallback = "en", .common_font = {.id = 1U}});
}
void tearDown(void) { loc_font_packs_shutdown(); }

static void test_a_language_outside_the_pack_shows_at_once(void) {
    TEST_ASSERT_TRUE(loc_font_packs_set_language("ru"));
    loc_font_packs_step();
    TEST_ASSERT_EQUAL_INT(1, s_lang);
    TEST_ASSERT_EQUAL_UINT32(1U, loc_font_packs_font().id);
}

static void test_the_fallback_shows_until_the_face_lands_and_the_choice_is_kept(void) {
    s_lang = 1;
    TEST_ASSERT_TRUE(loc_font_packs_set_language("ja"));
    TEST_ASSERT_EQUAL_STRING("ja", loc_font_packs_language());
    TEST_ASSERT_EQUAL_INT(0, s_lang);
    loc_font_packs_step();
    TEST_ASSERT_EQUAL_INT(0, s_lang);
    s_ready = true;
    loc_font_packs_step();
    TEST_ASSERT_EQUAL_INT(2, s_lang);
    TEST_ASSERT_NOT_EQUAL_UINT32(1U, loc_font_packs_font().id);
    TEST_ASSERT_EQUAL_INT(1, s_fonts_created);
}

static void test_once_the_face_is_in_a_switch_is_immediate_both_ways(void) {
    s_ready = true;
    TEST_ASSERT_TRUE(loc_font_packs_set_language("ja"));
    TEST_ASSERT_EQUAL_INT(2, s_lang);
    TEST_ASSERT_TRUE(loc_font_packs_set_language("ru"));
    TEST_ASSERT_EQUAL_INT(1, s_lang);
    TEST_ASSERT_EQUAL_UINT32(1U, loc_font_packs_font().id);
    TEST_ASSERT_TRUE(loc_font_packs_set_language("ja"));
    TEST_ASSERT_EQUAL_INT(2, s_lang);
    TEST_ASSERT_EQUAL_INT(1, s_fonts_created);
}

static void test_a_new_choice_during_the_wait_wins(void) {
    (void)loc_font_packs_set_language("ja");
    (void)loc_font_packs_set_language("ru");
    s_ready = true;
    loc_font_packs_step();
    TEST_ASSERT_EQUAL_INT(1, s_lang);
    TEST_ASSERT_EQUAL_UINT32(1U, loc_font_packs_font().id);
}

static void test_an_unknown_code_is_refused(void) {
    TEST_ASSERT_FALSE(loc_font_packs_set_language("xx"));
    TEST_ASSERT_EQUAL_STRING("en", loc_font_packs_language());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_a_language_outside_the_pack_shows_at_once);
    RUN_TEST(test_the_fallback_shows_until_the_face_lands_and_the_choice_is_kept);
    RUN_TEST(test_once_the_face_is_in_a_switch_is_immediate_both_ways);
    RUN_TEST(test_a_new_choice_during_the_wait_wins);
    RUN_TEST(test_an_unknown_code_is_refused);
    return UNITY_END();
}
