#include "features/limb_ik/limb_ik.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        ++failures;
    }
}

static float distance(limb_ik_point_t a, limb_ik_point_t b) {
    return hypotf(a.x - b.x, a.y - b.y);
}

static void check_lengths(limb_ik_point_t root, limb_ik_solution_t result,
                          float upper, float lower) {
    check(isfinite(result.joint.x) && isfinite(result.joint.y), "joint must be finite");
    check(isfinite(result.end.x) && isfinite(result.end.y), "end must be finite");
    check(fabsf(upper - distance(root, result.joint)) < 0.0001F,
          "upper segment must preserve length");
    check(fabsf(lower - distance(result.joint, result.end)) < 0.0001F,
          "lower segment must preserve length");
}

static void reachable_target_preserves_lengths_and_endpoint(void) {
    const limb_ik_point_t root = {1.0F, 2.0F};
    const limb_ik_point_t target = {2.0F, 3.0F};
    limb_ik_solution_t result;
    check(limb_ik_solve_2link(root, target, 1.2F, 0.8F, 1, &result),
          "reachable target must solve");
    check_lengths(root, result, 1.2F, 0.8F);
    check(distance(target, result.end) < 0.0001F, "reachable endpoint must equal target");
}

static void unreachable_targets_clamp_to_reach(void) {
    const limb_ik_point_t root = {0.0F, 0.0F};
    limb_ik_solution_t result;
    check(limb_ik_solve_2link(root, (limb_ik_point_t){50.0F, 0.0F}, 2.0F, 1.0F, 1, &result),
          "far target must solve");
    check_lengths(root, result, 2.0F, 1.0F);
    check(fabsf(3.0F - distance(root, result.end)) < 0.0001F,
          "far target must clamp to maximum reach");
    check(limb_ik_solve_2link(root, (limb_ik_point_t){0.1F, 0.0F}, 2.0F, 1.0F, -1, &result),
          "near target must solve");
    check_lengths(root, result, 2.0F, 1.0F);
    check(fabsf(1.0F - distance(root, result.end)) < 0.0001F,
          "near target must clamp to minimum reach");
}

static void root_target_has_finite_solution(void) {
    const limb_ik_point_t root = {-1.0F, 2.0F};
    limb_ik_solution_t result;
    check(limb_ik_solve_2link(root, root, 1.0F, 1.0F, 1, &result),
          "coincident target must solve");
    check_lengths(root, result, 1.0F, 1.0F);
    check(distance(root, result.end) < 0.0001F, "equal lengths must fold at root");
}

static void bend_selects_opposite_joint_side(void) {
    const limb_ik_point_t root = {0.0F, 0.0F};
    const limb_ik_point_t target = {1.0F, 0.0F};
    limb_ik_solution_t positive;
    limb_ik_solution_t negative;
    check(limb_ik_solve_2link(root, target, 1.0F, 1.0F, 1, &positive),
          "positive bend must solve");
    check(limb_ik_solve_2link(root, target, 1.0F, 1.0F, -1, &negative),
          "negative bend must solve");
    check(positive.joint.y * negative.joint.y < 0.0F,
          "bend directions must use opposite sides");
    check(distance(positive.end, negative.end) < 0.0001F,
          "bend directions must keep the same endpoint");
}

static void target_sweep_never_stretches_bones(void) {
    const limb_ik_point_t root = {0.25F, -0.75F};
    for (int x = -8; x <= 8; ++x) {
        for (int y = -8; y <= 8; ++y) {
            limb_ik_solution_t result;
            const limb_ik_point_t target = {(float)x * 0.3F, (float)y * 0.3F};
            check(limb_ik_solve_2link(root, target, 0.75F, 0.6F, 1, &result),
                  "finite sweep target must solve");
            check_lengths(root, result, 0.75F, 0.6F);
        }
    }
}

static void invalid_inputs_leave_previous_solution_unchanged(void) {
    const limb_ik_point_t root = {0.0F, 0.0F};
    const limb_ik_point_t target = {1.0F, 0.0F};
    limb_ik_solution_t result = {{4.0F, 5.0F}, {6.0F, 7.0F}};
    const limb_ik_solution_t previous = result;
    check(!limb_ik_solve_2link(root, target, 0.0F, 1.0F, 1, &result),
          "zero upper length must fail");
    check(!limb_ik_solve_2link(root, target, -1.0F, 1.0F, 1, &result),
          "negative upper length must fail");
    check(!limb_ik_solve_2link(root, target, INFINITY, 1.0F, 1, &result),
          "infinite length must fail");
    check(!limb_ik_solve_2link(root, (limb_ik_point_t){NAN, 1.0F}, 1.0F, 1.0F, 1, &result),
          "nonfinite target must fail");
    check(!limb_ik_solve_2link(root, target, 1.0F, 1.0F, 0, &result),
          "zero bend must fail");
    check(!limb_ik_solve_2link(root, target, 1.0F, 1.0F, 1, NULL),
          "null output must fail");
    check(memcmp(&previous, &result, sizeof result) == 0,
          "invalid inputs must leave output unchanged");
}

int main(void) {
    reachable_target_preserves_lengths_and_endpoint();
    unreachable_targets_clamp_to_reach();
    root_target_has_finite_solution();
    bend_selects_opposite_joint_side();
    target_sweep_never_stretches_bones();
    invalid_inputs_leave_previous_solution_unchanged();
    return failures == 0 ? 0 : 1;
}
