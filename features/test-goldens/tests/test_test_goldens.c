// Unity pulls in <stdnoreturn.h>, whose `noreturn` macro breaks the Windows CRT
// headers that follow it; system headers come first for that reason.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "features/test_goldens/test_goldens.h"

#ifdef _WIN32
#include <direct.h>
#define golden_mkdir(path) _mkdir(path)
#define golden_setenv(name, value) _putenv_s((name), (value))
#else
#include <sys/stat.h>
#define golden_mkdir(path) mkdir((path), 0777)
#define golden_setenv(name, value) setenv((name), (value), 1)
#endif

#define BANK_DIR "test_goldens_scratch"

void setUp(void)
{
    golden_mkdir(BANK_DIR);
    golden_setenv("GAME_GOLDENS_DIR", BANK_DIR);
    remove(BANK_DIR "/sample.golden");
}

void tearDown(void)
{
    remove(BANK_DIR "/sample.golden");
    golden_setenv("GAME_UPDATE_GOLDENS", "");
}

static void test_recording_stores_the_actual_value(void)
{
    golden_setenv("GAME_UPDATE_GOLDENS", "1");
    TEST_ASSERT_EQUAL_UINT64(5722u, test_golden_u64("sample", "object_count", 5722u));

    FILE *file = fopen(BANK_DIR "/sample.golden", "rb");
    TEST_ASSERT_NOT_NULL(file);
    char line[128] = { 0 };
    TEST_ASSERT_NOT_NULL(fgets(line, (int)sizeof(line), file));
    fclose(file);
    TEST_ASSERT_EQUAL_STRING("object_count = 5722\n", line);
}

static void test_comparing_returns_the_recorded_value(void)
{
    golden_setenv("GAME_UPDATE_GOLDENS", "1");
    test_golden_u64("sample", "object_count", 5722u);
    test_golden_f64("sample", "total_mass", 1051.94);
    test_golden_text("sample", "boss_id", "kraken");

    golden_setenv("GAME_UPDATE_GOLDENS", "");
    // A drifted actual must come back as the recorded value, so the caller's
    // own assertion is what fails, with both numbers in the message.
    TEST_ASSERT_EQUAL_UINT64(5722u, test_golden_u64("sample", "object_count", 6001u));
    // Unity excludes float asserts in several presets; compare by hand so the
    // feature test runs under every one of them.
    TEST_ASSERT_TRUE(fabs(test_golden_f64("sample", "total_mass", 900.0) - 1051.94) < 1e-9);
    TEST_ASSERT_EQUAL_STRING("kraken", test_golden_text("sample", "boss_id", "wyrm"));
}

static void test_recording_keeps_other_keys(void)
{
    golden_setenv("GAME_UPDATE_GOLDENS", "1");
    test_golden_i64("sample", "seed", -17);
    test_golden_i64("sample", "wave", 3);
    test_golden_i64("sample", "seed", -18);

    golden_setenv("GAME_UPDATE_GOLDENS", "");
    TEST_ASSERT_EQUAL_INT64(3, test_golden_i64("sample", "wave", 3));
    TEST_ASSERT_EQUAL_INT64(-18, test_golden_i64("sample", "seed", -18));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_recording_stores_the_actual_value);
    RUN_TEST(test_comparing_returns_the_recorded_value);
    RUN_TEST(test_recording_keeps_other_keys);
    return UNITY_END();
}
