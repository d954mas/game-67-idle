// The account calls on canned answers: each goes to its route under the
// session's bearer with its fields; a login that lands stores the login's
// session, a sign-up notes the login, a logout forgets the session; a refusal
// keeps the session and names why; nothing goes without a metaserver, while a
// call is out, or without the session a call acts for.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "online_room/meta_account.h"

void setUp(void) {}
void tearDown(void) {}

#define META "http://meta.test"

static char g_url[256];
static char g_bearer[96];
static char g_body[256];
static uint32_t g_calls;
static char g_stored[512];

static uint32_t fake_begin(void *user, const char *url, const char *bearer, const char *json, uint32_t timeout_ms) {
    (void)user;
    (void)timeout_ms;
    snprintf(g_url, sizeof g_url, "%s", url);
    snprintf(g_bearer, sizeof g_bearer, "%s", bearer != NULL ? bearer : "");
    snprintf(g_body, sizeof g_body, "%s", json != NULL ? json : "");
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
    (void)slot;
    if (g_stored[0] == '\0') { return NULL; }
    char *copy = malloc(strlen(g_stored) + 1U);
    strcpy(copy, g_stored);
    return copy;
}

static void store_save(void *user, const char *slot, const char *text) {
    (void)user;
    (void)slot;
    snprintf(g_stored, sizeof g_stored, "%s", text);
}

static const online_meta_entry_http k_http = {.begin = fake_begin, .poll = fake_poll, .cancel = fake_cancel};
static const online_meta_entry_storage k_storage = {.load = store_load, .store = store_save};

static online_meta_entry g_entry;
static online_meta_account g_account;

static void open_guest(void) {
    g_calls = 0U;
    snprintf(g_stored, sizeof g_stored, "{\"player_id\":9,\"secret\":\"guest\",\"account\":false,\"meta\":\"" META "\"}");
    online_meta_entry_init(&g_entry, META, false, &k_http, &k_storage, NULL);
    online_meta_account_init(&g_account, &g_entry);
}

static void answer(uint16_t status, const char *json) {
    online_meta_account_feed(&g_account, status, (const uint8_t *)json, strlen(json));
}

static void test_a_login_that_lands_stores_the_logins_session(void) {
    open_guest();
    TEST_ASSERT_TRUE(online_meta_account_login(&g_account, "nick", "correct horse"));
    TEST_ASSERT_EQUAL_STRING(META "/api/login", g_url);
    TEST_ASSERT_EQUAL_STRING("guest", g_bearer);
    TEST_ASSERT_EQUAL_STRING("{\"login\":\"nick\",\"password\":\"correct horse\"}", g_body);
    // One call at a time.
    TEST_ASSERT_FALSE(online_meta_account_logout(&g_account));
    answer(200U, "{\"player_id\":42,\"secret\":\"theirs\",\"login\":\"nick\"}");
    TEST_ASSERT_EQUAL_INT(ONLINE_ACCOUNT_DONE, g_account.state);
    TEST_ASSERT_TRUE(g_entry.player_id == 42U && g_entry.account);
    TEST_ASSERT_EQUAL_STRING("nick", g_entry.login);
    TEST_ASSERT_NOT_NULL(strstr(g_stored, "\"secret\":\"theirs\""));
}

static void test_sign_up_password_and_logout(void) {
    open_guest();
    TEST_ASSERT_TRUE(online_meta_account_register(&g_account, "nick", "correct horse"));
    TEST_ASSERT_EQUAL_STRING(META "/api/register", g_url);
    answer(200U, "{\"login\":\"nick\"}");
    TEST_ASSERT_EQUAL_STRING("nick", g_entry.login);
    TEST_ASSERT_NOT_NULL(strstr(g_stored, "\"account\":true"));
    TEST_ASSERT_TRUE(online_meta_account_change_password(&g_account, "correct horse", "battery staple"));
    TEST_ASSERT_EQUAL_STRING(META "/api/password", g_url);
    TEST_ASSERT_EQUAL_STRING("{\"old\":\"correct horse\",\"new\":\"battery staple\"}", g_body);
    answer(403U, "{\"error\":\"wrong\"}");
    TEST_ASSERT_EQUAL_INT(ONLINE_ACCOUNT_FAILED, g_account.state);
    TEST_ASSERT_EQUAL_STRING("wrong", g_account.error);
    TEST_ASSERT_TRUE(g_entry.has_session);
    TEST_ASSERT_TRUE(online_meta_account_logout(&g_account));
    TEST_ASSERT_EQUAL_STRING(META "/api/logout", g_url);
    TEST_ASSERT_EQUAL_STRING("", g_body);
    answer(204U, "");
    TEST_ASSERT_EQUAL_INT(ONLINE_ACCOUNT_DONE, g_account.state);
    TEST_ASSERT_FALSE(g_entry.has_session);
    TEST_ASSERT_EQUAL_STRING("{}", g_stored);
}

// A refusal speaks for the session the call went out with: once the entry
// holds another one, it is left alone.
static void test_a_refusal_forgets_only_the_session_it_was_sent_for(void) {
    open_guest();
    TEST_ASSERT_TRUE(online_meta_account_register(&g_account, "nick", "correct horse"));
    TEST_ASSERT_TRUE(online_meta_entry_set_identity(&g_entry, 10U, "fresh", NULL));
    answer(401U, "{\"error\":\"session\"}");
    TEST_ASSERT_EQUAL_INT(ONLINE_ACCOUNT_FAILED, g_account.state);
    TEST_ASSERT_FALSE(g_account.forgot);
    TEST_ASSERT_TRUE(g_entry.has_session && g_entry.player_id == 10U);
    TEST_ASSERT_FALSE(g_entry.signed_out);

    // The same session refused: forgotten, but a guest was never signed in.
    TEST_ASSERT_TRUE(online_meta_account_register(&g_account, "nick", "correct horse"));
    answer(401U, "{\"error\":\"session\"}");
    TEST_ASSERT_TRUE(g_account.forgot);
    TEST_ASSERT_FALSE(g_entry.has_session);
    TEST_ASSERT_FALSE(g_entry.signed_out);

    // An account's session refused: signed out.
    TEST_ASSERT_TRUE(online_meta_entry_set_identity(&g_entry, 42U, "theirs", "nick"));
    TEST_ASSERT_TRUE(online_meta_account_change_password(&g_account, "correct horse", "battery staple"));
    answer(401U, "{\"error\":\"session\"}");
    TEST_ASSERT_TRUE(g_account.forgot);
    TEST_ASSERT_TRUE(g_entry.signed_out);

    // A logout that lands after the entry moved on leaves the new session.
    TEST_ASSERT_TRUE(online_meta_entry_set_identity(&g_entry, 42U, "theirs", "nick"));
    TEST_ASSERT_TRUE(online_meta_account_logout(&g_account));
    TEST_ASSERT_TRUE(online_meta_entry_set_identity(&g_entry, 11U, "other", NULL));
    answer(204U, "");
    TEST_ASSERT_FALSE(g_account.forgot);
    TEST_ASSERT_TRUE(g_entry.has_session && g_entry.player_id == 11U);
}

static void test_nothing_goes_without_what_a_call_needs(void) {
    open_guest();
    online_meta_entry_forget(&g_entry);
    // Sign-up, password and logout act for the session; a login makes one.
    TEST_ASSERT_FALSE(online_meta_account_register(&g_account, "nick", "correct horse"));
    TEST_ASSERT_FALSE(online_meta_account_logout(&g_account));
    TEST_ASSERT_TRUE(online_meta_account_login(&g_account, "nick", "correct horse"));
    TEST_ASSERT_EQUAL_STRING("", g_bearer);
    online_meta_entry none;
    online_meta_entry_init(&none, NULL, false, &k_http, &k_storage, NULL);
    online_meta_account offline;
    online_meta_account_init(&offline, &none);
    TEST_ASSERT_FALSE(online_meta_account_login(&offline, "nick", "correct horse"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_a_login_that_lands_stores_the_logins_session);
    RUN_TEST(test_sign_up_password_and_logout);
    RUN_TEST(test_a_refusal_forgets_only_the_session_it_was_sent_for);
    RUN_TEST(test_nothing_goes_without_what_a_call_needs);
    return UNITY_END();
}
