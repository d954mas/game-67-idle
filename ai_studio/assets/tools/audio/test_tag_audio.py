import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tag_audio import (
    DEFAULT_VOCAB,
    allowed_groups,
    calibrate,
    duration_tag,
    load_vocabulary,
    parse_scopes,
    pick_labels,
    proposal,
    scoped_softmax,
)


class ScopeTest(unittest.TestCase):
    def test_scope_flag_parses_into_group_sets(self):
        self.assertEqual(parse_scopes(["ui=ui, mechanical", "magic=magic"]), {"ui": {"ui", "mechanical"}, "magic": {"magic"}})

    def test_malformed_scope_is_rejected(self):
        with self.assertRaises(ValueError):
            parse_scopes(["ui"])

    def test_record_without_scoped_tag_sees_whole_vocabulary(self):
        self.assertIsNone(allowed_groups(["sfx", "wav"], {"ui": {"ui"}}))
        self.assertEqual(allowed_groups(["sfx", "ui"], {"ui": {"ui"}, "magic": {"magic"}}), {"ui"})

    def test_scoped_softmax_ranks_only_allowed_groups(self):
        groups = {"click": "ui", "roar": "creatures", "beep": "mechanical"}
        labels, probs = scoped_softmax([1.0, 9.0, 1.0], ["click", "roar", "beep"], groups, {"ui", "mechanical"})
        self.assertEqual(labels, ["click", "beep"])
        self.assertAlmostEqual(sum(probs), 1.0)
        self.assertAlmostEqual(probs[0], 0.5)

    def test_scope_matching_no_label_falls_back_to_everything(self):
        labels, _ = scoped_softmax([0.0, 1.0], ["click", "roar"], {"click": "ui", "roar": "creatures"}, {"missing"})
        self.assertEqual(labels, ["click", "roar"])


class PickLabelsTest(unittest.TestCase):
    def test_best_label_is_always_kept(self):
        picked = pick_labels([0.05, 0.02, 0.01], ["a", "b", "c"], top_k=3, min_ratio=0.9)
        self.assertEqual([label for label, _ in picked], ["a"])

    def test_runners_up_need_a_share_of_the_best_score(self):
        picked = pick_labels([0.1, 0.4, 0.3, 0.25], ["a", "b", "c", "d"], top_k=3, min_ratio=0.7)
        self.assertEqual([label for label, _ in picked], ["b", "c"])

    def test_top_k_caps_the_count(self):
        picked = pick_labels([0.3, 0.3, 0.3, 0.3], ["a", "b", "c", "d"], top_k=2, min_ratio=0.1)
        self.assertEqual(len(picked), 2)


class CalibrateTest(unittest.TestCase):
    def test_label_that_wins_everywhere_is_pulled_back(self):
        rows = calibrate([[5.0, 1.0], [5.0, 3.0]])
        self.assertEqual(rows, [[0.0, -1.0], [0.0, 1.0]])

    def test_empty_run_is_left_alone(self):
        self.assertEqual(calibrate([]), [])


class ProposalTest(unittest.TestCase):
    def test_tags_join_label_group_and_duration_without_repeats(self):
        groups = {"sword swing": "combat", "sword slash": "combat"}
        result = proposal("id", [("sword swing", 0.6), ("sword slash", 0.4)], groups, 0.3)
        self.assertEqual(result["tags"], ["sword-swing", "combat", "sword-slash", "very-short"])
        self.assertEqual(result["summary"], "sword swing, sword slash")

    def test_duration_buckets_are_ordered(self):
        self.assertEqual([duration_tag(s) for s in (0.1, 1.0, 3.0, 9.0)], ["very-short", "short", "medium", "long"])


class VocabularyTest(unittest.TestCase):
    def test_duplicate_labels_keep_their_first_group(self):
        with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False, encoding="utf-8") as handle:
            json.dump({"one": ["pop", "click"], "two": ["pop", "boom"]}, handle)
        try:
            self.assertEqual(load_vocabulary(handle.name), [("pop", "one"), ("click", "one"), ("boom", "two")])
        finally:
            os.unlink(handle.name)

    def test_shipped_vocabulary_loads(self):
        self.assertGreater(len(load_vocabulary(DEFAULT_VOCAB)), 50)


if __name__ == "__main__":
    unittest.main()
