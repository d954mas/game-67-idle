#!/usr/bin/env python3
"""Encode a normalized Items Snapshot as the compact runtime package v1."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
from typing import Any

from generate_items_api_proof import xxh64


SNAPSHOT_SCHEMA = "items.snapshot.v1"
INSPECT_SCHEMA = "items.runtime.package.inspect.v1"
MAGIC = b"NTITEMS\0"
FORMAT_VERSION = 1
HEADER = struct.Struct("<8sIIIIQQ" + "III" * 6 + "32s")
ITEM = struct.Struct("<QIIIIIIIIII")
FIELD = struct.Struct("<QIIIIqqII")
LEVEL = struct.Struct("<IIIIIIII")
VALUE = struct.Struct("<IIIIq")
COST = struct.Struct("<IIq")
SECTION_NAMES = ("strings", "items", "fields", "levels", "values", "costs")
SECTION_STRIDES = (1, ITEM.size, FIELD.size, LEVEL.size, VALUE.size, COST.size)
MAX_ITEMS = 100_000
MAX_FIELDS = 256
MAX_LEVELS = 100_000
MAX_VALUES = 1_000_000
MAX_COSTS = 200_000
MAX_BYTES = 64 * 1024 * 1024
U32_MAX = (1 << 32) - 1
I64_MIN = -(1 << 63)
I64_MAX = (1 << 63) - 1
CONTENT_FINGERPRINT_OFFSET = 32
LEVEL_FREE = 1


class PackageFailure(ValueError):
    pass


class PackageArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        _fail("cli.arguments", message)


def _fail(code: str, message: str) -> None:
    raise PackageFailure(f"{code}: {message}")


def _u32(value: Any, code: str) -> int:
    if type(value) is not int or value < 0 or value > U32_MAX:
        _fail(code, "expected uint32")
    return value


def _i64(value: Any, code: str) -> int:
    if type(value) is not int or value < I64_MIN or value > I64_MAX:
        _fail(code, "expected int64")
    return value


def _align8(value: int) -> int:
    return (value + 7) & ~7


def _snapshot_digest(snapshot: dict[str, Any]) -> bytes:
    content_hash = snapshot.get("content_hash")
    if (not isinstance(content_hash, str) or not content_hash.startswith("sha256:")
            or len(content_hash) != 71):
        _fail("package.content_hash", "Snapshot requires sha256 content_hash")
    try:
        return bytes.fromhex(content_hash[7:])
    except ValueError:
        _fail("package.content_hash", "Snapshot content_hash is not hexadecimal")


def _schema_descriptor(fields: list[dict[str, Any]]) -> bytes:
    descriptor_fields = []
    for field in fields:
        if not isinstance(field, dict):
            _fail("package.field", "Snapshot field must be an object")
        descriptor_fields.append({
            key: field.get(key)
            for key in ("id", "member", "section", "type", "required_for", "min", "max", "rounding", "unit")
        })
    descriptor = {
        "accessor_abi": 1,
        "format_version": FORMAT_VERSION,
        "sections": [
            {"name": name, "stride": stride}
            for name, stride in zip(SECTION_NAMES, SECTION_STRIDES)
        ],
        "fields": descriptor_fields,
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
        _fail("package.cost_budget", f"cost count exceeds remaining budget {remaining}")
    result = []
    for entry in entries:
        ref = entry.get("item") if isinstance(entry, dict) else None
        item_id = ref.get("id") if isinstance(ref, dict) and ref.get("__studio_kind") == "item_ref" else None
        if not isinstance(entry, dict) or item_id not in item_indices:
            _fail(code, f"cost references unknown item: {item_id}")
        count = _i64(entry.get("count"), code)
        if count <= 0:
            _fail("package.cost_count", "cost count must be positive")
        result.append((item_indices[item_id], count))
    return result, False


def _string_table(strings: set[str]) -> tuple[bytes, dict[str, int]]:
    data = bytearray(b"\0")
    offsets: dict[str, int] = {}
    for value in sorted(strings):
        if not value or "\0" in value:
            _fail("package.string", "runtime strings must be non-empty UTF-8 without NUL")
        offsets[value] = len(data)
        data.extend(value.encode("utf-8"))
        data.append(0)
    return bytes(data), offsets


def _package_size(section_sizes: list[int]) -> tuple[int, list[int]]:
    offsets = []
    cursor = HEADER.size
    for size in section_sizes:
        cursor = _align8(cursor)
        offsets.append(cursor)
        cursor += size
    return cursor, offsets


def _materialize_sections(
    string_bytes: bytes,
    item_rows: list[tuple[int, ...]],
    field_rows: list[tuple[int, ...]],
    level_rows: list[tuple[int, ...]],
    value_rows: list[tuple[int, ...]],
    cost_rows: list[tuple[int, ...]],
) -> list[bytes]:
    return [
        string_bytes,
        b"".join(ITEM.pack(*row) for row in item_rows),
        b"".join(FIELD.pack(*row) for row in field_rows),
        b"".join(LEVEL.pack(*row) for row in level_rows),
        b"".join(VALUE.pack(*row) for row in value_rows),
        b"".join(COST.pack(*row) for row in cost_rows),
    ]


def _c_name(value: str) -> str:
    return "_".join(part.upper() for part in value.replace(".", "_").split("_") if part)


def render_abi_header(snapshot: dict[str, Any]) -> str:
    """Render only schema/item identity ABI; balance values never enter this header."""
    if not isinstance(snapshot, dict) or snapshot.get("schema") != SNAPSHOT_SCHEMA:
        _fail("package.snapshot_schema", f"expected {SNAPSHOT_SCHEMA}")
    fields = snapshot.get("fields")
    items = snapshot.get("items")
    if not isinstance(fields, list) or not isinstance(items, list):
        _fail("package.snapshot_shape", "Snapshot requires fields/items")
    sorted_fields = sorted(fields, key=lambda entry: entry.get("id", "") if isinstance(entry, dict) else "")
    sorted_items = sorted(items, key=lambda entry: entry.get("id", "") if isinstance(entry, dict) else "")
    item_macros: set[str] = set()
    item_hashes: dict[int, str] = {}
    field_macros: set[str] = set()
    lines = [
        "/* Generated by items_runtime_package.py; do not edit. */",
        "#ifndef ITEMS_CATALOG_ABI_GEN_H",
        "#define ITEMS_CATALOG_ABI_GEN_H",
        "",
        "#include <stdint.h>",
        '#include "features/items/items.h"',
        "",
        f"#define ITEMS_CATALOG_FORMAT_VERSION UINT32_C({FORMAT_VERSION})",
        f"#define ITEMS_CATALOG_SCHEMA_ABI UINT64_C(0x{xxh64(_schema_descriptor(sorted_fields)):016X})",
        f"#define ITEMS_CATALOG_ITEM_COUNT UINT32_C({len(sorted_items)})",
        f"#define ITEMS_CATALOG_FIELD_COUNT UINT32_C({len(sorted_fields)})",
        "",
    ]
    for item in sorted_items:
        item_id = item.get("id") if isinstance(item, dict) else None
        if not isinstance(item_id, str) or not item_id:
            _fail("package.item_id", "item id must be a non-empty string")
        macro = f"ITEM_{_c_name(item_id)}"
        digest = xxh64(item_id.encode("utf-8"))
        if macro in item_macros or digest in item_hashes:
            _fail("package.item_hash_collision", f"item identity collision: {item_id}")
        item_macros.add(macro)
        item_hashes[digest] = item_id
        lines.append(f"#define {macro} ((item_id_t){{ UINT64_C(0x{digest:016X}) }})")
    if sorted_items:
        lines.append("")
    for field in sorted_fields:
        field_id = field.get("id") if isinstance(field, dict) else None
        if not isinstance(field_id, str) or not field_id:
            _fail("package.field_id", "field id must be a non-empty string")
        macro = f"ITEM_FIELD_{_c_name(field_id)}"
        if macro in field_macros:
            _fail("package.field_id", f"field macro collision: {field_id}")
        field_macros.add(macro)
        lines.append(f"#define {macro} {json.dumps(field_id)}")
    lines.extend(["", "#endif", ""])
    return "\n".join(lines)


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


def build_package(
    snapshot: dict[str, Any], *,
    max_items: int = MAX_ITEMS,
    max_fields: int = MAX_FIELDS,
    max_levels: int = MAX_LEVELS,
    max_values: int = MAX_VALUES,
    max_costs: int = MAX_COSTS,
    max_bytes: int = MAX_BYTES,
) -> bytes:
    """Build package bytes from an already-normalized Snapshot only."""
    if not isinstance(snapshot, dict) or snapshot.get("schema") != SNAPSHOT_SCHEMA:
        _fail("package.snapshot_schema", f"expected {SNAPSHOT_SCHEMA}")
    items = snapshot.get("items")
    fields = snapshot.get("fields")
    runtime = snapshot.get("runtime_export")
    if not isinstance(items, list) or not isinstance(fields, list) or not isinstance(runtime, dict):
        _fail("package.snapshot_shape", "Snapshot requires items, fields, and runtime_export")
    for limit, code in (
        (max_items, "package.item_budget"), (max_fields, "package.field_budget"),
        (max_levels, "package.level_budget"), (max_values, "package.value_budget"),
        (max_costs, "package.cost_budget"), (max_bytes, "package.byte_budget"),
    ):
        if type(limit) is not int or limit < 0:
            _fail(code, "budget must be a non-negative integer")
    for value, limit, code in (
        (len(items), max_items, "package.item_budget"),
        (len(fields), max_fields, "package.field_budget"),
    ):
        if type(limit) is not int or limit < 0 or value > limit:
            _fail(code, f"count {value} exceeds {limit}")

    sorted_items = sorted(items, key=lambda entry: entry.get("id", "") if isinstance(entry, dict) else "")
    sorted_fields = sorted(fields, key=lambda entry: entry.get("id", "") if isinstance(entry, dict) else "")
    item_ids = [item.get("id") if isinstance(item, dict) else None for item in sorted_items]
    field_ids = [field.get("id") if isinstance(field, dict) else None for field in sorted_fields]
    if (any(not isinstance(item_id, str) or not item_id for item_id in item_ids)
            or len(item_ids) != len(set(item_ids))):
        _fail("package.item_id", "item ids must be unique strings")
    if (any(not isinstance(field_id, str) or not field_id for field_id in field_ids)
            or len(field_ids) != len(set(field_ids))):
        _fail("package.field_id", "field ids must be unique strings")

    runtime_items = runtime.get("items")
    if (runtime.get("schema") != "items.runtime_export.v1"
            or runtime.get("field_ids") != field_ids or not isinstance(runtime_items, list)):
        _fail("package.runtime_metadata", "runtime_export does not match Snapshot fields")
    if any(not isinstance(entry, dict) or not isinstance(entry.get("id"), str) for entry in runtime_items):
        _fail("package.runtime_metadata", "runtime item metadata is malformed")
    runtime_by_id = {entry["id"]: entry for entry in runtime_items}
    if sorted(runtime_by_id) != item_ids or len(runtime_by_id) != len(runtime_items):
        _fail("package.runtime_metadata", "runtime_export does not match Snapshot items")

    item_indices = {item_id: index for index, item_id in enumerate(item_ids)}
    field_indices = {field_id: index for index, field_id in enumerate(field_ids)}
    field_by_member: dict[str, tuple[int, dict[str, Any]]] = {}
    strings = set(item_ids + field_ids)
    item_hashes: dict[int, str] = {}
    for index, field in enumerate(sorted_fields):
        member = field.get("member")
        unit = field.get("unit")
        if not isinstance(member, str) or not isinstance(unit, str) or member in field_by_member:
            _fail("package.field", "fields require unique member and unit strings")
        field_by_member[member] = (index, field)
        strings.update((member, unit))
    for item in sorted_items:
        item_id = item["id"]
        kind = item.get("kind")
        if not isinstance(kind, str) or not kind:
            _fail("package.item", f"item {item_id} requires kind")
        strings.add(kind)
        digest = xxh64(item_id.encode("utf-8"))
        if digest in item_hashes:
            _fail("package.item_hash_collision", f"{item_hashes[digest]} collides with {item_id}")
        item_hashes[digest] = item_id
    string_bytes, string_offsets = _string_table(strings)

    field_rows: list[tuple[int, ...]] = []
    for field in sorted_fields:
        if field.get("type") != "i64":
            _fail("package.field_type", "v1 package supports i64 fields")
        minimum = _i64(field.get("min"), "package.field_range")
        maximum = _i64(field.get("max"), "package.field_range")
        if minimum > maximum:
            _fail("package.field_range", "field min exceeds max")
        field_rows.append((
            xxh64(field["id"].encode("utf-8")),
            string_offsets[field["id"]], string_offsets[field["member"]],
            string_offsets[field["unit"]], 1,
            minimum, maximum, 1, 0,
        ))

    item_rows: list[tuple[int, ...]] = []
    level_rows: list[tuple[int, ...]] = []
    value_rows: list[tuple[int, ...]] = []
    cost_rows: list[tuple[int, ...]] = []
    for item_index, item in enumerate(sorted_items):
        item_id = item["id"]
        unsupported = set(item) - {"id", "kind", "stack", "authoring_mode", "levels", "acquire"}
        if unsupported:
            _fail("package.unsupported_item_field", f"item {item_id}: {sorted(unsupported)[0]}")
        metadata = runtime_by_id[item_id]
        storage = metadata.get("storage")
        storage_code = 1 if storage == "unique" else 0 if storage == "stack" else -1
        if storage_code < 0:
            _fail("package.runtime_metadata", f"item {item_id} has invalid storage")
        stack = _u32(item.get("stack"), "package.stack")

        levels = item.get("levels")
        raw_rows = [] if levels is None else levels.get("rows") if isinstance(levels, dict) else None
        if not isinstance(raw_rows, list):
            _fail("package.levels", f"item {item_id} requires normalized level rows")
        if len(level_rows) + len(raw_rows) > max_levels:
            _fail("package.level_budget", f"level count exceeds {max_levels}")

        level_start = len(level_rows)
        acquire_start = len(cost_rows)
        acquire = item.get("acquire")
        if acquire is None:
            acquire_values = []
        elif not isinstance(acquire, dict) or set(acquire) != {"cost"}:
            _fail("package.acquire", f"item {item_id} acquire must contain only cost")
        else:
            acquire_values, acquire_free = _cost_values(
                acquire["cost"], item_indices, "package.acquire", max_costs - len(cost_rows),
            )
            if acquire_free:
                acquire_values = []
        cost_rows.extend((target, 0, count) for target, count in acquire_values)

        for level_index, row in enumerate(raw_rows, start=1):
            if not isinstance(row, dict):
                _fail("package.level", f"item {item_id} level must be an object")
            value_start = len(value_rows)
            members = sorted(set(row) - {"cost_to_reach"}, key=lambda key: field_by_member.get(key, (-1, {}))[0])
            if len(value_rows) + len(members) > max_values:
                _fail("package.value_budget", f"value count exceeds {max_values}")
            for member in members:
                if member not in field_by_member:
                    _fail("package.level_field", f"item {item_id} has unknown field {member}")
                field_index, _ = field_by_member[member]
                value_rows.append((item_index, level_index, field_index, 0, _i64(row[member], "package.level_value")))
            cost_start = len(cost_rows)
            costs, free = _cost_values(
                row.get("cost_to_reach"), item_indices, "package.level_cost", max_costs - len(cost_rows),
            )
            cost_rows.extend((target, 0, count) for target, count in costs)
            level_rows.append((
                item_index, level_index, value_start, len(value_rows) - value_start,
                cost_start, len(cost_rows) - cost_start, LEVEL_FREE if free else 0, 0,
            ))
        if metadata.get("level_count") != len(raw_rows):
            _fail("package.runtime_metadata", f"item {item_id} level count mismatch")
        item_rows.append((
            xxh64(item_id.encode("utf-8")), string_offsets[item_id], string_offsets[item["kind"]],
            storage_code, stack, level_start, len(raw_rows), acquire_start, len(acquire_values), 0, 0,
        ))

    for count, limit, code in (
        (len(level_rows), max_levels, "package.level_budget"),
        (len(value_rows), max_values, "package.value_budget"),
        (len(cost_rows), max_costs, "package.cost_budget"),
    ):
        if type(limit) is not int or limit < 0 or count > limit:
            _fail(code, f"count {count} exceeds {limit}")

    counts = (len(string_bytes), len(item_rows), len(sorted_fields), len(level_rows), len(value_rows), len(cost_rows))
    section_sizes = [count * stride for count, stride in zip(counts, SECTION_STRIDES)]
    total_size, offsets = _package_size(section_sizes)
    if type(max_bytes) is not int or max_bytes < 0 or total_size > max_bytes:
        _fail("package.byte_budget", f"package size {total_size} exceeds {max_bytes}")

    sections = _materialize_sections(
        string_bytes, item_rows, field_rows, level_rows, value_rows, cost_rows,
    )
    body = bytearray()
    cursor = HEADER.size
    for offset, section in zip(offsets, sections):
        body.extend(b"\0" * (offset - cursor))
        body.extend(section)
        cursor = offset + len(section)

    section_words = [word for offset, count, stride in zip(offsets, counts, SECTION_STRIDES) for word in (offset, count, stride)]
    header = HEADER.pack(
        MAGIC, FORMAT_VERSION, HEADER.size, total_size, 0, xxh64(_schema_descriptor(sorted_fields)), 0,
        *section_words, _snapshot_digest(snapshot),
    )
    package = bytearray(header + body)
    fingerprint = xxh64(bytes(package))
    struct.pack_into("<Q", package, CONTENT_FINGERPRINT_OFFSET, fingerprint)
    return bytes(package)


def inspect_package(package: bytes) -> dict[str, Any]:
    """Strict Python reference parser for the wire package."""
    if not isinstance(package, bytes) or len(package) < HEADER.size:
        _fail("package.header", "package is truncated")
    unpacked = HEADER.unpack_from(package)
    magic, version, header_size, payload_size, flags, schema_abi, content_fingerprint = unpacked[:7]
    if magic != MAGIC or version != FORMAT_VERSION or header_size != HEADER.size or payload_size != len(package) or flags != 0:
        _fail("package.header", "package header is invalid")
    check = bytearray(package)
    struct.pack_into("<Q", check, CONTENT_FINGERPRINT_OFFSET, 0)
    if xxh64(bytes(check)) != content_fingerprint:
        _fail("package.content_fingerprint", "package bytes do not match fingerprint")

    section_values = unpacked[7:25]
    snapshot_digest = unpacked[25]
    sections = {}
    previous_end = HEADER.size
    for index, (name, expected_stride) in enumerate(zip(SECTION_NAMES, SECTION_STRIDES)):
        offset, count, stride = section_values[index * 3:index * 3 + 3]
        end = offset + count * stride
        expected_offset = _align8(previous_end)
        if (stride != expected_stride or offset != expected_offset or end > len(package)
                or any(package[previous_end:offset])):
            _fail("package.layout", f"section {name} is invalid")
        sections[name] = {"offset": offset, "count": count, "stride": stride}
        previous_end = end
    if previous_end != len(package):
        _fail("package.layout", "package has trailing bytes")

    string_section = sections["strings"]
    string_data = package[string_section["offset"]:string_section["offset"] + string_section["count"]]
    if not string_data or string_data[0] != 0:
        _fail("package.string", "string table requires empty offset zero")

    def text(offset: int) -> str:
        if type(offset) is not int or offset <= 0 or offset >= len(string_data):
            _fail("package.string", "string offset is out of bounds")
        end = string_data.find(b"\0", offset)
        if end < 0:
            _fail("package.string", "string is not terminated")
        try:
            return string_data[offset:end].decode("utf-8")
        except UnicodeDecodeError:
            _fail("package.string", "string is not UTF-8")

    fields = []
    field_hashes: set[int] = set()
    field_section = sections["fields"]
    for index in range(field_section["count"]):
        row = FIELD.unpack_from(package, field_section["offset"] + index * FIELD.size)
        field_id = text(row[1])
        text(row[2])
        text(row[3])
        if (xxh64(field_id.encode("utf-8")) != row[0] or row[0] in field_hashes
                or row[4] != 1 or row[5] > row[6] or row[7] != 1 or row[8] != 0):
            _fail("package.field", "field row is invalid")
        field_hashes.add(row[0])
        fields.append(field_id)
    if fields != sorted(set(fields)):
        _fail("package.field", "field rows must be sorted and unique")

    raw_items = []
    item_hashes: set[int] = set()
    item_section = sections["items"]
    for index in range(item_section["count"]):
        row = ITEM.unpack_from(package, item_section["offset"] + index * ITEM.size)
        item_id = text(row[1])
        text(row[2])
        if (xxh64(item_id.encode("utf-8")) != row[0] or row[0] in item_hashes
                or row[3] not in (0, 1) or (row[3] == 1) != (row[4] == 1)
                or row[9] != 0 or row[10] != 0):
            _fail("package.item", "item row is invalid")
        item_hashes.add(row[0])
        raw_items.append(row)
    item_names = [text(row[1]) for row in raw_items]
    if item_names != sorted(set(item_names)):
        _fail("package.item", "item rows must be sorted and unique")

    cost_section = sections["costs"]
    costs = []
    for index in range(cost_section["count"]):
        target, flags_value, count = COST.unpack_from(package, cost_section["offset"] + index * COST.size)
        if target >= len(raw_items) or flags_value != 0 or count <= 0:
            _fail("package.cost", "cost row is invalid")
        costs.append({"item": item_names[target], "count": count})

    value_section = sections["values"]
    values = []
    for index in range(value_section["count"]):
        item_index, level, field_index, flags_value, value = VALUE.unpack_from(
            package, value_section["offset"] + index * VALUE.size,
        )
        if item_index >= len(raw_items) or field_index >= len(fields) or level < 1 or flags_value != 0:
            _fail("package.value", "value row is invalid")
        values.append((item_index, level, field_index, value))

    level_section = sections["levels"]
    raw_levels = []
    for index in range(level_section["count"]):
        row = LEVEL.unpack_from(package, level_section["offset"] + index * LEVEL.size)
        item_index, level, value_start, value_count, cost_start, cost_count, level_flags, reserved = row
        if (item_index >= len(raw_items) or level < 1 or value_start + value_count > len(values)
                or cost_start + cost_count > len(costs) or level_flags & ~LEVEL_FREE
                or (level_flags & LEVEL_FREE and cost_count) or reserved != 0):
            _fail("package.level", "level row is invalid")
        raw_levels.append(row)

    inspected_items = []
    inspected_levels = []
    level_cursor = value_cursor = cost_cursor = 0
    for item_index, (row, item_id) in enumerate(zip(raw_items, item_names)):
        level_start, level_count, acquire_start, acquire_count = row[5:9]
        if (level_start != level_cursor or acquire_start != cost_cursor
                or level_start + level_count > len(raw_levels)
                or acquire_start + acquire_count > len(costs)
                or (row[3] == 0 and level_count != 0)):
            _fail("package.span", "item spans are not canonical")
        cost_cursor += acquire_count
        inspected_items.append({
            "id": item_id, "kind": text(row[2]), "storage": "unique" if row[3] else "stack",
            "stack": row[4], "level_count": level_count,
            "acquire_costs": costs[acquire_start:acquire_start + acquire_count],
        })
        for expected_level in range(1, level_count + 1):
            level_row = raw_levels[level_cursor]
            owner, level, value_start, value_count, cost_start, cost_count, level_flags, _ = level_row
            if owner != item_index or level != expected_level or value_start != value_cursor or cost_start != cost_cursor:
                _fail("package.span", "level spans are not canonical")
            selected_values = values[value_start:value_start + value_count]
            if (any(value_item != item_index or value_level != level for value_item, value_level, _, _ in selected_values)
                    or [entry[2] for entry in selected_values] != sorted(set(entry[2] for entry in selected_values))):
                _fail("package.value", "value span does not match its level")
            value_cursor += value_count
            cost_cursor += cost_count
            result: dict[str, Any] = {
                "item": item_id, "level": level,
                "values": {fields[field_index]: value for _, _, field_index, value in selected_values},
            }
            if cost_count:
                result["costs"] = costs[cost_start:cost_start + cost_count]
            if level_flags & LEVEL_FREE:
                result["free"] = True
            inspected_levels.append(result)
            level_cursor += 1
    if level_cursor != len(raw_levels) or value_cursor != len(values) or cost_cursor != len(costs):
        _fail("package.span", "package rows are not covered exactly once")

    return {
        "schema": INSPECT_SCHEMA,
        "schema_abi_fingerprint": f"{schema_abi:016x}",
        "content_fingerprint": f"{content_fingerprint:016x}",
        "snapshot_content_hash": f"sha256:{snapshot_digest.hex()}",
        "sections": {
            name: {"count": sections[name]["count"], "stride": sections[name]["stride"]}
            for name in SECTION_NAMES
        },
        "items": inspected_items,
        "levels": inspected_levels,
    }


def _parser() -> argparse.ArgumentParser:
    parser = PackageArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    build = commands.add_parser("build", help="Build blob and ABI-stable generated header.")
    build.add_argument("--snapshot", type=Path, required=True)
    build.add_argument("--out", type=Path, required=True)
    build.add_argument("--header-out", type=Path, required=True)
    build.add_argument("--max-items", type=int, default=MAX_ITEMS)
    build.add_argument("--max-fields", type=int, default=MAX_FIELDS)
    build.add_argument("--max-levels", type=int, default=MAX_LEVELS)
    build.add_argument("--max-values", type=int, default=MAX_VALUES)
    build.add_argument("--max-costs", type=int, default=MAX_COSTS)
    build.add_argument("--max-bytes", type=int, default=MAX_BYTES)
    return parser


def main(argv: list[str] | None = None) -> int:
    try:
        args = _parser().parse_args(argv)
        source_path = args.snapshot.resolve()
        blob_path = args.out.resolve()
        header_path = args.header_out.resolve()
        if len({source_path, blob_path, header_path}) != 3:
            _fail("cli.output_paths", "snapshot, blob, and header paths must be distinct")
        snapshot = json.loads(source_path.read_text(encoding="utf-8"))
        package = build_package(
            snapshot,
            max_items=args.max_items, max_fields=args.max_fields,
            max_levels=args.max_levels, max_values=args.max_values,
            max_costs=args.max_costs, max_bytes=args.max_bytes,
        )
        header = render_abi_header(snapshot).encode("utf-8")
        inspected = inspect_package(package)
        changed = {
            "blob": write_if_different(blob_path, package),
            "header": write_if_different(header_path, header),
        }
        print(json.dumps({
            "schema": "items.runtime.package.build.v1",
            "snapshot_content_hash": inspected["snapshot_content_hash"],
            "schema_abi_fingerprint": inspected["schema_abi_fingerprint"],
            "content_fingerprint": inspected["content_fingerprint"],
            "bytes": len(package),
            "changed": changed,
        }, ensure_ascii=False, sort_keys=True))
        return 0
    except (PackageFailure, OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        print(json.dumps({
            "schema": "items.runtime.package.error.v1",
            "error": str(error),
        }, ensure_ascii=False, sort_keys=True), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
