#!/usr/bin/env python3
"""Emit the production Items catalog as typed C arrays from a normalized Snapshot."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import sys
import tempfile
from typing import Any

from items_c_identifiers import is_c_member_name
from items_snapshot import (
    ITEM_KEYS as SNAPSHOT_ITEM_KEYS,
    ITEM_REQUIRED_FOR,
    TRACK_REQUIRED_FOR,
    SnapshotFailure,
    validate_snapshot_content_hash,
)
from items_xxh64 import xxh64


SNAPSHOT_SCHEMA = "items.snapshot.v1"
RUNTIME_EXPORT_SCHEMA = "items.runtime_export.v1"
# Identifies the generated accessor surface; a change here invalidates every
# schema ABI fingerprint compiled against the previous surface.
ACCESSOR_ABI = 5
MAX_ITEMS = 100_000
MAX_FIELDS = 256
MAX_LEVELS = 100_000
MAX_VALUES = 1_000_000
MAX_COSTS = 200_000
U32_MAX = (1 << 32) - 1
I64_MIN = -(1 << 63)
I64_MAX = (1 << 63) - 1

TRANSITION_UNAVAILABLE = "ITEM_TRANSITION_UNAVAILABLE"
TRANSITION_FREE = "ITEM_TRANSITION_FREE"
TRANSITION_COST = "ITEM_TRANSITION_COST"

OUTPUT_NAMES = (
    "items_catalog.gen.h",
    "items_catalog.internal.gen.h",
    "items_catalog.gen.c",
    "items_catalog.luau",
)


class CatalogFailure(ValueError):
    pass


class CatalogArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        _fail("cli.arguments", message)


def _fail(code: str, message: str) -> None:
    raise CatalogFailure(f"{code}: {message}")


def _u32(value: Any, code: str) -> int:
    if type(value) is not int or value < 0 or value > U32_MAX:
        _fail(code, "expected uint32")
    return value


def _i64(value: Any, code: str) -> int:
    if type(value) is not int or value < I64_MIN or value > I64_MAX:
        _fail(code, "expected int64")
    return value


def _c_name(value: str) -> str:
    return "_".join(part.upper() for part in value.replace(".", "_").split("_") if part)


def _snapshot_digest(snapshot: dict[str, Any]) -> bytes:
    try:
        content_hash = validate_snapshot_content_hash(snapshot)
    except SnapshotFailure as error:
        _fail("catalog.content_hash", error.message)
    try:
        return bytes.fromhex(content_hash[7:])
    except ValueError:
        _fail("catalog.content_hash", "Snapshot content_hash is not hexadecimal")


def _schema_descriptor(fields: list[dict[str, Any]], item_ids: list[str]) -> bytes:
    descriptor = {
        "accessor_abi": ACCESSOR_ABI,
        "fields": [
            {
                key: field.get(key)
                for key in (
                    "id", "member", "section", "type", "required_for_items",
                    "min", "max", "rounding", "unit",
                )
            }
            for field in fields
        ],
        "item_ids": item_ids,
    }
    return json.dumps(descriptor, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")


def _cost_values(
    value: Any, item_indices: dict[str, int], code: str, remaining: int,
) -> tuple[list[tuple[int, int]], bool]:
    if value is None:
        return [], False
    if not isinstance(value, dict):
        _fail(code, "cost must be normalized object")
    kind = value.get("__studio_kind")
    if kind == "free":
        return [], True
    entries = [value] if kind == "cost" else value.get("entries") if kind == "costs" else None
    if not isinstance(entries, list) or not entries:
        _fail(code, "cost requires one or more normalized entries")
    if len(entries) > remaining:
        _fail("catalog.cost_budget", f"cost count exceeds remaining budget {remaining}")
    result = []
    for entry in entries:
        ref = entry.get("item") if isinstance(entry, dict) else None
        item_id = ref.get("id") if isinstance(ref, dict) and ref.get("__studio_kind") == "item_ref" else None
        if not isinstance(entry, dict) or item_id not in item_indices:
            _fail(code, f"cost references unknown item: {item_id}")
        count = _i64(entry.get("count"), code)
        if count <= 0:
            _fail("catalog.cost_count", "cost count must be positive")
        result.append((item_indices[item_id], count))
    return result, False


def normalize(
    snapshot: dict[str, Any], *,
    max_items: int = MAX_ITEMS,
    max_fields: int = MAX_FIELDS,
    max_levels: int = MAX_LEVELS,
    max_values: int = MAX_VALUES,
    max_costs: int = MAX_COSTS,
) -> dict[str, Any]:
    """Project an already-normalized Snapshot onto the facts the typed C API exposes."""
    if not isinstance(snapshot, dict) or snapshot.get("schema") != SNAPSHOT_SCHEMA:
        _fail("catalog.snapshot_schema", f"expected {SNAPSHOT_SCHEMA}")
    snapshot_digest = _snapshot_digest(snapshot)
    items = snapshot.get("items")
    fields = snapshot.get("fields")
    runtime = snapshot.get("runtime_export")
    if not isinstance(items, list) or not isinstance(fields, list) or not isinstance(runtime, dict):
        _fail("catalog.snapshot_shape", "Snapshot requires items, fields, and runtime_export")
    for limit, code in (
        (max_items, "catalog.item_budget"), (max_fields, "catalog.field_budget"),
        (max_levels, "catalog.level_budget"), (max_values, "catalog.value_budget"),
        (max_costs, "catalog.cost_budget"),
    ):
        if type(limit) is not int or limit < 0:
            _fail(code, "budget must be a non-negative integer")
    if len(items) > max_items:
        _fail("catalog.item_budget", f"count {len(items)} exceeds {max_items}")
    if len(fields) > max_fields:
        _fail("catalog.field_budget", f"count {len(fields)} exceeds {max_fields}")

    # Sorted identity order is the generated index order: item_def_ref_t indices
    # stay stable as long as the id set does.
    sorted_items = sorted(items, key=lambda entry: entry.get("id", "") if isinstance(entry, dict) else "")
    sorted_fields = sorted(fields, key=lambda entry: entry.get("id", "") if isinstance(entry, dict) else "")
    item_ids = [item.get("id") if isinstance(item, dict) else None for item in sorted_items]
    field_ids = [field.get("id") if isinstance(field, dict) else None for field in sorted_fields]
    if (any(not isinstance(item_id, str) or not item_id for item_id in item_ids)
            or len(item_ids) != len(set(item_ids))):
        _fail("catalog.item_id", "item ids must be unique strings")
    if (any(not isinstance(field_id, str) or not field_id for field_id in field_ids)
            or len(field_ids) != len(set(field_ids))):
        _fail("catalog.field_id", "field ids must be unique strings")

    runtime_items = runtime.get("items")
    if (runtime.get("schema") != RUNTIME_EXPORT_SCHEMA
            or runtime.get("field_ids") != field_ids or not isinstance(runtime_items, list)):
        _fail("catalog.runtime_metadata", "runtime_export does not match Snapshot fields")
    if any(not isinstance(entry, dict) or not isinstance(entry.get("id"), str) for entry in runtime_items):
        _fail("catalog.runtime_metadata", "runtime item metadata is malformed")
    runtime_by_id = {entry["id"]: entry for entry in runtime_items}
    if sorted(runtime_by_id) != item_ids or len(runtime_by_id) != len(runtime_items):
        _fail("catalog.runtime_metadata", "runtime_export does not match Snapshot items")

    item_indices = {item_id: index for index, item_id in enumerate(item_ids)}
    item_macros: set[str] = set()
    item_hashes: dict[int, str] = {}
    digests: list[int] = []
    for item_id in item_ids:
        macro = f"ITEM_{_c_name(item_id)}"
        digest = xxh64(item_id.encode("utf-8"))
        if macro in item_macros or digest in item_hashes:
            _fail("catalog.item_hash_collision", f"item identity collision: {item_id}")
        item_macros.add(macro)
        item_hashes[digest] = item_id
        digests.append(digest)

    for field in sorted_fields:
        required_for = field.get(ITEM_REQUIRED_FOR, [])
        if not isinstance(required_for, list) or not all(
            isinstance(capability, str) and capability for capability in required_for
        ):
            _fail("catalog.field", f"field {ITEM_REQUIRED_FOR} must contain capability names")

    # A field that names no item kind belongs to a neighbouring declaration space --
    # progression tracks, whose columns have their own generator and may be
    # fractional. The item catalog neither projects it nor constrains its type.
    catalog_fields = [field for field in sorted_fields if field.get(ITEM_REQUIRED_FOR)]

    field_records: list[dict[str, Any]] = []
    field_by_member: dict[str, int] = {}
    field_macros: set[str] = set()
    for index, field in enumerate(catalog_fields):
        member = field.get("member")
        unit = field.get("unit")
        # Say which field and which kind, or the author is left reading an ABI
        # complaint about a column they meant for a track.
        if field.get("type") != "i64" or field.get("rounding") != "exact":
            named = ", ".join(sorted(field[ITEM_REQUIRED_FOR]))
            _fail(
                "catalog.field",
                f"field {field.get('id')!r} is {field.get('type')}, but it is required for "
                f"the item kind {named}; the item catalog carries exact integers only -- "
                f"move it to {TRACK_REQUIRED_FOR}, or declare the field exact",
            )
        if not is_c_member_name(member) or not isinstance(unit, str) or not unit:
            _fail("catalog.field", f"field {field.get('id')!r} has invalid ABI metadata")
        if member in field_by_member:
            _fail("catalog.field", f"field member collision: {member}")
        minimum = _i64(field.get("min"), "catalog.field_range")
        maximum = _i64(field.get("max"), "catalog.field_range")
        if minimum > maximum:
            _fail("catalog.field_range", "field min exceeds max")
        macro = f"ITEM_FIELD_{_c_name(field['id'])}"
        if macro in field_macros:
            _fail("catalog.field", f"field macro collision: {field['id']}")
        field_macros.add(macro)
        field_by_member[member] = index
        field_records.append({
            "id": field["id"], "member": member, "unit": unit,
            "min": minimum, "max": maximum, "macro": macro,
        })

    capabilities: dict[str, list[int]] = {}
    capability_names: set[str] = set()
    for index, field in enumerate(catalog_fields):
        for capability in field[ITEM_REQUIRED_FOR]:
            capability_name = _c_name(capability).lower()
            if not capability_name:
                _fail("catalog.field", "capability name cannot be empty")
            if capability_name in capability_names and capability not in capabilities:
                _fail("catalog.field", f"capability C name collision: {capability}")
            capability_names.add(capability_name)
            capabilities.setdefault(capability, []).append(index)

    cost_entries: list[tuple[int, int]] = []
    # Span zero is the null cost: item_cost_ref_t treats it as "no requirement".
    cost_spans: list[tuple[int, int]] = [(0, 0)]

    def transition(node: Any, code: str) -> tuple[str, int]:
        values, free = _cost_values(node, item_indices, code, max_costs - len(cost_entries))
        if free:
            return TRANSITION_FREE, 0
        if not values:
            return TRANSITION_UNAVAILABLE, 0
        offset = len(cost_entries)
        cost_entries.extend(values)
        cost_spans.append((offset, len(values)))
        return TRANSITION_COST, len(cost_spans) - 1

    item_records: list[dict[str, Any]] = []
    level_total = 0
    value_total = 0
    for item_index, item in enumerate(sorted_items):
        item_id = item["id"]
        # The Snapshot owns the complete authoring contract; this projection
        # carries only what the typed API returns. The Snapshot digest still
        # feeds the content fingerprint, so any metadata edit is visible.
        unsupported = set(item) - SNAPSHOT_ITEM_KEYS
        if unsupported:
            _fail("catalog.unsupported_item_field", f"item {item_id}: {sorted(unsupported)[0]}")
        kind = item.get("kind")
        if not isinstance(kind, str) or not kind:
            _fail("catalog.item", f"item {item_id} requires kind")
        metadata = runtime_by_id[item_id]
        storage = metadata.get("storage")
        if storage not in ("stack", "unique"):
            _fail("catalog.runtime_metadata", f"item {item_id} has invalid storage")
        stack = _u32(item.get("stack"), "catalog.stack")
        if (storage == "unique") != (stack == 1):
            _fail("catalog.item", f"item {item_id} storage contradicts its stack size")

        currency = item.get("currency")
        currency_cap = None
        if currency is not None:
            if (not isinstance(currency, dict) or set(currency) != {"hud", "cap"}
                    or type(currency.get("cap")) is not int or currency["cap"] < 0
                    or currency["cap"] > I64_MAX or kind != "currency"):
                _fail("catalog.currency", f"item {item_id} has invalid currency block")
            currency_cap = currency["cap"]

        acquire = item.get("acquire")
        if acquire is None:
            acquire_transition = (TRANSITION_UNAVAILABLE, 0)
        elif not isinstance(acquire, dict) or set(acquire) != {"cost"}:
            _fail("catalog.acquire", f"item {item_id} acquire must contain only cost")
        else:
            acquire_transition = transition(acquire["cost"], "catalog.acquire")

        levels = item.get("levels")
        raw_rows = [] if levels is None else levels.get("rows") if isinstance(levels, dict) else None
        if not isinstance(raw_rows, list):
            _fail("catalog.levels", f"item {item_id} requires normalized level rows")
        if metadata.get("level_count") != len(raw_rows):
            _fail("catalog.runtime_metadata", f"item {item_id} level count mismatch")
        if storage == "stack" and raw_rows:
            _fail("catalog.levels", f"item {item_id} is stackable and cannot carry levels")
        level_total += len(raw_rows)
        if level_total > max_levels:
            _fail("catalog.level_budget", f"level count exceeds {max_levels}")

        members = [field_records[index]["member"] for index in capabilities.get(kind, [])]
        level_records = []
        for level, row in enumerate(raw_rows, start=1):
            if not isinstance(row, dict):
                _fail("catalog.level", f"item {item_id} level must be an object")
            unknown = sorted(set(row) - {"cost_to_reach"} - set(field_by_member))
            if unknown:
                _fail("catalog.level_field", f"item {item_id} has unknown field {unknown[0]}")
            values = {}
            for member in members:
                if member not in row:
                    _fail("catalog.level_field", f"item {item_id} level {level} is missing {member}")
                values[member] = _i64(row[member], "catalog.level_value")
            value_total += len(values)
            if value_total > max_values:
                _fail("catalog.value_budget", f"value count exceeds {max_values}")
            level_records.append({
                "values": values,
                "transition": transition(row.get("cost_to_reach"), "catalog.level_cost"),
            })

        item_records.append({
            "id": item_id, "kind": kind, "digest": digests[item_index],
            "stack": stack, "currency_cap": currency_cap,
            "acquire": acquire_transition, "levels": level_records,
        })

    if len(cost_entries) > max_costs:
        _fail("catalog.cost_budget", f"count {len(cost_entries)} exceeds {max_costs}")

    # The ABI covers the surface this catalog generates. A track column generates
    # nothing here, so retuning one is not an item-catalog ABI break.
    descriptor = _schema_descriptor(catalog_fields, item_ids)
    return {
        "items": item_records,
        "fields": field_records,
        "capabilities": capabilities,
        "cost_entries": cost_entries,
        "cost_spans": cost_spans,
        "schema_abi": xxh64(descriptor),
        "content_fingerprint": xxh64(descriptor + snapshot_digest),
    }


def _capability_name(capability: str) -> str:
    return _c_name(capability).lower()


def _c_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


def render_header(model: dict[str, Any]) -> str:
    lines = [
        "/* Generated by items_c_catalog.py; do not edit. */",
        "#ifndef ITEMS_CATALOG_GEN_H",
        "#define ITEMS_CATALOG_GEN_H",
        "",
        f"#define ITEMS_CATALOG_SCHEMA_ABI UINT64_C(0x{model['schema_abi']:016X})",
        f"#define ITEMS_CATALOG_CONTENT_FINGERPRINT UINT64_C(0x{model['content_fingerprint']:016X})",
        f"#define ITEMS_CATALOG_ITEM_COUNT UINT32_C({len(model['items'])})",
        f"#define ITEMS_CATALOG_FIELD_COUNT UINT32_C({len(model['fields'])})",
        f"#define ITEMS_CATALOG_CAPABILITY_COUNT UINT32_C({len(model['capabilities'])})",
        "",
    ]
    for item in model["items"]:
        lines.append(
            f"#define ITEM_{_c_name(item['id'])} ((item_id_t){{ UINT64_C(0x{item['digest']:016X}) }})"
        )
    if model["fields"]:
        lines.append("")
    for field in model["fields"]:
        lines.append(f"#define {field['macro']} {_c_string(field['id'])}")

    for capability, indices in model["capabilities"].items():
        name = _capability_name(capability)
        lines.extend([
            "",
            f"#define ITEMS_GAME_HAS_{_c_name(capability)} 1",
            f"typedef struct item_{name}_level_t {{",
        ])
        lines.extend(f"    int64_t {model['fields'][index]['member']};" for index in indices)
        lines.extend([
            "    item_transition_t cost_to_reach;",
            f"}} item_{name}_level_t;",
            "",
            f"bool items_is_{name}(item_def_ref_t ref);",
            f"uint32_t items_{name}_level_count(item_def_ref_t ref);",
            f"bool items_{name}_level_exists(item_def_ref_t ref, uint32_t level);",
            f"item_{name}_level_t items_{name}_level(item_def_ref_t ref, uint32_t level);",
        ])
    lines.extend(["", "#endif", ""])
    return "\n".join(lines)


def render_internal_header(_model: dict[str, Any]) -> str:
    return "\n".join([
        "/* Generated build-local seam; consumed only by items_api.c and this catalog. */",
        "#ifndef ITEMS_CATALOG_INTERNAL_GEN_H",
        "#define ITEMS_CATALOG_INTERNAL_GEN_H",
        "",
        '#include "features/items/items.h"',
        "",
        "uint32_t items_catalog_internal_item_count(void);",
        "item_id_t items_catalog_internal_item_id(uint32_t index);",
        "item_core_t items_catalog_internal_core(uint32_t index);",
        "const char *items_catalog_internal_def_id(uint32_t index);",
        "item_transition_t items_catalog_internal_acquire(uint32_t index);",
        "uint32_t items_catalog_internal_level_count(uint32_t index);",
        "item_transition_t items_catalog_internal_level_transition(uint32_t index, uint32_t level);",
        "uint32_t items_catalog_internal_cost_span_count(void);",
        "uint32_t items_catalog_internal_cost_count(uint32_t opaque);",
        "item_cost_entry_t items_catalog_internal_cost_at(uint32_t opaque, uint32_t index);",
        "bool items_catalog_internal_has_currency(uint32_t index);",
        "int64_t items_catalog_internal_currency_cap(uint32_t index);",
        "",
        "#endif",
        "",
    ])


def _transition_literal(transition: tuple[str, int]) -> str:
    kind, span = transition
    return f"{{ {kind}, {{ UINT32_C({span}) }} }}"


def render_source(model: dict[str, Any]) -> str:
    items = model["items"]
    lines = [
        "/* Generated by items_c_catalog.py; do not edit. */",
        '#include "features/items/items.h"',
        '#include "items_catalog.internal.gen.h"',
        '#include "core/nt_assert.h"',
        "",
        "typedef struct generated_span_t { uint32_t offset; uint32_t count; } generated_span_t;",
        "typedef struct generated_item_t {",
        "    item_core_t core;",
        "    const char *def_id;",
        "    int64_t currency_cap;",
        "    bool has_currency;",
        "} generated_item_t;",
        "",
        "static const generated_item_t s_items[] = {",
    ]
    for item in items:
        cap = item["currency_cap"]
        lines.append(
            f"    {{ {{ {{ UINT64_C(0x{item['digest']:016X}) }}, INT64_C({item['stack']}) }}, "
            f"{_c_string(item['id'])}, INT64_C({0 if cap is None else cap}), "
            f"{'true' if cap is not None else 'false'} }},"
        )
    if not items:
        lines.append('    { { { UINT64_C(0) }, INT64_C(0) }, "", INT64_C(0), false },')
    lines.extend([
        "};",
        "#define ITEMS_GENERATED_COUNT ITEMS_CATALOG_ITEM_COUNT",
        "",
        "static const item_transition_t s_acquire[] = {",
    ])
    for item in items:
        lines.append(f"    {_transition_literal(item['acquire'])},")
    if not items:
        lines.append(f"    {_transition_literal((TRANSITION_UNAVAILABLE, 0))},")
    lines.extend(["};", "", "static const item_transition_t s_level_transitions[] = {"])
    level_spans: list[tuple[int, int]] = []
    level_transitions = 0
    for item in items:
        level_spans.append((level_transitions, len(item["levels"])))
        for level in item["levels"]:
            lines.append(f"    {_transition_literal(level['transition'])},")
            level_transitions += 1
    if level_transitions == 0:
        lines.append(f"    {_transition_literal((TRANSITION_UNAVAILABLE, 0))},")
    lines.extend(["};", "", "static const generated_span_t s_level_spans[] = {"])
    for offset, count in level_spans:
        lines.append(f"    {{ UINT32_C({offset}), UINT32_C({count}) }},")
    if not level_spans:
        lines.append("    { UINT32_C(0), UINT32_C(0) },")
    lines.extend(["};", "", "static const item_cost_entry_t s_cost_entries[] = {"])
    for item_index, count in model["cost_entries"]:
        digest = items[item_index]["digest"]
        lines.append(f"    {{ {{ UINT64_C(0x{digest:016X}) }}, INT64_C({count}) }},")
    if not model["cost_entries"]:
        lines.append("    { { UINT64_C(0) }, INT64_C(0) },")
    lines.extend(["};", "", "static const generated_span_t s_cost_spans[] = {"])
    for offset, count in model["cost_spans"]:
        lines.append(f"    {{ UINT32_C({offset}), UINT32_C({count}) }},")
    lines.extend([
        "};",
        "#define ITEMS_COST_SPAN_COUNT ((uint32_t)(sizeof(s_cost_spans) / sizeof(s_cost_spans[0])))",
        "",
        "uint32_t items_catalog_internal_item_count(void) { return ITEMS_GENERATED_COUNT; }",
        "item_id_t items_catalog_internal_item_id(uint32_t index) { return s_items[index].core.id; }",
        "item_core_t items_catalog_internal_core(uint32_t index) { return s_items[index].core; }",
        "const char *items_catalog_internal_def_id(uint32_t index) { return s_items[index].def_id; }",
        "item_transition_t items_catalog_internal_acquire(uint32_t index) { return s_acquire[index]; }",
        "uint32_t items_catalog_internal_level_count(uint32_t index) { return s_level_spans[index].count; }",
        "item_transition_t items_catalog_internal_level_transition(uint32_t index, uint32_t level) {",
        "    NT_ASSERT(level > 0U && level <= s_level_spans[index].count);",
        "    return s_level_transitions[s_level_spans[index].offset + level - 1U];",
        "}",
        "uint32_t items_catalog_internal_cost_span_count(void) { return ITEMS_COST_SPAN_COUNT; }",
        "uint32_t items_catalog_internal_cost_count(uint32_t opaque) { return s_cost_spans[opaque].count; }",
        "item_cost_entry_t items_catalog_internal_cost_at(uint32_t opaque, uint32_t index) {",
        "    generated_span_t span = s_cost_spans[opaque];",
        "    return s_cost_entries[span.offset + index];",
        "}",
        "bool items_catalog_internal_has_currency(uint32_t index) { return s_items[index].has_currency; }",
        "int64_t items_catalog_internal_currency_cap(uint32_t index) { return s_items[index].currency_cap; }",
        "",
    ])

    for capability, indices in model["capabilities"].items():
        name = _capability_name(capability)
        members = [model["fields"][index]["member"] for index in indices]
        rows: list[str] = []
        spans: list[tuple[int, int]] = []
        for item in items:
            offset = len(rows)
            if item["kind"] == capability:
                for level in item["levels"]:
                    values = " ".join(
                        f".{member} = INT64_C({level['values'][member]})," for member in members
                    )
                    rows.append(
                        f"    {{ {values} .cost_to_reach = {_transition_literal(level['transition'])} }},"
                    )
            spans.append((offset, len(rows) - offset))
        lines.extend([f"static const item_{name}_level_t s_{name}_levels[] = {{"])
        lines.extend(rows)
        if not rows:
            lines.append("    { 0 },")
        lines.extend(["};", "", f"static const generated_span_t s_{name}_spans[] = {{"])
        for offset, count in spans:
            lines.append(f"    {{ UINT32_C({offset}), UINT32_C({count}) }},")
        if not spans:
            lines.append("    { UINT32_C(0), UINT32_C(0) },")
        lines.extend([
            "};",
            "",
            f"bool items_is_{name}(item_def_ref_t ref) {{",
            f"    return ref._index < ITEMS_GENERATED_COUNT && s_{name}_spans[ref._index].count > 0U;",
            "}",
            "",
            f"uint32_t items_{name}_level_count(item_def_ref_t ref) {{",
            f"    return items_is_{name}(ref) ? s_{name}_spans[ref._index].count : 0U;",
            "}",
            "",
            f"bool items_{name}_level_exists(item_def_ref_t ref, uint32_t level) {{",
            f"    return items_is_{name}(ref) && level > 0U && level <= s_{name}_spans[ref._index].count;",
            "}",
            "",
            f"item_{name}_level_t items_{name}_level(item_def_ref_t ref, uint32_t level) {{",
            f"    NT_ASSERT(items_{name}_level_exists(ref, level) && "
            f"\"items_{name}_level: invalid item or target level\");",
            f"    generated_span_t span = s_{name}_spans[ref._index];",
            f"    return s_{name}_levels[span.offset + level - 1U];",
            "}",
            "",
        ])
    return "\n".join(lines)


def render_luau(model: dict[str, Any]) -> str:
    lines = [
        "-- Generated by items_c_catalog.py; do not edit.",
        "---@meta",
        "",
        "---@class ItemsGameTransition",
        "---@field kind 'unavailable'|'free'|'cost'",
        "---@field cost table[]?",
        "",
    ]
    level_types = []
    for capability, indices in model["capabilities"].items():
        class_name = "".join(part.capitalize() for part in _capability_name(capability).split("_"))
        level_types.append(f"ItemsGame{class_name}LevelRow[]")
        lines.append(f"---@class ItemsGame{class_name}LevelRow")
        lines.append("---@field level integer")
        for index in indices:
            field = model["fields"][index]
            lines.append(
                f"---@field {field['member']} integer # "
                f"field_id={field['id']} unit={field['unit']}"
            )
        lines.extend(["---@field cost_to_reach ItemsGameTransition?", ""])
    lines.extend([
        "---@class ItemsGameItemDefinition",
        "---@field id string",
        "---@field kind string",
        "---@field tags string[]",
        "---@field stack integer",
        f"---@field levels {'|'.join(level_types) if level_types else 'table[]'}?",
        "---@field acquire ItemsGameTransition?",
        "",
        "---@class ItemsGameSchemaExtension",
        "---@field level_row table<string, unknown>",
        "",
        "local items = {}",
        "",
        "---@param definition ItemsGameItemDefinition",
        "function items.define(definition) end",
        "",
        "---@param extension ItemsGameSchemaExtension",
        "function items.extend_schema(extension) end",
        "",
        "return items",
        "",
    ])
    return "\n".join(lines)


def render(model: dict[str, Any]) -> dict[str, str]:
    return {
        "items_catalog.gen.h": render_header(model),
        "items_catalog.internal.gen.h": render_internal_header(model),
        "items_catalog.gen.c": render_source(model),
        "items_catalog.luau": render_luau(model),
    }


def write_if_different(path: Path, content: bytes) -> bool:
    """Atomically replace a generated file only when its bytes changed."""
    if path.exists() and path.read_bytes() == content:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except BaseException:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise
    return True


def generate(snapshot: dict[str, Any], out_dir: Path) -> dict[str, bool]:
    rendered = render(normalize(snapshot))
    return {
        name: write_if_different(out_dir / name, content.encode("utf-8"))
        for name, content in rendered.items()
    }


def main() -> int:
    parser = CatalogArgumentParser(description=__doc__)
    parser.add_argument("--snapshot", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    try:
        args = parser.parse_args()
        snapshot = json.loads(args.snapshot.read_text(encoding="utf-8"))
        changed = generate(snapshot, args.out_dir)
        print(json.dumps({
            "schema": "items.c_catalog.result.v1",
            "changed": changed,
            "outputs": {name: str(args.out_dir / name) for name in OUTPUT_NAMES},
        }, ensure_ascii=False, sort_keys=True))
        return 0
    except CatalogFailure as error:
        message = str(error)
        code, _, detail = message.partition(": ")
        print(json.dumps({
            "schema": "items.c_catalog.error.v1",
            "error": {"code": code, "message": detail or message},
        }, ensure_ascii=False, sort_keys=True), file=sys.stderr)
        return 1
    except (OSError, json.JSONDecodeError) as error:
        print(json.dumps({
            "schema": "items.c_catalog.error.v1",
            "error": {"code": "catalog.io", "message": str(error)},
        }, ensure_ascii=False, sort_keys=True), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
