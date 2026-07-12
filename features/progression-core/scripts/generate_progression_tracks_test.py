#!/usr/bin/env python3
"""Contract tests for progression-track codegen."""

from __future__ import annotations

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


def progression_schema(*, string_max: object = 64) -> dict:
    return {
        "schema": "game_seed.progression",
        "fragment": "progression",
        "string_max": string_max,
        "types": {"TrackState": {"kind": "object", "fields": {}}},
        "fields": {"tracks": {"type": "map<string,TrackState>", "max_count": 32}},
    }


class ProgressionTrackGeneratorTest(unittest.TestCase):
    def run_generator(
        self,
        *,
        track_id: str = "hero",
        schema: dict | None = None,
        include_state_schema: bool = True,
    ) -> tuple[subprocess.CompletedProcess[str], Path, tempfile.TemporaryDirectory[str]]:
        temp = tempfile.TemporaryDirectory()
        game = Path(temp.name) / "games" / "fixture"
        content = game / "content"
        state = game / "state"
        out = game / "build" / "generated"
        content.mkdir(parents=True)
        state.mkdir(parents=True)
        (content / "progression.json").write_text(json.dumps(progression_catalog(track_id)), encoding="utf-8")
        (content / "items.json").write_text(
            json.dumps({"items": [{"id": "coin", "currency": {}}]}), encoding="utf-8"
        )
        (state / "progression.schema.json").write_text(
            json.dumps(schema if schema is not None else progression_schema()), encoding="utf-8"
        )
        command = [
            sys.executable,
            str(SCRIPT),
            "--catalog",
            str(content / "progression.json"),
            "--items",
            str(content / "items.json"),
            "--out-dir",
            str(out),
        ]
        if include_state_schema:
            command.extend(["--state-schema", str(state / "progression.schema.json")])
        result = subprocess.run(command, cwd=game, capture_output=True, text=True, check=False)
        return result, out, temp

    def test_state_schema_argument_is_required(self) -> None:
        result, _out, temp = self.run_generator(include_state_schema=False)
        self.addCleanup(temp.cleanup)
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
                result, _out, temp = self.run_generator(schema=document)
                self.addCleanup(temp.cleanup)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("progression state schema validation", result.stderr)

    def test_track_id_boundary_comes_from_state_schema(self) -> None:
        accepted, _out, accepted_temp = self.run_generator(track_id="abc", schema=progression_schema(string_max=4))
        self.addCleanup(accepted_temp.cleanup)
        self.assertEqual(accepted.returncode, 0, accepted.stderr)

        rejected, _out, rejected_temp = self.run_generator(track_id="abcd", schema=progression_schema(string_max=4))
        self.addCleanup(rejected_temp.cleanup)
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("state schema string_max=4", rejected.stderr)

        multibyte_ok, _out, multibyte_ok_temp = self.run_generator(
            track_id="éé", schema=progression_schema(string_max=5)
        )
        self.addCleanup(multibyte_ok_temp.cleanup)
        self.assertEqual(multibyte_ok.returncode, 0, multibyte_ok.stderr)

        multibyte_bad, _out, multibyte_bad_temp = self.run_generator(
            track_id="ééé", schema=progression_schema(string_max=5)
        )
        self.addCleanup(multibyte_bad_temp.cleanup)
        self.assertNotEqual(multibyte_bad.returncode, 0)
        self.assertIn("6 UTF-8 bytes", multibyte_bad.stderr)

    def test_generated_provenance_uses_game_relative_content_path(self) -> None:
        result, out, temp = self.run_generator()
        self.addCleanup(temp.cleanup)
        self.assertEqual(result.returncode, 0, result.stderr)
        for name in ("progression_tracks.gen.h", "progression_tracks.gen.c"):
            generated = (out / name).read_text(encoding="utf-8")
            self.assertIn("from content/progression.json", generated)
            self.assertNotIn("templates/template/content/progression.json", generated)
            self.assertNotIn(temp.name.replace("\\", "/"), generated.replace("\\", "/"))

    def test_generated_provenance_cannot_terminate_or_split_the_c_comment(self) -> None:
        header = generator.render_header(0, "content/bad*/name\nprogression.json")
        self.assertIn("from content/bad* /name\\nprogression.json", header)
        self.assertEqual(header.count("*/"), 2)  # provenance comment + include-guard footer only


if __name__ == "__main__":
    unittest.main()
