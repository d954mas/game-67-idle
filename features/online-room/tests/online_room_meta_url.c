// The metaserver url the room sends its bearer to: https:// always; http://
// only to this machine or the compose service, the host matched whole after
// the last @ of the authority, which ends at the first of / ? # \; anything
// else only with --meta-insecure. The logged url drops its user:password@.
#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "online_room/meta.h"

void setUp(void) {}
void tearDown(void) {}

static void test_https_and_the_local_hosts_are_allowed(void) {
    static const char *const k_allowed[] = {
        "https://game.example",
        "https://game.example:8443/base",
        "http://127.0.0.1:8080",
        "http://localhost",
        "http://[::1]:8080/",
        "http://meta:8080",
        "http://user:pass@127.0.0.1:8080",
    };
    for (size_t index = 0U; index < sizeof k_allowed / sizeof k_allowed[0]; ++index) {
        TEST_ASSERT_TRUE_MESSAGE(online_meta_url_allowed(k_allowed[index], false), k_allowed[index]);
    }
}

static void test_a_remote_host_behind_a_local_looking_part_is_refused(void) {
    static const char *const k_refused[] = {
        "http://evil.example?@127.0.0.1",
        "http://evil.example#@meta",
        "http://evil.example/@127.0.0.1",
        "http://evil.example\\@localhost",
        "http://127.0.0.1.evil.example",
        "http://meta.evil.example",
        "http://localhostile",
        "http://127.0.0.1:80x",
        "http://127.0.0.1:",
        "http://[::1",
        "http://evil.example",
        "ftp://127.0.0.1",
        "127.0.0.1:8080",
    };
    for (size_t index = 0U; index < sizeof k_refused / sizeof k_refused[0]; ++index) {
        TEST_ASSERT_FALSE_MESSAGE(online_meta_url_allowed(k_refused[index], false), k_refused[index]);
    }
    TEST_ASSERT_TRUE(online_meta_url_allowed("http://evil.example", true));
    TEST_ASSERT_FALSE(online_meta_url_allowed("ftp://evil.example", true));
}

static void test_the_logged_url_drops_the_credentials_only(void) {
    char out[128];
    online_meta_url_loggable("https://user:pa@ss@game.example:8443/base", out, sizeof out);
    TEST_ASSERT_EQUAL_STRING("https://game.example:8443/base", out);
    online_meta_url_loggable("https://game.example/path?to=a@b", out, sizeof out);
    TEST_ASSERT_EQUAL_STRING("https://game.example/path?to=a@b", out);
    online_meta_url_loggable("http://evil.example#@meta", out, sizeof out);
    TEST_ASSERT_EQUAL_STRING("http://evil.example#@meta", out);
    online_meta_url_loggable("none", out, sizeof out);
    TEST_ASSERT_EQUAL_STRING("none", out);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_https_and_the_local_hosts_are_allowed);
    RUN_TEST(test_a_remote_host_behind_a_local_looking_part_is_refused);
    RUN_TEST(test_the_logged_url_drops_the_credentials_only);
    return UNITY_END();
}
