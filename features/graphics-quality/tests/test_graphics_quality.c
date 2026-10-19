#include "unity.h"

#include "features/graphics_quality/graphics_quality.h"

void setUp(void) {}
void tearDown(void) {}

static void test_software_renderers_guess_low_everywhere(void) {
    static const char *const k_software[] = {"Google SwiftShader", "llvmpipe (LLVM 15.0.7, 256 bits)",
        "ANGLE (Google, Vulkan 1.3.0 (SwiftShader Device (Subzero)), SwiftShader driver)", "Microsoft Basic Render Driver"};
    for (size_t i = 0; i < sizeof k_software / sizeof k_software[0]; ++i) {
        TEST_ASSERT_EQUAL(GRAPHICS_QUALITY_LOW, graphics_quality_guess(false, k_software[i]));
        TEST_ASSERT_EQUAL(GRAPHICS_QUALITY_LOW, graphics_quality_guess(true, k_software[i]));
    }
}

static void test_phone_gpus_split_on_the_flagship_line(void) {
    TEST_ASSERT_EQUAL(GRAPHICS_QUALITY_LOW, graphics_quality_guess(true, "ANGLE (ARM, Mali-G76, OpenGL ES 3.2)"));
    TEST_ASSERT_EQUAL(GRAPHICS_QUALITY_LOW, graphics_quality_guess(true, "Adreno (TM) 619"));
    TEST_ASSERT_EQUAL(GRAPHICS_QUALITY_HIGH, graphics_quality_guess(true, "ANGLE (Qualcomm, Adreno (TM) 750, OpenGL ES 3.2)"));
    TEST_ASSERT_EQUAL(GRAPHICS_QUALITY_HIGH, graphics_quality_guess(true, "Mali-G715-Immortalis MC11"));
    TEST_ASSERT_EQUAL(GRAPHICS_QUALITY_HIGH, graphics_quality_guess(true, "Apple GPU"));
}

static void test_a_phone_gpu_is_judged_as_a_phone_even_on_a_computer(void) {
    TEST_ASSERT_EQUAL(GRAPHICS_QUALITY_LOW, graphics_quality_guess(false, "Mali-G52 MC2"));
}

static void test_computers_guess_high_unless_old_integrated(void) {
    TEST_ASSERT_EQUAL(GRAPHICS_QUALITY_HIGH, graphics_quality_guess(false, "ANGLE (NVIDIA, NVIDIA GeForce RTX 3060 Direct3D11)"));
    TEST_ASSERT_EQUAL(GRAPHICS_QUALITY_HIGH, graphics_quality_guess(false, "ANGLE (Intel, Intel(R) UHD Graphics 620 Direct3D11)"));
    TEST_ASSERT_EQUAL(GRAPHICS_QUALITY_LOW, graphics_quality_guess(false, "ANGLE (Intel, Intel(R) HD Graphics 4000 Direct3D11)"));
}

static void test_an_unknown_device_is_never_degraded_on_a_computer(void) {
    TEST_ASSERT_EQUAL(GRAPHICS_QUALITY_HIGH, graphics_quality_guess(false, ""));
    TEST_ASSERT_EQUAL(GRAPHICS_QUALITY_HIGH, graphics_quality_guess(false, NULL));
}

static void test_low_shrinks_the_world_and_high_keeps_it_whole(void) {
    for (int mobile = 0; mobile <= 1; ++mobile) {
        TEST_ASSERT_EQUAL_FLOAT(1.0F, graphics_world_scale(GRAPHICS_QUALITY_HIGH, mobile != 0));
        const float low = graphics_world_scale(GRAPHICS_QUALITY_LOW, mobile != 0);
        TEST_ASSERT_TRUE(low > 0.0F && low < 1.0F);
    }
    // A computer's screen shows blur that a phone's small one hides.
    TEST_ASSERT_TRUE(graphics_world_scale(GRAPHICS_QUALITY_LOW, false) > graphics_world_scale(GRAPHICS_QUALITY_LOW, true));
}

static void test_low_starts_without_shadows(void) {
    TEST_ASSERT_TRUE(graphics_shadows_default(GRAPHICS_QUALITY_HIGH));
    TEST_ASSERT_FALSE(graphics_shadows_default(GRAPHICS_QUALITY_LOW));
}

static void test_names_round_trip_and_reject_strangers(void) {
    for (int i = 0; i < GRAPHICS_QUALITY_COUNT; ++i) {
        graphics_quality_t parsed = GRAPHICS_QUALITY_COUNT;
        TEST_ASSERT_TRUE(graphics_quality_parse(graphics_quality_name((graphics_quality_t)i), &parsed));
        TEST_ASSERT_EQUAL(i, parsed);
    }
    graphics_quality_t untouched = GRAPHICS_QUALITY_LOW;
    TEST_ASSERT_FALSE(graphics_quality_parse("ultra", &untouched));
    TEST_ASSERT_FALSE(graphics_quality_parse(NULL, &untouched));
    TEST_ASSERT_EQUAL(GRAPHICS_QUALITY_LOW, untouched);
}

static void test_native_device_is_a_computer_without_a_name(void) {
    graphics_device_t device = {.mobile = true, .gpu = "stale"};
    graphics_device_query(&device);
    TEST_ASSERT_FALSE(device.mobile);
    TEST_ASSERT_EQUAL_STRING("", device.gpu);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_software_renderers_guess_low_everywhere);
    RUN_TEST(test_phone_gpus_split_on_the_flagship_line);
    RUN_TEST(test_a_phone_gpu_is_judged_as_a_phone_even_on_a_computer);
    RUN_TEST(test_computers_guess_high_unless_old_integrated);
    RUN_TEST(test_an_unknown_device_is_never_degraded_on_a_computer);
    RUN_TEST(test_low_shrinks_the_world_and_high_keeps_it_whole);
    RUN_TEST(test_low_starts_without_shadows);
    RUN_TEST(test_names_round_trip_and_reject_strangers);
    RUN_TEST(test_native_device_is_a_computer_without_a_name);
    return UNITY_END();
}
