"""Tests for features/items-core/scripts/items_ops.py's destructive-change guard (T0327 И2c
deep-review follow-up, F4). Precedent: features/game-state/scripts/
generate_state_test.py (unittest against temp fixtures, module imported
in-process rather than shelled out).

Runs as a ctest target (CMakeLists.txt `items_ops_test`) alongside
`items_ops_validate` (F1) -- this file is the committed PROOF that the lock-
workflow rules themselves actually fire correctly; `items_ops_validate` is
the gate that catches a REAL destructive change to the template's own
content/items.json. Neither touches the real committed content/* files:
every test builds its own temp catalog/lock/state-schema triple (the real
content/item_fields.schema.json IS reused -- it is the generic, stable field
shape contract, not something these tests are exercising).

Runs as a ctest via CMakeLists.txt's `items_ops_test` target (Python3_EXECUTABLE
+ "${ITEMS_CORE_SCRIPTS}/items_ops_test.py", WORKING_DIRECTORY = the game's
template root), or directly via pytest from the repo root:

    node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_ops_test.py
"""

from __future__ import annotations

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from typing import Any

import items_ops

TEMPLATE_ROOT = Path(__file__).resolve().parents[3] / "templates" / "template"
FIELD_SCHEMA = TEMPLATE_ROOT / "content" / "item_fields.schema.json"


def _write(path: Path, obj: Any) -> Path:
    path.write_text(json.dumps(obj), encoding="utf-8")
    return path


def make_item(item_id: str, *, created: str = "2026-07-07", **overrides: Any) -> dict:
    item = {
        "id": item_id,
        "created": created,
        "display_name": item_id,
        "icon_asset_id": f"icon.{item_id}",
        "kind": "material",
        "tags": [],
        "base_value": 1,
        "stack": 999,
    }
    item.update(overrides)
    return item


def make_catalog(items: list[dict], namespace: str = "tmpl") -> dict:
    return {
        "schema": "game_seed.items_catalog",
        "namespace": namespace,
        "item_kinds": [{"id": "material", "label": "Material"}],
        "containers": [{"id": "backpack", "capacity": 20, "accept_policy": "any", "hidden": False}],
        "items": items,
    }


def make_lock(def_ids: list[str], removed: dict | None = None, schema_version: int = 2) -> dict:
    return {
        "schema": "game_seed.items_lock",
        "schema_version": schema_version,
        "def_ids": def_ids,
        "removed": removed if removed is not None else {},
    }


def make_state_schema(version: int) -> dict:
    return {
        "schema": "game_seed.items",
        "schema_version": 2,
        "fragment": "items",
        "version": version,
        "string_max": 64,
        "hooks": {"on_new_game": True, "reconcile": True},
        "types": {},
        "fields": {},
        "events": {},
    }


def run_validate(tmp_path: Path, catalog: dict, lock: Any, state_version: int) -> tuple[int, dict | None, str]:
    """Runs `items_ops.py validate --json` in-process (items_ops.main() takes
    argv directly, same pattern as generate_state.main() in
    generate_state_test.py) against temp fixtures. Returns
    (exit_code, parsed_stdout_json_or_None, stderr_text). `lock` is written
    as-is (may be deliberately malformed, e.g. test_malformed_lock_*) --
    callers use make_lock() for a well-shaped one."""
    catalog_path = _write(tmp_path / "catalog.json", catalog)
    lock_path = _write(tmp_path / "lock.json", lock)
    state_path = _write(tmp_path / "state.json", make_state_schema(state_version))
    argv = [
        "validate",
        "--catalog", str(catalog_path),
        "--schema", str(FIELD_SCHEMA),
        "--baseline", str(lock_path),
        "--state-schema", str(state_path),
        "--json",
    ]
    stdout = io.StringIO()
    stderr = io.StringIO()
    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
        rc = items_ops.main(argv)
    stdout_text = stdout.getvalue()
    payload = json.loads(stdout_text) if stdout_text.strip() else None
    return rc, payload, stderr.getvalue()


def error_rules(payload: dict) -> set[str]:
    return {e["rule"] for e in payload["errors"]}


def run_list(catalog_path: Path) -> tuple[int, dict | None, str]:
    """Runs `items_ops.py list --json` in-process, mirroring run_validate() above."""
    argv = ["list", "--catalog", str(catalog_path), "--json"]
    stdout = io.StringIO()
    stderr = io.StringIO()
    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
        rc = items_ops.main(argv)
    stdout_text = stdout.getvalue()
    payload = json.loads(stdout_text) if stdout_text.strip() else None
    return rc, payload, stderr.getvalue()


class LockWorkflowTests(unittest.TestCase):
    """(a)-(e) from the deep-review F4 packet."""

    def test_removed_without_reaction(self):
        # (a) tmpl.b was shipped (in def_ids) but is missing from the catalog
        # and has NO entry in `removed` yet -- must fail loud, not silently.
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            catalog = make_catalog([make_item("tmpl.a")])
            lock = make_lock(def_ids=["tmpl.a", "tmpl.b"])
            rc, payload, _ = run_validate(tmp_path, catalog, lock, state_version=1)

        self.assertEqual(rc, 1)
        self.assertIn("removed-without-reaction", error_rules(payload))
        hit = next(e for e in payload["errors"] if e["rule"] == "removed-without-reaction")
        self.assertEqual(hit["id"], "tmpl.b")

    def test_removed_version_not_shipped(self):
        # (b) tmpl.b's removal receipt claims fragment_version=2, but the
        # items fragment schema is still at version 1 -- declared, not
        # delivered.
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            catalog = make_catalog([make_item("tmpl.a")])
            lock = make_lock(def_ids=["tmpl.a"], removed={"tmpl.b": {"fragment_version": 2, "note": "test"}})
            rc, payload, _ = run_validate(tmp_path, catalog, lock, state_version=1)

        self.assertEqual(rc, 1)
        self.assertIn("removed-version-not-shipped", error_rules(payload))
        hit = next(e for e in payload["errors"] if e["rule"] == "removed-version-not-shipped")
        self.assertEqual(hit["id"], "tmpl.b")

    def test_lock_inconsistent(self):
        # (c) tmpl.b listed in BOTH def_ids and removed simultaneously.
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            catalog = make_catalog([make_item("tmpl.a"), make_item("tmpl.b")])
            lock = make_lock(def_ids=["tmpl.a", "tmpl.b"], removed={"tmpl.b": {"fragment_version": 2}})
            rc, payload, _ = run_validate(tmp_path, catalog, lock, state_version=2)

        self.assertEqual(rc, 1)
        self.assertIn("lock-inconsistent", error_rules(payload))
        hit = next(e for e in payload["errors"] if e["rule"] == "lock-inconsistent")
        self.assertEqual(hit["id"], "tmpl.b")

    def test_malformed_lock_def_ids_not_a_list_is_io_error(self):
        # (d) F3: a broken lock (def_ids not a list) must be an IO error
        # (exit 2), never a silent guard-disable that validates green.
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            catalog = make_catalog([make_item("tmpl.a")])
            lock = {"schema": "game_seed.items_lock", "schema_version": 2, "def_ids": "tmpl.a", "removed": {}}
            rc, payload, stderr_text = run_validate(tmp_path, catalog, lock, state_version=1)

        self.assertEqual(rc, 2)
        self.assertIsNone(payload)  # OpsError bypasses the --json envelope entirely
        self.assertIn("def_ids", stderr_text)

    def test_malformed_lock_wrong_schema_version_is_io_error(self):
        # (d, second shape) F3 also covers a lock format v1 leftover
        # (no `removed` key at all yet) or any other schema_version mismatch.
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            catalog = make_catalog([make_item("tmpl.a")])
            lock = {"schema": "game_seed.items_lock", "def_ids": ["tmpl.a"]}  # no schema_version, no removed
            rc, payload, stderr_text = run_validate(tmp_path, catalog, lock, state_version=1)

        self.assertEqual(rc, 2)
        self.assertIsNone(payload)
        self.assertIn("schema_version", stderr_text)

    def test_happy_batch_path(self):
        # (e) tmpl.b and tmpl.c were removed together in one release, sharing
        # ONE fragment_version -- batching (README "Lock workflow"), never
        # requires distinct versions per removed entry. Version already
        # shipped (state_version == fragment_version) -> clean green.
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            catalog = make_catalog([make_item("tmpl.a")])
            lock = make_lock(
                def_ids=["tmpl.a"],
                removed={
                    "tmpl.b": {"fragment_version": 3, "note": "batch-removed, no compensation"},
                    "tmpl.c": {"fragment_version": 3},  # note is optional
                },
            )
            rc, payload, _ = run_validate(tmp_path, catalog, lock, state_version=3)

        self.assertEqual(rc, 0)
        self.assertEqual(payload["errors"], [])

    def test_removed_def_restored_is_warning_not_error(self):
        # Restoration (a removed id reappears in the catalog) is legal --
        # warning only, ok stays true.
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            catalog = make_catalog([make_item("tmpl.a"), make_item("tmpl.b")])
            lock = make_lock(def_ids=["tmpl.a"], removed={"tmpl.b": {"fragment_version": 2}})
            rc, payload, _ = run_validate(tmp_path, catalog, lock, state_version=2)

        self.assertEqual(rc, 0)
        self.assertEqual(payload["errors"], [])
        self.assertIn("removed-def-restored", {w["rule"] for w in payload["warnings"]})


class CreatedFieldTests(unittest.TestCase):
    """Lead reversal 2026-07-07: `created` (ISO date) is required authoring
    data on every item def."""

    def test_created_missing(self):
        item = make_item("tmpl.a")
        del item["created"]
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            catalog = make_catalog([item])
            lock = make_lock(def_ids=[])
            rc, payload, _ = run_validate(tmp_path, catalog, lock, state_version=1)

        self.assertEqual(rc, 1)
        self.assertIn("created-missing", error_rules(payload))

    def test_created_malformed(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            catalog = make_catalog([make_item("tmpl.a", created="07/07/2026")])
            lock = make_lock(def_ids=[])
            rc, payload, _ = run_validate(tmp_path, catalog, lock, state_version=1)

        self.assertEqual(rc, 1)
        self.assertIn("created-invalid", error_rules(payload))

    def test_created_not_a_real_calendar_date(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            catalog = make_catalog([make_item("tmpl.a", created="2026-02-30")])  # right shape, no such day
            lock = make_lock(def_ids=[])
            rc, payload, _ = run_validate(tmp_path, catalog, lock, state_version=1)

        self.assertEqual(rc, 1)
        self.assertIn("created-invalid", error_rules(payload))


class StackFieldTests(unittest.TestCase):
    """Locks the collapsed single-int stack model (build_spec_stack_int_2026-07-08):
    schema requires `stack` to be a plain int >= 0, `equip` implies `stack == 1`,
    and `list --json` emits the raw int (never the old derived object)."""

    def test_stack_missing(self):
        item = make_item("tmpl.a")
        del item["stack"]
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            catalog = make_catalog([item])
            lock = make_lock(def_ids=[])
            rc, payload, _ = run_validate(tmp_path, catalog, lock, state_version=1)

        self.assertEqual(rc, 1)
        self.assertIn("generator-check", error_rules(payload))

    def test_stack_negative(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            catalog = make_catalog([make_item("tmpl.a", stack=-1)])
            lock = make_lock(def_ids=[])
            rc, payload, _ = run_validate(tmp_path, catalog, lock, state_version=1)

        self.assertEqual(rc, 1)
        self.assertIn("generator-check", error_rules(payload))

    def test_stack_bool_is_not_int(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            catalog = make_catalog([make_item("tmpl.a", stack=True)])
            lock = make_lock(def_ids=[])
            rc, payload, _ = run_validate(tmp_path, catalog, lock, state_version=1)

        self.assertEqual(rc, 1)
        self.assertIn("generator-check", error_rules(payload))

    def test_stack_old_object_shape_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            catalog = make_catalog([make_item("tmpl.a", stack={"stackable": True})])
            lock = make_lock(def_ids=[])
            rc, payload, _ = run_validate(tmp_path, catalog, lock, state_version=1)

        self.assertEqual(rc, 1)
        self.assertIn("generator-check", error_rules(payload))

    def test_equip_stack_unlimited_is_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            item = make_item("tmpl.a", stack=0, equip={"slot": "weapon"})
            catalog = make_catalog([item])
            lock = make_lock(def_ids=[])
            rc, payload, _ = run_validate(tmp_path, catalog, lock, state_version=1)

        self.assertEqual(rc, 1)
        self.assertIn("equip-stack", error_rules(payload))

    def test_equip_stack_one_is_clean(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            item = make_item("tmpl.a", stack=1, equip={"slot": "weapon"})
            catalog = make_catalog([item])
            lock = make_lock(def_ids=[])
            rc, payload, _ = run_validate(tmp_path, catalog, lock, state_version=1)

        self.assertEqual(rc, 0)
        self.assertEqual(payload["errors"], [])

    def test_list_json_emits_raw_int(self):
        # Pin: item_json_record must emit `stack` as the raw authored int, not
        # the old derived {"stackable", "max_stack", "unlimited"} object -- the
        # viewer renders it through the schema-v2 i64 path and would print
        # "[object Object]" on every card if this regressed.
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            catalog = make_catalog([
                make_item("tmpl.a", stack=0),
                make_item("tmpl.b", stack=99),
            ])
            catalog_path = _write(tmp_path / "catalog.json", catalog)
            rc, payload, _ = run_list(catalog_path)

        self.assertEqual(rc, 0)
        by_id = {rec["id"]: rec for rec in payload["items"]}
        self.assertIsInstance(by_id["tmpl.a"]["stack"], int)
        self.assertEqual(by_id["tmpl.a"]["stack"], 0)
        self.assertIsInstance(by_id["tmpl.b"]["stack"], int)
        self.assertEqual(by_id["tmpl.b"]["stack"], 99)


if __name__ == "__main__":
    unittest.main()
