#!/usr/bin/env python3
"""Contract tests for localization codegen.

    node ai_studio/dev_environment/python_run.mjs features/localization/scripts/loc_test.py
"""

from __future__ import annotations

import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import loc  # noqa: E402


def table_doc(strings: dict, *, languages: dict | None = None, fallback: str = "ru") -> dict:
    return {
        "languages": languages
        or {
            "ru": {"group_separator": " ", "group_min_digits": 5},
            "en": {"group_separator": ",", "group_min_digits": 4},
        },
        "fallback": fallback,
        "strings": strings,
    }


def build(doc: dict) -> loc.Table:
    """Semantic pass only. Most cases below feed deliberately broken documents
    and assert the generator's own message, which is more actionable than a
    jsonschema diagnostic -- so they address that layer directly."""
    return loc.Table(doc, "test.json")


def build_full(doc: dict) -> loc.Table:
    """Both passes, in the order load_table() runs them."""
    loc.validate_against_schema(doc)
    return loc.Table(doc, "test.json")


def generate(doc: dict, *, index_only: bool = False) -> tuple[Path, str]:
    """Run the CLI end to end into a temp dir; return (out_dir, stderr)."""
    tmp = Path(tempfile.mkdtemp())
    strings_path = tmp / "strings.json"
    strings_path.write_text(json.dumps(doc, ensure_ascii=False), encoding="utf-8")
    out_dir = tmp / "generated"
    stderr = io.StringIO()
    argv = ["generate", "--strings", str(strings_path), "--out-dir", str(out_dir)]
    if index_only:
        argv.append("--index-only")
    with contextlib.redirect_stderr(stderr):
        code = loc.main(argv)
    assert code == 0, code
    return out_dir, stderr.getvalue()


class ValidationErrors(unittest.TestCase):
    def assert_rejects(self, doc: dict, needle: str) -> None:
        with self.assertRaises(SystemExit) as caught:
            build(doc)
        self.assertIn(needle, str(caught.exception))

    def test_placeholder_present_in_one_language_only(self):
        self.assert_rejects(
            table_doc(
                {
                    "hud.score": {
                        "args": {"n": "int"},
                        "ru": "Счёт {n}",
                        "en": "Score",
                    }
                }
            ),
            "placeholder mismatch",
        )

    def test_arg_declared_but_unused(self):
        self.assert_rejects(
            table_doc({"hud.score": {"args": {"n": "int"}, "ru": "Счёт", "en": "Score"}}),
            "declares argument(s) ['n'] that no form uses",
        )

    def test_arg_used_but_undeclared(self):
        self.assert_rejects(
            table_doc({"hud.score": {"ru": "Счёт {n}", "en": "Score {n}"}}),
            "uses placeholder(s) ['n'] that are not declared",
        )

    def test_missing_plural_form(self):
        self.assert_rejects(
            table_doc(
                {
                    "hud.coins": {
                        "args": {"n": "int"},
                        "plural_on": "n",
                        "ru": {"one": "{n} монета", "few": "{n} монеты"},
                        "en": {"one": "{n} coin", "other": "{n} coins"},
                    }
                }
            ),
            "missing plural form(s) ['many']",
        )

    def test_unreachable_plural_form(self):
        self.assert_rejects(
            table_doc(
                {
                    "hud.coins": {
                        "args": {"n": "int"},
                        "plural_on": "n",
                        "ru": {"one": "{n} монета", "few": "{n} монеты", "many": "{n} монет"},
                        "en": {"one": "{n} coin", "few": "{n} coins", "other": "{n} coins"},
                    }
                }
            ),
            "unreachable under the 'one_other' rule",
        )

    def test_slavic_other_is_unreachable_too(self):
        # The rule that bit the first cut: `other` was REQUIRED for Russian
        # while plural_select() only ever returns one/few/many, so the corpus
        # carried a form nothing could render.
        self.assert_rejects(
            table_doc(
                {
                    "hud.coins": {
                        "args": {"n": "int"},
                        "plural_on": "n",
                        "ru": {
                            "one": "{n} монета",
                            "few": "{n} монеты",
                            "many": "{n} монет",
                            "other": "{n} монеты",
                        },
                        "en": {"one": "{n} coin", "other": "{n} coins"},
                    }
                }
            ),
            "unreachable under the 'slavic' rule",
        )

    def test_too_many_arguments(self):
        args = {f"a{i}": "int" for i in range(9)}
        text = "".join(f"{{a{i}}}" for i in range(9))
        self.assert_rejects(
            table_doc({"hud.wide": {"args": args, "ru": text, "en": text}}),
            "declares 9 arguments; the runtime renders at most 8",
        )

    def test_missing_fallback_language(self):
        self.assert_rejects(
            table_doc({"hud.score": {"en": "Score"}}),
            "no text for the fallback language 'ru'",
        )

    def test_mangled_identifier_collision(self):
        self.assert_rejects(
            table_doc(
                {
                    "hud.b_c": {"ru": "а", "en": "a"},
                    "hud_b.c": {"ru": "б", "en": "b"},
                }
            ),
            "both mangle to the C identifier 'hud_b_c'",
        )

    def test_key_regex(self):
        for bad in ("Hud.score", "hud", "hud.", ".score", "hud.Score", "hud..score", "hud.1score"):
            with self.subTest(key=bad):
                self.assert_rejects(table_doc({bad: {"ru": "а", "en": "a"}}), "must match")

    def test_plural_on_float_requires_other_only(self):
        doc = table_doc(
            {
                "hud.rate": {
                    "args": {"n": "float:1"},
                    "plural_on": "n",
                    "ru": {"one": "{n} монета", "few": "{n}", "many": "{n}"},
                    "en": {"other": "{n} coins"},
                }
            }
        )
        self.assert_rejects(doc, "is a float type, so only 'other' is ever selected")

    def test_plural_on_float_accepts_other_only(self):
        table = build(
            table_doc(
                {
                    "hud.rate": {
                        "args": {"n": "float:1"},
                        "plural_on": "n",
                        "ru": {"other": "{n} монеты"},
                        "en": {"other": "{n} coins"},
                    }
                }
            )
        )
        self.assertTrue(table.entries[0].is_plural)

    def test_plural_on_must_name_a_declared_arg(self):
        self.assert_rejects(
            table_doc({"hud.coins": {"args": {"n": "int"}, "plural_on": "k", "ru": {}, "en": {}}}),
            "plural_on 'k' is not a declared argument",
        )

    def test_plural_key_rejects_plain_string(self):
        self.assert_rejects(
            table_doc(
                {"hud.coins": {"args": {"n": "int"}, "plural_on": "n", "ru": "{n}", "en": "{n}"}}
            ),
            "must be an object of CLDR forms",
        )

    def test_non_plural_key_rejects_form_object(self):
        self.assert_rejects(
            table_doc({"hud.coins": {"ru": {"other": "монеты"}, "en": "coins"}}),
            "must be a string, got an object",
        )

    def test_unknown_arg_type(self):
        self.assert_rejects(
            table_doc({"hud.score": {"args": {"n": "int:thousands"}, "ru": "{n}", "en": "{n}"}}),
            "unknown type 'int:thousands'",
        )

    def test_unknown_entry_field(self):
        self.assert_rejects(
            table_doc({"hud.score": {"ru": "а", "en": "a", "de": "d"}}),
            "unknown field(s) ['de']",
        )

    def test_stray_brace(self):
        self.assert_rejects(
            table_doc({"hud.score": {"ru": "100% из {", "en": "a"}}),
            "unclosed '{'",
        )
        self.assert_rejects(
            table_doc({"hud.score": {"ru": "а } б", "en": "a"}}),
            "stray '}'",
        )

    def test_control_character(self):
        self.assert_rejects(
            table_doc({"hud.score": {"ru": "а\x01б", "en": "a"}}),
            "control character U+0001",
        )

    def test_fallback_must_be_declared(self):
        self.assert_rejects(
            table_doc({"hud.score": {"ru": "а", "en": "a"}}, fallback="de"),
            "'fallback' must name a declared language",
        )

    def test_unknown_language_needs_explicit_plural_rule(self):
        self.assert_rejects(
            table_doc(
                {"hud.score": {"ru": "а", "xq": "a"}},
                languages={
                    "ru": {"group_separator": " ", "group_min_digits": 5},
                    "xq": {"group_separator": ",", "group_min_digits": 4},
                },
            ),
            "no built-in CLDR plural rule",
        )

    def test_explicit_plural_rule_override(self):
        table = build(
            table_doc(
                {"hud.score": {"ru": "а", "xq": "a"}},
                languages={
                    "ru": {"group_separator": " ", "group_min_digits": 5},
                    "xq": {"group_separator": ",", "group_min_digits": 4, "plural_rule": "one_other"},
                },
            )
        )
        self.assertEqual(table.langs[1].plural_rule, "one_other")


class SchemaIsLoadBearing(unittest.TestCase):
    """The schema validates every document before the semantic pass, so the two
    must agree on the handful of values that appear in both. Without these the
    schema drifts into decoration -- which is what it was before this review."""

    @classmethod
    def setUpClass(cls):
        cls.schema = json.loads(loc.SCHEMA_PATH.read_text(encoding="utf-8"))
        cls.entry = cls.schema["definitions"]["entry"]

    def test_arg_type_enum_matches_the_generator(self):
        enum = self.entry["properties"]["args"]["additionalProperties"]["enum"]
        self.assertEqual(sorted(enum), sorted(loc.ARG_TYPES))

    def test_arg_cap_matches_the_runtime_cap(self):
        self.assertEqual(self.entry["properties"]["args"]["maxProperties"], loc.MAX_ARGS)

    def test_plural_rule_enum_matches_the_generator(self):
        enum = self.schema["properties"]["languages"]["additionalProperties"]["properties"]["plural_rule"]["enum"]
        self.assertEqual(sorted(enum), sorted(loc.PLURAL_RULE_CATS))

    def test_plural_categories_match_the_generator(self):
        for branch in self.entry["patternProperties"].values():
            forms = branch["oneOf"][1]["properties"]
            self.assertEqual(sorted(forms), sorted(loc.PLURAL_CATS))

    def test_key_pattern_matches_the_generator(self):
        self.assertEqual(self.schema["properties"]["strings"]["propertyNames"]["pattern"], loc.KEY_RE.pattern)

    def test_schema_rejects_a_shape_error_before_the_semantic_pass(self):
        with self.assertRaises(SystemExit) as caught:
            build_full(table_doc({"hud.score": {"args": {"n": 7}, "ru": "{n}", "en": "{n}"}}))
        self.assertIn("strings.schema.json", str(caught.exception))

    def test_schema_accepts_the_shipping_corpus_shape(self):
        self.assertEqual(len(build_full(GeneratedOutput.DOC).entries), 3)

    def test_schema_allows_the_dollar_schema_key(self):
        doc = table_doc({"hud.score": {"ru": "а", "en": "a"}})
        doc["$schema"] = "../schema/strings.schema.json"
        self.assertEqual(len(build_full(doc).entries), 1)


class Warnings(unittest.TestCase):
    def test_missing_non_fallback_translation_warns(self):
        table = build(table_doc({"hud.score": {"ru": "Счёт"}}))
        self.assertEqual(len(table.warnings), 1)
        self.assertIn("no 'en' translation for 'hud.score'", table.warnings[0])

    def test_warning_reaches_stderr_without_failing(self):
        _, stderr = generate(table_doc({"hud.score": {"ru": "Счёт"}}))
        self.assertIn("no 'en' translation for 'hud.score'", stderr)


class TemplateRendering(unittest.TestCase):
    def test_placeholder_becomes_index(self):
        self.assertEqual(loc.render_template("Нужно {parent} Ур {n}", ["parent", "n"], "t"), "Нужно {0} Ур {1}")

    def test_reordered_placeholders_keep_argument_indices(self):
        self.assertEqual(loc.render_template("{n} of {parent}", ["parent", "n"], "t"), "{1} of {0}")

    def test_literal_braces_stay_escaped_when_the_key_takes_arguments(self):
        self.assertEqual(loc.render_template("{{n}} = {n}", ["n"], "t"), "{{n}} = {0}")

    def test_literal_braces_are_resolved_when_the_key_takes_none(self):
        # loc_get() returns this pointer verbatim -- nothing unescapes it later.
        self.assertEqual(loc.render_template("{{готово}}", [], "t"), "{готово}")

    def test_percent_is_literal(self):
        self.assertEqual(loc.render_template("100% готово", [], "t"), "100% готово")


class GeneratedOutput(unittest.TestCase):
    DOC = table_doc(
        {
            "common.close": {"ru": "ЗАКРЫТЬ", "en": "Close"},
            "upgrades.requires": {
                "args": {"parent": "str", "n": "int"},
                "ru": "Нужно {parent} Ур {n}",
                "en": "Requires {parent} Lv {n}",
            },
            "upgrades.coins_needed": {
                "args": {"n": "int:group"},
                "plural_on": "n",
                "ru": {
                    "one": "нужна {n} монета",
                    "few": "нужно {n} монеты",
                    "many": "нужно {n} монет",
                },
                "en": {"one": "need {n} coin", "other": "need {n} coins"},
            },
        }
    )

    def setUp(self):
        self.out_dir, _ = generate(self.DOC)
        self.header = (self.out_dir / "loc_strings.gen.h").read_text(encoding="utf-8")
        self.source = (self.out_dir / "loc_strings.gen.c").read_text(encoding="utf-8")
        self.index = json.loads((self.out_dir / "loc_keys.gen.json").read_text(encoding="utf-8"))

    def test_lang_enum(self):
        self.assertIn("LOC_LANG_RU = 0,", self.header)
        self.assertIn("LOC_LANG_EN = 1,", self.header)
        self.assertIn("LOC_LANG_COUNT = 2", self.header)
        self.assertIn("#define LOC_FALLBACK_LANG LOC_LANG_RU", self.header)

    def test_keys_are_sorted_so_the_diff_is_authoring_order_independent(self):
        self.assertEqual(
            [entry["key"] for entry in self.index["keys"]],
            ["common.close", "upgrades.coins_needed", "upgrades.requires"],
        )

    def test_key0_enum_excludes_parameterized_keys(self):
        self.assertIn("LOC0_COMMON_CLOSE = 0,", self.header)
        self.assertIn("LOC0_COUNT = 1", self.header)
        self.assertNotIn("LOC0_UPGRADES_REQUIRES", self.header)

    def test_accessor_signatures_are_typed(self):
        self.assertIn("LocStr loc_common_close(void);", self.header)
        self.assertIn("LocStr loc_upgrades_requires(LocStr parent, int64_t n);", self.header)
        self.assertIn("LocStr loc_upgrades_coins_needed(int64_t n);", self.header)

    def test_source_text_is_greppable_from_the_header(self):
        self.assertIn('ru "ЗАКРЫТЬ"', self.header)
        self.assertIn('en "Close"', self.header)
        self.assertIn('ru "Нужно {parent} Ур {n}"', self.header)

    def test_forms_table_layout(self):
        # Non-plural: one slot per language. Plural: six CLDR slots per language.
        self.assertIn('static const char *const k_loc_forms_common_close[] = {', self.source)
        self.assertIn('"ЗАКРЫТЬ", /* ru */', self.source)
        self.assertIn('"нужно {0} монет", /* many */', self.source)
        self.assertIn("NULL, /* few */", self.source)

    def test_zero_arg_accessor_returns_static_pointer(self):
        self.assertIn("LocStr loc_common_close(void) {\n    return loc_get(LOC_COMMON_CLOSE);\n}", self.source)

    def test_formatted_accessor_builds_typed_args(self):
        self.assertIn("{ LOC_ARG_STR, 0, 0.0, parent.s },", self.source)
        self.assertIn("{ LOC_ARG_INT, n, 0.0, NULL },", self.source)
        self.assertIn("return loc_format(LOC_UPGRADES_REQUIRES, -1, args, 2);", self.source)
        self.assertIn("return loc_format(LOC_UPGRADES_COINS_NEEDED, 0, args, 1);", self.source)

    def test_by_key_maps_key0_to_table_index(self):
        self.assertIn("static const uint16_t k_loc_key0_index[] = {", self.source)
        self.assertIn("return loc_get((int)k_loc_key0_index[key]);", self.source)
        # Same contract split as loc.c: an out-of-range LocKey0 means a data
        # table carries a key nothing generated, so it traps rather than
        # rendering an empty string over the bug.
        self.assertIn('NT_ASSERT((int)key >= 0 && (int)key < LOC0_COUNT', self.source)

    def test_index_records_args_and_enum_names(self):
        by_key = {entry["key"]: entry for entry in self.index["keys"]}
        self.assertEqual(by_key["common.close"]["enum0"], "LOC0_COMMON_CLOSE")
        self.assertIsNone(by_key["upgrades.requires"]["enum0"])
        self.assertEqual(
            by_key["upgrades.requires"]["args"],
            [{"name": "parent", "type": "str"}, {"name": "n", "type": "int"}],
        )
        self.assertEqual(by_key["upgrades.coins_needed"]["plural_on"], "n")
        self.assertEqual(self.index["languages"], ["ru", "en"])
        self.assertEqual(self.index["fallback"], "ru")

    def test_index_only_skips_the_c_output(self):
        out_dir, _ = generate(self.DOC, index_only=True)
        self.assertTrue((out_dir / "loc_keys.gen.json").exists())
        self.assertFalse((out_dir / "loc_strings.gen.h").exists())
        self.assertFalse((out_dir / "loc_strings.gen.c").exists())

    def test_idempotent(self):
        before = (self.out_dir / "loc_strings.gen.c").stat().st_mtime_ns
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            loc.main(
                [
                    "generate",
                    "--strings",
                    str(self.out_dir.parent / "strings.json"),
                    "--out-dir",
                    str(self.out_dir),
                ]
            )
        self.assertEqual((self.out_dir / "loc_strings.gen.c").stat().st_mtime_ns, before)


if __name__ == "__main__":
    unittest.main()
