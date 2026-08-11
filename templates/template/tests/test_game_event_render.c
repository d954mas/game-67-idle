/* Unity ctest for the E3 descriptor-driven JSON renderer (game_event_render). Native, no
   devapi (the renderer is pure). Emits through the COMMITTED golden mini events, renders by
   descriptor, and asserts by re-parsing the JSON.

   HIGH-1: hash/type expectations are LABEL-AGNOSTIC. nt_hash inherits the preset's
   NT_HASH_LABELS: in devapi-debug labels are ON and nt_hash64_str auto-registers the name
   (type/kind come back as the NAME); in native-debug they are OFF (hex). Expectations are
   computed at runtime via nt_hash64_label(h) ? name : hex -- never hardcoded. */
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* clang-format off */
#include "cJSON.h"
#include "game_event_render.h"
#include "game_events.h"
#include "hash/nt_hash.h"
#include "mini_state_events.gen.h"
#include "unity.h"
/* clang-format on */

void setUp(void) { game_events_init(); }
void tearDown(void) { game_events_shutdown(); }

/* Runtime label-or-hex expectation for a type/hash value (HIGH-1). */
static void expected_label(nt_hash64_t h, char *buf, size_t cap) {
    const char *l = nt_hash64_label(h);
    if (l) {
        (void)snprintf(buf, cap, "%s", l);
    } else {
        (void)snprintf(buf, cap, "0x%016" PRIx64, h.value);
    }
}

/* 1. rich typed render: every field type + label-agnostic type/hash. */
static void test_rich_typed_render(void) {
    const uint8_t blob[3] = {1, 2, 3};
    const nt_hash64_t epic = nt_hash64_str("Epic");
    const void *p = mini_emit_cell_spawned(42, 3.5, epic, true, "hello", blob, 3);
    TEST_ASSERT_NOT_NULL(p);

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(1, n);

    char buf[GAME_EVENT_RENDER_LINE_MAX];
    const int len = game_event_render(&log[0], &mini_ev_cell_spawned_desc, buf, (int)sizeof buf);
    TEST_ASSERT_TRUE(len > 0 && len < (int)sizeof buf);

    cJSON *root = cJSON_Parse(buf);
    TEST_ASSERT_NOT_NULL(root);

    /* Registered event: type is desc->name unconditionally (never the hash label/hex). */
    const cJSON *type = cJSON_GetObjectItem(root, "type");
    TEST_ASSERT_TRUE(cJSON_IsString(type));
    TEST_ASSERT_EQUAL_STRING(mini_ev_cell_spawned_desc.name, type->valuestring);

    const cJSON *total = cJSON_GetObjectItem(root, "total"); /* i64 -> STRING (MED-3) */
    TEST_ASSERT_TRUE(cJSON_IsString(total));
    TEST_ASSERT_EQUAL_STRING("42", total->valuestring);

    const cJSON *rate = cJSON_GetObjectItem(root, "rate"); /* float -> number */
    TEST_ASSERT_TRUE(cJSON_IsNumber(rate));
    TEST_ASSERT_TRUE(fabs(rate->valuedouble - 3.5) < 1e-9);

    TEST_ASSERT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(root, "urgent")));

    const cJSON *label = cJSON_GetObjectItem(root, "label");
    TEST_ASSERT_TRUE(cJSON_IsString(label));
    TEST_ASSERT_EQUAL_STRING("hello", label->valuestring);

    char want_kind[64];
    expected_label(epic, want_kind, sizeof want_kind);
    const cJSON *kind = cJSON_GetObjectItem(root, "kind");
    TEST_ASSERT_TRUE(cJSON_IsString(kind));
    TEST_ASSERT_EQUAL_STRING(want_kind, kind->valuestring);

    const cJSON *b = cJSON_GetObjectItem(root, "blob"); /* { size:3, hex:"010203" } */
    TEST_ASSERT_TRUE(cJSON_IsObject(b));
    TEST_ASSERT_EQUAL_INT(3, (int)cJSON_GetObjectItem(b, "size")->valuedouble);
    TEST_ASSERT_EQUAL_STRING("010203", cJSON_GetObjectItem(b, "hex")->valuestring);

    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItem(root, "seq")));
    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItem(root, "tick")));
    cJSON_Delete(root);
}

/* 2. scalar-only: INT -> number, label-agnostic type. */
static void test_scalar_only(void) {
    (void)mini_emit_ticked(7);
    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(1, n);

    char buf[GAME_EVENT_RENDER_LINE_MAX];
    (void)game_event_render(&log[0], &mini_ev_ticked_desc, buf, (int)sizeof buf);
    cJSON *root = cJSON_Parse(buf);
    TEST_ASSERT_NOT_NULL(root);

    const cJSON *count = cJSON_GetObjectItem(root, "count");
    TEST_ASSERT_TRUE(cJSON_IsNumber(count));
    TEST_ASSERT_EQUAL_INT(7, (int)count->valuedouble);

    /* Registered event: type is desc->name unconditionally. */
    TEST_ASSERT_EQUAL_STRING(mini_ev_ticked_desc.name, cJSON_GetObjectItem(root, "type")->valuestring);
    cJSON_Delete(root);
}

/* 3. unknown (desc==NULL): { unknown:true, size, hex, type(label|hex) }. */
static void test_unknown_render(void) {
    const uint8_t payload[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    const nt_hash64_t raw = nt_hash64_str("raw.x");
    const void *p = game_event_emit(raw, payload, (uint32_t)sizeof payload, 1);
    TEST_ASSERT_NOT_NULL(p);

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(1, n);

    char buf[GAME_EVENT_RENDER_LINE_MAX];
    (void)game_event_render(&log[0], NULL, buf, (int)sizeof buf);
    cJSON *root = cJSON_Parse(buf);
    TEST_ASSERT_NOT_NULL(root);

    TEST_ASSERT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(root, "unknown")));
    TEST_ASSERT_EQUAL_INT((int)log[0].size, (int)cJSON_GetObjectItem(root, "size")->valuedouble);
    const cJSON *hex = cJSON_GetObjectItem(root, "hex");
    TEST_ASSERT_TRUE(cJSON_IsString(hex));
    TEST_ASSERT_EQUAL_STRING("deadbeef", hex->valuestring);

    char want[64];
    expected_label(raw, want, sizeof want);
    TEST_ASSERT_EQUAL_STRING(want, cJSON_GetObjectItem(root, "type")->valuestring);
    cJSON_Delete(root);
}

/* 4. a field far longer than any other still renders in full: the line budget is what
   makes that true, and it is the same budget every consumer must pass in. */
static void test_long_field_still_renders_whole(void) {
    char big[600];
    memset(big, 'a', sizeof big - 1u);
    big[sizeof big - 1u] = '\0';
    (void)mini_emit_cell_spawned(1, 1.0, nt_hash64_str("Rare"), false, big, NULL, 0);

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(1, n);

    /* The budget is what makes a long field renderable, so the line arrives whole. */
    char buf[GAME_EVENT_RENDER_LINE_MAX];
    (void)game_event_render(&log[0], &mini_ev_cell_spawned_desc, buf, (int)sizeof buf);
    cJSON *root = cJSON_Parse(buf);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NULL(cJSON_GetObjectItem(root, "truncated"));
    TEST_ASSERT_EQUAL_STRING(big, cJSON_GetObjectItem(root, "label")->valuestring);
    cJSON_Delete(root);
}

/* 6. bounds/robustness: a descriptor claiming fields past e->size (cell_spawned desc over a
   smaller ticked event) must not read out of bounds -- still valid JSON. */
static void test_bounds_robustness(void) {
    (void)mini_emit_ticked(9);
    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(1, n);

    char buf[GAME_EVENT_RENDER_LINE_MAX];
    (void)game_event_render(&log[0], &mini_ev_cell_spawned_desc, buf, (int)sizeof buf);
    cJSON *root = cJSON_Parse(buf);
    TEST_ASSERT_NOT_NULL(root); /* out-of-range fields skipped, no over-read */
    cJSON_Delete(root);
}

/* 7. repeated section: one JSON array of objects, in emit order, alongside the scalars. */
static void test_repeated_render(void) {
    const MiniEvPaidCostIn cost[2] = {
        {"tmpl.coin", 50, 120},
        {"tmpl.wood", 3, 8},
    };
    (void)mini_emit_paid(2, "buy", cost, 2);

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(1, n);

    char buf[GAME_EVENT_RENDER_LINE_MAX];
    const int len = game_event_render(&log[0], &mini_ev_paid_desc, buf, (int)sizeof buf);
    TEST_ASSERT_TRUE(len > 0 && len < (int)sizeof buf);

    cJSON *root = cJSON_Parse(buf);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_STRING("buy", cJSON_GetObjectItem(root, "reason")->valuestring);

    const cJSON *arr = cJSON_GetObjectItem(root, "cost");
    TEST_ASSERT_TRUE(cJSON_IsArray(arr));
    TEST_ASSERT_EQUAL_INT(2, cJSON_GetArraySize(arr));

    const cJSON *first = cJSON_GetArrayItem(arr, 0);
    TEST_ASSERT_EQUAL_STRING("tmpl.coin", cJSON_GetObjectItem(first, "def_id")->valuestring);
    TEST_ASSERT_EQUAL_STRING("50", cJSON_GetObjectItem(first, "amount")->valuestring); /* i64 renders as string */
    TEST_ASSERT_EQUAL_STRING("120", cJSON_GetObjectItem(first, "before")->valuestring);

    const cJSON *second = cJSON_GetArrayItem(arr, 1);
    TEST_ASSERT_EQUAL_STRING("tmpl.wood", cJSON_GetObjectItem(second, "def_id")->valuestring);
    TEST_ASSERT_EQUAL_STRING("3", cJSON_GetObjectItem(second, "amount")->valuestring);
    cJSON_Delete(root);
}

/* 8. an empty list renders as an empty array, not a missing key. */
static void test_repeated_render_empty(void) {
    (void)mini_emit_paid(0, "free", NULL, 0);

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(1, n);

    char buf[GAME_EVENT_RENDER_LINE_MAX];
    (void)game_event_render(&log[0], &mini_ev_paid_desc, buf, (int)sizeof buf);
    cJSON *root = cJSON_Parse(buf);
    TEST_ASSERT_NOT_NULL(root);

    const cJSON *arr = cJSON_GetObjectItem(root, "cost");
    TEST_ASSERT_TRUE(cJSON_IsArray(arr));
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(arr));
    cJSON_Delete(root);
}

/* 9. a repeated descriptor over a SMALLER foreign payload: the count read from those bytes
   is arbitrary, so the walk must clamp to whole records that fit and never over-read. */
static void test_repeated_bounds_robustness(void) {
    (void)mini_emit_ticked(9);
    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(1, n);

    char buf[GAME_EVENT_RENDER_LINE_MAX];
    (void)game_event_render(&log[0], &mini_ev_paid_desc, buf, (int)sizeof buf);
    cJSON *root = cJSON_Parse(buf);
    TEST_ASSERT_NOT_NULL(root);
    cJSON_Delete(root);
}

/* A payload the budget cannot render -- escaping alone can sextuple it -- must not take
   the process down with it. The line degrades to one that still names the event. */
static void test_a_rendering_too_long_for_the_budget_degrades_to_a_named_line(void) {
    char big[GAME_EVENT_EMIT_MAX - 64u];
    memset(big, 1, sizeof big - 1u); /* every byte escapes to six characters */
    big[sizeof big - 1u] = 0;
    (void)mini_emit_cell_spawned(1, 1.0, nt_hash64_str("Rare"), false, big, NULL, 0);

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(1, n);

    char buf[GAME_EVENT_RENDER_LINE_MAX];
    const int len = game_event_render(&log[0], &mini_ev_cell_spawned_desc, buf, (int)sizeof buf);
    TEST_ASSERT_TRUE(len > 0 && len < (int)sizeof buf);
    cJSON *root = cJSON_Parse(buf);
    TEST_ASSERT_NOT_NULL_MESSAGE(root, buf); /* still parseable */
    TEST_ASSERT_TRUE(cJSON_IsTrue(cJSON_GetObjectItem(root, "truncated")));
    TEST_ASSERT_EQUAL_STRING("mini.cell_spawned", cJSON_GetObjectItem(root, "type")->valuestring);
    cJSON_Delete(root);
}

int main(void) {
    nt_hash_init(&(nt_hash_desc_t){0}); /* once: type-hash accessors + labels need hash init */

    UNITY_BEGIN();
    RUN_TEST(test_rich_typed_render);
    RUN_TEST(test_scalar_only);
    RUN_TEST(test_unknown_render);
    RUN_TEST(test_long_field_still_renders_whole);
    RUN_TEST(test_a_rendering_too_long_for_the_budget_degrades_to_a_named_line);
    RUN_TEST(test_bounds_robustness);
    RUN_TEST(test_repeated_render);
    RUN_TEST(test_repeated_render_empty);
    RUN_TEST(test_repeated_bounds_robustness);
    const int result = UNITY_END();

    nt_hash_shutdown();
    return result;
}
