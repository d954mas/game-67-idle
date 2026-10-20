// The client's entry through the metaserver on canned answers: a first run
// is a new guest, stored, then a v2 enter whose ticket and room come back; a
// stored session enters at once; only the metaserver's unknown_session starts
// over as a guest, with the old record kept aside until the guest is in, and
// any other 401 keeps the session; a busy or refused enter, or a ticket of
// the wrong size, fails the try; a metaserver the page URL chose keeps its
// session out of the storage, a stored session goes to no other metaserver,
// and another metaserver's session has a slot of its own.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "online_room/meta_entry.h"

void setUp(void) {}
void tearDown(void) {}

#define META "http://meta.test"
// A 34-byte ticket in base64url.
#define TICKET "AlQIBwYFBAMCAQcAAAAe8VNlKNe4uNP6lzyQsZzidfjIXA"

static char g_urls[8][256];
static char g_bearers[8][96];
static char g_json[8][64];
static uint32_t g_calls;
static char *g_stored;          // the session slot
static char g_previous[512];    // the slot a start over keeps the old record in

static uint32_t fake_begin(void *user, const char *url, const char *bearer, const char *json, uint32_t timeout_ms) {
    (void)user;
    (void)timeout_ms;
    TEST_ASSERT_TRUE(g_calls < 8U);
    snprintf(g_urls[g_calls], sizeof g_urls[0], "%s", url);
    snprintf(g_bearers[g_calls], sizeof g_bearers[0], "%s", bearer != NULL ? bearer : "");
    snprintf(g_json[g_calls], sizeof g_json[0], "%s", json != NULL ? json : "");
    g_calls += 1U;
    return g_calls;
}

static bool fake_poll(void *user, uint32_t handle, uint16_t *status, uint8_t **body, size_t *size) {
    (void)user;
    (void)handle;
    (void)status;
    (void)body;
    (void)size;
    return false;
}

static void fake_cancel(void *user, uint32_t handle) {
    (void)user;
    (void)handle;
}

static char *store_load(void *user, const char *slot) {
    (void)user;
    TEST_ASSERT_EQUAL_STRING(ONLINE_META_ENTRY_SLOT, slot);
    if (g_stored == NULL) { return NULL; }
    char *copy = malloc(strlen(g_stored) + 1U);
    strcpy(copy, g_stored);
    return copy;
}

static void store_save(void *user, const char *slot, const char *text) {
    (void)user;
    if (strcmp(slot, ONLINE_META_ENTRY_SLOT ONLINE_META_ENTRY_PREVIOUS_SUFFIX) == 0) {
        snprintf(g_previous, sizeof g_previous, "%s", text);
        return;
    }
    TEST_ASSERT_EQUAL_STRING(ONLINE_META_ENTRY_SLOT, slot);
    free(g_stored);
    g_stored = malloc(strlen(text) + 1U);
    strcpy(g_stored, text);
}

static const online_meta_entry_http k_http = {.begin = fake_begin, .poll = fake_poll, .cancel = fake_cancel};
static const online_meta_entry_storage k_storage = {.load = store_load, .store = store_save};

static void reset(const char *stored) {
    g_calls = 0U;
    free(g_stored);
    g_stored = NULL;
    g_previous[0] = '\0';
    if (stored != NULL) { store_save(NULL, ONLINE_META_ENTRY_SLOT, stored); }
}

static void answer(online_meta_entry *entry, uint16_t status, const char *json) {
    online_meta_entry_feed(entry, status, (const uint8_t *)json, strlen(json));
}

static const char *k_enter = "{\"room_id\":7,\"url\":\"ws://127.0.0.1:7101/\",\"ticket\":\"" TICKET "\",\"expires_at\":1700000030,\"login\":\"\"}";

static void test_a_first_run_is_a_stored_guest_then_a_ticket(void) {
    reset(NULL);
    online_meta_entry entry;
    online_meta_entry_init(&entry, META "/", false, &k_http, &k_storage, NULL);
    TEST_ASSERT_TRUE(online_meta_entry_enabled(&entry));
    online_meta_entry_begin(&entry);
    TEST_ASSERT_EQUAL_STRING(META "/api/guest", g_urls[0]);
    TEST_ASSERT_EQUAL_STRING("", g_bearers[0]);
    answer(&entry, 201U, "{\"player_id\":4242,\"secret\":\"s3cr-t_x\"}");
    TEST_ASSERT_NOT_NULL(g_stored);
    TEST_ASSERT_NOT_NULL(strstr(g_stored, "\"player_id\":4242"));
    TEST_ASSERT_NOT_NULL(strstr(g_stored, "\"meta\":\"" META "\""));
    TEST_ASSERT_EQUAL_STRING(META "/api/enter", g_urls[1]);
    TEST_ASSERT_EQUAL_STRING("s3cr-t_x", g_bearers[1]);
    answer(&entry, 200U, k_enter);
    TEST_ASSERT_EQUAL_INT(ONLINE_META_ENTRY_READY, entry.state);
    TEST_ASSERT_EQUAL_STRING("ws://127.0.0.1:7101/", entry.room_url);
    TEST_ASSERT_EQUAL_UINT32(7U, entry.room_id);
    TEST_ASSERT_EQUAL_size_t(ONLINE_META_ENTRY_TICKET_SIZE, entry.ticket_size);
    TEST_ASSERT_EQUAL_UINT8(2U, entry.ticket[0]);
    TEST_ASSERT_EQUAL_UINT8(0x54U, entry.ticket[1]);
}

static void test_page_entry_intent_survives_guest_then_enter_but_retry_omits_it(void) {
    reset(NULL);
    online_meta_entry entry;
    online_meta_entry_init(&entry, META, false, &k_http, &k_storage, NULL);
    entry.page_entry = true;
    online_meta_entry_begin(&entry);
    TEST_ASSERT_EQUAL_STRING("", g_json[0]);
    answer(&entry, 201U, "{\"player_id\":4242,\"secret\":\"s3cr-t_x\"}");
    TEST_ASSERT_EQUAL_STRING("{\"page_entry\":true}", g_json[1]);
    online_meta_entry_cancel(&entry);
    entry.page_entry = false;
    online_meta_entry_begin(&entry);
    TEST_ASSERT_EQUAL_STRING("", g_json[2]);
}

static void test_a_stored_session_enters_and_an_unknown_one_starts_over(void) {
    reset("{\"player_id\":9,\"secret\":\"old\",\"account\":false,\"meta\":\"" META "\"}");
    online_meta_entry entry;
    online_meta_entry_init(&entry, META, false, &k_http, &k_storage, NULL);
    online_meta_entry_begin(&entry);
    TEST_ASSERT_EQUAL_STRING(META "/api/enter", g_urls[0]);
    TEST_ASSERT_EQUAL_STRING("old", g_bearers[0]);
    answer(&entry, 401U, "{\"error\":\"unknown_session\"}");
    TEST_ASSERT_EQUAL_STRING(META "/api/guest", g_urls[1]);
    // The forgotten session waits aside until the new guest has entered.
    TEST_ASSERT_NOT_NULL(strstr(g_previous, "\"secret\":\"old\""));
    answer(&entry, 201U, "{\"player_id\":10,\"secret\":\"new\"}");
    TEST_ASSERT_NOT_NULL(strstr(g_stored, "\"secret\":\"new\""));
    TEST_ASSERT_NOT_NULL(strstr(g_previous, "\"secret\":\"old\""));
    answer(&entry, 200U, k_enter);
    TEST_ASSERT_EQUAL_STRING("{}", g_previous);
    TEST_ASSERT_EQUAL_INT(ONLINE_META_ENTRY_READY, entry.state);
    TEST_ASSERT_TRUE(entry.player_id == 10U);
    // Every try buys its own ticket.
    online_meta_entry_begin(&entry);
    TEST_ASSERT_EQUAL_UINT32(4U, g_calls);
    TEST_ASSERT_EQUAL_STRING(META "/api/enter", g_urls[3]);
}

// A 401 that is not the metaserver saying it forgot the session (a proxy's,
// a broken deploy's) fails the try and keeps the session as stored.
static void test_a_stray_401_keeps_the_session(void) {
    static const char *const k_bodies[] = {"{\"error\":\"session\"}", "<html>401</html>", ""};
    for (uint32_t index = 0U; index < 3U; ++index) {
        reset("{\"player_id\":9,\"secret\":\"old\",\"account\":true,\"meta\":\"" META "\"}");
        online_meta_entry entry;
        online_meta_entry_init(&entry, META, false, &k_http, &k_storage, NULL);
        online_meta_entry_begin(&entry);
        answer(&entry, 401U, k_bodies[index]);
        TEST_ASSERT_EQUAL_STRING("", g_previous);
        TEST_ASSERT_EQUAL_INT(ONLINE_META_ENTRY_FAILED, entry.state);
        TEST_ASSERT_EQUAL_UINT32(1U, g_calls);
        TEST_ASSERT_NOT_NULL(strstr(g_stored, "\"secret\":\"old\""));
        TEST_ASSERT_TRUE(entry.has_session && entry.player_id == 9U);
    }
}

static void test_a_refused_enter_or_a_bad_ticket_fails_the_try(void) {
    static const struct {
        uint16_t status;
        const char *body;
    } k_cases[] = {
        {503U, "{\"error\":\"no room has a free seat\"}"},
        {429U, "{\"error\":\"slow down\"}"},
        {200U, "{\"room_id\":7,\"url\":\"ws://127.0.0.1:7101/\",\"ticket\":\"AAAA\",\"expires_at\":1}"},
        {200U, "{\"room_id\":7,\"url\":\"http://127.0.0.1:7101/\",\"ticket\":\"" TICKET "\",\"expires_at\":1}"},
        {200U, "not json"},
        {0U, ""},
    };
    for (uint32_t index = 0U; index < sizeof k_cases / sizeof k_cases[0]; ++index) {
        reset("{\"player_id\":9,\"secret\":\"old\",\"account\":false,\"meta\":\"" META "\"}");
        online_meta_entry entry;
        online_meta_entry_init(&entry, META, false, &k_http, &k_storage, NULL);
        online_meta_entry_begin(&entry);
        answer(&entry, k_cases[index].status, k_cases[index].body);
        TEST_ASSERT_EQUAL_INT(ONLINE_META_ENTRY_FAILED, entry.state);
        TEST_ASSERT_EQUAL_size_t(0U, entry.ticket_size);
    }
}

static void test_a_foreign_metaserver_gets_no_stored_session_and_stores_none(void) {
    reset("{\"player_id\":9,\"secret\":\"old\",\"account\":false,\"meta\":\"" META "\"}");
    online_meta_entry entry;
    online_meta_entry_init(&entry, "http://elsewhere.test", true, &k_http, &k_storage, NULL);
    online_meta_entry_begin(&entry);
    TEST_ASSERT_EQUAL_STRING("http://elsewhere.test/api/guest", g_urls[0]);
    answer(&entry, 201U, "{\"player_id\":11,\"secret\":\"theirs\"}");
    TEST_ASSERT_NOT_NULL(strstr(g_stored, "\"secret\":\"old\""));
}

// The build's own metaserver keeps the shared slot; any other, a local one
// included, gets a slot named by its url, so a developer's run never
// overwrites the session a device keeps with the real one.
static void test_another_metaserver_has_a_slot_of_its_own(void) {
    char slot[ONLINE_META_ENTRY_SLOT_MAX];
    online_meta_entry_slot("https://game.example/", "https://game.example", slot);
    TEST_ASSERT_EQUAL_STRING(ONLINE_META_ENTRY_SLOT, slot);
    char local[ONLINE_META_ENTRY_SLOT_MAX];
    online_meta_entry_slot("http://127.0.0.1:8080", "https://game.example", local);
    TEST_ASSERT_TRUE(strncmp(local, ONLINE_META_ENTRY_SLOT "_", strlen(ONLINE_META_ENTRY_SLOT) + 1U) == 0);
    char other[ONLINE_META_ENTRY_SLOT_MAX];
    online_meta_entry_slot("http://127.0.0.1:8081", "https://game.example", other);
    TEST_ASSERT_TRUE(strcmp(local, other) != 0);
    for (const char *at = local; *at != '\0'; ++at) { TEST_ASSERT_TRUE((*at >= 'a' && *at <= 'z') || (*at >= '0' && *at <= '9') || *at == '_'); }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_a_first_run_is_a_stored_guest_then_a_ticket);
    RUN_TEST(test_page_entry_intent_survives_guest_then_enter_but_retry_omits_it);
    RUN_TEST(test_a_stored_session_enters_and_an_unknown_one_starts_over);
    RUN_TEST(test_a_stray_401_keeps_the_session);
    RUN_TEST(test_a_refused_enter_or_a_bad_ticket_fails_the_try);
    RUN_TEST(test_a_foreign_metaserver_gets_no_stored_session_and_stores_none);
    RUN_TEST(test_another_metaserver_has_a_slot_of_its_own);
    return UNITY_END();
}
