#!/usr/bin/env python3
"""Generate the supported per-fragment game state C API from a v2 state schema."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[3]
TOOL_LABEL = "features/game-state/scripts/generate_state.py"

INT64_MIN = -(2**63)
INT64_MAX = 2**63 - 1

FRAGMENT_RE = re.compile(r"[a-z_][a-z0-9_]*")
C_IDENT_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


def default_schema_path() -> Path:
    game_schema = ROOT / "state" / "game_state.schema.json"
    if game_schema.exists():
        return game_schema
    return ROOT / "templates" / "template" / "state" / "game_state.schema.json"


def default_out_dir(schema_path: Path | None = None) -> Path:
    schema = (schema_path or default_schema_path()).resolve()
    try:
        parts = schema.relative_to(ROOT).parts
    except ValueError:
        return schema.parent.parent / "build" / "generated" / "game-state"
    if len(parts) >= 4 and parts[0] in {"templates", "games"} and parts[2] == "state":
        return ROOT / parts[0] / parts[1] / "build" / "generated" / "game-state"
    if len(parts) >= 2 and parts[0] == "state":
        return ROOT / "build" / "generated" / "game-state"
    return schema.parent.parent / "build" / "generated" / "game-state"


SCHEMA_PATH = default_schema_path()
OUT_DIR = default_out_dir(SCHEMA_PATH)

# i64 rides the JSON wire as a decimal string (double loses precision above 2^53).
SCALAR_TYPES = {"bool", "int", "i64", "float", "string", "string?", "enum"}

# Event field vocabulary (event_system_design §3/§5). Names are aligned with the
# state schema (float/string, not f32/str) so agents don't split the dictionaries,
# but event `float` is C `double` (f64) and event `int` is int32_t (E2 §2/§15).
EVENT_NAME_RE = re.compile(r"[a-z][a-z0-9_]*")
EVENT_FIELD_TYPES = {"bool", "int", "i64", "float", "string", "hash", "bytes"}
# Envelope/accessor names the generated struct/accessor would shadow.
EVENT_RESERVED_FIELDS = {"type", "seq", "tick"}
# Field names whose generated string/bytes accessor <ns>_ev_<evt>_<name> would REDEFINE
# a generated per-event symbol: `desc` collides with the descriptor
# `<ns>_ev_<evt>_desc`; `fields` collides with the descriptor array
# `<ns>_ev_<evt>_fields[]`. Without this guard that is a dirty duplicate-symbol
# compile error (a real T0327 footgun) instead of a clean SystemExit.
EVENT_SYMBOL_RESERVED_FIELDS = {"desc", "fields"}
# Event names reserved to keep the per-fragment table/count/register symbols
# (<ns>_ev_descs / <ns>_ev_desc_count / <ns>_ev_register) unambiguous.
EVENT_RESERVED_NAMES = {"descs", "desc_count", "register"}

EVENT_FIELD_C_TYPE = {
    "bool": "bool",
    "int": "int32_t",
    "i64": "int64_t",
    "float": "double",
    "string": "uint32_t",  # inline byte offset -> NUL string
    "hash": "nt_hash64_t",
    "bytes": "uint32_t",   # inline byte offset (paired with <name>_len)
}
EVENT_FIELD_FT_ENUM = {
    "bool": "GAME_EVENT_FT_BOOL",
    "int": "GAME_EVENT_FT_INT",
    "i64": "GAME_EVENT_FT_I64",
    "float": "GAME_EVENT_FT_FLOAT",
    "string": "GAME_EVENT_FT_STRING",
    "hash": "GAME_EVENT_FT_HASH",
    "bytes": "GAME_EVENT_FT_BYTES",
}
# By-value emit argument type for scalar/hash fields (string/bytes use pointers).
EVENT_FIELD_EMIT_ARG = {
    "bool": "bool",
    "int": "int32_t",
    "i64": "int64_t",
    "float": "double",
    "hash": "nt_hash64_t",
}


def c_ident(value: str) -> str:
    value = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", value)
    ident = re.sub(r"[^a-zA-Z0-9]+", "_", value).strip("_").lower()
    if not ident:
        raise SystemExit(f"cannot make C identifier from {value!r}")
    if ident[0].isdigit():
        ident = "_" + ident
    return ident


def c_macro(value: str) -> str:
    return c_ident(value).upper()


def _pascal(ident: str) -> str:
    return "".join(part[:1].upper() + part[1:] for part in ident.split("_") if part)


class Ns:
    """Per-fragment C namespace derived from schema.fragment (state doc §7)."""

    def __init__(self, fragment_id: str) -> None:
        pascal = _pascal(fragment_id)
        self.id = fragment_id
        self.ident = fragment_id
        self.upper = fragment_id.upper()
        self.pascal = pascal
        self.type = f"{pascal}State"          # e.g. GameState / MiniState
        self.fn = f"{fragment_id}_state_"     # e.g. game_state_ / mini_state_
        self.macro = f"{self.upper}_STATE_"   # e.g. GAME_STATE_ / MINI_STATE_
        self.inst = f"{fragment_id}_state"    # e.g. game_state / mini_state
        self.frag = f"{fragment_id}_state_fragment"


# Set once per generation (single-run, single-thread) from schema.fragment.
NS: Ns = Ns("game")


def relative_label(path: Path) -> str:
    try:
        return path.relative_to(ROOT).as_posix()
    except ValueError:
        return path.as_posix()


# ---------------------------------------------------------------------------
# Schema loading + validation (v2 only)
# ---------------------------------------------------------------------------


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


def field_c_type(field: dict[str, Any]) -> str:
    typ = field["type"]
    if typ == "bool":
        return "bool"
    if typ == "int":
        return "int"
    if typ == "i64":
        return "int64_t"
    if typ == "float":
        return "float"
    if typ == "enum":
        return "int"
    if typ == "string":
        return "char"
    if typ == "string?":
        return "optional_string"
    raise AssertionError(typ)


def default_enum_macro(enum_name: str, value: str) -> str:
    return f"{NS.macro}{c_macro(enum_name)}_{c_macro(value)}"


def c_float(value: Any) -> str:
    text = f"{float(value):.8g}"
    if "." not in text and "e" not in text and "E" not in text:
        text += ".0"
    return f"{text}F"


def c_int(value: Any) -> str:
    if not isinstance(value, int) or isinstance(value, bool):
        raise SystemExit(f"expected integer value, got {value!r}")
    return str(value)


def c_i64(value: Any) -> str:
    if not isinstance(value, int) or isinstance(value, bool):
        raise SystemExit(f"expected i64 integer value, got {value!r}")
    return f"{value}LL"


def c_string(value: Any) -> str:
    if not isinstance(value, str):
        raise SystemExit(f"expected string value, got {value!r}")
    return json.dumps(value)


def load_events(events_raw: Any) -> dict[str, dict[str, Any]]:
    """Parse+validate the v2 `events` section (event_system_design §5, E2 §2/§8).
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


def load_schema(schema_path: Path = SCHEMA_PATH) -> dict[str, Any]:
    with schema_path.open("r", encoding="utf-8") as f:
        raw = json.load(f)
    if not isinstance(raw, dict):
        raise SystemExit("schema root must be an object")

    # M3: reject v1 schemas up front with a readable message (not a KeyError deep
    # in a renderer). A v1 schema has no schema_version and/or carries "document".
    if "schema_version" not in raw or "document" in raw:
        raise SystemExit(
            "v1 schema unsupported by v2 generator; rebuild rb-dark from its shipping tag"
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

    # Events are a SEPARATE family (event_system_design §14 p.13): parsed+validated
    # here, but NOT consumed by the state renderers or the embedded normalized schema,
    # so the state/schema/devapi output stays byte-identical (E2 §0/§15 dev.7).
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

    global NS
    NS = Ns(fragment)

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


# ---------------------------------------------------------------------------
# Type / collection helpers
# ---------------------------------------------------------------------------


def map_type_name(type_text: str) -> str | None:
    m = re.fullmatch(r"map<string,([A-Za-z_][A-Za-z0-9_]*)>", type_text)
    return m.group(1) if m else None


def is_map_type(type_text: str) -> bool:
    return map_type_name(type_text) is not None


def is_list_type(type_text: str) -> bool:
    return type_text == "list<string>"


def collection_macro(path: str) -> str:
    return f"{NS.macro}MAX_{c_macro(path)}"


def object_type_c_name(type_name: str) -> str:
    return f"{NS.pascal}{type_name}"


def object_type_func_name(type_name: str) -> str:
    return c_ident(type_name)


def schema_types(schema: dict[str, Any]) -> dict[str, dict[str, Any]]:
    types = schema.get("types", {})
    return types if isinstance(types, dict) else {}


def map_fields(schema: dict[str, Any]) -> list[dict[str, Any]]:
    return [f for f in schema["fields"] if is_map_type(f["type"])]


def list_fields(schema: dict[str, Any]) -> list[dict[str, Any]]:
    return [f for f in schema["fields"] if is_list_type(f["type"])]


def scalar_fields(schema: dict[str, Any]) -> list[dict[str, Any]]:
    return [f for f in schema["fields"] if f["type"] in SCALAR_TYPES]


def scalar_type_fields(fields: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return [f for f in fields if f["type"] in SCALAR_TYPES]


def enum_alias(field: dict[str, Any]) -> str | None:
    path = field["path"]
    return path[: -len("_index")] if path.endswith("_index") else None


def enum_table(field: dict[str, Any]) -> tuple[str, str]:
    """Return (names_table, count_macro) for an enum field."""
    enum_name = field["enum"]
    return f"k_{c_ident(enum_name)}_names", f"{NS.macro}{c_macro(enum_name)}_COUNT"


# ---------------------------------------------------------------------------
# Enums
# ---------------------------------------------------------------------------


def render_enum(enum_name: str, values: list[str]) -> str:
    type_name = f"{NS.type}{enum_name}"
    lines = [f"typedef enum {type_name} {{"]
    for i, value in enumerate(values):
        suffix = f" = {i}" if i == 0 else ""
        lines.append(f"    {NS.macro}{c_macro(enum_name)}_{c_macro(value)}{suffix},")
    lines.append(f"    {NS.macro}{c_macro(enum_name)}_COUNT,")
    lines.append(f"}} {type_name};")
    return "\n".join(lines)


def render_enum_name_decls(schema: dict[str, Any]) -> str:
    return "\n".join(
        f"const char *{NS.fn}{c_ident(name)}_name(int value);" for name in schema["enums"]
    )


def render_enum_tables(schema: dict[str, Any]) -> str:
    blocks: list[str] = []
    for name, values in schema["enums"].items():
        ename = c_ident(name)
        lines = [f"static const char *const k_{ename}_names[{NS.macro}{c_macro(name)}_COUNT] = {{"]
        lines.extend(f'    "{value}",' for value in values)
        lines.append("};")
        blocks.append("\n".join(lines))
    for name in schema["enums"]:
        ename = c_ident(name)
        blocks.append(
            f"const char *{NS.fn}{ename}_name(int value) {{\n"
            f"    return (value >= 0 && value < {NS.macro}{c_macro(name)}_COUNT) ? k_{ename}_names[value] : \"unknown\";\n"
            f"}}"
        )
    return "\n\n".join(blocks)


# ---------------------------------------------------------------------------
# Constants + structs
# ---------------------------------------------------------------------------


def render_state_constants(schema: dict[str, Any]) -> str:
    lines: list[str] = []

    def emit_scalar(field: dict[str, Any], prefix: str = "") -> None:
        typ = field["type"]
        name = c_macro(f"{prefix}{field['path']}")
        if typ == "enum":
            lines.append(f"#define {NS.macro}{name}_DEFAULT {default_enum_macro(field['enum'], field['default'])}")
        elif typ == "int":
            lines.append(f"#define {NS.macro}{name}_DEFAULT {c_int(field['default'])}")
            lines.append(f"#define {NS.macro}{name}_MIN {c_int(field['min'])}")
            lines.append(f"#define {NS.macro}{name}_MAX {c_int(field['max'])}")
        elif typ == "i64":
            lines.append(f"#define {NS.macro}{name}_DEFAULT {c_i64(field['default'])}")
            lines.append(f"#define {NS.macro}{name}_MIN {c_i64(field['min'])}")
            lines.append(f"#define {NS.macro}{name}_MAX {c_i64(field['max'])}")
        elif typ == "float":
            lines.append(f"#define {NS.macro}{name}_DEFAULT {c_float(field['default'])}")
            lines.append(f"#define {NS.macro}{name}_MIN {c_float(field['min'])}")
            lines.append(f"#define {NS.macro}{name}_MAX {c_float(field['max'])}")
        elif typ == "string" and "default" in field:
            lines.append(f"#define {NS.macro}{name}_DEFAULT {c_string(field['default'])}")
        elif typ == "string?" and isinstance(field.get("default"), str):
            lines.append(f"#define {NS.macro}{name}_DEFAULT {c_string(field['default'])}")
        elif typ == "bool":
            lines.append(f"#define {NS.macro}{name}_DEFAULT {1 if field['default'] else 0}")

    for field in scalar_fields(schema):
        emit_scalar(field)
    for type_name, type_def in schema_types(schema).items():
        for field in scalar_type_fields(type_def["fields"]):
            emit_scalar(field, f"{type_name}.")
    for field in map_fields(schema) + list_fields(schema):
        lines.append(f"#define {collection_macro(field['path'])} {c_int(field['max_count'])}")
    return "\n".join(lines)


def render_struct_scalar_field(field: dict[str, Any], indent: str = "    ") -> list[str]:
    name = c_ident(field["path"])
    typ = field["type"]
    if typ == "string":
        return [f"{indent}char {name}[{NS.macro}STRING_MAX];"]
    if typ == "string?":
        return [f"{indent}bool has_{name};", f"{indent}char {name}[{NS.macro}STRING_MAX];"]
    return [f"{indent}{field_c_type(field)} {name};"]


def render_object_structs(schema: dict[str, Any]) -> str:
    blocks: list[str] = []
    for type_name, type_def in schema_types(schema).items():
        lines = [f"typedef struct {object_type_c_name(type_name)} {{"]
        lines.append("    bool used;")
        lines.append(f"    char key[{NS.macro}STRING_MAX];")
        for field in type_def["fields"]:
            lines.extend(render_struct_scalar_field(field))
        lines.append(f"}} {object_type_c_name(type_name)};")
        blocks.append("\n".join(lines))
    return "\n\n".join(blocks)


def render_state_struct(schema: dict[str, Any]) -> str:
    lines = [f"typedef struct {NS.type} {{"]
    for field in schema["fields"]:
        typ = field["type"]
        name = c_ident(field["path"])
        if typ in SCALAR_TYPES:
            lines.extend(render_struct_scalar_field(field))
        elif is_list_type(typ):
            lines.append(f"    char {name}[{collection_macro(field['path'])}][{NS.macro}STRING_MAX];")
            lines.append(f"    int {name}_count;")
        elif (type_name := map_type_name(typ)):
            lines.append(f"    {object_type_c_name(type_name)} {name}[{collection_macro(field['path'])}];")
    lines.append(f"}} {NS.type};")
    return "\n".join(lines)


def render_header(schema: dict[str, Any], schema_label: str) -> str:
    enums = schema["enums"]
    enum_blocks = "\n\n".join(render_enum(name, values) for name, values in enums.items())
    guard = f"{NS.macro}GENERATED_H"
    devapi_block = (
        f"#if NT_DEVAPI_ENABLED\n"
        f"void {NS.fn}register_devapi(void);\n"
        f"#endif\n"
    )
    return f"""#ifndef {guard}
#define {guard}

/* Generated by {TOOL_LABEL} from {schema_label}. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cJSON.h"
#include "game_save.h"

#define {NS.macro}SCHEMA_ID "{schema["schema"]}"
#define {NS.macro}FRAGMENT_ID "{schema["fragment"]}"
#define {NS.macro}VERSION {schema["version"]}
#define {NS.macro}STRING_MAX {schema["string_max"]}

{render_state_constants(schema)}

{enum_blocks}

{render_object_structs(schema)}

{render_state_struct(schema)}

/* Instance owned by this fragment TU (the shared global-state monolith is gone).
   Feature LOGIC works with it directly or through its own API. */
extern {NS.type} {NS.inst};

{render_enum_name_decls(schema)}

void   {NS.fn}init_defaults({NS.type} *state);
bool   {NS.fn}validate(const {NS.type} *state, char *error, int error_cap);
cJSON *{NS.fn}schema_json(void);
cJSON *{NS.fn}to_json(const {NS.type} *state);
cJSON *{NS.fn}get_path_json(const {NS.type} *state, const char *path, char *error, int error_cap);
bool   {NS.fn}set_path_json({NS.type} *state, const char *path, const cJSON *value, char *error, int error_cap);
bool   {NS.fn}patch_json({NS.type} *state, const cJSON *values, char *error, int error_cap);
bool   {NS.fn}from_json({NS.type} *state, const cJSON *json, char *error, int error_cap);

/* Generated descriptor — replaces the hand-written fragment adapter. */
extern const GameSaveFragment {NS.frag};

{devapi_block}
#endif
"""


# ---------------------------------------------------------------------------
# Scalar codegen
# ---------------------------------------------------------------------------


def render_scalar_default_assignment(field: dict[str, Any], target: str, prefix: str = "") -> list[str]:
    ident = c_ident(field["path"])
    macro = f"{NS.macro}{c_macro(prefix + field['path'])}"
    typ = field["type"]
    if typ == "string":
        if "default" in field:
            return [f"    (void)gsj_copy_text({target}->{ident}, sizeof({target}->{ident}), {macro}_DEFAULT);"]
        return []
    if typ == "string?":
        if isinstance(field.get("default"), str):
            return [
                f"    {target}->has_{ident} = true;",
                f"    (void)gsj_copy_text({target}->{ident}, sizeof({target}->{ident}), {macro}_DEFAULT);",
            ]
        return [f"    {target}->has_{ident} = false;"]
    return [f"    {target}->{ident} = {macro}_DEFAULT;"]


def render_scalar_validation(field: dict[str, Any], target: str, path: str, prefix: str = "") -> list[str]:
    ident = c_ident(field["path"])
    macro = f"{NS.macro}{c_macro(prefix + field['path'])}"
    typ = field["type"]
    if typ == "enum":
        _, count_macro = enum_table(field)
        condition = f"{target}->{ident} < 0 || {target}->{ident} >= {count_macro}"
    elif typ in {"int", "float", "i64"}:
        condition = f"{target}->{ident} < {macro}_MIN || {target}->{ident} > {macro}_MAX"
    elif typ == "string":
        condition = f"{target}->{ident}[0] == '\\0'"
    else:
        return []
    return [
        f"    if ({condition}) {{",
        f'        gsj_set_error(error, error_cap, "{path} out of range");',
        "        return false;",
        "    }",
    ]


def render_cjson_add_scalar(field: dict[str, Any], target: str, state_expr: str, key: str) -> list[str]:
    ident = c_ident(field["path"])
    typ = field["type"]
    if typ == "enum":
        alias = enum_alias(field)
        ename = c_ident(field["enum"])
        if alias:
            return [
                f'    cJSON_AddNumberToObject({target}, "{key}", {state_expr}->{ident});',
                f'    cJSON_AddStringToObject({target}, "{alias}", {NS.fn}{ename}_name({state_expr}->{ident}));',
            ]
        return [f'    cJSON_AddStringToObject({target}, "{key}", {NS.fn}{ename}_name({state_expr}->{ident}));']
    if typ == "int":
        return [f'    cJSON_AddNumberToObject({target}, "{key}", {state_expr}->{ident});']
    if typ == "i64":
        return [f'    gsj_add_i64({target}, "{key}", {state_expr}->{ident});']
    if typ == "float":
        return [f'    cJSON_AddNumberToObject({target}, "{key}", (double){state_expr}->{ident});']
    if typ == "bool":
        return [f'    cJSON_AddBoolToObject({target}, "{key}", {state_expr}->{ident});']
    if typ == "string":
        return [f'    cJSON_AddStringToObject({target}, "{key}", {state_expr}->{ident});']
    if typ == "string?":
        return [
            f"    if ({state_expr}->has_{ident}) {{",
            f'        cJSON_AddStringToObject({target}, "{key}", {state_expr}->{ident});',
            "    } else {",
            f'        cJSON_AddNullToObject({target}, "{key}");',
            "    }",
        ]
    raise AssertionError(typ)


def render_get_scalar_expr(field: dict[str, Any], state_expr: str) -> str:
    ident = c_ident(field["path"])
    typ = field["type"]
    if typ == "enum":
        ename = c_ident(field["enum"])
        return f"cJSON_CreateString({NS.fn}{ename}_name({state_expr}->{ident}))"
    if typ == "int":
        return f"cJSON_CreateNumber({state_expr}->{ident})"
    if typ == "i64":
        # Compound literal lives until the end of the full return expression;
        # gsj_i64_to_string fills it and cJSON_CreateString copies immediately.
        return f"cJSON_CreateString(gsj_i64_to_string({state_expr}->{ident}, (char[21]){{0}}, 21))"
    if typ == "float":
        return f"cJSON_CreateNumber((double){state_expr}->{ident})"
    if typ == "bool":
        return f"cJSON_CreateBool({state_expr}->{ident})"
    if typ == "string":
        return f"cJSON_CreateString({state_expr}->{ident})"
    if typ == "string?":
        return f"{state_expr}->has_{ident} ? cJSON_CreateString({state_expr}->{ident}) : cJSON_CreateNull()"
    raise AssertionError(typ)


def render_get_scalar_if(field: dict[str, Any], state_expr: str, path: str) -> list[str]:
    lines = [
        f'    if (strcmp(field, "{path}") == 0) {{',
        f"        return {render_get_scalar_expr(field, state_expr)};",
        "    }",
    ]
    alias = enum_alias(field) if field["type"] == "enum" else None
    if alias:
        ename = c_ident(field["enum"])
        lines.extend([
            f'    if (strcmp(field, "{alias}") == 0) {{',
            f"        return cJSON_CreateString({NS.fn}{ename}_name({state_expr}->{c_ident(field['path'])}));",
            "    }",
        ])
    return lines


def render_set_scalar_if(field: dict[str, Any], state_expr: str, path: str, prefix: str = "", compare_var: str = "field") -> list[str]:
    ident = c_ident(field["path"])
    macro = f"{NS.macro}{c_macro(prefix + field['path'])}"
    typ = field["type"]
    head = [f'    if (strcmp({compare_var}, "{path}") == 0) {{']
    if typ == "enum":
        names_table, count_macro = enum_table(field)
        body = [
            f"        if (!gsj_parse_enum_value(value, {names_table}, {count_macro}, &{state_expr}->{ident}, error, error_cap)) {{ return false; }}",
            "        return true;",
        ]
    elif typ == "int":
        body = [
            "        int parsed = 0;",
            f"        if (!gsj_parse_int_value(value, {macro}_MIN, {macro}_MAX, &parsed, error, error_cap)) {{ return false; }}",
            f"        {state_expr}->{ident} = parsed;",
            "        return true;",
        ]
    elif typ == "i64":
        body = [
            f"        if (!gsj_parse_i64_value(value, {macro}_MIN, {macro}_MAX, &{state_expr}->{ident}, error, error_cap)) {{ return false; }}",
            "        return true;",
        ]
    elif typ == "float":
        body = [
            "        if (!cJSON_IsNumber(value)) { gsj_set_error(error, error_cap, \"expected number\"); return false; }",
            "        float parsed = (float)value->valuedouble;",
            f"        if (parsed < {macro}_MIN || parsed > {macro}_MAX) {{ gsj_set_error(error, error_cap, \"number out of range\"); return false; }}",
            f"        {state_expr}->{ident} = parsed;",
            "        return true;",
        ]
    elif typ == "bool":
        body = [
            "        if (!cJSON_IsBool(value)) { gsj_set_error(error, error_cap, \"expected bool\"); return false; }",
            f"        {state_expr}->{ident} = cJSON_IsTrue(value);",
            "        return true;",
        ]
    elif typ == "string":
        body = [
            f"        if (!cJSON_IsString(value) || !gsj_copy_text({state_expr}->{ident}, sizeof({state_expr}->{ident}), value->valuestring)) {{ gsj_set_error(error, error_cap, \"expected short string\"); return false; }}",
            "        return true;",
        ]
    elif typ == "string?":
        body = [
            "        if (cJSON_IsNull(value)) {",
            f"            {state_expr}->has_{ident} = false;",
            f"            {state_expr}->{ident}[0] = '\\0';",
            "            return true;",
            "        }",
            f"        if (!cJSON_IsString(value) || !gsj_copy_text({state_expr}->{ident}, sizeof({state_expr}->{ident}), value->valuestring)) {{ gsj_set_error(error, error_cap, \"expected short string or null\"); return false; }}",
            f"        {state_expr}->has_{ident} = true;",
            "        return true;",
        ]
    else:
        raise AssertionError(typ)
    return head + body + ["    }"]


def render_read_scalar(field: dict[str, Any], source: str, target: str, key: str, prefix: str = "") -> list[str]:
    ident = c_ident(field["path"])
    macro = f"{NS.macro}{c_macro(prefix + field['path'])}"
    typ = field["type"]
    if typ == "enum":
        names_table, count_macro = enum_table(field)
        call = f'gsj_read_enum({source}, "{key}", {names_table}, {count_macro}, &{target}->{ident}, error, error_cap)'
    elif typ == "int":
        call = f'gsj_read_int_range({source}, "{key}", {macro}_MIN, {macro}_MAX, &{target}->{ident}, error, error_cap)'
    elif typ == "i64":
        call = f'gsj_read_i64({source}, "{key}", {macro}_MIN, {macro}_MAX, &{target}->{ident}, error, error_cap)'
    elif typ == "float":
        call = f'gsj_read_float_range({source}, "{key}", {macro}_MIN, {macro}_MAX, &{target}->{ident}, error, error_cap)'
    elif typ == "bool":
        call = f'gsj_read_bool({source}, "{key}", &{target}->{ident}, error, error_cap)'
    elif typ == "string":
        call = f'gsj_read_string({source}, "{key}", {target}->{ident}, sizeof({target}->{ident}), error, error_cap)'
    elif typ == "string?":
        var = c_ident(source + "_" + key)
        return [
            f'    const cJSON *{var} = gsj_object_item({source}, "{key}");',
            f"    if ({var}) {{",
            f"        if (cJSON_IsNull({var})) {{",
            f"            {target}->has_{ident} = false;",
            f"        }} else if (cJSON_IsString({var}) && gsj_copy_text({target}->{ident}, sizeof({target}->{ident}), {var}->valuestring)) {{",
            f"            {target}->has_{ident} = true;",
            "        } else {",
            "            gsj_set_error(error, error_cap, \"expected short string or null\");",
            "            return false;",
            "        }",
            "    }",
        ]
    else:
        raise AssertionError(typ)
    return [f"    if (!{call}) {{ return false; }}"]


# ---------------------------------------------------------------------------
# Object + collection helpers
# ---------------------------------------------------------------------------


def render_object_helpers(schema: dict[str, Any]) -> str:
    blocks: list[str] = []
    for type_name, type_def in schema_types(schema).items():
        fname = object_type_func_name(type_name)
        cname = object_type_c_name(type_name)
        default_lines = [
            f"static void {fname}_init_defaults({cname} *obj, const char *key) {{",
            "    memset(obj, 0, sizeof(*obj));",
            "    obj->used = true;",
            "    (void)gsj_copy_text(obj->key, sizeof(obj->key), key);",
        ]
        for field in type_def["fields"]:
            default_lines.extend(render_scalar_default_assignment(field, "obj", f"{type_name}."))
        default_lines.append("}")

        validate_lines = [f"static bool {fname}_validate(const {cname} *obj, char *error, int error_cap) {{"]
        validate_lines.append("    if (!obj->used) { return true; }")
        validate_lines.append("    if (obj->key[0] == '\\0') { gsj_set_error(error, error_cap, \"object key is empty\"); return false; }")
        for field in type_def["fields"]:
            validate_lines.extend(render_scalar_validation(field, "obj", field["path"], f"{type_name}."))
        validate_lines.append("    return true;")
        validate_lines.append("}")

        to_json_lines = [f"static cJSON *{fname}_to_json(const {cname} *obj) {{"]
        to_json_lines.append("    cJSON *json = cJSON_CreateObject();")
        for field in type_def["fields"]:
            to_json_lines.extend(render_cjson_add_scalar(field, "json", "obj", field["path"]))
        to_json_lines.append("    return json;")
        to_json_lines.append("}")

        get_lines = [f"static cJSON *{fname}_get_field_json(const {cname} *obj, const char *field, char *error, int error_cap) {{"]
        get_lines.append("    if (!field || field[0] == '\\0') { return " + f"{fname}_to_json(obj); }}")
        for field in type_def["fields"]:
            get_lines.extend(render_get_scalar_if(field, "obj", field["path"]))
        get_lines.append("    gsj_set_error(error, error_cap, \"unknown object field\");")
        get_lines.append("    return NULL;")
        get_lines.append("}")

        set_lines = [f"static bool {fname}_set_field_json({cname} *obj, const char *field, const cJSON *value, char *error, int error_cap) {{"]
        for field in type_def["fields"]:
            set_lines.extend(render_set_scalar_if(field, "obj", field["path"], f"{type_name}."))
        set_lines.append("    gsj_set_error(error, error_cap, \"unknown object field\");")
        set_lines.append("    return false;")
        set_lines.append("}")

        from_lines = [f"static bool {fname}_from_json({cname} *obj, const cJSON *json, char *error, int error_cap) {{"]
        from_lines.append("    if (!cJSON_IsObject(json)) { gsj_set_error(error, error_cap, \"object must be json object\"); return false; }")
        for field in type_def["fields"]:
            from_lines.extend(render_read_scalar(field, "json", "obj", field["path"], f"{type_name}."))
        from_lines.append(f"    return {fname}_validate(obj, error, error_cap);")
        from_lines.append("}")

        blocks.append("\n".join(default_lines + [""] + validate_lines + [""] + to_json_lines + [""] + get_lines + [""] + set_lines + [""] + from_lines))
    return "\n\n".join(blocks)


def render_collection_helpers(schema: dict[str, Any]) -> str:
    blocks: list[str] = []
    for field in map_fields(schema):
        ident = c_ident(field["path"])
        type_name = map_type_name(field["type"])
        assert type_name is not None
        fname = object_type_func_name(type_name)
        cname = object_type_c_name(type_name)
        max_macro = collection_macro(field["path"])
        blocks.append(f"""
static {cname} *find_{ident}({NS.type} *state, const char *key) {{
    for (int i = 0; i < {max_macro}; i++) {{
        if (state->{ident}[i].used && strcmp(state->{ident}[i].key, key) == 0) {{
            return &state->{ident}[i];
        }}
    }}
    return NULL;
}}

static const {cname} *find_{ident}_const(const {NS.type} *state, const char *key) {{
    for (int i = 0; i < {max_macro}; i++) {{
        if (state->{ident}[i].used && strcmp(state->{ident}[i].key, key) == 0) {{
            return &state->{ident}[i];
        }}
    }}
    return NULL;
}}

static {cname} *alloc_{ident}({NS.type} *state, const char *key) {{
    {cname} *existing = find_{ident}(state, key);
    if (existing) {{ return existing; }}
    for (int i = 0; i < {max_macro}; i++) {{
        if (!state->{ident}[i].used) {{
            {fname}_init_defaults(&state->{ident}[i], key);
            return state->{ident}[i].used ? &state->{ident}[i] : NULL;
        }}
    }}
    return NULL;
}}

static cJSON *{ident}_to_json(const {NS.type} *state) {{
    cJSON *json = cJSON_CreateObject();
    for (int i = 0; i < {max_macro}; i++) {{
        if (state->{ident}[i].used) {{
            cJSON_AddItemToObject(json, state->{ident}[i].key, {fname}_to_json(&state->{ident}[i]));
        }}
    }}
    return json;
}}

static bool set_{ident}_from_json({NS.type} *state, const cJSON *json, char *error, int error_cap) {{
    if (!cJSON_IsObject(json)) {{ gsj_set_error(error, error_cap, \"map must be object\"); return false; }}
    for (int i = 0; i < {max_macro}; i++) {{ memset(&state->{ident}[i], 0, sizeof(state->{ident}[i])); }}
    const cJSON *child = NULL;
    cJSON_ArrayForEach(child, json) {{
        if (!child->string) {{ continue; }}
        {cname} *obj = alloc_{ident}(state, child->string);
        if (!obj) {{ gsj_set_error(error, error_cap, \"too many map entries or long key\"); return false; }}
        if (!{fname}_from_json(obj, child, error, error_cap)) {{ return false; }}
    }}
    return true;
}}
""")
    for field in list_fields(schema):
        ident = c_ident(field["path"])
        max_macro = collection_macro(field["path"])
        blocks.append(f"""
static cJSON *{ident}_to_json(const {NS.type} *state) {{
    cJSON *json = cJSON_CreateArray();
    for (int i = 0; i < state->{ident}_count; i++) {{
        cJSON_AddItemToArray(json, cJSON_CreateString(state->{ident}[i]));
    }}
    return json;
}}

static bool set_{ident}_from_json({NS.type} *state, const cJSON *json, char *error, int error_cap) {{
    if (!cJSON_IsArray(json)) {{ gsj_set_error(error, error_cap, \"list must be array\"); return false; }}
    int count = cJSON_GetArraySize((cJSON *)json);
    if (count > {max_macro}) {{ gsj_set_error(error, error_cap, \"too many list entries\"); return false; }}
    state->{ident}_count = 0;
    for (int i = 0; i < count; i++) {{
        const cJSON *entry = cJSON_GetArrayItem((cJSON *)json, i);
        if (!cJSON_IsString(entry) || !gsj_copy_text(state->{ident}[i], sizeof(state->{ident}[i]), entry->valuestring)) {{
            gsj_set_error(error, error_cap, \"list entry must be short string\");
            return false;
        }}
        state->{ident}_count++;
    }}
    return true;
}}
""")
    return "\n\n".join(blocks)


# ---------------------------------------------------------------------------
# Top-level (de)serialization
# ---------------------------------------------------------------------------


def parent_var_for(path: str) -> tuple[str, str, str]:
    if "." not in path:
        return "root", path, ""
    group, key = path.split(".", 1)
    return c_ident(group), key, group


def render_to_json(schema: dict[str, Any]) -> str:
    lines: list[str] = []
    emitted_groups: set[str] = set()
    for field in schema["fields"]:
        parent, key, group = parent_var_for(field["path"])
        if group and group not in emitted_groups:
            emitted_groups.add(group)
            lines.append(f'    cJSON *{parent} = cJSON_AddObjectToObject(root, "{group}");')
        if field["type"] in SCALAR_TYPES:
            lines.extend(render_cjson_add_scalar(field, parent, "state", key))
        elif is_map_type(field["type"]) or is_list_type(field["type"]):
            lines.append(f'    cJSON_AddItemToObject({parent}, "{key}", {c_ident(field["path"])}_to_json(state));')
    return "\n".join(lines)


def render_defaults(schema: dict[str, Any]) -> str:
    lines: list[str] = []
    for field in scalar_fields(schema):
        lines.extend(render_scalar_default_assignment(field, "state"))
    for field in list_fields(schema):
        default = field.get("default")
        if not default:
            continue
        ident = c_ident(field["path"])
        for index, value in enumerate(default):
            lines.append(f"    (void)gsj_copy_text(state->{ident}[{index}], sizeof(state->{ident}[{index}]), {c_string(value)});")
        lines.append(f"    state->{ident}_count = {len(default)};")
    return "\n".join(lines)


def render_validate(schema: dict[str, Any]) -> str:
    lines: list[str] = []
    for field in scalar_fields(schema):
        lines.extend(render_scalar_validation(field, "state", field["path"]))
    for field in list_fields(schema):
        ident = c_ident(field["path"])
        max_macro = collection_macro(field["path"])
        lines.extend([
            f"    if (state->{ident}_count < 0 || state->{ident}_count > {max_macro}) {{",
            f'        gsj_set_error(error, error_cap, "{field["path"]} count out of range");',
            "        return false;",
            "    }",
            f"    for (int i = 0; i < state->{ident}_count; i++) {{",
            f"        if (state->{ident}[i][0] == '\\0') {{",
            f'            gsj_set_error(error, error_cap, "{field["path"]} contains empty id");',
            "            return false;",
            "        }",
            "    }",
        ])
    for field in map_fields(schema):
        ident = c_ident(field["path"])
        type_name = map_type_name(field["type"])
        assert type_name is not None
        fname = object_type_func_name(type_name)
        max_macro = collection_macro(field["path"])
        lines.extend([
            f"    for (int i = 0; i < {max_macro}; i++) {{",
            f"        if (state->{ident}[i].used && !{fname}_validate(&state->{ident}[i], error, error_cap)) {{",
            "            return false;",
            "        }",
            "    }",
        ])
    return "\n".join(lines)


def render_get_path(schema: dict[str, Any]) -> str:
    lines: list[str] = []
    for field in scalar_fields(schema):
        path = field["path"]
        lines.append(f'    if (strcmp(path, "{path}") == 0) {{')
        lines.append(f"        return {render_get_scalar_expr(field, 'state')};")
        lines.append("    }")
    for field in list_fields(schema):
        path = field["path"]
        ident = c_ident(path)
        lines.extend([
            f'    if (strcmp(path, "{path}") == 0) {{',
            f"        return {ident}_to_json(state);",
            "    }",
        ])
    for field in map_fields(schema):
        path = field["path"]
        ident = c_ident(path)
        type_name = map_type_name(field["type"])
        assert type_name is not None
        fname = object_type_func_name(type_name)
        prefix_len = len(path) + 1
        lines.extend([
            f'    if (strcmp(path, "{path}") == 0) {{',
            f"        return {ident}_to_json(state);",
            "    }",
            f'    if (strncmp(path, "{path}.", {prefix_len}) == 0) {{',
            f"        const char *key = path + {prefix_len};",
            "        const char *field = strchr(key, '.');",
            f"        char map_key[{NS.macro}STRING_MAX];",
            "        if (field) {",
            "            size_t len = (size_t)(field - key);",
            "            if (len == 0 || len >= sizeof(map_key)) { gsj_set_error(error, error_cap, \"bad map key\"); return NULL; }",
            "            memcpy(map_key, key, len);",
            "            map_key[len] = '\\0';",
            "            field++;",
            "        } else if (!gsj_copy_text(map_key, sizeof(map_key), key)) {",
            "            gsj_set_error(error, error_cap, \"bad map key\");",
            "            return NULL;",
            "        }",
            f"        const {object_type_c_name(type_name)} *obj = find_{ident}_const(state, map_key);",
            "        if (!obj) { gsj_set_error(error, error_cap, \"unknown map key\"); return NULL; }",
            f"        return {fname}_get_field_json(obj, field, error, error_cap);",
            "    }",
        ])
    return "\n".join(lines)


def render_set_path(schema: dict[str, Any]) -> str:
    lines: list[str] = []
    for field in scalar_fields(schema):
        lines.extend(render_set_scalar_if(field, "state", field["path"], compare_var="path"))
    for field in list_fields(schema):
        path = field["path"]
        ident = c_ident(path)
        lines.extend([
            f'    if (strcmp(path, "{path}") == 0) {{',
            f"        return set_{ident}_from_json(state, value, error, error_cap);",
            "    }",
        ])
    for field in map_fields(schema):
        path = field["path"]
        ident = c_ident(path)
        type_name = map_type_name(field["type"])
        assert type_name is not None
        fname = object_type_func_name(type_name)
        prefix_len = len(path) + 1
        lines.extend([
            f'    if (strcmp(path, "{path}") == 0) {{',
            f"        return set_{ident}_from_json(state, value, error, error_cap);",
            "    }",
            f'    if (strncmp(path, "{path}.", {prefix_len}) == 0) {{',
            f"        const char *key = path + {prefix_len};",
            "        const char *field = strchr(key, '.');",
            f"        char map_key[{NS.macro}STRING_MAX];",
            "        if (field) {",
            "            size_t len = (size_t)(field - key);",
            "            if (len == 0 || len >= sizeof(map_key)) { gsj_set_error(error, error_cap, \"bad map key\"); return false; }",
            "            memcpy(map_key, key, len);",
            "            map_key[len] = '\\0';",
            "            field++;",
            "        } else if (!gsj_copy_text(map_key, sizeof(map_key), key)) {",
            "            gsj_set_error(error, error_cap, \"bad map key\");",
            "            return false;",
            "        }",
            f"        {object_type_c_name(type_name)} *obj = alloc_{ident}(state, map_key);",
            "        if (!obj) { gsj_set_error(error, error_cap, \"too many map entries or long key\"); return false; }",
            f"        return field ? {fname}_set_field_json(obj, field, value, error, error_cap) : {fname}_from_json(obj, value, error, error_cap);",
            "    }",
        ])
    return "\n".join(lines)


def render_from_json(schema: dict[str, Any]) -> str:
    lines: list[str] = []
    emitted_groups: set[str] = set()
    for field in schema["fields"]:
        parent, key, group = parent_var_for(field["path"])
        if group and group not in emitted_groups:
            emitted_groups.add(group)
            lines.append(f'    const cJSON *{parent} = gsj_object_item(json, "{group}");')
        source = parent if group else "json"
        if field["type"] in SCALAR_TYPES:
            lines.extend(render_read_scalar(field, source, "(&next)", key))
        elif is_list_type(field["type"]) or is_map_type(field["type"]):
            item_var = c_ident(f"{field['path']}_json")
            lines.extend([
                f'    const cJSON *{item_var} = gsj_object_item({source}, "{key}");',
                f"    if ({item_var} && !set_{c_ident(field['path'])}_from_json(&next, {item_var}, error, error_cap)) {{",
                "        return false;",
                "    }",
            ])
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Descriptor + migration table + hooks
# ---------------------------------------------------------------------------


def render_fragment_descriptor(schema: dict[str, Any]) -> str:
    migrations = schema["migrations"]
    hooks = schema["hooks"]
    pre: list[str] = []

    steps_field = "NULL"
    if migrations:
        for entry in migrations:
            pre.append(f'extern bool {entry["fn"]}(cJSON *frag, char *err, int cap);')
        pre.append(f"static const GameSaveMigrateFn {NS.inst}_migration_steps[] = {{")
        for entry in migrations:
            pre.append(f'    {entry["fn"]},')
        pre.append("};")
        steps_field = f"{NS.inst}_migration_steps"

    on_new_game = "NULL"
    reconcile = "NULL"
    if hooks.get("on_new_game"):
        pre.append(f"extern void {NS.ident}_on_new_game(void);")
        on_new_game = f"{NS.ident}_on_new_game"
    if hooks.get("reconcile"):
        pre.append(f"extern void {NS.ident}_reconcile(void);")
        reconcile = f"{NS.ident}_reconcile"

    wrappers = (
        f"static void   frag_reset(void)                                             {{ {NS.fn}init_defaults(&{NS.inst}); }}\n"
        f"static cJSON *frag_to_json(void)                                           {{ return {NS.fn}to_json(&{NS.inst}); }}\n"
        f"static bool   frag_from_json(const cJSON *j, char *e, int c)               {{ return {NS.fn}from_json(&{NS.inst}, j, e, c); }}\n"
        f"static cJSON *frag_get_path(const char *s, char *e, int c)                 {{ return {NS.fn}get_path_json(&{NS.inst}, s, e, c); }}\n"
        f"static bool   frag_set_path(const char *s, const cJSON *v, char *e, int c) {{ return {NS.fn}set_path_json(&{NS.inst}, s, v, e, c); }}\n"
        f"static cJSON *frag_schema(void)                                            {{ return {NS.fn}schema_json(); }}"
    )

    descriptor = (
        f"const GameSaveFragment {NS.frag} = {{\n"
        f"    .id            = {NS.macro}FRAGMENT_ID,\n"
        f"    .version       = {NS.macro}VERSION,\n"
        f"    .steps         = {steps_field},\n"
        f"    .reset         = frag_reset,\n"
        f"    .on_new_game   = {on_new_game},\n"
        f"    .to_json       = frag_to_json,\n"
        f"    .from_json     = frag_from_json,\n"
        f"    .reconcile     = {reconcile},\n"
        f"    .get_path_json = frag_get_path,\n"
        f"    .set_path_json = frag_set_path,\n"
        f"    .schema_json   = frag_schema,\n"
        f"}};"
    )

    parts = [wrappers]
    if pre:
        parts.append("\n".join(pre))
    parts.append(descriptor)
    return "\n\n".join(parts)


# ---------------------------------------------------------------------------
# Embedded normalized schema
# ---------------------------------------------------------------------------


def normalized_schema_for_embed(schema: dict[str, Any]) -> dict[str, Any]:
    """Canonical form the runtime embeds and the editor/smoke bot reads (§A4.4):
    fields (and each type's fields) are LISTS of {path, ...spec}; document is a
    compat duplicate of fragment so the transitional smoke bot stays green."""
    embed: dict[str, Any] = {}
    embed["schema"] = schema["schema"]
    embed["document"] = schema["fragment"]
    embed["fragment"] = schema["fragment"]
    embed["version"] = schema["version"]
    embed["schema_version"] = 2
    embed["string_max"] = schema["string_max"]
    embed["enums"] = schema["enums"]
    embed["types"] = {
        name: {"kind": "object", "fields": type_def["fields"]}
        for name, type_def in schema["types"].items()
    }
    embed["fields"] = schema["fields"]
    return embed


def render_schema_header(schema: dict[str, Any], schema_label: str) -> str:
    canonical = json.dumps(normalized_schema_for_embed(schema), ensure_ascii=False, separators=(",", ":"))
    chunks = [canonical[i : i + 1800] for i in range(0, len(canonical), 1800)]
    literal_lines = [f"    {json.dumps(chunk)}, \\" for chunk in chunks]
    guard = f"{NS.macro}SCHEMA_GEN_H"
    return (
        f"#ifndef {guard}\n"
        f"#define {guard}\n\n"
        f"/* Generated by {TOOL_LABEL} from {schema_label}. */\n"
        f"#define {NS.macro}SCHEMA_JSON_CHUNKS \\\n"
        "    { \\\n"
        + "\n".join(literal_lines)
        + "\n"
        + '    "" \\\n'
        + "    }\n\n"
        "#endif\n"
    )


# ---------------------------------------------------------------------------
# Source + devapi
# ---------------------------------------------------------------------------


def render_generic_source(schema: dict[str, Any], schema_label: str) -> str:
    return f"""#include "{NS.id}_state.h"
#include "{NS.id}_state_schema.gen.h"
#include "game_state_json.h"

/* Generated by {TOOL_LABEL} from {schema_label}. */

#include <stdlib.h>
#include <string.h>

{render_enum_tables(schema)}

{NS.type} {NS.inst};   /* fragment instance (ownership lives here) */

{render_object_helpers(schema)}

{render_collection_helpers(schema)}

void {NS.fn}init_defaults({NS.type} *state) {{
    memset(state, 0, sizeof(*state));
{render_defaults(schema)}
}}

bool {NS.fn}validate(const {NS.type} *state, char *error, int error_cap) {{
    if (!state) {{ gsj_set_error(error, error_cap, "state is null"); return false; }}
{render_validate(schema)}
    return true;
}}

cJSON *{NS.fn}schema_json(void) {{
    const char *chunks[] = {NS.macro}SCHEMA_JSON_CHUNKS;
    size_t len = 0;
    for (size_t i = 0; chunks[i][0] != '\\0'; i++) {{ len += strlen(chunks[i]); }}
    char *json = (char *)malloc(len + 1U);
    if (!json) {{ return cJSON_CreateObject(); }}
    char *cursor = json;
    for (size_t i = 0; chunks[i][0] != '\\0'; i++) {{
        size_t chunk_len = strlen(chunks[i]);
        memcpy(cursor, chunks[i], chunk_len);
        cursor += chunk_len;
    }}
    json[len] = '\\0';
    cJSON *root = cJSON_Parse(json);
    free(json);
    return root ? root : cJSON_CreateObject();
}}

cJSON *{NS.fn}to_json(const {NS.type} *state) {{
    cJSON *root = cJSON_CreateObject();
{render_to_json(schema)}
    return root;
}}

cJSON *{NS.fn}get_path_json(const {NS.type} *state, const char *path, char *error, int error_cap) {{
    if (!path || path[0] == '\\0') {{ return {NS.fn}to_json(state); }}
{render_get_path(schema)}
    gsj_set_error(error, error_cap, "unknown state path");
    return NULL;
}}

bool {NS.fn}set_path_json({NS.type} *state, const char *path, const cJSON *value, char *error, int error_cap) {{
    if (!state || !path || !path[0] || !value) {{ gsj_set_error(error, error_cap, "path and value are required"); return false; }}
{render_set_path(schema)}
    gsj_set_error(error, error_cap, "unknown state path");
    return false;
}}

bool {NS.fn}patch_json({NS.type} *state, const cJSON *values, char *error, int error_cap) {{
    if (!cJSON_IsObject(values)) {{ gsj_set_error(error, error_cap, "values must be an object"); return false; }}
    {NS.type} next = *state;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, values) {{
        if (!item->string || !{NS.fn}set_path_json(&next, item->string, item, error, error_cap)) {{ return false; }}
    }}
    if (!{NS.fn}validate(&next, error, error_cap)) {{ return false; }}
    *state = next;
    return true;
}}

bool {NS.fn}from_json({NS.type} *state, const cJSON *json, char *error, int error_cap) {{
    if (!cJSON_IsObject(json)) {{ gsj_set_error(error, error_cap, "state json must be object"); return false; }}
    {NS.type} next;
    {NS.fn}init_defaults(&next);
{render_from_json(schema)}
    if (!{NS.fn}validate(&next, error, error_cap)) {{ return false; }}
    *state = next;
    return true;
}}

{render_fragment_descriptor(schema)}
"""


def render_devapi_source(schema: dict[str, Any], schema_label: str) -> str:
    fn = NS.fn
    inst = NS.inst
    fid = NS.id
    return f"""#include "{fid}_state.h"

#if NT_DEVAPI_ENABLED

/* Generated by {TOOL_LABEL} from {schema_label}.
   Transitional single-fragment DevAPI (A5 replaces this with the shell registry
   dispatch). Routes through the fragment descriptor + game_save shell; there is
   no shared global state and no monolith file I/O. */

#include <stdio.h>
#include <string.h>

#include "devapi/nt_devapi.h"
#include "game_save.h"

/* Dev-only, single-threaded: one static buffer for dynamic error messages the
   engine reads after the handler returns (err->message must outlive the call). */
static char s_state_err[256];

static bool state_fail(nt_devapi_error *err, const char *code, const char *message) {{
    (void)snprintf(s_state_err, sizeof(s_state_err), "%s", message);
    err->code = code;
    err->message = s_state_err;
    return false;
}}

/* err already carries a message snprintf'd into s_state_err by a {fn}* call. */
static bool state_fail_buf(nt_devapi_error *err, const char *code) {{
    err->code = code;
    err->message = s_state_err;
    return false;
}}

/* {fn}* helpers return freshly-built cJSON; the engine ABI fills a pre-created
   object, so transplant src's members into result_obj. */
static bool state_emit(cJSON *result_obj, cJSON *src, nt_devapi_error *err) {{
    if (!src) {{
        return state_fail(err, "internal", "failed to build state json");
    }}
    cJSON *child = src->child;
    while (child) {{
        cJSON *next = child->next;
        cJSON_DetachItemViaPointer(src, child);
        cJSON_AddItemToObject(result_obj, child->string, child);
        child = next;
    }}
    cJSON_Delete(src);
    return true;
}}

static bool check_doc(const cJSON *params, nt_devapi_error *err) {{
    const cJSON *doc = cJSON_GetObjectItemCaseSensitive(params, "doc");
    if (!doc || (cJSON_IsString(doc) && strcmp(doc->valuestring, "{fid}") == 0)) {{
        return true;
    }}
    return state_fail(err, "bad_params", "unsupported state document");
}}

static bool ep_state_schema(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {{
    (void)user;
    if (!check_doc(params, err)) {{ return false; }}
    return state_emit(result_obj, {fn}schema_json(), err);
}}

static bool ep_state_get(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {{
    (void)user;
    if (!check_doc(params, err)) {{ return false; }}
    const cJSON *path = cJSON_GetObjectItemCaseSensitive(params, "path");
    const char *state_path = cJSON_IsString(path) ? path->valuestring : "";
    cJSON *value = {fn}get_path_json(&{inst}, state_path, s_state_err, (int)sizeof(s_state_err));
    if (!value) {{
        return state_fail_buf(err, "bad_params");
    }}
    /* result_obj is an object, but a path value can be any JSON, so wrap it. */
    cJSON_AddItemToObject(result_obj, "path", cJSON_CreateString(state_path));
    cJSON_AddItemToObject(result_obj, "value", value);
    return true;
}}

static bool ep_state_set(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {{
    (void)user;
    if (!check_doc(params, err)) {{ return false; }}
    const cJSON *path = cJSON_GetObjectItemCaseSensitive(params, "path");
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(params, "value");
    if (!cJSON_IsString(path) || !value) {{
        return state_fail(err, "bad_params", "path and value are required");
    }}
    if (!{fn}set_path_json(&{inst}, path->valuestring, value, s_state_err, (int)sizeof(s_state_err))) {{
        return state_fail_buf(err, "bad_params");
    }}
    game_save_mark_dirty();
    return state_emit(result_obj, {fn}to_json(&{inst}), err);
}}

static bool ep_state_patch(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {{
    (void)user;
    if (!check_doc(params, err)) {{ return false; }}
    const cJSON *values = cJSON_GetObjectItemCaseSensitive(params, "values");
    if (!cJSON_IsObject(values)) {{
        return state_fail(err, "bad_params", "values object is required");
    }}
    if (!{fn}patch_json(&{inst}, values, s_state_err, (int)sizeof(s_state_err))) {{
        return state_fail_buf(err, "bad_params");
    }}
    game_save_mark_dirty();
    return state_emit(result_obj, {fn}to_json(&{inst}), err);
}}

static bool ep_state_reset(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {{
    (void)user;
    if (!check_doc(params, err)) {{ return false; }}
    {fn}init_defaults(&{inst});
    game_save_mark_dirty();
    return state_emit(result_obj, {fn}to_json(&{inst}), err);
}}

static bool ep_state_save(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {{
    (void)user;
    if (!check_doc(params, err)) {{ return false; }}
    if (!game_save_flush(s_state_err, (int)sizeof(s_state_err))) {{
        return state_fail_buf(err, "internal");
    }}
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(obj, "saved", true);
    return state_emit(result_obj, obj, err);
}}

static bool ep_state_load(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {{
    (void)user;
    if (!check_doc(params, err)) {{ return false; }}
    game_save_load_result_t result;
    game_save_load(&result);
    return state_emit(result_obj, {fn}to_json(&{inst}), err);
}}

void {fn}register_devapi(void) {{
    static const nt_devapi_command_desc descs[] = {{
        {{"{fid}.state.schema", "{fid}", "Return the {fid} state JSON schema.", "doc?", "schema object", "immediate", "none"}},
        {{"{fid}.state.get", "{fid}", "Get a state value by path.", "doc?, path", "path, value", "immediate", "none"}},
        {{"{fid}.state.set", "{fid}", "Set a state value by path.", "doc?, path, value", "state object", "immediate", "mutates state"}},
        {{"{fid}.state.patch", "{fid}", "Patch multiple state values.", "doc?, values", "state object", "immediate", "mutates state"}},
        {{"{fid}.state.save", "{fid}", "Flush state to storage.", "doc?", "saved", "immediate", "writes file"}},
        {{"{fid}.state.load", "{fid}", "Reload state from storage.", "doc?", "state object", "immediate", "mutates state"}},
        {{"{fid}.state.reset", "{fid}", "Reset state to defaults.", "doc?", "state object", "immediate", "mutates state"}},
    }};
    const nt_devapi_handler_fn fns[] = {{
        ep_state_schema, ep_state_get, ep_state_set, ep_state_patch,
        ep_state_save, ep_state_load, ep_state_reset,
    }};
    for (size_t i = 0; i < sizeof(fns) / sizeof(fns[0]); ++i) {{
        (void)nt_devapi_register(&descs[i], fns[i], NULL);
    }}
}}

#endif
"""


def render_source(schema: dict[str, Any], schema_label: str = "state/game_state.schema.json") -> str:
    return render_generic_source(schema, schema_label)


# ---------------------------------------------------------------------------
# Events (typed event structs + emit helpers + descriptors)  [E2]
# ---------------------------------------------------------------------------


def schema_events(schema: dict[str, Any]) -> dict[str, dict[str, Any]]:
    events = schema.get("events", {})
    return events if isinstance(events, dict) else {}


def event_struct_c_name(evt: str) -> str:
    return f"{NS.pascal}Ev{_pascal(evt)}"          # MiniEvCellSpawned


def event_emit_fn(evt: str) -> str:
    return f"{NS.id}_emit_{evt}"                    # mini_emit_cell_spawned


def event_type_fn(evt: str) -> str:
    return f"{NS.id}_ev_{evt}_type"                 # mini_ev_cell_spawned_type


def event_desc_name(evt: str) -> str:
    return f"{NS.id}_ev_{evt}_desc"                 # mini_ev_cell_spawned_desc


def event_full_name(evt: str) -> str:
    return f"{NS.id}.{evt}"                         # "mini.cell_spawned"


def event_accessor(evt: str, field_name: str) -> str:
    return f"{NS.id}_ev_{evt}_{field_name}"         # mini_ev_cell_spawned_label


def event_has_inline(fields: list[dict[str, Any]]) -> bool:
    return any(f["type"] in ("string", "bytes") for f in fields)


def render_event_struct_fields(fields: list[dict[str, Any]]) -> list[str]:
    lines: list[str] = []
    for f in fields:
        name = f["name"]
        typ = f["type"]
        if typ == "float":
            lines.append(f"    double {name}; /* schema 'float' == C double (f64); event float != state float */")
        elif typ == "string":
            lines.append(f"    uint32_t {name}; /* byte offset -> inline NUL string (read via accessor) */")
        elif typ == "bytes":
            lines.append(f"    uint32_t {name}; /* byte offset -> inline bytes (read via accessor) */")
            lines.append(f"    uint32_t {name}_len; /* length of the inline bytes */")
        else:
            lines.append(f"    {EVENT_FIELD_C_TYPE[typ]} {name};")
    return lines


def render_event_accessors(evt: str, fields: list[dict[str, Any]]) -> list[str]:
    struct = event_struct_c_name(evt)
    lines: list[str] = []
    for f in fields:
        name = f["name"]
        acc = event_accessor(evt, name)
        if f["type"] == "string":
            lines.append(f"static inline const char *{acc}(const {struct} *e) {{")
            lines.append(f"    return (const char *)e + e->{name};")
            lines.append("}")
        elif f["type"] == "bytes":
            lines.append(f"static inline const void *{acc}(const {struct} *e) {{")
            lines.append(f"    return (const uint8_t *)e + e->{name};")
            lines.append("}")
            lines.append(f"static inline uint32_t {acc}_len(const {struct} *e) {{")
            lines.append(f"    return e->{name}_len;")
            lines.append("}")
    return lines


def render_event_emit_args(fields: list[dict[str, Any]]) -> str:
    args: list[str] = []
    for f in fields:
        name = f["name"]
        typ = f["type"]
        if typ == "string":
            args.append(f"const char *{name}")
        elif typ == "bytes":
            args.append(f"const void *{name}, uint32_t {name}_len")
        else:
            args.append(f"{EVENT_FIELD_EMIT_ARG[typ]} {name}")
    return ", ".join(args) if args else "void"


def render_event_emit_body(evt: str, fields: list[dict[str, Any]]) -> list[str]:
    struct = event_struct_c_name(evt)
    type_fn = event_type_fn(evt)
    emit_fn = event_emit_fn(evt)
    lines: list[str] = []
    if not event_has_inline(fields):
        # scalar-only: a direct local struct, no staging (E2 §6).
        lines.append(f"    {struct} ev;")
        lines.append("    memset(&ev, 0, sizeof(ev));")
        for f in fields:
            lines.append(f"    ev.{f['name']} = {f['name']};")
        lines.append(f"    return game_event_emit({type_fn}(), &ev, (uint32_t)sizeof(ev), _Alignof({struct}));")
        return lines
    # inline strings/bytes: aligned union staging (positional-independent packing).
    lines.append("    union {")
    lines.append(f"        {struct} ev;")
    lines.append("        uint8_t bytes[GAME_EVENT_EMIT_MAX];")
    lines.append("    } u;")
    lines.append("    memset(&u, 0, sizeof(u.ev)); /* deterministic struct padding; strings written below */")
    for f in fields:
        if f["type"] not in ("string", "bytes"):
            lines.append(f"    u.ev.{f['name']} = {f['name']};")
    lines.append("")
    lines.append("    uint32_t off = (uint32_t)sizeof(u.ev);")
    terms: list[str] = []
    for f in fields:
        name = f["name"]
        if f["type"] == "string":
            lines.append(f'    const char *{name}_s = {name} ? {name} : "";')
            lines.append(f"    size_t {name}_n = strlen({name}_s) + 1u; /* incl. NUL */")
            terms.append(f"{name}_n")
        elif f["type"] == "bytes":
            terms.append(f"(size_t){name}_len")
    cond = " + ".join(["(size_t)off", *terms])
    lines.append(f"    if ({cond} > sizeof(u.bytes)) {{")
    lines.append(f'        NT_ASSERT(0 && "{emit_fn} payload exceeds GAME_EVENT_EMIT_MAX");')
    lines.append(f'        nt_log_warn("{emit_fn}: payload exceeds GAME_EVENT_EMIT_MAX (%u B) -> dropped", (unsigned)GAME_EVENT_EMIT_MAX);')
    lines.append("        return NULL; /* release: warned drop (no dropped-counter -- E1's counter is private/frozen) */")
    lines.append("    }")
    first_write = True
    for f in fields:
        name = f["name"]
        if f["type"] == "string":
            if not first_write:
                lines.append("")
            first_write = False
            lines.append(f"    u.ev.{name} = off;")
            lines.append(f"    memcpy(u.bytes + off, {name}_s, {name}_n);")
            lines.append(f"    off += (uint32_t){name}_n;")
        elif f["type"] == "bytes":
            if not first_write:
                lines.append("")
            first_write = False
            lines.append(f"    u.ev.{name} = off;")
            lines.append(f"    u.ev.{name}_len = {name}_len;")
            lines.append(f"    if ({name}_len != 0u && {name} != NULL) {{ memcpy(u.bytes + off, {name}, {name}_len); }}")
            lines.append(f"    off += {name}_len;")
    lines.append(f"    return game_event_emit({type_fn}(), &u, off, _Alignof({struct}));")
    return lines


def render_event_descriptor(evt: str, fields: list[dict[str, Any]]) -> list[str]:
    struct = event_struct_c_name(evt)
    fields_arr = f"{NS.id}_ev_{evt}_fields"
    lines = [f"static const game_event_field_t {fields_arr}[] = {{"]
    for f in fields:
        name = f["name"]
        ft = EVENT_FIELD_FT_ENUM[f["type"]]
        if f["type"] == "bytes":
            len_off = f"(uint32_t)offsetof({struct}, {name}_len)"
        else:
            len_off = "0u"
        lines.append(f'    {{ "{name}", {ft}, (uint32_t)offsetof({struct}, {name}), {len_off} }},')
    lines.append("};")
    lines.append(f"const game_event_desc_t {event_desc_name(evt)} = {{")
    lines.append(f'    "{event_full_name(evt)}",')
    lines.append(f"    (uint32_t)sizeof({struct}),")
    lines.append(f"    {fields_arr},")
    lines.append(f"    (int)(sizeof({fields_arr}) / sizeof({fields_arr}[0])),")
    lines.append("};")
    return lines


def render_events_header(schema: dict[str, Any], schema_label: str) -> str:
    events = schema_events(schema)
    guard = f"{NS.macro}EVENTS_GEN_H"
    parts: list[str] = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        f"/* Generated by {TOOL_LABEL} from {schema_label}. */",
        "",
        "#include <stdbool.h>",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        '#include "hash/nt_hash.h"    /* nt_hash64_t */',
        '#include "game_event_desc.h" /* game_event_desc_t + field-type enum */',
        "",
    ]
    for evt, spec in events.items():
        fields = spec["fields"]
        struct = event_struct_c_name(evt)
        parts.append(f"/* ---- {event_full_name(evt)} ---- */")
        parts.append(f"typedef struct {struct} {{")
        parts.extend(render_event_struct_fields(fields))
        parts.append(f"}} {struct};")
        parts.append("")
        parts.append(f'nt_hash64_t {event_type_fn(evt)}(void); /* nt_hash64_str("{event_full_name(evt)}"), cached */')
        parts.append("")
        parts.append(f"const void *{event_emit_fn(evt)}({render_event_emit_args(fields)});")
        accessors = render_event_accessors(evt, fields)
        if accessors:
            parts.append("")
            parts.extend(accessors)
        parts.append("")
        parts.append(f"extern const game_event_desc_t {event_desc_name(evt)};")
        parts.append("")
    parts.append("/* ---- fragment event table + label registration ---- */")
    parts.append(f"extern const game_event_desc_t *const {NS.id}_ev_descs[];")
    parts.append(f"extern const int {NS.id}_ev_desc_count;")
    parts.append("")
    parts.append(f"void {NS.id}_ev_register(void); /* register debug labels; call once after nt_hash_init */")
    parts.append("")
    parts.append(f"#endif /* {guard} */")
    parts.append("")
    return "\n".join(parts)


def render_events_source(schema: dict[str, Any], schema_label: str) -> str:
    events = schema_events(schema)
    parts: list[str] = [
        f'#include "{NS.id}_state_events.gen.h"',
        "",
        f"/* Generated by {TOOL_LABEL} from {schema_label}. */",
        "",
    ]
    if not events:
        # Empty fragment (no events): a zero-length array is invalid in C, so emit a
        # 1-element NULL stub; consumers gate on count==0 and never deref (E2 §6).
        parts.append(f"const game_event_desc_t *const {NS.id}_ev_descs[1] = {{ NULL }};")
        parts.append(f"const int {NS.id}_ev_desc_count = 0;")
        parts.append(f"void {NS.id}_ev_register(void) {{ }}")
        parts.append("")
        return "\n".join(parts)
    parts.append("#include <stddef.h> /* offsetof, max_align_t */")
    parts.append("#include <string.h> /* memcpy, memset, strlen */")
    parts.append("")
    parts.append('#include "core/nt_assert.h"')
    parts.append('#include "game_events.h" /* game_event_emit, game_event_register_type_name */')
    parts.append('#include "log/nt_log.h"  /* nt_log_warn on staging overflow (release-visible) */')
    parts.append("")
    for evt in events:
        struct = event_struct_c_name(evt)
        parts.append(f"_Static_assert(_Alignof({struct}) <= _Alignof(max_align_t),")
        parts.append(f'               "{struct} over-aligned for game_event_emit");')
    parts.append("")
    for evt, spec in events.items():
        fields = spec["fields"]
        parts.append(f"/* ---- {event_full_name(evt)} ---- */")
        parts.append(f"nt_hash64_t {event_type_fn(evt)}(void) {{")
        parts.append("    static nt_hash64_t h;")
        parts.append(f'    if (!h.value) {{ h = nt_hash64_str("{event_full_name(evt)}"); }}')
        parts.append("    return h;")
        parts.append("}")
        parts.append("")
        parts.append(f"const void *{event_emit_fn(evt)}({render_event_emit_args(fields)}) {{")
        parts.extend(render_event_emit_body(evt, fields))
        parts.append("}")
        parts.append("")
        parts.extend(render_event_descriptor(evt, fields))
        parts.append("")
    parts.append("/* ---- fragment event table ---- */")
    parts.append(f"const game_event_desc_t *const {NS.id}_ev_descs[] = {{")
    for evt in events:
        parts.append(f"    &{event_desc_name(evt)},")
    parts.append("};")
    parts.append(f"const int {NS.id}_ev_desc_count = {len(events)};")
    parts.append("")
    parts.append(f"void {NS.id}_ev_register(void) {{")
    for evt in events:
        parts.append(f'    game_event_register_type_name({event_type_fn(evt)}(), "{event_full_name(evt)}");')
    parts.append("}")
    parts.append("")
    return "\n".join(parts)


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------


def write_if_changed(path: Path, text: str) -> bool:
    old = path.read_text(encoding="utf-8") if path.exists() else None
    if old == text:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return True


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--schema", default=None, help="Schema JSON to generate from.")
    parser.add_argument("--out-dir", default=None, help="Directory for generated fragment state files.")
    parser.add_argument("--fragment", default=None, help="Expected fragment id (asserted against schema.fragment).")
    args = parser.parse_args(argv)

    schema_path = Path(args.schema).resolve() if args.schema else default_schema_path().resolve()
    out_dir = Path(args.out_dir).resolve() if args.out_dir else default_out_dir(schema_path).resolve()
    schema_label = relative_label(schema_path)

    schema = load_schema(schema_path)
    fragment = schema["fragment"]
    if args.fragment is not None and args.fragment != fragment:
        raise SystemExit(f"--fragment {args.fragment!r} does not match schema.fragment {fragment!r}")

    header_path = out_dir / f"{fragment}_state.h"
    source_path = out_dir / f"{fragment}_state.c"
    devapi_source_path = out_dir / f"{fragment}_state_devapi.c"
    schema_header_path = out_dir / f"{fragment}_state_schema.gen.h"
    events_header_path = out_dir / f"{fragment}_state_events.gen.h"
    events_source_path = out_dir / f"{fragment}_state_events.gen.c"

    changed = []
    if write_if_changed(header_path, render_header(schema, schema_label)):
        changed.append(header_path)
    if write_if_changed(source_path, render_source(schema, schema_label)):
        changed.append(source_path)
    if write_if_changed(schema_header_path, render_schema_header(schema, schema_label)):
        changed.append(schema_header_path)
    if write_if_changed(devapi_source_path, render_devapi_source(schema, schema_label)):
        changed.append(devapi_source_path)
    # E2: typed event structs/emit/descriptors (separate family; always written even
    # for an empty events section so the CMake OUTPUT set is deterministic).
    if write_if_changed(events_header_path, render_events_header(schema, schema_label)):
        changed.append(events_header_path)
    if write_if_changed(events_source_path, render_events_source(schema, schema_label)):
        changed.append(events_source_path)
    if changed:
        for path in changed:
            print(f"generated {relative_label(path)}")
    else:
        print("state generated files are up to date")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
