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


def cost(item_id: str, count: int) -> dict:
    return {
        "__studio_kind": "cost",
        "count": count,
        "item": {"__studio_kind": "item_ref", "id": item_id},
    }


def costs(*entries: tuple[str, int]) -> dict:
    return {
        "__studio_kind": "costs",
        "entries": [cost(item_id, count) for item_id, count in entries],
    }


def track(
    track_id: str = "hero",
    *,
    mode: str = "manual",
    kind: str = "upgrade",
    rows: list[dict] | None = None,
) -> dict:
    """A snapshot track. Row 0 is the un-upgraded state, so a default track has
    two reachable levels."""
    return {
        "id": track_id,
        "kind": kind,
        "mode": mode,
        "authoring_mode": "table",
        "levels": {
            "mode": "table",
            "rows": rows if rows is not None else [
                {},
                {"cost_to_reach": cost("coin", 50)},
                {"cost_to_reach": cost("coin", 75)},
            ],
        },
    }


def field(
    field_id: str, member: str, field_type: str, *,
    required_for_tracks: list[str] | None = None,
    required_for_items: list[str] | None = None,
) -> dict:
    """A snapshot field. It is a progression column only while it names track kinds."""
    entry = {
        "id": field_id,
        "member": member,
        "type": field_type,
        "section": "level_row",
    }
    if required_for_tracks is not None:
        entry["required_for_tracks"] = required_for_tracks
    if required_for_items is not None:
        entry["required_for_items"] = required_for_items
    return entry


def items_snapshot(
    *,
    schema: str = "items.snapshot.v1",
    tracks: list[dict] | None = None,
    fields: list[dict] | None = None,
) -> dict:
    return {
        "schema": schema,
        "fields": fields if fields is not None else [],
        "items": [{"id": "coin", "kind": "currency", "currency": {}}],
        "tracks": tracks if tracks is not None else [track()],
    }


def progression_schema(
    *, string_max: object = 64, max_count: object = 32, level_max: object = 9999,
) -> dict:
    """The shape of a real game-owned fragment: every storage bound the generator
    must respect is declared here, not duplicated in the generator."""
    return {
        "schema": "game_seed.progression",
        "fragment": "progression",
        "string_max": string_max,
        "types": {"TrackState": {"kind": "object", "fields": {
            "level": {"type": "int", "default": 0, "min": 0, "max": level_max},
            "xp": {"type": "i64", "default": 0, "min": 0, "max": 9000000000000000000},
        }}},
        "fields": {"tracks": {"type": "map<string,TrackState>", "max_count": max_count}},
    }


def run_direct(args: list[str]) -> int:
    with contextlib.redirect_stderr(io.StringIO()):
        return generator.main(args)


class ProgressionTrackGeneratorTest(unittest.TestCase):
    def generator_args(
        self,
        *,
        snapshot: dict | None = None,
        schema: dict | None = None,
        include_state_schema: bool = True,
    ) -> tuple[list[str], Path, tempfile.TemporaryDirectory[str], Path]:
        temp = tempfile.TemporaryDirectory()
        game = Path(temp.name) / "games" / "fixture"
        state = game / "state"
        out = game / "build" / "generated"
        state.mkdir(parents=True)
        snapshot_path = game / "build" / "items" / "items.snapshot.json"
        snapshot_path.parent.mkdir(parents=True)
        snapshot_path.write_text(
            json.dumps(snapshot if snapshot is not None else items_snapshot()), encoding="utf-8"
        )
        (state / "progression.schema.json").write_text(
            json.dumps(schema if schema is not None else progression_schema()), encoding="utf-8"
        )
        args = ["--snapshot", str(snapshot_path), "--out-dir", str(out)]
        if include_state_schema:
            args.extend(["--state-schema", str(state / "progression.schema.json")])
        return args, out, temp, game

    def generate(self, snapshot: dict, *, schema: dict | None = None) -> str:
        args, out, temp, _game = self.generator_args(snapshot=snapshot, schema=schema)
        self.addCleanup(temp.cleanup)
        self.assertEqual(run_direct(args), 0)
        return (out / "progression_tracks.gen.c").read_text(encoding="utf-8")

    # -- input identity ------------------------------------------------------

    def test_rejects_non_snapshot_items_input(self) -> None:
        args, _out, temp, _game = self.generator_args(
            snapshot=items_snapshot(schema="legacy.items.json")
        )
        self.addCleanup(temp.cleanup)
        with self.assertRaisesRegex(SystemExit, "snapshot schema must be 'items.snapshot.v1'"):
            generator.main(args)

    def test_rejects_snapshot_without_a_tracks_section(self) -> None:
        snapshot = items_snapshot()
        del snapshot["tracks"]
        args, _out, temp, _game = self.generator_args(snapshot=snapshot)
        self.addCleanup(temp.cleanup)
        with self.assertRaisesRegex(SystemExit, "snapshot 'tracks' must be an array"):
            generator.main(args)

    def test_state_schema_argument_is_required(self) -> None:
        args, _out, temp, game = self.generator_args(include_state_schema=False)
        self.addCleanup(temp.cleanup)
        result = subprocess.run(
            [sys.executable, str(SCRIPT), *args], cwd=game, capture_output=True, text=True, check=False
        )
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
            progression_schema(max_count=0),
            progression_schema(level_max="lots"),
        ]
        for document in invalid_documents:
            with self.subTest(document=document):
                args, _out, temp, _game = self.generator_args(schema=document)
                self.addCleanup(temp.cleanup)
                with self.assertRaisesRegex(SystemExit, "progression state schema validation"):
                    generator.main(args)

    def test_track_id_boundary_comes_from_state_schema(self) -> None:
        schema = progression_schema(string_max=4)
        accepted, _out, accepted_temp, _game = self.generator_args(
            snapshot=items_snapshot(tracks=[track("abc")]), schema=schema
        )
        self.addCleanup(accepted_temp.cleanup)
        self.assertEqual(run_direct(accepted), 0)

        rejected, _out, rejected_temp, _game = self.generator_args(
            snapshot=items_snapshot(tracks=[track("abcd")]), schema=schema
        )
        self.addCleanup(rejected_temp.cleanup)
        with self.assertRaisesRegex(SystemExit, "state schema string_max=4"):
            generator.main(rejected)

        multibyte_bad, _out, multibyte_temp, _game = self.generator_args(
            snapshot=items_snapshot(tracks=[track("ééé")]), schema=progression_schema(string_max=5)
        )
        self.addCleanup(multibyte_temp.cleanup)
        with self.assertRaisesRegex(SystemExit, "6 UTF-8 bytes"):
            generator.main(multibyte_bad)

    def test_rejects_ids_colliding_on_one_c_identifier(self) -> None:
        args, _out, temp, _game = self.generator_args(
            snapshot=items_snapshot(tracks=[track("a.b"), track("a_b")])
        )
        self.addCleanup(temp.cleanup)
        with self.assertRaisesRegex(SystemExit, "both sanitize to the C identifier"):
            generator.main(args)

    def test_rejects_a_catalog_with_no_tracks(self) -> None:
        """A zero-length k_tracks[] is not valid C, and sizeof over it is worse."""
        args, _out, temp, _game = self.generator_args(snapshot=items_snapshot(tracks=[]))
        self.addCleanup(temp.cleanup)
        with self.assertRaisesRegex(SystemExit, "declares no tracks"):
            generator.main(args)

    def test_rejects_more_tracks_than_the_save_can_hold(self) -> None:
        tracks = [track(f"t{index}") for index in range(3)]
        args, _out, temp, _game = self.generator_args(
            snapshot=items_snapshot(tracks=tracks), schema=progression_schema(max_count=2)
        )
        self.addCleanup(temp.cleanup)
        with self.assertRaisesRegex(SystemExit, "stores at most 2"):
            generator.main(args)

    def test_level_cap_comes_from_the_state_schema(self) -> None:
        rows = [{}] + [{"cost_to_reach": cost("coin", 1)} for _ in range(3)]
        args, _out, temp, _game = self.generator_args(
            snapshot=items_snapshot(tracks=[track(rows=rows)]),
            schema=progression_schema(level_max=2),
        )
        self.addCleanup(temp.cleanup)
        with self.assertRaisesRegex(SystemExit, "caps 'level' at 2"):
            generator.main(args)

    def test_rejects_a_price_longer_than_one_payment(self) -> None:
        """A 17-item price bakes fine and then refuses to pay, so it dies here."""
        wide = costs(*[(f"item{index}", 1) for index in range(17)])
        args, _out, temp, _game = self.generator_args(snapshot=items_snapshot(tracks=[track(rows=[
            {}, {"cost_to_reach": wide},
        ])]))
        self.addCleanup(temp.cleanup)
        with self.assertRaisesRegex(SystemExit, "prices level 1 in 17 items"):
            generator.main(args)

    def test_rejects_a_column_that_would_redefine_the_header_s_own_macro(self) -> None:
        args, _out, temp, _game = self.generator_args(snapshot=items_snapshot(
            fields=[field("up.count", "count", "i64", required_for_tracks=["upgrade"])]))
        self.addCleanup(temp.cleanup)
        with self.assertRaisesRegex(SystemExit, "would redefine PROGRESSION_VALUE_COUNT"):
            generator.main(args)

    def test_two_tracks_render_two_independent_tables(self) -> None:
        source = self.generate(items_snapshot(tracks=[
            track("alpha", rows=[{}, {"cost_to_reach": cost("coin", 7)}]),
            track("beta", rows=[{}, {"cost_to_reach": cost("coin", 9)}]),
        ]))
        self.assertIn("COST_ALPHA_0[] = {", source)
        self.assertIn("COST_BETA_0[] = {", source)
        self.assertIn(".steps = STEPS_ALPHA,", source)
        self.assertIn(".steps = STEPS_BETA,", source)
        self.assertLess(source.index('.id = "alpha",'), source.index('.id = "beta",'))

    def test_rejects_a_track_with_no_reachable_level(self) -> None:
        args, _out, temp, _game = self.generator_args(
            snapshot=items_snapshot(tracks=[track(rows=[{}])])
        )
        self.addCleanup(temp.cleanup)
        with self.assertRaisesRegex(SystemExit, "reaches level 0"):
            generator.main(args)

    # -- level base conversion -----------------------------------------------

    def test_row_one_is_the_first_step_not_a_level(self) -> None:
        """The Snapshot is 1-based (row 0 is the un-upgraded state), the runtime is
        0-based (steps[L] leaves level L). Three rows are two steps."""
        source = self.generate(items_snapshot())
        self.assertIn(".max_level = 2,", source)
        self.assertIn("{ COST_HERO_0, 1, 0LL, NULL, 0 },", source)
        self.assertIn("{ COST_HERO_1, 1, 0LL, NULL, 0 },", source)
        steps = source.split('STEPS_HERO[] = {')[1].split('};')[0]
        # exactly max_level entries: a trailing row would price a level nothing reaches
        self.assertEqual(steps.count('{ COST_HERO'), 2)
        self.assertIn("static const progression_amount_t COST_HERO_0[] = {\n    { \"coin\", 50LL },\n};", source)

    # -- price and grants ----------------------------------------------------

    def test_multi_item_price_bakes_in_declared_order(self) -> None:
        source = self.generate(items_snapshot(tracks=[track(rows=[
            {},
            {"cost_to_reach": costs(("wood", 3), ("coin", 50))},
        ])]))
        self.assertIn(
            'static const progression_amount_t COST_HERO_0[] = {\n'
            '    { "wood", 3LL },\n'
            '    { "coin", 50LL },\n};',
            source,
        )
        self.assertIn("{ COST_HERO_0, 2, 0LL, NULL, 0 },", source)

    def test_free_step_bakes_a_null_price(self) -> None:
        source = self.generate(items_snapshot(tracks=[track(rows=[
            {},
            {"cost_to_reach": {"__studio_kind": "free"}},
        ])]))
        self.assertIn("{ NULL, 0, 0LL, NULL, 0 },", source)

    def test_threshold_track_bakes_xp_instead_of_a_price(self) -> None:
        source = self.generate(items_snapshot(tracks=[track(mode="threshold", rows=[
            {},
            {"xp_to_reach": 120},
        ])]))
        self.assertIn(".mode = PROGRESSION_MODE_THRESHOLD,", source)
        self.assertIn("{ NULL, 0, 120LL, NULL, 0 },", source)

    def test_grants_bake_beside_the_price(self) -> None:
        source = self.generate(items_snapshot(tracks=[track(rows=[
            {},
            {"cost_to_reach": cost("coin", 10), "grants": costs(("gem", 1), ("wood", 2))},
        ])]))
        self.assertIn(
            'static const progression_amount_t GRANTS_HERO_0[] = {\n'
            '    { "gem", 1LL },\n'
            '    { "wood", 2LL },\n};',
            source,
        )
        self.assertIn("{ COST_HERO_0, 1, 0LL, GRANTS_HERO_0, 2 },", source)

    def test_rejects_a_non_positive_amount(self) -> None:
        args, _out, temp, _game = self.generator_args(snapshot=items_snapshot(tracks=[track(rows=[
            {},
            {"cost_to_reach": cost("coin", 0)},
        ])]))
        self.addCleanup(temp.cleanup)
        with self.assertRaisesRegex(SystemExit, "quantity amount must be a positive int64"):
            generator.main(args)

    # -- columns -------------------------------------------------------------

    def test_only_fields_a_track_kind_requires_become_columns(self) -> None:
        """A field no track kind requires belongs to the item catalog; the two
        dictionaries are disjoint by construction, not by convention."""
        snapshot = items_snapshot(fields=[
            field("gear.damage", "damage", "i64", required_for_items=["weapon"]),
            field("up.rate", "rate", "i64", required_for_tracks=["upgrade"]),
        ])
        args, out, temp, _game = self.generator_args(snapshot=snapshot)
        self.addCleanup(temp.cleanup)
        self.assertEqual(run_direct(args), 0)
        header = (out / "progression_tracks.gen.h").read_text(encoding="utf-8")
        self.assertIn("#define PROGRESSION_VALUE_COUNT 1", header)
        self.assertIn("#define PROGRESSION_VALUE_RATE ((progression_value_t)0u)", header)
        self.assertNotIn("PROGRESSION_VALUE_DAMAGE", header)

    def test_an_item_kind_named_after_a_track_kind_carries_nothing_over(self) -> None:
        """The two spaces name their kinds independently, so an item column named
        after this track's kind is still an item column."""
        snapshot = items_snapshot(fields=[
            field("gear.rate", "gear_rate", "i64", required_for_items=["upgrade"]),
            field("up.rate", "rate", "i64", required_for_tracks=["upgrade"]),
        ])
        args, out, temp, _game = self.generator_args(snapshot=snapshot)
        self.addCleanup(temp.cleanup)
        self.assertEqual(run_direct(args), 0)
        header = (out / "progression_tracks.gen.h").read_text(encoding="utf-8")
        self.assertIn("#define PROGRESSION_VALUE_COUNT 1", header)
        self.assertNotIn("PROGRESSION_VALUE_GEAR_RATE", header)

    def test_column_index_follows_field_id_order(self) -> None:
        snapshot = items_snapshot(fields=[
            field("up.rate", "rate", "i64", required_for_tracks=["upgrade"]),
            field("up.cap", "cap", "i64", required_for_tracks=["upgrade"]),
        ])
        args, out, temp, _game = self.generator_args(snapshot=snapshot)
        self.addCleanup(temp.cleanup)
        self.assertEqual(run_direct(args), 0)
        header = (out / "progression_tracks.gen.h").read_text(encoding="utf-8")
        self.assertIn("#define PROGRESSION_VALUE_CAP ((progression_value_t)0u)", header)
        self.assertIn("#define PROGRESSION_VALUE_RATE ((progression_value_t)1u)", header)

    def test_exact_and_fractional_columns_split_into_two_tables(self) -> None:
        snapshot = items_snapshot(
            fields=[
                field("up.cap", "cap", "i64", required_for_tracks=["upgrade"]),
                field("up.rate", "rate", "f64", required_for_tracks=["upgrade"]),
            ],
            tracks=[track(rows=[
                {"cap": 0, "rate": 0.0},
                {"cap": 7, "rate": 1.5, "cost_to_reach": cost("coin", 50)},
            ])],
        )
        args, out, temp, _game = self.generator_args(snapshot=snapshot)
        self.addCleanup(temp.cleanup)
        self.assertEqual(run_direct(args), 0)
        header = (out / "progression_tracks.gen.h").read_text(encoding="utf-8")
        source = (out / "progression_tracks.gen.c").read_text(encoding="utf-8")
        self.assertIn("#define PROGRESSION_VALUE_COUNT 2", header)
        # Both tables carry every column so one index reads both; the read that
        # does not answer for a column is the assert, not a second index space.
        self.assertIn("const bool k_progression_value_exact[] = {\n    true,\n    false,\n};", source)
        self.assertIn("static const int64_t EXACT_HERO[] = {\n    0LL, 0LL,\n    7LL, 0LL,\n};", source)
        self.assertIn("static const double FRACTIONAL_HERO[] = {\n    0.0, 0.0,\n    0.0, 1.5,\n};", source)
        self.assertIn(".exact = EXACT_HERO,", source)
        self.assertIn(".fractional = FRACTIONAL_HERO,", source)
        self.assertIn(".value_count = 2u,", source)

    def test_fractional_literal_reads_back_exactly(self) -> None:
        """The value is computed once on the build machine; the literal must
        round-trip or the runtime silently reads a different number."""
        value = 0.1 + 0.2
        snapshot = items_snapshot(
            fields=[field("up.rate", "rate", "f64", required_for_tracks=["upgrade"])],
            tracks=[track(rows=[{"rate": 0.0}, {"rate": value, "cost_to_reach": cost("coin", 1)}])],
        )
        source = self.generate(snapshot)
        literal = source.split("FRACTIONAL_HERO[] = {")[1].split("};")[0].strip().split("\n")[1].strip(" ,")
        self.assertEqual(float(literal), value)

    def test_a_catalog_without_columns_still_compiles(self) -> None:
        """A zero-length array is invalid C, so the empty dictionary emits a
        one-element table nothing indexes."""
        args, out, temp, _game = self.generator_args()
        self.addCleanup(temp.cleanup)
        self.assertEqual(run_direct(args), 0)
        header = (out / "progression_tracks.gen.h").read_text(encoding="utf-8")
        source = (out / "progression_tracks.gen.c").read_text(encoding="utf-8")
        self.assertIn("#define PROGRESSION_VALUE_COUNT 0", header)
        self.assertIn("const bool k_progression_value_exact[1] = { false };", source)
        self.assertIn(".exact = NULL,", source)
        self.assertIn(".fractional = NULL,", source)
        self.assertIn(".value_count = 0u,", source)

    def test_a_column_belongs_to_the_kinds_that_declare_it(self) -> None:
        """Two kinds with different column sets: the dictionary is the union, and each
        track's mask says which slots it actually answers for."""
        snapshot = items_snapshot(
            fields=[
                field("up.rate", "rate", "i64", required_for_tracks=["upgrade"]),
                field("ms.reward", "reward", "i64", required_for_tracks=["milestone"]),
            ],
            tracks=[
                track("a", kind="upgrade", rows=[
                    {"rate": 0}, {"rate": 3, "cost_to_reach": cost("coin", 50)},
                ]),
                track("b", kind="milestone", rows=[
                    {"reward": 0}, {"reward": 9, "cost_to_reach": cost("coin", 50)},
                ]),
            ],
        )
        args, out, temp, _game = self.generator_args(snapshot=snapshot)
        self.addCleanup(temp.cleanup)
        self.assertEqual(run_direct(args), 0)
        source = (out / "progression_tracks.gen.c").read_text(encoding="utf-8")
        self.assertIn(".owned_values = (UINT64_C(1) << PROGRESSION_VALUE_RATE),", source)
        self.assertIn(".owned_values = (UINT64_C(1) << PROGRESSION_VALUE_REWARD),", source)
        # The unowned slot is still a slot: one stride reads both tracks' rows.
        self.assertIn("static const int64_t EXACT_A[] = {\n    0LL, 0LL,\n    0LL, 3LL,\n};", source)
        self.assertIn("static const int64_t EXACT_B[] = {\n    0LL, 0LL,\n    9LL, 0LL,\n};", source)

    def test_a_track_owning_every_column_ors_the_whole_mask(self) -> None:
        snapshot = items_snapshot(
            fields=[
                field("up.cap", "cap", "i64", required_for_tracks=["upgrade"]),
                field("up.rate", "rate", "f64", required_for_tracks=["upgrade"]),
            ],
            tracks=[track(rows=[
                {"cap": 0, "rate": 0.0},
                {"cap": 7, "rate": 1.5, "cost_to_reach": cost("coin", 50)},
            ])],
        )
        source = self.generate(snapshot)
        self.assertIn(
            ".owned_values = (UINT64_C(1) << PROGRESSION_VALUE_CAP) | "
            "(UINT64_C(1) << PROGRESSION_VALUE_RATE),",
            source,
        )

    def test_a_track_kind_with_no_columns_owns_nothing(self) -> None:
        """A threshold track priced only in xp declares no column, and says so rather
        than pointing at the dictionary's first slot."""
        snapshot = items_snapshot(
            fields=[field("up.rate", "rate", "i64", required_for_tracks=["upgrade"])],
            tracks=[
                track("a", kind="upgrade", rows=[
                    {"rate": 0}, {"rate": 3, "cost_to_reach": cost("coin", 50)},
                ]),
                track("b", kind="rank", mode="threshold", rows=[{}, {"xp_to_reach": 100}]),
            ],
        )
        source = self.generate(snapshot)
        self.assertIn(".owned_values = 0u,", source)

    def test_rejects_more_columns_than_the_ownership_mask_holds(self) -> None:
        snapshot = items_snapshot(
            fields=[
                field(f"up.c{index}", f"c{index}", "i64", required_for_tracks=["upgrade"])
                for index in range(65)
            ],
            tracks=[track(rows=[
                {f"c{index}": 0 for index in range(65)},
                {**{f"c{index}": 1 for index in range(65)}, "cost_to_reach": cost("coin", 50)},
            ])],
        )
        args, _out, temp, _game = self.generator_args(snapshot=snapshot)
        self.addCleanup(temp.cleanup)
        with self.assertRaisesRegex(SystemExit, "at most 64 progression columns"):
            generator.main(args)

    def test_rejects_a_column_that_is_neither_exact_nor_fractional(self) -> None:
        snapshot = items_snapshot(
            fields=[field("up.label", "label", "string", required_for_tracks=["upgrade"])]
        )
        args, _out, temp, _game = self.generator_args(snapshot=snapshot)
        self.addCleanup(temp.cleanup)
        with self.assertRaisesRegex(SystemExit, "must be i64 or f64"):
            generator.main(args)

    # -- output --------------------------------------------------------------

    def test_generated_output_carries_no_build_machine_path(self) -> None:
        args, out, temp, _game = self.generator_args()
        self.addCleanup(temp.cleanup)
        self.assertEqual(run_direct(args), 0)
        for name in ("progression_tracks.gen.h", "progression_tracks.gen.c"):
            generated = (out / name).read_text(encoding="utf-8")
            self.assertIn("from the Items Snapshot tracks section", generated)
            self.assertNotIn(temp.name.replace("\\", "/"), generated.replace("\\", "/"))

    def test_rerun_over_unchanged_input_rewrites_nothing(self) -> None:
        args, out, temp, _game = self.generator_args()
        self.addCleanup(temp.cleanup)
        self.assertEqual(run_direct(args), 0)
        stamps = {p.name: p.stat().st_mtime_ns for p in out.iterdir()}
        self.assertEqual(run_direct(args), 0)
        self.assertEqual({p.name: p.stat().st_mtime_ns for p in out.iterdir()}, stamps)


if __name__ == "__main__":
    unittest.main()
