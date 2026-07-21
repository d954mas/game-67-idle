#!/usr/bin/env python3
"""Contract tests for progression-track codegen."""

from __future__ import annotations

import contextlib
import io
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import generate_progression_tracks as generator


SCRIPT = Path(__file__).with_name("generate_progression_tracks.py")


def progression_catalog(track_id: str) -> dict:
    return {
        "namespace": "test",
        "tracks": [
            {
                "id": track_id,
                "mode": "manual",
                "currency_def": "coin",
                "max_level": 1,
                "curve": {"type": "exp", "base": 1, "growth_num": 1, "growth_den": 1},
            }
        ],
    }


def progression_catalog_table(track_id: str, values: list, *, max_level: int | None = None) -> dict:
    """Like progression_catalog(), but with a curve.type 'table' entry. max_level
    defaults to len(values) (the golden/happy-path case); a caller that wants a
    mismatch passes an explicit max_level != len(values)."""
    return {
        "namespace": "test",
        "tracks": [
            {
                "id": track_id,
                "mode": "manual",
                "currency_def": "coin",
                "max_level": max_level if max_level is not None else len(values),
                "curve": {"type": "table", "values": values},
            }
        ],
    }


def progression_schema(*, string_max: object = 64) -> dict:
    return {
        "schema": "game_seed.progression",
        "fragment": "progression",
        "string_max": string_max,
        "types": {"TrackState": {"kind": "object", "fields": {}}},
        "fields": {"tracks": {"type": "map<string,TrackState>", "max_count": 32}},
    }


def items_snapshot(*, schema: str = "items.snapshot.v1") -> dict:
    return {
        "schema": schema,
        "items": [
            {"id": "coin", "kind": "currency", "currency": {}},
        ],
    }


def run_direct(args: list[str]) -> int:
    with contextlib.redirect_stderr(io.StringIO()):
        return generator.main(args)


class ProgressionTrackGeneratorTest(unittest.TestCase):
    def generator_args(
        self,
        *,
        track_id: str = "hero",
        catalog: dict | None = None,
        schema: dict | None = None,
        include_state_schema: bool = True,
    ) -> tuple[list[str], Path, tempfile.TemporaryDirectory[str], Path]:
        temp = tempfile.TemporaryDirectory()
        game = Path(temp.name) / "games" / "fixture"
        content = game / "content"
        state = game / "state"
        out = game / "build" / "generated"
        content.mkdir(parents=True)
        state.mkdir(parents=True)
        (content / "progression.json").write_text(
            json.dumps(catalog if catalog is not None else progression_catalog(track_id)), encoding="utf-8"
        )
        snapshot = game / "build" / "items" / "items.snapshot.json"
        snapshot.parent.mkdir(parents=True)
        snapshot.write_text(json.dumps(items_snapshot()), encoding="utf-8")
        (state / "progression.schema.json").write_text(
            json.dumps(schema if schema is not None else progression_schema()), encoding="utf-8"
        )
        args = [
            "--catalog",
            str(content / "progression.json"),
            "--items-snapshot",
            str(snapshot),
            "--out-dir",
            str(out),
        ]
        if include_state_schema:
            args.extend(["--state-schema", str(state / "progression.schema.json")])
        return args, out, temp, game

    def test_rejects_non_snapshot_items_input(self) -> None:
        args, _out, temp, game = self.generator_args()
        self.addCleanup(temp.cleanup)
        snapshot = game / "build" / "items" / "items.snapshot.json"
        snapshot.write_text(json.dumps(items_snapshot(schema="legacy.items.json")), encoding="utf-8")
        with self.assertRaisesRegex(SystemExit, "items snapshot schema"):
            generator.main(args)

    def test_state_schema_argument_is_required(self) -> None:
        args, _out, temp, game = self.generator_args(include_state_schema=False)
        self.addCleanup(temp.cleanup)
        result = subprocess.run([sys.executable, str(SCRIPT), *args], cwd=game, capture_output=True, text=True, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("--state-schema", result.stderr)

    def test_rejects_wrong_schema_identity_and_shape(self) -> None:
        invalid_documents = [
            {**progression_schema(), "schema": "other"},
            {**progression_schema(), "fragment": "other"},
            {**progression_schema(), "string_max": True},
            {**progression_schema(), "string_max": 1},
            {**progression_schema(), "fields": {"tracks": {"type": "array"}}},
            {**progression_schema(), "types": {"TrackState": {"kind": "scalar"}}},
        ]
        for document in invalid_documents:
            with self.subTest(document=document):
                args, _out, temp, _game = self.generator_args(schema=document)
                self.addCleanup(temp.cleanup)
                with self.assertRaisesRegex(SystemExit, "progression state schema validation"):
                    generator.main(args)

    def test_track_id_boundary_comes_from_state_schema(self) -> None:
        accepted, _out, accepted_temp, _game = self.generator_args(track_id="abc", schema=progression_schema(string_max=4))
        self.addCleanup(accepted_temp.cleanup)
        self.assertEqual(run_direct(accepted), 0)

        rejected, _out, rejected_temp, _game = self.generator_args(track_id="abcd", schema=progression_schema(string_max=4))
        self.addCleanup(rejected_temp.cleanup)
        with self.assertRaisesRegex(SystemExit, "state schema string_max=4"):
            generator.main(rejected)

        multibyte_ok, _out, multibyte_ok_temp, _game = self.generator_args(
            track_id="éé", schema=progression_schema(string_max=5)
        )
        self.addCleanup(multibyte_ok_temp.cleanup)
        self.assertEqual(run_direct(multibyte_ok), 0)

        multibyte_bad, _out, multibyte_bad_temp, _game = self.generator_args(
            track_id="ééé", schema=progression_schema(string_max=5)
        )
        self.addCleanup(multibyte_bad_temp.cleanup)
        with self.assertRaisesRegex(SystemExit, "6 UTF-8 bytes"):
            generator.main(multibyte_bad)

    def test_generated_provenance_uses_game_relative_content_path(self) -> None:
        args, out, temp, _game = self.generator_args()
        self.addCleanup(temp.cleanup)
        self.assertEqual(run_direct(args), 0)
        for name in ("progression_tracks.gen.h", "progression_tracks.gen.c"):
            generated = (out / name).read_text(encoding="utf-8")
            self.assertIn("from content/progression.json", generated)
            self.assertNotIn("templates/template/content/progression.json", generated)
            self.assertNotIn(temp.name.replace("\\", "/"), generated.replace("\\", "/"))

    def test_generated_provenance_cannot_terminate_or_split_the_c_comment(self) -> None:
        header = generator.render_header(0, "content/bad*/name\nprogression.json")
        self.assertIn("from content/bad* /name\\nprogression.json", header)
        self.assertEqual(header.count("*/"), 2)  # provenance comment + include-guard footer only

    # -- curve.type "table" (hand-authored verbatim per-level costs) ----------

    def test_table_curve_bakes_values_verbatim(self) -> None:
        """Golden case: a hand-authored `curve.type: "table"` track bakes its
        `values` VERBATIM into the generated COST_<TRACK>[] array -- no floor,
        no rounding, no reordering (contrast with "exp", which computes)."""
        values = [10, 0, 25, 999999999999, 1]
        catalog = progression_catalog_table("hero", values)
        args, out, temp, _game = self.generator_args(catalog=catalog)
        self.addCleanup(temp.cleanup)
        self.assertEqual(run_direct(args), 0)

        source = (out / "progression_tracks.gen.c").read_text(encoding="utf-8")
        expected_array = "static const int64_t COST_HERO[] = {\n" + "".join(
            f"    {value}LL,\n" for value in values
        ) + "};"
        self.assertIn(expected_array, source)
        self.assertIn(".max_level = 5,", source)
        self.assertIn(".cost_count = 5,", source)

    def test_table_curve_rejects_values_length_mismatch(self) -> None:
        catalog = progression_catalog_table("hero", [1, 2], max_level=3)
        args, _out, temp, _game = self.generator_args(catalog=catalog)
        self.addCleanup(temp.cleanup)
        with self.assertRaisesRegex(
            SystemExit, r"has 2 values but max_level is 3 -- values length must equal max_level"
        ):
            generator.main(args)

    def test_table_curve_rejects_negative_value(self) -> None:
        catalog = progression_catalog_table("hero", [5, -1, 3])
        args, _out, temp, _game = self.generator_args(catalog=catalog)
        self.addCleanup(temp.cleanup)
        with self.assertRaisesRegex(SystemExit, r"curve\.values\[1\] must be a non-negative int64"):
            generator.main(args)

    def test_unknown_curve_type_still_rejected(self) -> None:
        catalog = progression_catalog_table("hero", [1, 2])
        catalog["tracks"][0]["curve"] = {"type": "linear", "values": [1, 2]}
        args, _out, temp, _game = self.generator_args(catalog=catalog)
        self.addCleanup(temp.cleanup)
        with self.assertRaisesRegex(SystemExit, r"supported: 'exp', 'table'"):
            generator.main(args)


if __name__ == "__main__":
    unittest.main()
