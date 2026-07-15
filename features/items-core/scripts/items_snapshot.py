#!/usr/bin/env python3
"""Build and inspect a deterministic Items Snapshot from evaluator JSON."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
from typing import Any


EVALUATION_SCHEMA = "items.lua.evaluation.v1"
SNAPSHOT_SCHEMA = "items.snapshot.v1"
QUERY_SCHEMA = "items.snapshot.query.v1"
DEFAULT_QUERY_ROWS = 1_000


class SnapshotFailure(Exception):
    def __init__(self, code: str, message: str, path: str = "$") -> None:
        super().__init__(f"{code}: {message}")
        self.code = code
        self.message = message
        self.path = path


def _fail(code: str, message: str, path: str = "$") -> None:
    raise SnapshotFailure(code, message, path)


class SnapshotArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        _fail("cli.arguments", message, "$.arguments")


def _canonical(value: Any, path: str = "$") -> Any:
    if value is None or isinstance(value, (bool, str, int)):
        return value
    if isinstance(value, float):
        if not math.isfinite(value):
            _fail("snapshot.non_finite", "value must be finite", path)
        return value
    if isinstance(value, list):
        return [_canonical(child, f"{path}[{index}]") for index, child in enumerate(value)]
    if isinstance(value, dict):
        if not all(isinstance(key, str) for key in value):
            _fail("snapshot.key_type", "object keys must be strings", path)
        return {
            key: _canonical(value[key], f"{path}.{key}")
            for key in sorted(value)
        }
    _fail("snapshot.value_type", f"unsupported value type: {type(value).__name__}", path)


def _json_bytes(value: Any) -> bytes:
    return json.dumps(
        value, ensure_ascii=False, sort_keys=True, separators=(",", ":"),
    ).encode("utf-8")


def _references(value: Any) -> set[str]:
    found: set[str] = set()
    if isinstance(value, list):
        for child in value:
            found.update(_references(child))
    elif isinstance(value, dict):
        if value.get("__studio_kind") == "item_ref":
            item_id = value.get("id")
            if not isinstance(item_id, str) or not item_id:
                _fail("snapshot.reference_id", "item_ref requires a non-empty id")
            found.add(item_id)
        for child in value.values():
            found.update(_references(child))
    return found


def build_snapshot(evaluation: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(evaluation, dict) or evaluation.get("schema") != EVALUATION_SCHEMA:
        _fail("snapshot.evaluation_schema", f"expected {EVALUATION_SCHEMA}")
    raw_items = evaluation.get("items")
    if not isinstance(raw_items, list):
        _fail("snapshot.items", "evaluation items must be a list", "$.items")

    items: list[dict[str, Any]] = []
    seen: set[str] = set()
    for index, raw_item in enumerate(raw_items):
        path = f"$.items[{index}]"
        if not isinstance(raw_item, dict):
            _fail("snapshot.item", "item must be an object", path)
        item = _canonical(raw_item, path)
        item_id = item.get("id")
        if not isinstance(item_id, str) or not item_id:
            _fail("snapshot.item_id", "item requires a non-empty id", f"{path}.id")
        if item_id in seen:
            _fail("snapshot.duplicate_item", f"duplicate item id: {item_id}", f"{path}.id")
        seen.add(item_id)
        items.append(item)
    items.sort(key=lambda item: item["id"])

    dependencies: dict[str, list[str]] = {}
    for item in items:
        item_id = item["id"]
        refs = sorted(_references(item))
        unknown = [ref for ref in refs if ref not in seen]
        if unknown:
            _fail(
                "snapshot.unknown_reference",
                f"{item_id} references unknown item: {unknown[0]}",
                f"$.items.{item_id}",
            )
        dependencies[item_id] = refs

    dependents = {item_id: [] for item_id in sorted(seen)}
    for item_id, refs in dependencies.items():
        for ref in refs:
            dependents[ref].append(item_id)
    for refs in dependents.values():
        refs.sort()

    content_hash = "sha256:" + hashlib.sha256(_json_bytes({
        "schema": SNAPSHOT_SCHEMA,
        "items": items,
    })).hexdigest()
    snapshot = {
        "schema": SNAPSHOT_SCHEMA,
        "content_hash": content_hash,
        "items": items,
        "dependencies": dependencies,
        "dependents": dependents,
    }
    backend = evaluation.get("backend")
    if backend is not None:
        if not isinstance(backend, dict):
            _fail("snapshot.backend", "evaluation backend must be an object", "$.backend")
        snapshot["evaluator"] = _canonical(backend, "$.backend")
    return snapshot


def query_snapshot(
    snapshot: dict[str, Any],
    *,
    item_id: str,
    field: str | None = None,
    level_from: int | None = None,
    level_to: int | None = None,
    include_inputs: bool = False,
    include_dependents: bool = False,
    max_rows: int = DEFAULT_QUERY_ROWS,
) -> dict[str, Any]:
    if not isinstance(snapshot, dict) or snapshot.get("schema") != SNAPSHOT_SCHEMA:
        _fail("query.snapshot_schema", f"expected {SNAPSHOT_SCHEMA}")
    if max_rows < 1:
        _fail("query.max_rows", "max_rows must be positive")
    items = snapshot.get("items")
    if not isinstance(items, list):
        _fail("query.items", "snapshot items must be a list", "$.items")
    item = None
    for index, candidate in enumerate(items):
        if not isinstance(candidate, dict):
            _fail("query.item", "snapshot item must be an object", f"$.items[{index}]")
        if candidate.get("id") == item_id:
            item = candidate
    if item is None:
        _fail("query.item_not_found", f"unknown item: {item_id}", "$.item")

    levels = item.get("levels", {})
    if not isinstance(levels, dict):
        _fail("query.levels", "item levels must be an object", f"$.items.{item_id}.levels")
    rows = levels.get("rows", [])
    if not isinstance(rows, list):
        _fail("query.levels", "item levels.rows must be a list", f"$.items.{item_id}.levels.rows")
    if not rows:
        if level_from is not None or level_to is not None:
            _fail("query.level_range", "item has only 0 levels")
        start, selected = 1, []
    else:
        start = 1 if level_from is None else level_from
        end = len(rows) if level_to is None else level_to
        if start < 1 or end < start:
            _fail("query.level_range", "level range must be positive and ordered")
        if end > len(rows):
            _fail("query.level_range", f"item has only {len(rows)} levels")
        selected = rows[start - 1:end]
    if len(selected) > max_rows:
        _fail("query.row_limit", f"query exceeds {max_rows} level rows; provide a smaller range")

    result_rows = []
    field_found = field is None or field in item
    for level, row in enumerate(selected, start=start):
        if not isinstance(row, dict):
            _fail("query.level_row", "level row must be an object")
        if field is None:
            values = row
        elif field in row:
            values = {field: row[field]}
            field_found = True
        else:
            values = {}
        result_rows.append({"level": level, "values": values})
    if field is not None and not field_found:
        _fail("query.field_not_found", f"unknown field for {item_id}: {field}", "$.field")

    item_result: dict[str, Any] = {"id": item_id}
    top_level = {
        key: value for key, value in item.items()
        if key not in {"id", "levels"}
    }
    if field is None and top_level:
        item_result["values"] = top_level
    elif field is not None and field in item:
        item_result["values"] = {field: item[field]}
    if rows or level_from is not None or level_to is not None:
        item_result["levels"] = result_rows
    result: dict[str, Any] = {
        "schema": QUERY_SCHEMA,
        "content_hash": snapshot.get("content_hash"),
        "item": item_result,
    }
    if include_inputs:
        dependencies = snapshot.get("dependencies", {})
        if not isinstance(dependencies, dict) or not isinstance(dependencies.get(item_id, []), list):
            _fail("query.dependencies", "snapshot dependencies must map item ids to lists")
        result["inputs"] = dependencies.get(item_id, [])
    if include_dependents:
        dependents = snapshot.get("dependents", {})
        if not isinstance(dependents, dict) or not isinstance(dependents.get(item_id, []), list):
            _fail("query.dependents", "snapshot dependents must map item ids to lists")
        result["dependents"] = dependents.get(item_id, [])
    return result


def _load(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        _fail("snapshot.read", f"cannot read {path}: {error}")
    if not isinstance(value, dict):
        _fail("snapshot.root", "JSON root must be an object")
    return value


def _write_if_different(path: Path, payload: dict[str, Any]) -> None:
    encoded = _json_bytes(payload) + b"\n"
    temporary = path.with_name(path.name + ".tmp")
    try:
        if path.exists() and path.read_bytes() == encoded:
            return
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary.write_bytes(encoded)
        temporary.replace(path)
    except OSError as error:
        _fail("snapshot.write", f"cannot write {path}: {error}")


def main(argv: list[str] | None = None) -> int:
    parser = SnapshotArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    build = subparsers.add_parser("build")
    build.add_argument("--evaluation", type=Path, required=True)
    build.add_argument("--out", type=Path, required=True)
    query = subparsers.add_parser("query")
    query.add_argument("--snapshot", type=Path, required=True)
    query.add_argument("--item", required=True)
    query.add_argument("--field")
    query.add_argument("--level-from", type=int)
    query.add_argument("--level-to", type=int)
    query.add_argument("--inputs", action="store_true")
    query.add_argument("--dependents", action="store_true")
    query.add_argument("--max-rows", type=int, default=DEFAULT_QUERY_ROWS)
    try:
        args = parser.parse_args(argv)
        if args.command == "build":
            _write_if_different(args.out, build_snapshot(_load(args.evaluation)))
        else:
            payload = query_snapshot(
                _load(args.snapshot), item_id=args.item, field=args.field,
                level_from=args.level_from, level_to=args.level_to,
                include_inputs=args.inputs, include_dependents=args.dependents,
                max_rows=args.max_rows,
            )
            print(_json_bytes(payload).decode("utf-8"))
        return 0
    except SnapshotFailure as error:
        print(json.dumps({
            "schema": "items.snapshot.error.v1",
            "error": {"code": error.code, "message": error.message, "path": error.path},
        }, ensure_ascii=False, sort_keys=True), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
