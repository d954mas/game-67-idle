#include "ui/ui_scale_policy.h"

#include <stdio.h>

static int close_enough(float actual, float expected) {
    const float delta = actual - expected;
    return (delta < 0.0F ? -delta : delta) < 0.01F;
}

static int check_reference(float fb_w, float fb_h, float dpr,
                           float expected_w, float expected_h) {
    const ui_scale_reference_t ref =
        ui_scale_reference_for_framebuffer(fb_w, fb_h, dpr);
    if (!close_enough(ref.width, expected_w) ||
        !close_enough(ref.height, expected_h)) {
        fprintf(stderr, "UI scale reference mismatch\n");
        return 1;
    }
    return 0;
}

int main(void) {
    int failed = 0;
    failed += check_reference(480.0F, 270.0F, 1.0F, 640.0F, 360.0F);
    failed += check_reference(640.0F, 360.0F, 1.0F, 640.0F, 360.0F);
    failed += check_reference(1280.0F, 720.0F, 1.0F, 1280.0F, 720.0F);
    failed += check_reference(960.0F, 540.0F, 2.0F, 640.0F, 360.0F);
    failed += check_reference(2560.0F, 1440.0F, 2.0F, 1280.0F, 720.0F);
    failed += check_reference(360.0F, 640.0F, 1.0F, 360.0F, 640.0F);
    return failed == 0 ? 0 : 1;
}
