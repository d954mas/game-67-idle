#!/usr/bin/env python3
"""Build and inspect a deterministic Items Snapshot from evaluator JSON."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sys
from typing import Any

from items_c_identifiers import is_c_member_name


EVALUATION_SCHEMA = "items.lua.evaluation.v1"
SNAPSHOT_SCHEMA = "items.snapshot.v1"
QUERY_SCHEMA = "items.snapshot.query.v1"
CHART_SCHEMA = "items.snapshot.chart.v1"
REQUIREMENTS_SCHEMA = "items.snapshot.requirements.v1"
RUNTIME_EXPORT_SCHEMA = "items.runtime_export.v1"
DIFF_SCHEMA = "items.snapshot.diff.v1"
DEFAULT_QUERY_ROWS = 1_000
DEFAULT_CHART_POINTS = 200
DEFAULT_REQUIREMENT_RESULTS = 1_000
DEFAULT_DIFF_CHANGES = 1_000
MAX_EXACT_INTEGER = 9_007_199_254_740_991
FIELD_ID_RE = re.compile(r"^[a-z][a-z0-9_]*(?:\.[a-z][a-z0-9_]*)+$")
ITEM_ID_RE = re.compile(r"^[a-z][a-z0-9_]*\.[a-z][a-z0-9_]*$")
MEMBER_RE = re.compile(r"^[a-z][a-z0-9_]*$")
PROVENANCE_BY_MODE = {
    "single": {"single"},
    "table": {"table"},
    "generate": {"generate", "override"},
    "columns": {"columns", "override"},
}


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


def _source(value: Any, code: str, path: str, *, kind: str = "definition") -> dict[str, Any]:
    if not isinstance(value, dict):
        _fail(code, "source must be an object", path)
    line = value.get("line")
    column = value.get("column")
    end_line = value.get("end_line")
    end_column = value.get("end_column")
    snippet = value.get("snippet")
    if (
        not isinstance(value.get("file"), str) or not value["file"]
        or type(line) is not int or line < 1
        or type(column) is not int or column < 1
        or type(end_line) is not int or end_line != line
        or type(end_column) is not int
        or not isinstance(snippet, str) or not snippet or len(snippet) > 4_096
        or column != 1 or end_column != len(snippet) + 1
        or value.get("kind") != kind
    ):
        _fail(code, f"source requires a bounded span/snippet and {kind} kind", path)
    return _canonical(value, path)


def _normalize_fields(evaluation: dict[str, Any]) -> tuple[list[dict[str, Any]], dict[str, dict[str, Any]]]:
    raw_fields = evaluation.get("fields")
    if not isinstance(raw_fields, list):
        _fail("snapshot.fields", "evaluation fields must be a list", "$.fields")
    fields: list[dict[str, Any]] = []
    ids: set[str] = set()
    members: set[tuple[str, str]] = set()
    for index, raw_field in enumerate(raw_fields):
        path = f"$.fields[{index}]"
        if not isinstance(raw_field, dict):
            _fail("snapshot.field", "field must be an object", path)
        field = _canonical(raw_field, path)
        field_id = field.get("id")
        member = field.get("member")
        section = field.get("section")
        required_for = field.get("required_for")
        minimum, maximum = field.get("min"), field.get("max")
        if not isinstance(field_id, str) or FIELD_ID_RE.fullmatch(field_id) is None:
            _fail("snapshot.field_id", "field requires a stable dotted lowercase id", f"{path}.id")
        if field_id.startswith("items."):
            _fail("snapshot.sealed_field_id", "items.* field ids are sealed", f"{path}.id")
        if field_id in ids:
            _fail("snapshot.duplicate_field", f"duplicate field id: {field_id}", f"{path}.id")
        if not is_c_member_name(member):
            _fail(
                "snapshot.field_member",
                "field member must be a lowercase non-reserved C identifier",
                f"{path}.member",
            )
        if section != "level_row":
            _fail("snapshot.field_section", "v1 supports level_row fields", f"{path}.section")
        if (section, member) in members:
            _fail("snapshot.duplicate_member", f"duplicate field member: {section}.{member}", path)
        if field.get("type") != "i64":
            _fail("snapshot.field_type", "v1 typed Snapshot supports i64 fields", f"{path}.type")
        if (not isinstance(required_for, list) or not required_for
                or not all(isinstance(kind, str) and MEMBER_RE.fullmatch(kind) for kind in required_for)
                or len(required_for) != len(set(required_for))):
            _fail("snapshot.required_for", "required_for must be a unique non-empty kind list", f"{path}.required_for")
        if (type(minimum) is not int or type(maximum) is not int
                or minimum < -MAX_EXACT_INTEGER or maximum > MAX_EXACT_INTEGER or minimum > maximum):
            _fail("snapshot.field_range", "i64 field requires an ordered exact min/max range", path)
        if field.get("rounding") != "exact":
            _fail("snapshot.field_rounding", "i64 field rounding must be exact", f"{path}.rounding")
        for key in ("unit", "label_key"):
            if not isinstance(field.get(key), str) or not field[key]:
                _fail("snapshot.field_metadata", f"field requires non-empty {key}", f"{path}.{key}")
        ids.add(field_id)
        members.add((section, member))
        fields.append(field)
    fields.sort(key=lambda field: field["id"])

    raw_sources = evaluation.get("field_sources", {})
    if not isinstance(raw_sources, dict) or set(raw_sources) != ids:
        _fail("snapshot.field_sources", "field_sources must exactly match registered field ids", "$.field_sources")
    sources = {
        field_id: _source(
            raw_sources[field_id], "snapshot.field_source", f"$.field_sources.{field_id}", kind="field",
        )
        for field_id in sorted(ids)
    }
    return fields, sources


def _validate_typed_rows(items: list[dict[str, Any]], fields: list[dict[str, Any]]) -> None:
    known_members = {field["member"] for field in fields}
    for item_index, item in enumerate(items):
        required_fields = [field for field in fields if item.get("kind") in field["required_for"]]
        levels = item.get("levels")
        if levels is None:
            if required_fields:
                member = required_fields[0]["member"]
                _fail(
                    "snapshot.required_field", f"missing required field: {member}",
                    f"$.items[{item_index}].levels",
                )
            continue
        if not isinstance(levels, dict) or not isinstance(levels.get("rows"), list):
            _fail("snapshot.levels", "levels require a rows list", f"$.items[{item_index}].levels")
        if required_fields and not levels["rows"]:
            member = required_fields[0]["member"]
            _fail(
                "snapshot.required_field", f"missing required field: {member}",
                f"$.items[{item_index}].levels.rows",
            )
        for row_index, row in enumerate(levels["rows"]):
            path = f"$.items[{item_index}].levels.rows[{row_index}]"
            if not isinstance(row, dict):
                _fail("snapshot.level_row", "level row must be an object", path)
            unknown = sorted(set(row) - known_members - {"cost_to_reach"})
            if unknown:
                _fail("snapshot.unknown_field", f"unknown level field: {unknown[0]}", f"{path}.{unknown[0]}")
            for field in fields:
                member = field["member"]
                required = item.get("kind") in field["required_for"]
                if required and member not in row:
                    _fail("snapshot.required_field", f"missing required field: {member}", f"{path}.{member}")
                if member not in row:
                    continue
                if not required:
                    _fail("snapshot.field_kind", f"field {member} is not valid for kind {item.get('kind')!r}", f"{path}.{member}")
                value = row[member]
                if type(value) is not int:
                    _fail("snapshot.field_type", f"field {member} requires i64", f"{path}.{member}")
                if value < field["min"] or value > field["max"]:
                    _fail("snapshot.field_range", f"field {member} is outside declared range", f"{path}.{member}")


def _validate_level_provenance(items: list[dict[str, Any]]) -> None:
    for item_index, item in enumerate(items):
        path = f"$.items[{item_index}]"
        mode = item.get("authoring_mode")
        levels = item.get("levels")
        if levels is None:
            if mode != "none":
                _fail("snapshot.authoring_mode", "item without levels requires authoring_mode none", f"{path}.authoring_mode")
            continue
        if mode not in PROVENANCE_BY_MODE or levels.get("mode") != mode:
            _fail("snapshot.authoring_mode", "level mode must match authoring_mode", f"{path}.authoring_mode")
        rows = levels.get("rows")
        provenance = levels.get("provenance")
        if not isinstance(rows, list) or not rows:
            _fail("snapshot.levels", "levelled item requires at least one row", f"{path}.levels.rows")
        if mode == "single" and len(rows) != 1:
            _fail("snapshot.authoring_mode", "single mode requires exactly one row", f"{path}.levels.rows")
        if not isinstance(provenance, list) or len(provenance) != len(rows):
            _fail("snapshot.provenance", "provenance must match level rows", f"{path}.levels.provenance")
        for row_index, (row, row_provenance) in enumerate(zip(rows, provenance)):
            row_path = f"{path}.levels.provenance[{row_index}]"
            if not isinstance(row, dict) or not isinstance(row_provenance, dict):
                _fail("snapshot.provenance", "each level row requires a provenance object", row_path)
            if set(row_provenance) != set(row):
                _fail("snapshot.provenance", "provenance keys must exactly match row values", row_path)
            if not all(
                isinstance(value, str) and value in PROVENANCE_BY_MODE[mode]
                for value in row_provenance.values()
            ):
                _fail("snapshot.provenance", f"provenance is inconsistent with {mode} mode", row_path)


def _normalize_requirements(
    evaluation: dict[str, Any], item_ids: set[str],
) -> tuple[list[dict[str, Any]], dict[str, dict[str, Any]], dict[str, dict[str, Any]]]:
    raw_requirements = evaluation.get("requirements")
    if not isinstance(raw_requirements, list):
        _fail("snapshot.requirements", "evaluation requirements must be a list", "$.requirements")
    requirements: list[dict[str, Any]] = []
    ids: set[str] = set()
    waived_ids: set[str] = set()

    def validate_evidence(value: Any, path: str, seen: set[int], depth: int, budget: list[int]) -> None:
        budget[0] += 1
        if budget[0] > 1_000 or depth > 32:
            _fail("snapshot.requirement_evidence", "evidence exceeds structural bounds", path)
        if value is None or isinstance(value, (bool, str, int)):
            return
        if isinstance(value, float):
            if not math.isfinite(value):
                _fail("snapshot.requirement_evidence", "evidence numbers must be finite", path)
            return
        if not isinstance(value, (dict, list)):
            _fail("snapshot.requirement_evidence", "evidence must contain only JSON-safe values", path)
        identity = id(value)
        if identity in seen:
            _fail("snapshot.requirement_evidence", "evidence cannot contain cycles", path)
        seen.add(identity)
        if isinstance(value, dict):
            if not all(isinstance(key, str) for key in value):
                _fail("snapshot.requirement_evidence", "evidence object keys must be strings", path)
            for key, child in value.items():
                validate_evidence(child, f"{path}.{key}", seen, depth + 1, budget)
        else:
            for child_index, child in enumerate(value):
                validate_evidence(child, f"{path}[{child_index}]", seen, depth + 1, budget)
        seen.remove(identity)

    for index, raw_requirement in enumerate(raw_requirements):
        path = f"$.requirements[{index}]"
        if not isinstance(raw_requirement, dict):
            _fail("snapshot.requirement", "requirement result must be an object", path)
        raw_evidence = raw_requirement.get("evidence")
        if not isinstance(raw_evidence, dict) or set(raw_evidence) != {"expected", "actual"}:
            _fail("snapshot.requirement_evidence", "evidence requires expected and actual", f"{path}.evidence")
        evidence_budget = [0]
        for evidence_key in ("expected", "actual"):
            if not isinstance(raw_evidence[evidence_key], (dict, list)):
                _fail("snapshot.requirement_evidence", "expected and actual must be objects or arrays", f"{path}.evidence.{evidence_key}")
            validate_evidence(
                raw_evidence[evidence_key], f"{path}.evidence.{evidence_key}",
                set(), 0, evidence_budget,
            )
        requirement = _canonical(raw_requirement, path)
        requirement_id = requirement.get("id")
        required_keys = {"id", "severity", "status", "evidence", "dependencies"}
        allowed_keys = required_keys | {"waiver"}
        if set(requirement) - allowed_keys or not required_keys.issubset(requirement):
            _fail("snapshot.requirement", "requirement result has invalid keys", path)
        if not isinstance(requirement_id, str) or FIELD_ID_RE.fullmatch(requirement_id) is None:
            _fail("snapshot.requirement_id", "requirement requires a stable dotted lowercase id", f"{path}.id")
        if requirement_id in ids:
            _fail("snapshot.requirement_id", f"duplicate requirement: {requirement_id}", f"{path}.id")
        if requirement.get("severity") not in {"warning", "error"}:
            _fail("snapshot.requirement_severity", "severity must be warning or error", f"{path}.severity")
        if requirement.get("status") not in {"pass", "fail"}:
            _fail("snapshot.requirement_status", "status must be pass or fail", f"{path}.status")
        evidence = requirement.get("evidence")
        if not isinstance(evidence, dict) or set(evidence) != {"expected", "actual"}:
            _fail("snapshot.requirement_evidence", "evidence requires expected and actual", f"{path}.evidence")
        dependencies = requirement.get("dependencies")
        if (not isinstance(dependencies, list)
                or not all(isinstance(item_id, str) for item_id in dependencies)
                or len(dependencies) != len(set(dependencies))):
            _fail("snapshot.requirement_dependencies", "dependencies must be a unique item id list", f"{path}.dependencies")
        unknown = sorted(set(dependencies) - item_ids)
        if unknown:
            _fail("snapshot.requirement_dependencies", f"unknown dependency: {unknown[0]}", f"{path}.dependencies")
        requirement["dependencies"] = sorted(dependencies)
        waiver = requirement.get("waiver")
        if waiver is not None:
            if (requirement["status"] != "fail" or not isinstance(waiver, dict)
                    or set(waiver) != {"reason", "reviewed_by"}
                    or not isinstance(waiver["reason"], str) or not waiver["reason"]
                    or not isinstance(waiver["reviewed_by"], str) or not waiver["reviewed_by"]):
                _fail("snapshot.waiver", "only a failing requirement accepts reason and reviewed_by", f"{path}.waiver")
            requirement["effective_status"] = "waived"
            waived_ids.add(requirement_id)
        else:
            requirement["effective_status"] = requirement["status"]
        ids.add(requirement_id)
        requirements.append(requirement)
    requirements.sort(key=lambda requirement: requirement["id"])

    raw_sources = evaluation.get("requirement_sources")
    if not isinstance(raw_sources, dict) or set(raw_sources) != ids:
        _fail("snapshot.requirement_sources", "requirement_sources must exactly match requirement ids", "$.requirement_sources")
    sources = {
        requirement_id: _source(
            raw_sources[requirement_id], "snapshot.requirement_source",
            f"$.requirement_sources.{requirement_id}", kind="requirement",
        )
        for requirement_id in sorted(ids)
    }
    raw_waiver_sources = evaluation.get("waiver_sources")
    if not isinstance(raw_waiver_sources, dict) or set(raw_waiver_sources) != waived_ids:
        _fail("snapshot.waiver_sources", "waiver_sources must exactly match waived requirement ids", "$.waiver_sources")
    waiver_sources = {
        requirement_id: _source(
            raw_waiver_sources[requirement_id], "snapshot.waiver_source",
            f"$.waiver_sources.{requirement_id}", kind="waiver",
        )
        for requirement_id in sorted(waived_ids)
    }
    return requirements, sources, waiver_sources


def _runtime_export_metadata(
    items: list[dict[str, Any]], fields: list[dict[str, Any]],
) -> dict[str, Any]:
    metadata = []
    item_ids: set[str] = set()
    for index, item in enumerate(items):
        item_id = item.get("id") if isinstance(item, dict) else None
        if not isinstance(item_id, str) or ITEM_ID_RE.fullmatch(item_id) is None:
            _fail("snapshot.item_id", "item requires a two-segment dotted lowercase id", f"$.items[{index}].id")
        if item_id in item_ids:
            _fail("snapshot.duplicate_item", f"duplicate item id: {item_id}", f"$.items[{index}].id")
        item_ids.add(item_id)
        stack = item.get("stack")
        if type(stack) is not int or stack < 0:
            _fail("snapshot.stack", f"item {item_id} requires integer stack >= 0", f"$.items[{index}].stack")
        levels = item.get("levels")
        if levels is None:
            level_count = 0
        elif not isinstance(levels, dict) or not isinstance(levels.get("rows"), list):
            _fail("snapshot.levels", f"item {item_id} levels require rows", f"$.items[{index}].levels")
        else:
            level_count = len(levels["rows"])
        storage = "unique" if stack == 1 else "stack"
        if storage == "stack" and level_count != 0:
            _fail("snapshot.storage", f"stack item {item_id} cannot have levels", f"$.items[{index}].levels")
        metadata.append({
            "id": item_id,
            "storage": storage,
            "level_count": level_count,
        })
    metadata.sort(key=lambda entry: entry["id"])

    field_ids = []
    for index, field in enumerate(fields):
        field_id = field.get("id") if isinstance(field, dict) else None
        if not isinstance(field_id, str) or FIELD_ID_RE.fullmatch(field_id) is None:
            _fail("snapshot.field_id", "field requires a stable dotted lowercase id", f"$.fields[{index}].id")
        field_ids.append(field_id)
    if len(field_ids) != len(set(field_ids)):
        _fail("snapshot.duplicate_field", "runtime field ids must be unique", "$.fields")
    field_ids.sort()
    return {
        "schema": RUNTIME_EXPORT_SCHEMA,
        "field_ids": field_ids,
        "items": metadata,
    }


def build_snapshot(evaluation: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(evaluation, dict) or evaluation.get("schema") != EVALUATION_SCHEMA:
        _fail("snapshot.evaluation_schema", f"expected {EVALUATION_SCHEMA}")
    raw_items = evaluation.get("items")
    if not isinstance(raw_items, list):
        _fail("snapshot.items", "evaluation items must be a list", "$.items")
    fields, field_sources = _normalize_fields(evaluation)

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
    _validate_typed_rows(items, fields)
    _validate_level_provenance(items)
    runtime_export = _runtime_export_metadata(items, fields)
    requirements, requirement_sources, waiver_sources = _normalize_requirements(evaluation, seen)

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
        "fields": fields,
        "items": items,
        "requirements": requirements,
    })).hexdigest()
    snapshot = {
        "schema": SNAPSHOT_SCHEMA,
        "content_hash": content_hash,
        "fields": fields,
        "items": items,
        "requirements": requirements,
        "runtime_export": runtime_export,
        "dependencies": dependencies,
        "dependents": dependents,
    }
    if field_sources:
        snapshot["field_sources"] = field_sources
    if requirement_sources:
        snapshot["requirement_sources"] = requirement_sources
    if waiver_sources:
        snapshot["waiver_sources"] = waiver_sources
    raw_sources = evaluation.get("sources")
    if not isinstance(raw_sources, dict) or set(raw_sources) != seen:
        _fail("snapshot.sources", "sources must exactly match item ids", "$.sources")
    sources: dict[str, dict[str, Any]] = {}
    for item_id in sorted(raw_sources):
        sources[item_id] = _source(
            raw_sources[item_id], "snapshot.source", f"$.sources.{item_id}",
        )
    snapshot["sources"] = sources
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
    mode = item.get("authoring_mode")
    if rows:
        if mode not in PROVENANCE_BY_MODE or levels.get("mode") != mode:
            _fail("query.provenance", "level mode must match authoring_mode")
        if mode == "single" and len(rows) != 1:
            _fail("query.provenance", "single mode requires exactly one row")
    elif "levels" in item or mode != "none":
        _fail("query.provenance", "item without levels requires authoring_mode none")
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
    provenance_rows = levels.get("provenance", [])
    if not isinstance(provenance_rows, list) or len(provenance_rows) != len(rows):
        _fail("query.provenance", "level provenance must match rows", f"$.items.{item_id}.levels.provenance")
    selected_provenance = provenance_rows[start - 1:start - 1 + len(selected)]
    if len(selected) > max_rows:
        _fail("query.row_limit", f"query exceeds {max_rows} level rows; provide a smaller range")

    result_rows = []
    field_found = field is None or field in item
    field_in_rows = False
    for level, (row, row_provenance) in enumerate(
        zip(selected, selected_provenance), start=start,
    ):
        if not isinstance(row, dict):
            _fail("query.level_row", "level row must be an object")
        if not isinstance(row_provenance, dict) or set(row_provenance) != set(row):
            _fail("query.provenance", "level provenance must match row values")
        if not all(
            isinstance(value, str) and value in PROVENANCE_BY_MODE[mode]
            for value in row_provenance.values()
        ):
            _fail("query.provenance", f"provenance is inconsistent with {mode} mode")
        if field is None:
            values = row
            value_provenance = row_provenance
        elif field in row:
            values = {field: row[field]}
            value_provenance = {field: row_provenance[field]}
            field_found = True
            field_in_rows = True
        else:
            values = {}
            value_provenance = {}
        result_rows.append({
            "level": level, "values": values, "provenance": value_provenance,
        })
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
    if field is not None and field_in_rows and field != "cost_to_reach":
        raw_fields = snapshot.get("fields", [])
        if not isinstance(raw_fields, list):
            _fail("query.fields", "snapshot fields must be a list", "$.fields")
        matching_fields = [
            candidate for candidate in raw_fields
            if isinstance(candidate, dict)
            and candidate.get("section") == "level_row"
            and candidate.get("member") == field
            and isinstance(candidate.get("required_for"), list)
            and item.get("kind") in candidate["required_for"]
        ]
        if len(matching_fields) != 1:
            _fail("query.field_schema", f"expected one applicable schema for field: {field}", "$.fields")
        field_sources = snapshot.get("field_sources", {})
        if not isinstance(field_sources, dict):
            _fail("query.field_source", "snapshot field_sources must be an object", "$.field_sources")
        field_id = matching_fields[0].get("id")
        if not isinstance(field_id, str):
            _fail("query.field_schema", "selected field requires a stable id", "$.fields")
        if field_id not in field_sources:
            _fail("query.field_source", f"missing source for field: {field_id}", "$.field_sources")
        try:
            normalized_fields, normalized_sources = _normalize_fields({
                "fields": [matching_fields[0]],
                "field_sources": {field_id: field_sources[field_id]},
            })
        except SnapshotFailure as error:
            code = "query.field_source" if error.code in {
                "snapshot.field_source", "snapshot.field_sources",
            } else "query.field_schema"
            _fail(code, error.message, error.path)
        result["field"] = {
            "schema": normalized_fields[0],
            "source": normalized_sources[field_id],
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
    sources = snapshot.get("sources", {})
    if not isinstance(sources, dict):
        _fail("query.source", "snapshot sources must be an object", "$.sources")
    if item_id not in sources:
        _fail("query.source", f"missing source for item: {item_id}", "$.sources")
    result["source"] = _source(sources[item_id], "query.source", f"$.sources.{item_id}")
    runtime_export = snapshot.get("runtime_export")
    raw_fields = snapshot.get("fields")
    if not isinstance(runtime_export, dict) or not isinstance(raw_fields, list):
        _fail("query.runtime", "snapshot runtime_export metadata is invalid", "$.runtime_export")
    try:
        expected_runtime = _runtime_export_metadata(items, raw_fields)
    except SnapshotFailure as error:
        _fail("query.runtime", error.message, error.path)
    if _json_bytes(runtime_export) != _json_bytes(expected_runtime):
        _fail("query.runtime", "runtime metadata does not match snapshot fields/items", "$.runtime_export")
    runtime = next(entry for entry in expected_runtime["items"] if entry["id"] == item_id)
    result["runtime"] = {
        "storage": runtime["storage"],
        "level_count": runtime["level_count"],
    }
    return result


def chart_snapshot(
    snapshot: dict[str, Any],
    *,
    item_id: str,
    field: str,
    level_from: int | None = None,
    level_to: int | None = None,
    max_points: int = DEFAULT_CHART_POINTS,
) -> dict[str, Any]:
    if type(max_points) is not int or max_points < 2 or max_points > DEFAULT_QUERY_ROWS:
        _fail("chart.max_points", f"max_points must be between 2 and {DEFAULT_QUERY_ROWS}")
    query = query_snapshot(
        snapshot, item_id=item_id, field=field,
        level_from=level_from, level_to=level_to,
        max_rows=MAX_EXACT_INTEGER,
    )
    field_metadata = query.get("field")
    if not isinstance(field_metadata, dict):
        _fail("chart.field", "chart requires one registered numeric level field", "$.field")
    field_schema = field_metadata.get("schema")
    if not isinstance(field_schema, dict):
        _fail("chart.field", "chart requires a typed field schema", "$.field.schema")
    minimum, maximum = field_schema.get("min"), field_schema.get("max")
    rows = query.get("item", {}).get("levels", [])
    if not isinstance(rows, list) or not rows:
        _fail("chart.field", "chart requires at least one level value", "$.item.levels")

    source_points: list[dict[str, Any]] = []
    for index, row in enumerate(rows):
        values = row.get("values") if isinstance(row, dict) else None
        provenance = row.get("provenance") if isinstance(row, dict) else None
        value = values.get(field) if isinstance(values, dict) else None
        value_provenance = provenance.get(field) if isinstance(provenance, dict) else None
        if (type(value) is not int or type(minimum) is not int or type(maximum) is not int
                or value < minimum or value > maximum or not isinstance(value_provenance, str)):
            _fail("chart.field", "chart points require typed values and provenance", f"$.item.levels[{index}]")
        source_points.append({
            "level": row["level"], "value": value, "provenance": value_provenance,
        })

    count = len(source_points)
    if count > max_points:
        indices = [
            index * (count - 1) // (max_points - 1)
            for index in range(max_points)
        ]
        points = [source_points[index] for index in indices]
        method = "even-index"
    else:
        points = source_points
        method = "none"
    values = [point["value"] for point in source_points]
    return {
        "schema": CHART_SCHEMA,
        "content_hash": snapshot.get("content_hash"),
        "item": item_id,
        "field": field_metadata,
        "bounds": {
            "level_from": source_points[0]["level"],
            "level_to": source_points[-1]["level"],
            "value_min": min(values),
            "value_max": max(values),
        },
        "downsampling": {
            "applied": count > max_points,
            "method": method,
            "source_points": count,
            "returned_points": len(points),
            "max_points": max_points,
        },
        "points": points,
    }


def query_requirements(
    snapshot: dict[str, Any],
    *,
    item_id: str | None = None,
    severity: str | None = None,
    max_results: int = DEFAULT_REQUIREMENT_RESULTS,
) -> dict[str, Any]:
    if not isinstance(snapshot, dict) or snapshot.get("schema") != SNAPSHOT_SCHEMA:
        _fail("requirements.snapshot_schema", f"expected {SNAPSHOT_SCHEMA}")
    if type(max_results) is not int or max_results < 1 or max_results > DEFAULT_REQUIREMENT_RESULTS:
        _fail("requirements.max_results", f"max_results must be between 1 and {DEFAULT_REQUIREMENT_RESULTS}")
    if severity is not None and severity not in {"warning", "error"}:
        _fail("requirements.severity", "severity must be warning or error")
    raw_items = snapshot.get("items")
    if not isinstance(raw_items, list):
        _fail("requirements.items", "snapshot items must be a list", "$.items")
    item_ids = {
        item["id"] for item in raw_items
        if isinstance(item, dict) and isinstance(item.get("id"), str)
    }
    if len(item_ids) != len(raw_items):
        _fail("requirements.items", "snapshot items require unique string ids", "$.items")
    if item_id is not None and item_id not in item_ids:
        _fail("requirements.item_not_found", f"unknown item: {item_id}", "$.item")
    try:
        raw_requirements = snapshot.get("requirements")
        normalized_input = [
            {key: value for key, value in requirement.items() if key != "effective_status"}
            if isinstance(requirement, dict) else requirement
            for requirement in raw_requirements
        ] if isinstance(raw_requirements, list) else raw_requirements
        requirements, sources, waiver_sources = _normalize_requirements({
            "requirements": normalized_input,
            "requirement_sources": snapshot.get("requirement_sources", {}),
            "waiver_sources": snapshot.get("waiver_sources", {}),
        }, item_ids)
    except SnapshotFailure as error:
        code = "requirements.source" if "source" in error.code else "requirements.result"
        _fail(code, error.message, error.path)
    selected = [
        requirement for requirement in requirements
        if (item_id is None or item_id in requirement["dependencies"])
        and (severity is None or requirement["severity"] == severity)
    ]
    if len(selected) > max_results:
        _fail("requirements.result_limit", f"query exceeds {max_results} results; add a filter")
    results = []
    for requirement in selected:
        requirement_id = requirement["id"]
        result = {**requirement, "source": sources[requirement_id]}
        if requirement_id in waiver_sources:
            result["waiver_source"] = waiver_sources[requirement_id]
        results.append(result)
    return {
        "schema": REQUIREMENTS_SCHEMA,
        "content_hash": snapshot.get("content_hash"),
        "filters": {"item": item_id, "severity": severity},
        "results": results,
    }


def _diff_items(snapshot: dict[str, Any]) -> dict[str, dict[str, Any]]:
    if not isinstance(snapshot, dict) or snapshot.get("schema") != SNAPSHOT_SCHEMA:
        _fail("diff.snapshot_schema", f"expected {SNAPSHOT_SCHEMA}")
    items = snapshot.get("items")
    if not isinstance(items, list):
        _fail("diff.items", "snapshot items must be a list", "$.items")
    result: dict[str, dict[str, Any]] = {}
    for index, item in enumerate(items):
        if not isinstance(item, dict) or not isinstance(item.get("id"), str) or not item["id"]:
            _fail("diff.item", "snapshot item requires an id", f"$.items[{index}]")
        normalized = _canonical(item, f"$.items[{index}]")
        if normalized["id"] in result:
            _fail("diff.item", f"duplicate item id: {normalized['id']}", f"$.items[{index}].id")
        result[normalized["id"]] = normalized
    return result


def _diff_requirements(snapshot: dict[str, Any]) -> dict[str, dict[str, Any]]:
    items = snapshot.get("items")
    if not isinstance(items, list):
        _fail("diff.items", "snapshot items must be a list", "$.items")
    item_ids = {
        item["id"] for item in items
        if isinstance(item, dict) and isinstance(item.get("id"), str)
    }
    if len(item_ids) != len(items):
        _fail("diff.items", "snapshot items require unique string ids", "$.items")
    raw_requirements = snapshot.get("requirements")
    normalized_input = [
        {key: value for key, value in requirement.items() if key != "effective_status"}
        if isinstance(requirement, dict) else requirement
        for requirement in raw_requirements
    ] if isinstance(raw_requirements, list) else raw_requirements
    requirements, _sources, _waiver_sources = _normalize_requirements({
        "requirements": normalized_input,
        "requirement_sources": snapshot.get("requirement_sources", {}),
        "waiver_sources": snapshot.get("waiver_sources", {}),
    }, item_ids)
    return {requirement["id"]: requirement for requirement in requirements}


def _pointer(path: str, key: str | int) -> str:
    token = str(key).replace("~", "~0").replace("/", "~1")
    return f"{path}/{token}"


def _record(changes: list[dict[str, Any]], change: dict[str, Any], max_changes: int) -> None:
    if len(changes) >= max_changes:
        _fail("diff.change_limit", f"diff exceeds {max_changes} changes")
    changes.append(change)


def _diff_value(
    before: Any,
    after: Any,
    *,
    identity: dict[str, str],
    path: str,
    changes: list[dict[str, Any]],
    max_changes: int,
) -> None:
    if isinstance(before, dict) and isinstance(after, dict):
        for key in sorted(set(before) | set(after)):
            child_path = _pointer(path, key)
            if key not in after:
                _record(changes, {
                    "op": "remove", **identity, "path": child_path,
                    "before": before[key],
                }, max_changes)
            elif key not in before:
                _record(changes, {
                    "op": "add", **identity, "path": child_path,
                    "after": after[key],
                }, max_changes)
            else:
                _diff_value(
                    before[key], after[key], identity=identity, path=child_path,
                    changes=changes, max_changes=max_changes,
                )
        return
    if isinstance(before, list) and isinstance(after, list):
        shared = min(len(before), len(after))
        for index in range(shared):
            _diff_value(
                before[index], after[index], identity=identity,
                path=_pointer(path, index), changes=changes, max_changes=max_changes,
            )
        for index in range(shared, len(before)):
            _record(changes, {
                "op": "remove", **identity, "path": _pointer(path, index),
                "before": before[index],
            }, max_changes)
        for index in range(shared, len(after)):
            _record(changes, {
                "op": "add", **identity, "path": _pointer(path, index),
                "after": after[index],
            }, max_changes)
        return
    if type(before) is not type(after) or before != after:
        _record(changes, {
            "op": "replace", **identity, "path": path,
            "before": before, "after": after,
        }, max_changes)


def diff_snapshots(
    before: dict[str, Any],
    after: dict[str, Any],
    *,
    max_changes: int = DEFAULT_DIFF_CHANGES,
) -> dict[str, Any]:
    if type(max_changes) is not int or max_changes < 1:
        _fail("diff.max_changes", "max_changes must be positive")
    before_items = _diff_items(before)
    after_items = _diff_items(after)
    before_requirements = _diff_requirements(before)
    after_requirements = _diff_requirements(after)
    changes: list[dict[str, Any]] = []
    for item_id in sorted(set(before_items) | set(after_items)):
        if item_id not in after_items:
            _record(changes, {
                "op": "remove", "item": item_id, "path": "",
                "before": before_items[item_id],
            }, max_changes)
        elif item_id not in before_items:
            _record(changes, {
                "op": "add", "item": item_id, "path": "",
                "after": after_items[item_id],
            }, max_changes)
        else:
            _diff_value(
                before_items[item_id], after_items[item_id], identity={"item": item_id},
                path="", changes=changes, max_changes=max_changes,
            )
    for requirement_id in sorted(set(before_requirements) | set(after_requirements)):
        if requirement_id not in after_requirements:
            _record(changes, {
                "op": "remove", "requirement": requirement_id, "path": "",
                "before": before_requirements[requirement_id],
            }, max_changes)
        elif requirement_id not in before_requirements:
            _record(changes, {
                "op": "add", "requirement": requirement_id, "path": "",
                "after": after_requirements[requirement_id],
            }, max_changes)
        else:
            _diff_value(
                before_requirements[requirement_id], after_requirements[requirement_id],
                identity={"requirement": requirement_id}, path="",
                changes=changes, max_changes=max_changes,
            )
    return {
        "schema": DIFF_SCHEMA,
        "before_hash": before.get("content_hash"),
        "after_hash": after.get("content_hash"),
        "changes": changes,
    }


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
    chart = subparsers.add_parser("chart")
    chart.add_argument("--snapshot", type=Path, required=True)
    chart.add_argument("--item", required=True)
    chart.add_argument("--field", required=True)
    chart.add_argument("--level-from", type=int)
    chart.add_argument("--level-to", type=int)
    chart.add_argument("--max-points", type=int, default=DEFAULT_CHART_POINTS)
    requirements = subparsers.add_parser("requirements")
    requirements.add_argument("--snapshot", type=Path, required=True)
    requirements.add_argument("--item")
    requirements.add_argument("--severity")
    requirements.add_argument("--max-results", type=int, default=DEFAULT_REQUIREMENT_RESULTS)
    diff = subparsers.add_parser("diff")
    diff.add_argument("--before", type=Path, required=True)
    diff.add_argument("--after", type=Path, required=True)
    diff.add_argument("--max-changes", type=int, default=DEFAULT_DIFF_CHANGES)
    try:
        args = parser.parse_args(argv)
        if args.command == "build":
            _write_if_different(args.out, build_snapshot(_load(args.evaluation)))
        elif args.command == "query":
            payload = query_snapshot(
                _load(args.snapshot), item_id=args.item, field=args.field,
                level_from=args.level_from, level_to=args.level_to,
                include_inputs=args.inputs, include_dependents=args.dependents,
                max_rows=args.max_rows,
            )
            print(_json_bytes(payload).decode("utf-8"))
        elif args.command == "chart":
            payload = chart_snapshot(
                _load(args.snapshot), item_id=args.item, field=args.field,
                level_from=args.level_from, level_to=args.level_to,
                max_points=args.max_points,
            )
            print(_json_bytes(payload).decode("utf-8"))
        elif args.command == "requirements":
            payload = query_requirements(
                _load(args.snapshot), item_id=args.item, severity=args.severity,
                max_results=args.max_results,
            )
            print(_json_bytes(payload).decode("utf-8"))
        else:
            payload = diff_snapshots(
                _load(args.before), _load(args.after), max_changes=args.max_changes,
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
