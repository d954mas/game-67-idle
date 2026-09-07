#include "unity.h"

#include "features/leaderboard/leaderboard_portal.h"

void setUp(void) {}
void tearDown(void) {}

/* The portal backend arrives with the adapter bridge; until then this target
   exists so its owner can fill one file without touching the build. */
void test_the_portal_backend_exists(void) {
    TEST_ASSERT_NOT_NULL(leaderboard_portal_backend());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_the_portal_backend_exists);
    return UNITY_END();
}
