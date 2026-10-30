import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import tempfile

from describe_audio import context_for, load_partial, parse_answer, parse_contexts, proposal


class ContextTest(unittest.TestCase):
    def test_contexts_parse_and_attach_to_matching_tags(self):
        contexts = parse_contexts(['ui="menu clicks, error buzzers"', "magic=fireballs"])
        self.assertEqual(contexts, {"ui": "menu clicks, error buzzers", "magic": "fireballs"})
        self.assertIn("menu clicks", context_for(["sfx", "ui"], contexts))
        self.assertEqual(context_for(["sfx"], contexts), "")

    def test_malformed_context_is_rejected(self):
        with self.assertRaises(ValueError):
            parse_contexts(["ui"])


class ParseAnswerTest(unittest.TestCase):
    def test_json_wrapped_in_prose_is_read(self):
        description, tags = parse_answer('Sure! ```json\n{"description": "A sword hits a shield.", "tags": ["Sword Clash", "metal"]}\n```')
        self.assertEqual(description, "A sword hits a shield.")
        self.assertEqual(tags, ["sword-clash", "metal"])

    def test_backslash_escaped_json_is_read(self):
        description, tags = parse_answer('{\\"description\\": \\"Metal hit\\", \\"tags\\": [\\"metal\\"]}')
        self.assertEqual((description, tags), ("Metal hit", ["metal"]))

    def test_non_latin_tags_are_dropped(self):
        self.assertEqual(parse_answer('{"description": "toy car", "tags": ["toy", "玩具车"]}')[1], ["toy"])

    def test_prompt_echo_tags_are_dropped(self):
        self.assertEqual(parse_answer('{"description": "x", "tags": ["Sound Effect", "video game asset", "door"]}')[1], ["door"])

    def test_comma_string_tags_are_split(self):
        self.assertEqual(parse_answer('{"description": "x", "tags": "click, ui beep"}')[1], ["click", "ui-beep"])

    def test_plain_sentence_falls_back_without_tags(self):
        self.assertEqual(parse_answer("A dog barking twice."), ("A dog barking twice.", []))


class PartialTest(unittest.TestCase):
    def test_interrupted_run_keeps_whole_rows_and_skips_a_cut_line(self):
        with tempfile.NamedTemporaryFile("w", suffix=".jsonl", delete=False, encoding="utf-8") as handle:
            handle.write('{"asset_id": "a", "tags": ["x"]}\n{"asset_id": "b", "ta')
        try:
            self.assertEqual(list(load_partial(handle.name)), ["a"])
        finally:
            os.unlink(handle.name)

    def test_missing_partial_means_a_fresh_run(self):
        self.assertEqual(load_partial("does-not-exist.partial.jsonl"), {})


class ProposalTest(unittest.TestCase):
    def test_duration_tag_is_appended_once(self):
        result = proposal("id", "A click.", ["click", "short", "click"], 1.0)
        self.assertEqual(result["tags"], ["click", "short"])
        self.assertEqual(result["summary"], "click, short, click")


if __name__ == "__main__":
    unittest.main()
