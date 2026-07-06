"""Unit tests for expand_jobs.py (T0330). Pure: zero API calls, zero subprocess.

Run: python -m unittest test_expand_jobs   (from this scripts/ directory)
"""
from __future__ import annotations

import json
import os
import re
import shutil
import tempfile
import unittest
from pathlib import Path

from expand_jobs import REPO_ROOT, expand


def base_config() -> dict:
    """A small, fully valid config: 1 big axis (grade, 2 values) x 1 vary axis
    (shape, 2 values) on a 1x2 grid -> 2 sheets, 2 jobs. Every test mutates a
    fresh copy of this so tests cannot leak state into each other."""
    return {
        "pack": "t-pack",
        "style_prefix": "flat vector icon style, thick clean outlines, soft shading",
        "subject_template": "a {grade} generator building",
        "axes": {
            "grade": ["rusty", "gilded"],
            "shape": ["furnace", "drill"],
        },
        "sheet": {"vary": "shape", "grid": [1, 2]},
        "background": "magenta",
        "candidates": 1,
        "max_jobs": 12,
        "gen": {"size": "1024x1024", "quality": "high", "model": "gpt-image-2"},
        "out_dir": "tmp/packs/t-pack",
    }


EXPECTED_PROMPT_RUSTY = (
    "[TASK]\nSheet generation: 1x2 grid, one object per filled cell.\n\n"
    "[SUBJECT]\na rusty generator building cell 1 (top-left): shape=furnace; cell 2: shape=drill.\n\n"
    "[STYLE]\nflat vector icon style, thick clean outlines, soft shading\n\n"
    "[COMPOSITION]\nSingle consistent scale across all cells. Each object centered within its cell "
    "with generous gutter margins. No overlapping between cells, no borders, no grid lines.\n\n"
    "[BACKGROUND]\nSolid uniform #FF00FF background, no gradients, no texture.\n\n"
    "[CONSTRAINTS]\nNo text, no labels, no watermark, no grid lines. Identical, consistent lighting "
    "across all cells.\n\n"
    "[OUTPUT]\nOne sheet image, 1x2 grid, 2 filled cell(s) as described; any remaining cells left as "
    "empty background."
)


class GoldenExpansionTest(unittest.TestCase):
    def test_golden_expansion_full_structure_and_order(self) -> None:
        jobs = expand(base_config())
        self.assertEqual(len(jobs), 2)

        self.assertEqual(
            jobs[0],
            {
                "prompt": EXPECTED_PROMPT_RUSTY,
                "out": "tmp/packs/t-pack/grade-rusty.png",
                "name": "t-pack: rusty",
                "size": "1024x1024",
                "quality": "high",
                "model": "gpt-image-2",
                "pack": "t-pack",
                "cells": [
                    {"cell": [0, 0], "axes": {"grade": "rusty", "shape": "furnace"}},
                    {"cell": [0, 1], "axes": {"grade": "rusty", "shape": "drill"}},
                ],
            },
        )
        self.assertEqual(jobs[1]["out"], "tmp/packs/t-pack/grade-gilded.png")
        self.assertEqual(jobs[1]["name"], "t-pack: gilded")
        self.assertEqual(
            jobs[1]["cells"],
            [
                {"cell": [0, 0], "axes": {"grade": "gilded", "shape": "furnace"}},
                {"cell": [0, 1], "axes": {"grade": "gilded", "shape": "drill"}},
            ],
        )
        # stable order: sheets follow axes/value declaration order (grade: rusty, gilded)
        self.assertEqual([job["out"] for job in jobs], [
            "tmp/packs/t-pack/grade-rusty.png",
            "tmp/packs/t-pack/grade-gilded.png",
        ])

    def test_repeat_call_is_byte_identical(self) -> None:
        config = base_config()
        first = json.dumps(expand(config), ensure_ascii=False, indent=1)
        second = json.dumps(expand(config), ensure_ascii=False, indent=1)
        self.assertEqual(first, second)


class BackgroundMappingTest(unittest.TestCase):
    def test_magenta_omits_background_field_and_uses_ff00ff(self) -> None:
        jobs = expand(base_config())
        for job in jobs:
            self.assertNotIn("background", job)
            self.assertIn("#FF00FF", job["prompt"])

    def test_green_omits_background_field_and_uses_00ff00(self) -> None:
        config = base_config()
        config["background"] = "green"
        jobs = expand(config)
        for job in jobs:
            self.assertNotIn("background", job)
            self.assertIn("#00FF00", job["prompt"])

    def test_transparent_is_a_hard_error(self) -> None:
        config = base_config()
        config["background"] = "transparent"
        with self.assertRaises(SystemExit) as ctx:
            expand(config)
        self.assertIn("REST", str(ctx.exception))


class SanitizationTest(unittest.TestCase):
    def test_cyrillic_spaces_slashes_do_not_break_out_paths(self) -> None:
        config = base_config()
        config["axes"] = {
            "grade": ["Ржавый профиль", "gold/silver style"],
            "shape": ["furnace"],
        }
        config["sheet"] = {"vary": "shape", "grid": [1, 1]}
        jobs = expand(config)
        self.assertEqual(len(jobs), 2)
        path_re = re.compile(r"^[a-z0-9_\-./]+$")
        for job in jobs:
            self.assertRegex(job["out"], path_re)
        # raw values survive untouched in cells[].axes
        self.assertEqual(jobs[0]["cells"][0]["axes"]["grade"], "Ржавый профиль")
        self.assertEqual(jobs[1]["cells"][0]["axes"]["grade"], "gold/silver style")


class AnchorTest(unittest.TestCase):
    def test_anchor_present_adds_input_image_to_every_job(self) -> None:
        with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as handle:
            handle.write(b"fake-png-bytes")
            anchor_path = handle.name
        try:
            config = base_config()
            config["anchor"] = anchor_path
            jobs = expand(config)
            self.assertEqual(len(jobs), 2)
            # anchor is already absolute; expand_jobs still normalizes separators (as_posix)
            expected = Path(anchor_path).as_posix()
            for job in jobs:
                self.assertEqual(job["input_image"], [expected])
        finally:
            os.unlink(anchor_path)

    def test_anchor_missing_file_is_a_loud_error(self) -> None:
        config = base_config()
        config["anchor"] = "tmp/does/not/exist/anchor.png"
        with self.assertRaises(SystemExit):
            expand(config)

    def test_relative_anchor_is_emitted_as_an_absolute_path(self) -> None:
        # CWD-independence proof: a relative anchor resolves against the repo root,
        # not the caller's CWD, and the emitted input_image is always absolute.
        tmp_dir = Path(tempfile.mkdtemp(dir=str(REPO_ROOT / "tmp")))
        anchor_abs = tmp_dir / "anchor.png"
        anchor_abs.write_bytes(b"fake-png-bytes")
        original_cwd = os.getcwd()
        try:
            os.chdir(tempfile.gettempdir())
            config = base_config()
            config["anchor"] = anchor_abs.relative_to(REPO_ROOT).as_posix()
            jobs = expand(config)
            expected = anchor_abs.resolve().as_posix()
            for job in jobs:
                self.assertEqual(job["input_image"], [expected])
                self.assertTrue(os.path.isabs(job["input_image"][0]))
        finally:
            os.chdir(original_cwd)
            shutil.rmtree(tmp_dir)


class CandidatesTest(unittest.TestCase):
    def test_candidates_greater_than_one_suffixes_out_with_cn(self) -> None:
        config = base_config()
        config["axes"] = {"shape": ["furnace", "drill"]}  # 1 sheet only
        config["sheet"] = {"vary": "shape", "grid": [1, 2]}
        config["subject_template"] = "a generator building"
        config["candidates"] = 2
        jobs = expand(config)
        self.assertEqual(len(jobs), 2)
        self.assertEqual(jobs[0]["out"], "tmp/packs/t-pack/t-pack__c1.png")
        self.assertEqual(jobs[1]["out"], "tmp/packs/t-pack/t-pack__c2.png")
        self.assertEqual(jobs[0]["name"], "t-pack (c1)")
        self.assertEqual(jobs[1]["name"], "t-pack (c2)")
        # same sheet -> identical prompt across candidates
        self.assertEqual(jobs[0]["prompt"], jobs[1]["prompt"])


class VaryOnlyEdgeCaseTest(unittest.TestCase):
    def test_zero_big_axes_yields_exactly_one_sheet(self) -> None:
        config = {
            "pack": "solo-pack",
            "style_prefix": "style card verbatim block",
            "subject_template": "a generator building",  # no {slots} needed/allowed
            "axes": {"shape": ["furnace", "drill"]},
            "sheet": {"vary": "shape", "grid": [1, 2]},
            "background": "magenta",
            "out_dir": "tmp/packs/solo-pack",
        }
        jobs = expand(config)
        self.assertEqual(len(jobs), 1)
        self.assertEqual(jobs[0]["out"], "tmp/packs/solo-pack/solo-pack.png")
        self.assertEqual(jobs[0]["name"], "solo-pack")
        self.assertEqual(len(jobs[0]["cells"]), 2)


class ValidationErrorsTest(unittest.TestCase):
    def test_max_jobs_exceeded_reports_count_and_axes(self) -> None:
        config = base_config()
        config["max_jobs"] = 1  # base config expands to 2 sheets x 1 candidate = 2 jobs
        with self.assertRaises(SystemExit) as ctx:
            expand(config)
        message = str(ctx.exception)
        self.assertIn("2", message)
        self.assertIn("max_jobs=1", message)
        self.assertIn("grade", message)
        self.assertIn("shape", message)

    def test_vary_does_not_fit_grid(self) -> None:
        config = base_config()
        config["axes"]["shape"] = ["furnace", "drill", "reactor"]
        config["sheet"]["grid"] = [1, 2]  # only 2 cells for 3 vary values
        with self.assertRaises(SystemExit) as ctx:
            expand(config)
        self.assertIn("grid", str(ctx.exception))

    def test_vary_hard_ceiling_of_nine(self) -> None:
        config = base_config()
        config["axes"]["shape"] = [f"shape{i}" for i in range(10)]  # 10 > hard cap 9
        config["sheet"]["grid"] = [2, 5]  # grid itself would fit 10, cap still applies
        with self.assertRaises(SystemExit) as ctx:
            expand(config)
        self.assertIn("9", str(ctx.exception))

    def test_big_axis_missing_slot_in_subject_template(self) -> None:
        config = base_config()
        config["subject_template"] = "a generator building"  # missing {grade}
        with self.assertRaises(SystemExit) as ctx:
            expand(config)
        self.assertIn("grade", str(ctx.exception))

    def test_unknown_axis_in_sheet_vary(self) -> None:
        config = base_config()
        config["sheet"]["vary"] = "material"  # not a key of axes
        with self.assertRaises(SystemExit) as ctx:
            expand(config)
        self.assertIn("material", str(ctx.exception))

    def test_invalid_gen_size(self) -> None:
        config = base_config()
        config["gen"]["size"] = "2048x2048"
        with self.assertRaises(SystemExit) as ctx:
            expand(config)
        self.assertIn("2048x2048", str(ctx.exception))

    def test_prompt_over_20kb_is_a_loud_error(self) -> None:
        config = base_config()
        config["style_prefix"] = "x" * 25000
        with self.assertRaises(SystemExit) as ctx:
            expand(config)
        self.assertIn("byte", str(ctx.exception))


class SlugCollisionTest(unittest.TestCase):
    def test_big_axis_slug_collision_is_a_loud_error(self) -> None:
        config = base_config()
        # "Rusty!!" and "rusty" both slugify to "rusty" -> same sheet stem/out path.
        config["axes"]["grade"] = ["Rusty!!", "rusty"]
        with self.assertRaises(SystemExit) as ctx:
            expand(config)
        message = str(ctx.exception)
        self.assertIn("Rusty!!", message)
        self.assertIn("rusty", message)

    def test_vary_slug_collision_is_a_loud_error(self) -> None:
        config = base_config()
        # "Furnace!!" and "furnace" both slugify to "furnace" -> slice_pack region
        # names within the sheet would collide (silent _002 mislabel).
        config["axes"]["shape"] = ["Furnace!!", "furnace"]
        with self.assertRaises(SystemExit) as ctx:
            expand(config)
        message = str(ctx.exception)
        self.assertIn("Furnace!!", message)
        self.assertIn("furnace", message)


if __name__ == "__main__":
    unittest.main()
