from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any

from .naming import c_ident

INT64_MIN = -(2**63)
INT64_MAX = 2**63 - 1
FRAGMENT_RE = re.compile(r"[a-z_][a-z0-9_]*")
C_IDENT_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
SCALAR_TYPES = {"bool", "int", "i64", "float", "string", "string?", "enum"}
EVENT_NAME_RE = re.compile(r"[a-z][a-z0-9_]*")
EVENT_FIELD_TYPES = {"bool", "int", "i64", "float", "string", "hash", "bytes"}
EVENT_RESERVED_FIELDS = {"type", "seq", "tick"}
EVENT_SYMBOL_RESERVED_FIELDS = {"desc", "fields"}
EVENT_RESERVED_NAMES = {"descs", "desc_count", "register"}
EVENT_FIELD_C_TYPE = {"bool": "bool", "int": "int32_t", "i64": "int64_t", "float": "double", "string": "uint32_t", "hash": "nt_hash64_t", "bytes": "uint32_t"}
EVENT_FIELD_FT_ENUM = {"bool": "GAME_EVENT_FT_BOOL", "int": "GAME_EVENT_FT_INT", "i64": "GAME_EVENT_FT_I64", "float": "GAME_EVENT_FT_FLOAT", "string": "GAME_EVENT_FT_STRING", "hash": "GAME_EVENT_FT_HASH", "bytes": "GAME_EVENT_FT_BYTES"}
EVENT_FIELD_EMIT_ARG = {"bool": "bool", "int": "int32_t", "i64": "int64_t", "float": "double", "hash": "nt_hash64_t"}

def map_type_name(type_text: str) -> str | None:
    match = re.fullmatch(r"map<string,([A-Za-z_][A-Za-z0-9_]*)>", type_text)
    return match.group(1) if match else None

def is_list_type(type_text: str) -> bool:
    return type_text == "list<string>"

def _normalize_fields(raw_fields: Any, scope: str) -> list[dict[str, Any]]:
    """v2 fields/type-fields are a MAP path -> spec; expand to an ordered list
    with the key injected as ``path`` (file order = struct/golden order)."""
    if not isinstance(raw_fields, dict):
        raise SystemExit(f"{scope} must be an object (map of path -> spec)")
    result: list[dict[str, Any]] = []
    for path, spec in raw_fields.items():
        if not isinstance(spec, dict):
            raise SystemExit(f"{scope}.{path} must be an object")
        entry: dict[str, Any] = {"path": path}
        entry.update(spec)
        result.append(entry)
    return result


def validate_field_names(scope: str, fields: list[dict[str, Any]], reserved_names: set[str], forbid_v: bool) -> None:
    seen_paths: set[str] = set()
    seen_idents: dict[str, str] = {}
    for field in fields:
        if not isinstance(field, dict):
            raise SystemExit(f"{scope} entries must be objects")
        path = field.get("path")
        if not isinstance(path, str) or not path:
            raise SystemExit(f"{scope} field path must be a non-empty string")
        if forbid_v and path == "v":
            raise SystemExit('field path "v" is reserved for the save envelope version')
        if path in reserved_names:
            raise SystemExit(f"{path} is reserved and cannot be reused")
        if path in seen_paths:
            raise SystemExit(f"duplicate field path {path}")
        ident = c_ident(path)
        if ident in seen_idents:
            raise SystemExit(
                f"field paths {seen_idents[ident]} and {path} collide on C identifier {ident}"
            )
        seen_paths.add(path)
        seen_idents[ident] = path

def load_events(events_raw: Any) -> dict[str, dict[str, Any]]:
    """Parse+validate the v2 `events` section (the event schema contract).
    Returns an ordered dict evt_name -> {"fields": [{"name","type","doc"}, ...]}.
    Hard lint = charset + c_ident collisions + reserved (type/seq/tick + synthesized
    <bytes>_len) + type dictionary. Past-tense naming is skill advice, not a hard fail."""
    if not isinstance(events_raw, dict):
        raise SystemExit("events must be a map of name -> spec")
    events: dict[str, dict[str, Any]] = {}
    seen_event_idents: dict[str, str] = {}
    for evt_name, evt_spec in events_raw.items():
        if not isinstance(evt_name, str) or not EVENT_NAME_RE.fullmatch(evt_name):
            raise SystemExit(f"event name {evt_name!r} must match [a-z][a-z0-9_]*")
        if evt_name in EVENT_RESERVED_NAMES:
            raise SystemExit(
                f"event name {evt_name} is reserved for the per-fragment event table/count/register symbols"
            )
        evt_ident = c_ident(evt_name)
        if evt_ident in seen_event_idents:
            raise SystemExit(
                f"event names {seen_event_idents[evt_ident]} and {evt_name} collide on C identifier {evt_ident}"
            )
        seen_event_idents[evt_ident] = evt_name
        if not isinstance(evt_spec, dict):
            raise SystemExit(f"event {evt_name} spec must be an object")
        extra = set(evt_spec.keys()) - {"fields", "doc"}
        if extra:
            raise SystemExit(f"event {evt_name} has unsupported keys {sorted(extra)}")
        fields_raw = evt_spec.get("fields", {})
        if not isinstance(fields_raw, dict):
            raise SystemExit(f"event {evt_name}.fields must be a map of name -> spec")
        fields: list[dict[str, Any]] = []
        seen_field_idents: dict[str, str] = {}
        declared_names: set[str] = set()
        for fname, fspec in fields_raw.items():
            if not isinstance(fname, str) or not EVENT_NAME_RE.fullmatch(fname):
                raise SystemExit(f"event {evt_name} field name {fname!r} must match [a-z][a-z0-9_]*")
            if fname in EVENT_RESERVED_FIELDS:
                raise SystemExit(f"event {evt_name} field {fname} is reserved (envelope/accessor)")
            if fname in EVENT_SYMBOL_RESERVED_FIELDS:
                raise SystemExit(
                    f"event {evt_name} field {fname} is reserved: its accessor would redefine the "
                    f"generated per-event descriptor/fields symbols"
                )
            fident = c_ident(fname)
            if fident in seen_field_idents:
                raise SystemExit(
                    f"event {evt_name} fields {seen_field_idents[fident]} and {fname} collide on C identifier {fident}"
                )
            seen_field_idents[fident] = fname
            if not isinstance(fspec, dict):
                raise SystemExit(f"event {evt_name}.{fname} spec must be an object")
            fextra = set(fspec.keys()) - {"type", "doc"}
            if fextra:
                raise SystemExit(f"event {evt_name}.{fname} has unsupported keys {sorted(fextra)}")
            ftype = fspec.get("type")
            if ftype not in EVENT_FIELD_TYPES:
                raise SystemExit(f"unknown event field type {ftype!r} for {evt_name}.{fname}")
            declared_names.add(fname)
            fields.append({"name": fname, "type": ftype, "doc": fspec.get("doc")})
        # L3: a bytes field <name> synthesizes a uint32 <name>_len member; it must not
        # collide (by name OR c_ident) with a declared field, else a duplicate C member
        # is a dirty compile error instead of a clean SystemExit.
        for field in fields:
            if field["type"] != "bytes":
                continue
            len_name = f"{field['name']}_len"
            if len_name in declared_names or c_ident(len_name) in seen_field_idents:
                raise SystemExit(
                    f"bytes field {field['name']} synthesizes {len_name} which collides with a declared field"
                )
        events[evt_name] = {"fields": fields}
    return events


def load_schema(schema_path: Path) -> dict[str, Any]:
    with schema_path.open("r", encoding="utf-8") as f:
        raw = json.load(f)
    if not isinstance(raw, dict):
        raise SystemExit("schema root must be an object")

    # M3: reject v1 schemas up front with a readable message (not a KeyError deep
    # in a renderer). A v1 schema has no schema_version and/or carries "document".
    if "schema_version" not in raw or "document" in raw:
        raise SystemExit(
            "v1 schema unsupported by v2 generator; rebuild the game from its shipping tag"
        )
    if raw.get("schema_version") != 2:
        raise SystemExit("schema_version must be 2")

    schema_label = raw.get("schema")
    if not isinstance(schema_label, str) or not schema_label:
        raise SystemExit("schema id must be a non-empty string")

    fragment = raw.get("fragment")
    if not isinstance(fragment, str) or not FRAGMENT_RE.fullmatch(fragment):
        raise SystemExit("fragment must be a string matching [a-z_][a-z0-9_]*")

    version = raw.get("version")
    if not isinstance(version, int) or isinstance(version, bool) or version <= 0:
        raise SystemExit("version must be a positive integer")

    string_max = raw.get("string_max")
    if not isinstance(string_max, int) or isinstance(string_max, bool) or string_max < 2:
        raise SystemExit("string_max must be an integer >= 2")

    reserved_raw = raw.get("reserved", [])
    if not isinstance(reserved_raw, list):
        raise SystemExit("reserved must be a list of field names")
    for item in reserved_raw:
        if not isinstance(item, str) or not item:
            raise SystemExit("reserved entries must be non-empty strings")

    hooks = raw.get("hooks", {})
    if not isinstance(hooks, dict):
        raise SystemExit("hooks must be an object")
    for key, value in hooks.items():
        if key not in ("on_new_game", "reconcile") or not isinstance(value, bool):
            raise SystemExit("hooks may only set on_new_game/reconcile to booleans")

    migrations = raw.get("migrations", [])
    if not isinstance(migrations, list):
        raise SystemExit("migrations must be a list")
    seen_fn: set[str] = set()
    for index, entry in enumerate(migrations):
        expected_to = index + 2
        if not isinstance(entry, dict):
            raise SystemExit("migration entries must be objects")
        if entry.get("to_version") != expected_to:
            raise SystemExit(f"migration {index} to_version must be {expected_to} (monotone 2..version)")
        fn = entry.get("fn")
        if not isinstance(fn, str) or not C_IDENT_RE.fullmatch(fn):
            raise SystemExit("migration fn must be a valid C identifier")
        if fn in seen_fn:
            raise SystemExit(f"duplicate migration fn {fn}")
        seen_fn.add(fn)
    if version != len(migrations) + 1:
        raise SystemExit(
            f"version ({version}) must equal len(migrations)+1 ({len(migrations) + 1})"
        )

    enums = raw.get("enums", {})
    if not isinstance(enums, dict):
        raise SystemExit("enums must be an object")
    for name, values in enums.items():
        if not isinstance(name, str) or not isinstance(values, list) or not values:
            raise SystemExit("enum values must be non-empty arrays")
        if any(not isinstance(value, str) or not value for value in values):
            raise SystemExit(f"enum {name} contains a bad value")

    types_raw = raw.get("types", {})
    if not isinstance(types_raw, dict):
        raise SystemExit("types must be an object")
    types: dict[str, dict[str, Any]] = {}
    for type_name, type_def in types_raw.items():
        if not isinstance(type_name, str) or not type_name or not c_ident(type_name):
            raise SystemExit("type names must be non-empty strings")
        if not isinstance(type_def, dict) or type_def.get("kind") != "object":
            raise SystemExit(f"types.{type_name} must be an object type")
        type_fields = _normalize_fields(type_def.get("fields"), f"types.{type_name}.fields")
        types[type_name] = {"kind": "object", "fields": type_fields}

    fields = _normalize_fields(raw.get("fields"), "fields")

    # Events are a separate family: parsed and validated
    # here, but NOT consumed by the state renderers or the embedded normalized schema,
    # so the state/schema output stays byte-identical.
    events = load_events(raw.get("events", {}))

    schema: dict[str, Any] = {
        "schema": schema_label,
        "schema_version": 2,
        "fragment": fragment,
        "version": version,
        "string_max": string_max,
        "reserved": list(reserved_raw),
        "hooks": hooks,
        "migrations": migrations,
        "enums": enums,
        "types": types,
        "fields": fields,
        "events": events,
    }


    validate_supported_shape(schema)
    return schema


def validate_supported_shape(schema: dict[str, Any]) -> None:
    reserved_names = set(schema.get("reserved", []))
    enums = schema["enums"]
    types = schema["types"]

    for type_name, type_def in types.items():
        validate_field_names(f"types.{type_name}", type_def["fields"], set(), forbid_v=False)
        for field in type_def["fields"]:
            validate_field_shape(schema, field, enums, types, allow_collections=False)

    validate_field_names("fields", schema["fields"], reserved_names, forbid_v=True)
    for field in schema["fields"]:
        validate_field_shape(schema, field, enums, types, allow_collections=True)


def validate_field_shape(
    schema: dict[str, Any],
    field: dict[str, Any],
    enums: dict[str, Any],
    types: dict[str, Any],
    allow_collections: bool,
) -> None:
    path = field.get("path")
    typ = field.get("type")
    if not isinstance(path, str) or not path:
        raise SystemExit("field path must be a non-empty string")
    if typ in SCALAR_TYPES:
        if typ == "enum" and field.get("enum") not in enums:
            raise SystemExit(f"{path} references unknown enum")
        if typ in {"int", "float"}:
            for key in ("default", "min", "max"):
                if key not in field:
                    raise SystemExit(f"{path} must declare {key}")
        if typ == "i64":
            for key in ("default", "min", "max"):
                if key not in field:
                    raise SystemExit(f"{path} must declare {key}")
            for key in ("default", "min", "max"):
                if not isinstance(field[key], int) or isinstance(field[key], bool):
                    raise SystemExit(f"{path} i64 {key} must be an integer")
            if not (INT64_MIN <= field["min"] <= field["max"] <= INT64_MAX):
                raise SystemExit(f"{path} i64 min/max must fit int64 with min <= max")
            if not (field["min"] <= field["default"] <= field["max"]):
                raise SystemExit(f"{path} i64 default must be within [min, max]")
        if typ == "bool" and not isinstance(field.get("default"), bool):
            raise SystemExit(f"{path} must declare bool default")
        if typ == "enum" and field.get("default") not in enums[field["enum"]]:
            raise SystemExit(f"{path} has bad enum default")
        if typ == "string":
            validate_string_length(schema, field, path)
        if typ == "string?":
            validate_string_length(schema, field, path)
            if "default" in field and field["default"] is not None and not isinstance(field["default"], str):
                raise SystemExit(f"{path} optional string default must be null or string")
        return
    if not allow_collections:
        raise SystemExit(f"unsupported object field type for {path}: {typ}")
    if is_list_type(typ):
        validate_max_count(field, path)
        default = field.get("default")
        if default is not None:
            if not isinstance(default, list):
                raise SystemExit(f"{path} list default must be an array")
            if len(default) > field["max_count"]:
                raise SystemExit(f"{path} list default exceeds max_count")
            for entry in default:
                if not isinstance(entry, str) or not entry:
                    raise SystemExit(f"{path} list default entries must be non-empty strings")
                if len(entry) >= schema["string_max"]:
                    raise SystemExit(f"{path} list default entry exceeds string_max")
        return
    map_name = map_type_name(typ)
    if map_name:
        if map_name not in types:
            raise SystemExit(f"{path} references unknown type {map_name}")
        validate_max_count(field, path)
        return
    raise SystemExit(f"unsupported field type for {path}: {typ}")


def validate_string_length(schema: dict[str, Any], field: dict[str, Any], path: str) -> None:
    if not isinstance(field.get("max_length"), int) or field["max_length"] <= 0 or field["max_length"] >= schema["string_max"]:
        raise SystemExit(f"{path} must declare max_length from 1 to string_max-1")


def validate_max_count(field: dict[str, Any], path: str) -> None:
    if not isinstance(field.get("max_count"), int) or field["max_count"] <= 0:
        raise SystemExit(f"{path} must declare positive max_count")
