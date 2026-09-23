import math
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from motion_math import contact_runs, detect_contacts, foot_slide, lock_plan, remove_twist, two_bone_ik


def distance(a, b):
    return math.dist(a, b)


def synthetic_walk(frames=90, fps=30.0, stride=0.4, lift=0.08):
    """One foot: planted for the first half of each 30-frame cycle, swinging in the second."""
    ankles = []
    x = 0.0
    for f in range(frames):
        phase = (f % 30) / 30.0
        if phase < 0.5:
            z = 0.07
        else:
            swing = (phase - 0.5) / 0.5
            x += stride / 15.0
            z = 0.07 + lift * math.sin(math.pi * swing)
        ankles.append((x + 0.002 * math.sin(f), 0.1, z))
    return ankles


class ContactTests(unittest.TestCase):
    def test_walk_plants_match_stance_phases(self):
        flags = detect_contacts(synthetic_walk(), fps=30.0, leg_length=0.9)
        runs = contact_runs(flags)
        self.assertEqual(len(runs), 3)
        for start, end in runs:
            self.assertGreaterEqual(end - start + 1, 10)

    def test_flicker_shorter_than_min_run_is_dropped(self):
        ankles = [(0.0, 0.0, 0.5)] * 10 + [(0.0, 0.0, 0.0)] * 2 + [(0.0, 0.0, 0.5)] * 10
        flags = detect_contacts(ankles, fps=30.0, leg_length=0.9, min_run=3)
        self.assertFalse(any(flags))

    def test_short_gap_inside_plant_is_filled(self):
        ankles = [(0.0, 0.0, 0.0)] * 5 + [(0.0, 0.0, 0.5)] + [(0.0, 0.0, 0.0)] * 5
        flags = detect_contacts(ankles, fps=30.0, leg_length=0.9, max_gap=2)
        self.assertEqual(contact_runs(flags), [(0, 10)])

    def test_empty_clip(self):
        self.assertEqual(detect_contacts([], fps=30.0, leg_length=1.0), [])


def apply_plan(ankles, plan):
    out = []
    for p, (w, anchor) in zip(ankles, plan):
        if anchor is None:
            out.append(p)
        else:
            out.append((p[0] + (anchor[0] - p[0]) * w, p[1] + (anchor[1] - p[1]) * w, p[2]))
    return out


class LockTests(unittest.TestCase):
    def test_plants_are_fully_pinned_and_ramps_sit_outside(self):
        flags = [False] * 5 + [True] * 6 + [False] * 5
        plan = lock_plan([(float(i), 0.0, 0.0) for i in range(16)], flags, ramp=3)
        weights = [w for w, _ in plan]
        self.assertTrue(all(weights[i] == 1.0 for i in range(5, 11)))
        self.assertEqual(weights[0], 0.0)
        self.assertEqual(weights[15], 0.0)
        self.assertLess(weights[2], weights[3])
        self.assertLess(weights[3], weights[4])
        self.assertTrue(all(0.0 <= w < 1.0 for i, w in enumerate(weights) if not flags[i]))

    def test_anchor_is_constant_within_a_plant(self):
        ankles = synthetic_walk()
        flags = detect_contacts(ankles, fps=30.0, leg_length=0.9)
        plan = lock_plan(ankles, flags)
        for start, end in contact_runs(flags):
            self.assertEqual(len({plan[i][1] for i in range(start, end + 1)}), 1)

    def test_ramp_never_crosses_into_neighbouring_plant(self):
        flags = [True] * 4 + [False] * 2 + [True] * 4
        ankles = [(0.0, 0.0, 0.0)] * 4 + [(0.5, 0.0, 0.1)] * 2 + [(1.0, 0.0, 0.0)] * 4
        plan = lock_plan(ankles, flags, ramp=3)
        self.assertTrue(all(plan[i] == (1.0, (0.0, 0.0)) for i in range(4)))
        self.assertTrue(all(plan[i] == (1.0, (1.0, 0.0)) for i in range(6, 10)))

    def test_pinning_removes_slide(self):
        ankles = synthetic_walk()
        flags = detect_contacts(ankles, fps=30.0, leg_length=0.9)
        pinned = apply_plan(ankles, lock_plan(ankles, flags))
        self.assertGreater(foot_slide(ankles, flags), 0.0)
        self.assertAlmostEqual(foot_slide(pinned, flags), 0.0)


def axis_angle(axis, degrees):
    half = math.radians(degrees) / 2
    s = math.sin(half)
    return (math.cos(half), axis[0] * s, axis[1] * s, axis[2] * s)


def mul(a, b):
    aw, ax, ay, az = a
    bw, bx, by, bz = b
    return (aw * bw - ax * bx - ay * by - az * bz,
            aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw)


class TwistTests(unittest.TestCase):
    def assertQuatEqual(self, a, b):
        sign = 1.0 if sum(x * y for x, y in zip(a, b)) >= 0 else -1.0
        for x, y in zip(a, b):
            self.assertAlmostEqual(x, sign * y, places=6)

    def test_pure_twist_becomes_identity(self):
        self.assertQuatEqual(remove_twist(axis_angle((0, 1, 0), 120)), (1, 0, 0, 0))

    def test_pure_bend_is_kept(self):
        bend = axis_angle((1, 0, 0), 70)
        self.assertQuatEqual(remove_twist(bend), bend)

    def test_negated_quaternion_gives_the_same_swing(self):
        q = mul(axis_angle((1, 0, 0), 70), axis_angle((0, 1, 0), 110))
        self.assertQuatEqual(remove_twist(tuple(-v for v in q)), remove_twist(q))

    def test_near_half_turn_swing_stays_finite(self):
        swing = remove_twist(axis_angle((1, 0, 0), 179.9999))
        self.assertTrue(all(math.isfinite(v) for v in swing))
        self.assertAlmostEqual(sum(v * v for v in swing), 1.0, places=6)

    def test_bend_with_twist_keeps_only_the_bend(self):
        bend = axis_angle((1, 0, 0), 70)
        self.assertQuatEqual(remove_twist(mul(bend, axis_angle((0, 1, 0), 110))), bend)


class IkTests(unittest.TestCase):
    def test_reachable_target_is_hit_and_bone_lengths_hold(self):
        hip, hint = (0.0, 0.0, 1.0), (0.1, 0.0, 0.55)
        knee, ankle = two_bone_ik(hip, hint, (0.05, 0.1, 0.2), 0.45, 0.42)
        self.assertAlmostEqual(distance(ankle, (0.05, 0.1, 0.2)), 0.0, places=6)
        self.assertAlmostEqual(distance(hip, knee), 0.45, places=6)
        self.assertAlmostEqual(distance(knee, ankle), 0.42, places=6)

    def test_knee_bends_toward_hint(self):
        hip = (0.0, 0.0, 1.0)
        knee, _ = two_bone_ik(hip, (0.3, 0.0, 0.55), (0.0, 0.0, 0.3), 0.45, 0.42)
        self.assertGreater(knee[0], 0.0)
        knee, _ = two_bone_ik(hip, (-0.3, 0.0, 0.55), (0.0, 0.0, 0.3), 0.45, 0.42)
        self.assertLess(knee[0], 0.0)

    def test_unreachable_target_clamps_without_nan(self):
        hip = (0.0, 0.0, 1.0)
        knee, ankle = two_bone_ik(hip, (0.1, 0.0, 0.5), (0.0, 0.0, -5.0), 0.45, 0.42)
        self.assertTrue(all(math.isfinite(v) for v in knee + ankle))
        self.assertLess(distance(hip, ankle), 0.87)
        self.assertAlmostEqual(distance(hip, knee), 0.45, places=6)

    def test_straight_hint_still_bends(self):
        hip = (0.0, 0.0, 1.0)
        knee, _ = two_bone_ik(hip, (0.0, 0.0, 0.5), (0.0, 0.0, 0.3), 0.45, 0.42)
        self.assertGreater(math.hypot(knee[0], knee[1]), 0.0)


if __name__ == "__main__":
    unittest.main()
