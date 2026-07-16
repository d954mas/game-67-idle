#!/usr/bin/env python3
"""Reference exporter from a normalized Items proof Snapshot to typed C.

This intentionally does not evaluate Lua and does not define the runtime blob.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import tempfile
from pathlib import Path
from typing import Any, Callable

from items_c_identifiers import is_c_member_name
from items_xxh64 import xxh64


DEF_ID_RE = re.compile(r"^[a-z][a-z0-9_]*\.[a-z][a-z0-9_]*$")
FIELD_ID_RE = re.compile(r"^[a-z][a-z0-9_]*(?:\.[a-z][a-z0-9_]*)+$")
SUPPORTED_C_TYPES = {"int32_t", "uint32_t", "int64_t", "uint64_t", "float", "double"}
SEALED_FIELDS = {"items.level.cost_to_reach"}
INT_RANGES = {
    "int32_t": (-(1 << 31), (1 << 31) - 1),
    "uint32_t": (0, (1 << 32) - 1),
    "int64_t": (-(1 << 63), (1 << 63) - 1),
    "uint64_t": (0, (1 << 64) - 1),
}
ROOT_KEYS = {"schema", "schema_version", "source", "reserved_field_ids", "fields", "views", "items"}
FIELD_KEYS = {
    "field_id", "member", "scope", "c_type", "required_for", "min", "max",
    "unit", "rounding", "label_key", "ui", "evolution", "source",
}
REQUIRED_FIELD_KEYS = FIELD_KEYS
ITEM_KEYS = {"def_id", "kind", "tags", "stack", "authoring_mode", "acquire", "levels", "source"}
REQUIRED_ITEM_KEYS = {"def_id", "kind", "tags", "stack", "authoring_mode", "levels", "source"}
LEVEL_CORE_KEYS = {"level", "cost_to_reach", "provenance", "source"}
SOURCE_KEYS = {"file", "line", "column"}
COST_KEYS = {"item_ref", "count"}
TRANSITION_KEYS = {"kind", "cost"}
FIELD_UI_KEYS = {"format", "description_key"}
FIELD_EVOLUTION_KEYS = {"since", "deprecated"}
VIEW_KEYS = {"view_id", "layout", "order", "chart", "source"}
VIEW_CHART_KEYS = {"field_ids"}
TAG_RE = re.compile(r"^[a-z][a-z0-9_-]*$")
AUTHORING_MODES = {"none", "single", "table", "generate", "columns"}
PROVENANCE_VALUES = {"single", "table", "generate", "columns", "override"}


class DiagnosticError(ValueError):
    def __init__(self, diagnostic: dict[str, Any]):
        super().__init__(diagnostic["code"])
        self.diagnostic = diagnostic


def _source(node: Any, root: dict[str, Any]) -> tuple[str, int, int]:
    span = node.get("source", {}) if isinstance(node, dict) else {}
    if not isinstance(span, dict):
        span = {}
    root_source = root.get("source", "<snapshot>")
    if isinstance(root_source, dict):
        root_source = root_source.get("file", "<snapshot>")
    file = span.get("file", root_source)
    line = span.get("line", 1)
    column = span.get("column", 1)
    if not isinstance(file, str) or not file:
        file = str(root_source) if isinstance(root_source, str) else "<snapshot>"
    if not isinstance(line, int) or isinstance(line, bool) or line < 1:
        line = 1
    if not isinstance(column, int) or isinstance(column, bool) or column < 1:
        column = 1
    return file, line, column


def _fail(code: str, path: str, node: Any, root: dict[str, Any]) -> None:
    file, line, column = _source(node, root)
    raise DiagnosticError({"code": code, "file": file, "line": line, "column": column, "path": path})


def _unknown_keys(node: dict[str, Any], allowed: set[str], path: str, root: dict[str, Any]) -> None:
    extra = sorted(set(node) - allowed)
    if extra:
        _fail("unknown-key", f"{path}.{extra[0]}", node, root)


def _validate_source(node: dict[str, Any], path: str, root: dict[str, Any]) -> None:
    source = node.get("source")
    if not isinstance(source, dict):
        _fail("invalid-source-span", f"{path}.source", node, root)
    _unknown_keys(source, SOURCE_KEYS, f"{path}.source", root)
    if (not isinstance(source.get("file"), str) or not source["file"]
            or not isinstance(source.get("line"), int) or isinstance(source.get("line"), bool)
            or source["line"] < 1
            or not isinstance(source.get("column"), int) or isinstance(source.get("column"), bool)
            or source["column"] < 1):
        _fail("invalid-source-span", f"{path}.source", node, root)


def _validate_costs(
    costs: Any,
    path: str,
    node: dict[str, Any],
    root: dict[str, Any],
    items_by_id: dict[str, dict[str, Any]],
) -> None:
    if not isinstance(costs, list):
        _fail("cost-list-required", path, node, root)
    seen: set[str] = set()
    for index, cost in enumerate(costs):
        cost_path = f"{path}[{index}]"
        if not isinstance(cost, dict):
            _fail("cost-entry-required", cost_path, node, root)
        _unknown_keys(cost, COST_KEYS, cost_path, root)
        item_ref = cost.get("item_ref")
        if item_ref not in items_by_id:
            _fail("unknown-item-ref", f"{cost_path}.item_ref", node, root)
        if item_ref in seen:
            _fail("duplicate-cost-resource", f"{cost_path}.item_ref", node, root)
        seen.add(item_ref)
        if items_by_id[item_ref]["stack"] == 1:
            _fail("cost-resource-not-stackable", f"{cost_path}.item_ref", node, root)
        count = cost.get("count")
        if not isinstance(count, int) or isinstance(count, bool) or count <= 0 or count > (1 << 63) - 1:
            _fail("invalid-cost-count", f"{cost_path}.count", node, root)


def _validate_transition(
    transition: Any,
    path: str,
    node: dict[str, Any],
    root: dict[str, Any],
    items_by_id: dict[str, dict[str, Any]],
) -> None:
    if not isinstance(transition, dict):
        _fail("invalid-transition", path, node, root)
    _unknown_keys(transition, TRANSITION_KEYS, path, root)
    kind = transition.get("kind")
    if kind == "free":
        if set(transition) != {"kind"}:
            _fail("invalid-transition", path, node, root)
        return
    if kind == "cost":
        if set(transition) != {"kind", "cost"} or not isinstance(transition["cost"], list) or not transition["cost"]:
            _fail("invalid-transition", path, node, root)
        _validate_costs(transition["cost"], f"{path}.cost", node, root, items_by_id)
        return
    _fail("invalid-transition", path, node, root)


def _c_name(identity: str) -> str:
    return re.sub(r"[^A-Za-z0-9]", "_", identity).upper()


def load_and_validate(snapshot: dict[str, Any], hasher: Callable[..., int] = xxh64) -> dict[str, Any]:
    root = snapshot
    if not isinstance(root, dict):
        _fail("snapshot-object-required", "$", {}, {})
    _unknown_keys(root, ROOT_KEYS, "$", root)
    if root.get("schema") != "items.normalized_api_proof" or root.get("schema_version") != 1:
        _fail("unsupported-snapshot-schema", "$", root, root)
    fields = root.get("fields")
    views = root.get("views")
    reserved_field_ids = root.get("reserved_field_ids")
    items = root.get("items")
    if (not isinstance(fields, list) or not isinstance(views, list)
            or not isinstance(reserved_field_ids, list) or not isinstance(items, list)):
        _fail("snapshot-lists-required", "$", root, root)
    if any(not isinstance(field_id, str) or not FIELD_ID_RE.fullmatch(field_id)
           for field_id in reserved_field_ids):
        _fail("invalid-field-id", "$.reserved_field_ids", root, root)
    if len(set(reserved_field_ids)) != len(reserved_field_ids):
        _fail("duplicate-field-id", "$.reserved_field_ids", root, root)

    field_ids: set[str] = set()
    field_c_names: dict[str, str] = {}
    field_members: set[str] = set()
    field_by_member: dict[str, dict[str, Any]] = {}
    capability_names: dict[str, str] = {}
    for index, field in enumerate(fields):
        path = f"$.fields[{index}]"
        if not isinstance(field, dict):
            _fail("field-object-required", path, root, root)
        missing = sorted(REQUIRED_FIELD_KEYS - set(field))
        if missing:
            _fail("field-metadata-required", f"{path}.{missing[0]}", field, root)
        _unknown_keys(field, FIELD_KEYS, path, root)
        _validate_source(field, path, root)
        field_id = field.get("field_id")
        if field_id in SEALED_FIELDS or (isinstance(field_id, str) and field_id.startswith("items.")):
            _fail("sealed-field-redefinition", f"{path}.field_id", field, root)
        if not isinstance(field_id, str) or not FIELD_ID_RE.fullmatch(field_id):
            _fail("invalid-field-id", f"{path}.field_id", field, root)
        if field_id in field_ids:
            _fail("duplicate-field-id", f"{path}.field_id", field, root)
        if field_id in reserved_field_ids:
            _fail("reserved-field-id-reused", f"{path}.field_id", field, root)
        field_ids.add(field_id)
        field_c_name = _c_name(field_id)
        previous_field_id = field_c_names.get(field_c_name)
        if previous_field_id is not None and previous_field_id != field_id:
            _fail("field-c-name-collision", f"{path}.field_id", field, root)
        field_c_names[field_c_name] = field_id
        member = field.get("member")
        if not is_c_member_name(member):
            _fail("invalid-field-member", f"{path}.member", field, root)
        if member in field_members:
            _fail("duplicate-field-member", f"{path}.member", field, root)
        field_members.add(member)
        field_by_member[member] = field
        if field.get("scope") != "level_row":
            _fail("unsupported-field-scope", f"{path}.scope", field, root)
        if field.get("c_type") not in SUPPORTED_C_TYPES:
            _fail("unsupported-c-type", f"{path}.c_type", field, root)
        required_for = field["required_for"]
        if (not isinstance(required_for, list) or len(required_for) != 1
                or not isinstance(required_for[0], str)):
            _fail("field-capability-cardinality", f"{path}.required_for", field, root)
        capability = required_for[0]
        if not re.fullmatch(r"[a-z][a-z0-9_]*", capability):
            _fail("invalid-capability-id", f"{path}.required_for[0]", field, root)
        capability_c_name = _c_name(capability)
        previous_capability = capability_names.get(capability_c_name)
        if previous_capability is not None and previous_capability != capability:
            _fail("capability-c-name-collision", f"{path}.required_for[0]", field, root)
        capability_names[capability_c_name] = capability
        low = field["min"]
        high = field["max"]
        if (not isinstance(low, (int, float)) or isinstance(low, bool)
                or not isinstance(high, (int, float)) or isinstance(high, bool)
                or not math.isfinite(low) or not math.isfinite(high) or low > high):
            _fail("invalid-field-range", path, field, root)
        if (not isinstance(field["unit"], str) or not field["unit"]
                or not isinstance(field["rounding"], str) or not field["rounding"]
                or not isinstance(field["label_key"], str) or not field["label_key"]):
            _fail("invalid-field-metadata", path, field, root)
        ui = field["ui"]
        if not isinstance(ui, dict):
            _fail("invalid-field-metadata", f"{path}.ui", field, root)
        extra_ui = sorted(set(ui) - FIELD_UI_KEYS)
        if extra_ui:
            _fail("field-ui-key-forbidden", f"{path}.ui.{extra_ui[0]}", field, root)
        if (not isinstance(ui.get("format"), str)
                or not isinstance(ui.get("description_key"), str)):
            _fail("invalid-field-metadata", f"{path}.ui", field, root)
        evolution = field["evolution"]
        if not isinstance(evolution, dict):
            _fail("invalid-field-evolution", f"{path}.evolution", field, root)
        _unknown_keys(evolution, FIELD_EVOLUTION_KEYS, f"{path}.evolution", root)
        if (not isinstance(evolution.get("since"), int)
                or isinstance(evolution.get("since"), bool)
                or evolution["since"] < 1
                or not isinstance(evolution.get("deprecated"), bool)):
            _fail("invalid-field-evolution", f"{path}.evolution", field, root)

    for index, view in enumerate(views):
        path = f"$.views[{index}]"
        if not isinstance(view, dict):
            _fail("view-object-required", path, root, root)
        forbidden = sorted(set(view) & {"label_key", "unit", "c_type", "rounding"})
        if forbidden:
            _fail("view-schema-metadata-forbidden", f"{path}.{forbidden[0]}", view, root)
        _unknown_keys(view, VIEW_KEYS, path, root)
        _validate_source(view, path, root)
        if not isinstance(view.get("view_id"), str) or not FIELD_ID_RE.fullmatch(view["view_id"]):
            _fail("invalid-view-id", f"{path}.view_id", view, root)
        if not isinstance(view.get("layout"), str):
            _fail("invalid-view-layout", f"{path}.layout", view, root)
        order = view.get("order")
        if not isinstance(order, list):
            _fail("view-field-list-required", f"{path}.order", view, root)
        chart = view.get("chart")
        if not isinstance(chart, dict):
            _fail("view-chart-required", f"{path}.chart", view, root)
        _unknown_keys(chart, VIEW_CHART_KEYS, f"{path}.chart", root)
        chart_fields = chart.get("field_ids")
        if not isinstance(chart_fields, list):
            _fail("view-field-list-required", f"{path}.chart.field_ids", view, root)
        for collection_path, references in ((f"{path}.order", order), (f"{path}.chart.field_ids", chart_fields)):
            for ref_index, field_id in enumerate(references):
                if field_id not in field_ids:
                    _fail("unknown-view-field", f"{collection_path}[{ref_index}]", view, root)

    item_ids: set[str] = set()
    items_by_id: dict[str, dict[str, Any]] = {}
    item_names: dict[str, str] = {}
    item_hashes: dict[int, str] = {}
    for index, item in enumerate(items):
        path = f"$.items[{index}]"
        if not isinstance(item, dict):
            _fail("item-object-required", path, root, root)
        missing = sorted(REQUIRED_ITEM_KEYS - set(item))
        if missing:
            _fail("item-metadata-required", f"{path}.{missing[0]}", item, root)
        _unknown_keys(item, ITEM_KEYS, path, root)
        _validate_source(item, path, root)
        def_id = item.get("def_id")
        if not isinstance(def_id, str) or not DEF_ID_RE.fullmatch(def_id):
            _fail("invalid-def-id", f"{path}.def_id", item, root)
        if def_id in item_ids:
            _fail("duplicate-def-id", f"{path}.def_id", item, root)
        item_ids.add(def_id)
        items_by_id[def_id] = item
        kind = item["kind"]
        if not isinstance(kind, str) or not TAG_RE.fullmatch(kind):
            _fail("invalid-kind", f"{path}.kind", item, root)
        tags = item["tags"]
        if not isinstance(tags, list):
            _fail("invalid-item-tag", f"{path}.tags", item, root)
        seen_tags: set[str] = set()
        for tag_index, tag in enumerate(tags):
            if not isinstance(tag, str) or not TAG_RE.fullmatch(tag):
                _fail("invalid-item-tag", f"{path}.tags[{tag_index}]", item, root)
            if tag == kind:
                _fail("tag-equals-kind", f"{path}.tags[{tag_index}]", item, root)
            if tag in seen_tags:
                _fail("duplicate-item-tag", f"{path}.tags[{tag_index}]", item, root)
            seen_tags.add(tag)
        stack = item["stack"]
        if not isinstance(stack, int) or isinstance(stack, bool) or stack < 0 or stack > (1 << 63) - 1:
            _fail("invalid-stack", f"{path}.stack", item, root)
        if item["authoring_mode"] not in AUTHORING_MODES:
            _fail("invalid-authoring-mode", f"{path}.authoring_mode", item, root)
        c_name = _c_name(def_id)
        if c_name in item_names:
            _fail("c-name-collision", f"{path}.def_id", item, root)
        item_names[c_name] = def_id
        digest = hasher(def_id.encode("utf-8"), seed=0)
        if digest in item_hashes:
            _fail("item-id-hash-collision", f"{path}.def_id", item, root)
        item_hashes[digest] = def_id

    for index, item in enumerate(items):
        path = f"$.items[{index}]"
        kind = item["kind"]
        levels = item.get("levels")
        if not isinstance(levels, list):
            _fail("levels-list-required", f"{path}.levels", item, root)
        acquire = item.get("acquire")
        if acquire is not None:
            _validate_transition(acquire, f"{path}.acquire", item, root, items_by_id)
        mode = item["authoring_mode"]
        if mode == "none" and levels:
            _fail("authoring-mode-cardinality", f"{path}.levels", item, root)
        if mode == "single" and len(levels) != 1:
            _fail("authoring-mode-cardinality", f"{path}.levels", item, root)
        if mode in {"table", "generate", "columns"} and not levels:
            _fail("authoring-mode-cardinality", f"{path}.levels", item, root)
        for row_index, row in enumerate(levels):
            row_path = f"{path}.levels[{row_index}]"
            if not isinstance(row, dict):
                _fail("level-row-required", row_path, item, root)
            if "source" not in row or "provenance" not in row:
                _fail("level-metadata-required", row_path, row, root)
            _unknown_keys(row, LEVEL_CORE_KEYS | field_members, row_path, root)
            _validate_source(row, row_path, root)
            expected_level = row_index + 1
            if row.get("level") != expected_level:
                _fail("level-not-contiguous", f"{row_path}.level", row, root)
            if expected_level == 1 and "cost_to_reach" in row:
                _fail("level-one-transition", row_path, row, root)
            if expected_level >= 2 and "cost_to_reach" not in row:
                _fail("level-transition-required", f"{row_path}.cost_to_reach", row, root)
            if "cost_to_reach" in row:
                _validate_transition(row["cost_to_reach"], f"{row_path}.cost_to_reach", row, root, items_by_id)
            for member, field in field_by_member.items():
                if kind in field["required_for"] and member not in row:
                    _fail("required-field-missing", f"{row_path}.{member}", row, root)
            provenance = row["provenance"]
            if not isinstance(provenance, dict):
                _fail("invalid-provenance", f"{row_path}.provenance", row, root)
            expected_provenance = {member for member in field_members if member in row}
            if "cost_to_reach" in row:
                expected_provenance.add("cost_to_reach")
            missing_provenance = sorted(expected_provenance - set(provenance))
            if missing_provenance:
                _fail(
                    "missing-provenance-field",
                    f"{row_path}.provenance.{missing_provenance[0]}",
                    row,
                    root,
                )
            for key, value in provenance.items():
                if key not in expected_provenance:
                    _fail("unknown-provenance-field", f"{row_path}.provenance.{key}", row, root)
                if value not in PROVENANCE_VALUES:
                    _fail("invalid-provenance-value", f"{row_path}.provenance.{key}", row, root)
                valid_for_mode = value == mode or (mode == "columns" and value == "override")
                if not valid_for_mode:
                    _fail("provenance-mode-mismatch", f"{row_path}.provenance.{key}", row, root)
            for member, field in field_by_member.items():
                required = kind in field.get("required_for", [])
                if required and member not in row:
                    _fail("required-field-missing", f"{row_path}.{member}", row, root)
                if member not in row:
                    continue
                if not required:
                    _fail("field-wrong-kind", f"{row_path}.{member}", row, root)
                value = row[member]
                c_type = field["c_type"]
                if c_type in INT_RANGES:
                    if not isinstance(value, int) or isinstance(value, bool):
                        _fail("integer-required", f"{row_path}.{member}", row, root)
                    low, high = INT_RANGES[c_type]
                    if value < low or value > high:
                        _fail("integer-overflow", f"{row_path}.{member}", row, root)
                else:
                    if not isinstance(value, (int, float)) or isinstance(value, bool):
                        _fail("number-required", f"{row_path}.{member}", row, root)
                    if not math.isfinite(value):
                        _fail("non-finite-field-value", f"{row_path}.{member}", row, root)
                if "min" in field and value < field["min"] or "max" in field and value > field["max"]:
                    _fail("value-out-of-range", f"{row_path}.{member}", row, root)
        for field in fields:
            if kind in field.get("required_for", []) and len(levels) == 0:
                _fail("required-field-cardinality", f"{path}.levels", item, root)
    return root


def _write_if_different(path: Path, content: str) -> None:
    encoded = content.encode("utf-8")
    if path.exists() and path.read_bytes() == encoded:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(handle, "wb") as stream:
            stream.write(encoded)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except BaseException:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def _render_luau(snapshot: dict[str, Any]) -> str:
    capability_fields: dict[str, list[dict[str, Any]]] = {}
    for field in snapshot["fields"]:
        capability_fields.setdefault(field["required_for"][0], []).append(field)

    def class_name(capability: str) -> str:
        return "".join(part.capitalize() for part in capability.split("_"))

    def lua_type(c_type: str) -> str:
        return "integer" if c_type in INT_RANGES else "number"

    lines = [
        "-- Generated by generate_items_api_proof.py; do not edit.",
        "---@meta",
        "",
        "---@class ItemsGameTransition",
        "---@field kind 'unavailable'|'free'|'cost'",
        "---@field cost table[]?",
        "",
    ]
    for capability, fields in capability_fields.items():
        lines.append(f"---@class ItemsGame{class_name(capability)}LevelRow")
        lines.append("---@field level integer")
        for field in fields:
            lines.append(
                f"---@field {field['member']} {lua_type(field['c_type'])} # "
                f"field_id={field['field_id']} unit={field['unit']} label_key={field['label_key']}"
            )
        lines.extend(["---@field cost_to_reach ItemsGameTransition?", ""])

    level_types = [f"ItemsGame{class_name(capability)}LevelRow[]" for capability in capability_fields]
    levels_annotation = "|".join(level_types) if level_types else "table[]"
    lines.extend([
        "---@class ItemsGameItemDefinition",
        "---@field id string",
        "---@field kind string",
        "---@field tags string[]",
        "---@field stack integer",
        f"---@field levels {levels_annotation}?",
        "---@field acquire ItemsGameTransition?",
        "",
        "---@class ItemsGameSchemaExtension",
        "---@field level_row table<string, unknown>",
        "",
        "---@class ItemsGameViewDefinition",
        "---@field id string",
        "---@field layout string",
        "---@field order string[]",
        "---@field chart table?",
        "",
        "local items = {}",
        "",
        "---@param definition ItemsGameItemDefinition",
        "function items.define(definition) end",
        "",
        "---@param extension ItemsGameSchemaExtension",
        "function items.extend_schema(extension) end",
        "",
        "---@param view ItemsGameViewDefinition",
        "function items.view(view) end",
        "",
        "return items",
        "",
    ])
    return "\n".join(lines)


def _render(snapshot: dict[str, Any]) -> tuple[str, str, str]:
    fields = snapshot["fields"]
    items = snapshot["items"]
    item_index = {item["def_id"]: index for index, item in enumerate(items)}
    capability_fields: dict[str, list[dict[str, Any]]] = {}
    for field in fields:
        capability_fields.setdefault(field["required_for"][0], []).append(field)
    header = [
        "/* Generated by generate_items_api_proof.py; do not edit. */",
        "#ifndef ITEMS_GAME_GEN_H",
        "#define ITEMS_GAME_GEN_H",
        "",
    ]
    for item in items:
        digest = xxh64(item["def_id"].encode("utf-8"), seed=0)
        header.append(f"#define ITEM_{_c_name(item['def_id'])} ((item_id_t){{ UINT64_C(0x{digest:016X}) }})")
    for field in fields:
        header.append(f"#define ITEM_FIELD_{_c_name(field['field_id'])} {json.dumps(field['field_id'])}")
    for capability, capability_schema in capability_fields.items():
        capability_upper = _c_name(capability)
        header.extend([
            "",
            f"#define ITEMS_GAME_HAS_{capability_upper} 1",
            f"typedef struct item_{capability}_level_t {{",
        ])
        for field in capability_schema:
            header.append(f"    {field['c_type']} {field['member']};")
        header.extend([
            "    item_transition_t cost_to_reach;",
            f"}} item_{capability}_level_t;",
            "",
            f"bool items_is_{capability}(item_def_ref_t ref);",
            f"uint32_t items_{capability}_level_count(item_def_ref_t ref);",
            f"bool items_{capability}_level_exists(item_def_ref_t ref, uint32_t level);",
            f"item_{capability}_level_t items_{capability}_level(item_def_ref_t ref, uint32_t level);",
        ])
    header.extend(["", "#endif", ""])

    internal = [
        "/* Generated build-local seam; consumed only by items_api.c and this catalog. */",
        "#ifndef ITEMS_GAME_INTERNAL_GEN_H",
        "#define ITEMS_GAME_INTERNAL_GEN_H",
        "",
        '#include "features/items/items.h"',
        "",
        "uint32_t items_game_internal_item_count(void);",
        "item_id_t items_game_internal_item_id(uint32_t index);",
        "item_core_t items_game_internal_core(uint32_t index);",
        "const char *items_game_internal_def_id(uint32_t index);",
        "item_transition_t items_game_internal_acquire(uint32_t index);",
        "uint32_t items_game_internal_cost_span_count(void);",
        "uint32_t items_game_internal_cost_count(uint32_t opaque);",
        "item_cost_entry_t items_game_internal_cost_at(uint32_t opaque, uint32_t index);",
        "",
        "#endif",
        "",
    ]

    cost_entries: list[tuple[int, int]] = []
    cost_spans: list[tuple[int, int]] = [(0, 0)]

    def add_cost(costs: list[dict[str, Any]]) -> int:
        offset = len(cost_entries)
        for cost in costs:
            cost_entries.append((item_index[cost["item_ref"]], cost["count"]))
        cost_spans.append((offset, len(costs)))
        return len(cost_spans) - 1

    acquire_transitions: list[tuple[str, int]] = []
    for item in items:
        acquire = item.get("acquire")
        if acquire is None:
            acquire_transitions.append(("ITEM_TRANSITION_UNAVAILABLE", 0))
        elif acquire["kind"] == "cost":
            acquire_transitions.append(("ITEM_TRANSITION_COST", add_cost(acquire["cost"])))
        else:
            acquire_transitions.append(("ITEM_TRANSITION_FREE", 0))

    capability_rows: dict[str, list[tuple[dict[str, Any], str, int]]] = {}
    capability_spans: dict[str, list[tuple[int, int]]] = {}
    for capability in capability_fields:
        rows: list[tuple[dict[str, Any], str, int]] = []
        spans: list[tuple[int, int]] = []
        for item in items:
            offset = len(rows)
            if item["kind"] == capability:
                for row in item["levels"]:
                    if "cost_to_reach" not in row:
                        transition = ("ITEM_TRANSITION_UNAVAILABLE", 0)
                    elif row["cost_to_reach"]["kind"] == "cost":
                        transition = ("ITEM_TRANSITION_COST", add_cost(row["cost_to_reach"]["cost"]))
                    else:
                        transition = ("ITEM_TRANSITION_FREE", 0)
                    rows.append((row, transition[0], transition[1]))
            spans.append((offset, len(rows) - offset))
        capability_rows[capability] = rows
        capability_spans[capability] = spans

    def c_string(value: str) -> str:
        return json.dumps(value, ensure_ascii=True)

    def c_number(value: int | float, c_type: str) -> str:
        if c_type == "int32_t":
            return f"INT32_C({value})"
        if c_type == "uint32_t":
            return f"UINT32_C({value})"
        if c_type == "int64_t":
            return f"INT64_C({value})"
        if c_type == "uint64_t":
            return f"UINT64_C({value})"
        rendered = repr(float(value))
        return f"{rendered}f" if c_type == "float" else rendered

    source = [
        "/* Generated by generate_items_api_proof.py; do not edit. */",
        '#include "features/items/items.h"',
        '#include "items_game.internal.gen.h"',
        '#include "core/nt_assert.h"',
        "",
        "typedef struct generated_cost_span_t { uint32_t offset; uint32_t count; } generated_cost_span_t;",
        "typedef struct generated_item_t { item_core_t core; const char *def_id; } generated_item_t;",
        "",
        "static const generated_item_t s_items[] = {",
    ]
    for item in items:
        digest = xxh64(item["def_id"].encode("utf-8"), seed=0)
        source.append(
            f"    {{ {{ {{ UINT64_C(0x{digest:016X}) }}, INT64_C({item['stack']}) }}, "
            f"{c_string(item['def_id'])} }},"
        )
    source.extend([
        "};",
        "#define ITEMS_GENERATED_COUNT ((uint32_t)(sizeof(s_items) / sizeof(s_items[0])))",
        "",
        "static const item_transition_t s_acquire[] = {",
    ])
    for kind, cost in acquire_transitions:
        source.append(f"    {{ {kind}, {{ UINT32_C({cost}) }} }},")
    source.extend(["};", "", "static const item_cost_entry_t s_cost_entries[] = {"])
    if cost_entries:
        for item_ref, count in cost_entries:
            digest = xxh64(items[item_ref]["def_id"].encode("utf-8"), seed=0)
            source.append(f"    {{ {{ UINT64_C(0x{digest:016X}) }}, INT64_C({count}) }},")
    else:
        source.append("    { { UINT64_C(0) }, INT64_C(0) },")
    source.extend(["};", "", "static const generated_cost_span_t s_cost_spans[] = {"])
    for offset, count in cost_spans:
        source.append(f"    {{ UINT32_C({offset}), UINT32_C({count}) }},")
    source.extend([
        "};",
        "#define ITEMS_COST_SPAN_COUNT ((uint32_t)(sizeof(s_cost_spans) / sizeof(s_cost_spans[0])))",
        "",
        "uint32_t items_game_internal_item_count(void) { return ITEMS_GENERATED_COUNT; }",
        "item_id_t items_game_internal_item_id(uint32_t index) { return s_items[index].core.id; }",
        "item_core_t items_game_internal_core(uint32_t index) { return s_items[index].core; }",
        "const char *items_game_internal_def_id(uint32_t index) { return s_items[index].def_id; }",
        "item_transition_t items_game_internal_acquire(uint32_t index) { return s_acquire[index]; }",
        "uint32_t items_game_internal_cost_span_count(void) { return ITEMS_COST_SPAN_COUNT; }",
        "uint32_t items_game_internal_cost_count(uint32_t opaque) { return s_cost_spans[opaque].count; }",
        "item_cost_entry_t items_game_internal_cost_at(uint32_t opaque, uint32_t index) {",
        "    generated_cost_span_t span = s_cost_spans[opaque];",
        "    return s_cost_entries[span.offset + index];",
        "}",
        "",
    ])

    for capability, capability_schema in capability_fields.items():
        rows = capability_rows[capability]
        spans = capability_spans[capability]
        source.extend([
            f"typedef struct generated_{capability}_span_t {{ uint32_t offset; uint32_t count; }} generated_{capability}_span_t;",
            "",
            f"static const item_{capability}_level_t s_{capability}_levels[] = {{",
        ])
        for row, kind, cost in rows:
            members = " ".join(
                f".{field['member']} = {c_number(row[field['member']], field['c_type'])},"
                for field in capability_schema
            )
            source.append(
                f"    {{ {members} .cost_to_reach = {{ {kind}, {{ UINT32_C({cost}) }} }} }},"
            )
        if not rows:
            source.append("    { 0 },")
        source.extend(["};", "", f"static const generated_{capability}_span_t s_{capability}_spans[] = {{"])
        for offset, count in spans:
            source.append(f"    {{ UINT32_C({offset}), UINT32_C({count}) }},")
        source.extend([
            "};",
            "",
            f"bool items_is_{capability}(item_def_ref_t ref) {{",
            f"    return ref._index < ITEMS_GENERATED_COUNT && s_{capability}_spans[ref._index].count > 0;",
            "}",
            "",
            f"uint32_t items_{capability}_level_count(item_def_ref_t ref) {{",
            f"    return items_is_{capability}(ref) ? s_{capability}_spans[ref._index].count : 0;",
            "}",
            "",
            f"bool items_{capability}_level_exists(item_def_ref_t ref, uint32_t level) {{",
            f"    return items_is_{capability}(ref) && level > 0 && level <= s_{capability}_spans[ref._index].count;",
            "}",
            "",
            f"item_{capability}_level_t items_{capability}_level(item_def_ref_t ref, uint32_t level) {{",
            f"    NT_ASSERT(items_{capability}_level_exists(ref, level) && \"items_{capability}_level: invalid item or target level\");",
            f"    generated_{capability}_span_t span = s_{capability}_spans[ref._index];",
            f"    return s_{capability}_levels[span.offset + level - 1];",
            "}",
            "",
        ])
    return "\n".join(header), "\n".join(internal), "\n".join(source) + "\n"


def generate(snapshot_path: Path, out_dir: Path, hasher: Callable[..., int] = xxh64) -> None:
    snapshot = json.loads(snapshot_path.read_text(encoding="utf-8"))
    load_and_validate(snapshot, hasher=hasher)
    header, internal, source = _render(snapshot)
    _write_if_different(out_dir / "items_game.gen.h", header)
    _write_if_different(out_dir / "items_game.internal.gen.h", internal)
    _write_if_different(out_dir / "items_game.gen.c", source)
    _write_if_different(out_dir / "items_game.luau", _render_luau(snapshot))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--snapshot", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    try:
        generate(args.snapshot, args.out_dir)
    except (OSError, json.JSONDecodeError) as error:
        print(json.dumps({"code": "input-error", "message": str(error)}, sort_keys=True))
        return 2
    except DiagnosticError as error:
        print(json.dumps(error.diagnostic, sort_keys=True))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
