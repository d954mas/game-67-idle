#!/usr/bin/env python3
"""Generate the supported game state C API from a state schema JSON file."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
SCHEMA_PATH = ROOT / "state" / "game_state.schema.json"
OUT_DIR = ROOT / "src" / "generated"
HEADER_PATH = OUT_DIR / "game_state.h"
SOURCE_PATH = OUT_DIR / "game_state.c"
DEVAPI_SOURCE_PATH = OUT_DIR / "game_state_devapi.c"
SCHEMA_HEADER_PATH = OUT_DIR / "game_state_schema.gen.h"
SOURCE_TEMPLATE_PATH = Path(__file__).with_name("game_state.c.in")

# Fields the generated template hard-codes (items/inventory/equipment) plus the
# infra conventions every game keeps. Game-specific fields live in the schema.
REQUIRED_FIELDS = {
    "settings.master_volume",
    "settings.sfx_volume",
    "tutorial.done",
    "items",
    "inventory.item_ids",
    "equipment.hand_item_id",
}

SCALAR_TYPES = {"bool", "int", "float", "string", "string?", "enum"}


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


def relative_label(path: Path) -> str:
    try:
        return path.relative_to(ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def load_schema(schema_path: Path = SCHEMA_PATH) -> dict[str, Any]:
    with schema_path.open("r", encoding="utf-8") as f:
        schema = json.load(f)
    if schema.get("schema") != "game_seed.state":
        raise SystemExit("schema id must be game_seed.state")
    if schema.get("document") != "game":
        raise SystemExit("document must be game")
    if schema.get("version") not in (1, 2, 3):
        raise SystemExit("this generator supports version 1, 2 or 3")
    if not isinstance(schema.get("string_max"), int) or schema["string_max"] < 2:
        raise SystemExit("string_max must be an integer >= 2")
    if not isinstance(schema.get("fields"), list):
        raise SystemExit("fields must be a list")
    paths = {field.get("path") for field in schema["fields"] if isinstance(field, dict)}
    missing = sorted(REQUIRED_FIELDS - paths)
    if missing:
        raise SystemExit("schema missing required fields: " + ", ".join(missing))
    validate_supported_shape(schema)
    return schema


def validate_supported_shape(schema: dict[str, Any]) -> None:
    reserved_ids = set()
    reserved_paths = set()
    for item in schema.get("reserved", []):
        if not isinstance(item, dict) or not isinstance(item.get("id"), int) or not isinstance(item.get("path"), str):
            raise SystemExit("reserved entries must include integer id and string path")
        reserved_ids.add(item["id"])
        reserved_paths.add(item["path"])

    enums = schema.get("enums", {})
    if not isinstance(enums, dict):
        raise SystemExit("enums must be an object")
    for name, values in enums.items():
        if not isinstance(name, str) or not isinstance(values, list) or not values:
            raise SystemExit("enum values must be non-empty arrays")
        if any(not isinstance(value, str) or not value for value in values):
            raise SystemExit(f"enum {name} contains a bad value")

    collections = schema.get("collections", {})
    for key in ("items_max", "inventory_item_ids_max"):
        if not isinstance(collections.get(key), int) or collections[key] <= 0:
            raise SystemExit(f"collections.{key} must be a positive integer")

    item = schema.get("types", {}).get("ItemInstance")
    if not isinstance(item, dict) or item.get("kind") != "object":
        raise SystemExit("types.ItemInstance object is required")
    item_fields = item.get("fields", [])
    item_paths = {field.get("path") for field in item_fields if isinstance(field, dict)}
    if item_paths != {"def_id", "count", "level", "durability"}:
        raise SystemExit("types.ItemInstance currently supports def_id/count/level/durability")
    validate_field_ids("types.ItemInstance", item_fields, reserved_ids, reserved_paths)

    validate_field_ids("fields", schema["fields"], reserved_ids, reserved_paths)
    for field in schema["fields"]:
        if not isinstance(field, dict):
            raise SystemExit("field entries must be objects")
        path = field.get("path")
        typ = field.get("type")
        if not isinstance(path, str) or not path:
            raise SystemExit("field path must be a non-empty string")
        if typ not in SCALAR_TYPES and typ not in {"map<string,ItemInstance>", "list<string>"}:
            raise SystemExit(f"unsupported field type for {path}: {typ}")
        if typ == "enum" and field.get("enum") not in enums:
            raise SystemExit(f"{path} references unknown enum")
        if typ in {"map<string,ItemInstance>", "list<string>"}:
            if not isinstance(field.get("max_count"), int) or field["max_count"] <= 0:
                raise SystemExit(f"{path} must declare positive max_count")
        if typ in {"string", "string?"}:
            if not isinstance(field.get("max_length"), int) or field["max_length"] <= 0 or field["max_length"] >= schema["string_max"]:
                raise SystemExit(f"{path} must declare max_length from 1 to string_max-1")


def validate_field_ids(scope: str, fields: list[Any], reserved_ids: set[int], reserved_paths: set[str]) -> None:
    seen_ids: dict[int, str] = {}
    seen_paths: set[str] = set()
    for field in fields:
        if not isinstance(field, dict):
            raise SystemExit(f"{scope} entries must be objects")
        path = field.get("path")
        field_id = field.get("id")
        if not isinstance(path, str) or not path:
            raise SystemExit(f"{scope} field path must be a non-empty string")
        if not isinstance(field_id, int) or field_id <= 0:
            raise SystemExit(f"{path} must declare a positive integer id")
        if path in reserved_paths:
            raise SystemExit(f"{path} is reserved and cannot be reused")
        if field_id in reserved_ids:
            raise SystemExit(f"{path} uses reserved id {field_id}")
        if path in seen_paths:
            raise SystemExit(f"duplicate field path {path}")
        if field_id in seen_ids:
            raise SystemExit(f"duplicate field id {field_id}: {seen_ids[field_id]} and {path}")
        seen_paths.add(path)
        seen_ids[field_id] = path


def field_c_type(field: dict[str, Any]) -> str:
    typ = field["type"]
    if typ == "bool":
        return "bool"
    if typ == "int":
        return "int"
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
    return f"GAME_STATE_{c_macro(enum_name)}_{c_macro(value)}"


def field_by_path(fields: list[dict[str, Any]], path: str) -> dict[str, Any]:
    for field in fields:
        if field["path"] == path:
            return field
    raise SystemExit(f"schema missing field {path}")


def c_float(value: Any) -> str:
    text = f"{float(value):.8g}"
    if "." not in text and "e" not in text and "E" not in text:
        text += ".0"
    return f"{text}F"


def c_int(value: Any) -> str:
    if not isinstance(value, int):
        raise SystemExit(f"expected integer value, got {value!r}")
    return str(value)


def c_string(value: Any) -> str:
    if not isinstance(value, str):
        raise SystemExit(f"expected string value, got {value!r}")
    return json.dumps(value)


def render_state_constants(schema: dict[str, Any]) -> str:
    lines: list[str] = []

    def emit_scalar(field: dict[str, Any], prefix: str = "") -> None:
        typ = field["type"]
        name = c_macro(f"{prefix}{field['path']}")
        if typ == "enum":
            lines.append(f"#define GAME_STATE_{name}_DEFAULT {default_enum_macro(field['enum'], field['default'])}")
        elif typ == "int":
            lines.append(f"#define GAME_STATE_{name}_DEFAULT {c_int(field['default'])}")
            lines.append(f"#define GAME_STATE_{name}_MIN {c_int(field['min'])}")
            lines.append(f"#define GAME_STATE_{name}_MAX {c_int(field['max'])}")
        elif typ == "float":
            lines.append(f"#define GAME_STATE_{name}_DEFAULT {c_float(field['default'])}")
            lines.append(f"#define GAME_STATE_{name}_MIN {c_float(field['min'])}")
            lines.append(f"#define GAME_STATE_{name}_MAX {c_float(field['max'])}")
        elif typ == "string":
            if "default" in field:
                lines.append(f"#define GAME_STATE_{name}_DEFAULT {c_string(field['default'])}")
        elif typ == "bool":
            lines.append(f"#define GAME_STATE_{name}_DEFAULT {1 if field['default'] else 0}")

    for field in schema["fields"]:
        if field["type"] in SCALAR_TYPES:
            emit_scalar(field)
    for field in schema["types"]["ItemInstance"]["fields"]:
        if field["type"] in SCALAR_TYPES:
            emit_scalar(field, "item.")
    return "\n".join(lines)


def render_enum(enum_name: str, values: list[str]) -> str:
    type_name = f"GameState{enum_name}"
    lines = [f"typedef enum {type_name} {{"]
    for i, value in enumerate(values):
        suffix = f" = {i}" if i == 0 else ""
        lines.append(f"    GAME_STATE_{c_macro(enum_name)}_{c_macro(value)}{suffix},")
    lines.append(f"    GAME_STATE_{c_macro(enum_name)}_COUNT,")
    lines.append(f"}} {type_name};")
    return "\n".join(lines)


def render_state_struct(schema: dict[str, Any]) -> str:
    lines = ["typedef struct GameState {"]
    for field in schema["fields"]:
        path = field["path"]
        typ = field["type"]
        name = c_ident(path)
        if typ in {"map<string,ItemInstance>", "list<string>"}:
            continue
        if typ == "string":
            lines.append(f"    char {name}[GAME_STATE_STRING_MAX];")
        elif typ == "string?":
            lines.append(f"    bool has_{name};")
            lines.append(f"    char {name}[GAME_STATE_STRING_MAX];")
        else:
            lines.append(f"    {field_c_type(field)} {name};")
    lines.append("    GameItemInstance items[GAME_STATE_MAX_ITEMS];")
    lines.append("    char inventory_item_ids[GAME_STATE_MAX_INVENTORY_ITEM_IDS][GAME_STATE_STRING_MAX];")
    lines.append("    int inventory_item_ids_count;")
    lines.append("} GameState;")
    return "\n".join(lines)


def render_header(schema: dict[str, Any], schema_label: str = "state/game_state.schema.json") -> str:
    enums = schema["enums"]
    collections = schema["collections"]
    enum_blocks = "\n\n".join(render_enum(name, values) for name, values in enums.items())
    return f"""#ifndef GAME_STATE_GENERATED_H
#define GAME_STATE_GENERATED_H

/* Generated by tools/state_codegen/generate_state.py from {schema_label}. */

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"

#define GAME_STATE_SCHEMA_ID "{schema["schema"]}"
#define GAME_STATE_DOCUMENT "{schema["document"]}"
#define GAME_STATE_VERSION {schema["version"]}
#define GAME_STATE_STRING_MAX {schema["string_max"]}
#define GAME_STATE_MAX_ITEMS {collections["items_max"]}
#define GAME_STATE_MAX_INVENTORY_ITEM_IDS {collections["inventory_item_ids_max"]}

{render_state_constants(schema)}

{enum_blocks}

typedef struct GameItemInstance {{
    bool used;
    char instance_id[GAME_STATE_STRING_MAX];
    char def_id[GAME_STATE_STRING_MAX];
    int count;
    int level;
    float durability;
}} GameItemInstance;

{render_state_struct(schema)}

typedef bool (*game_state_changed_fn)(const char *path, void *user, char *error, int error_cap);

extern GameState g_game_state;

{render_enum_name_decls(schema)}

void game_state_init_defaults(GameState *state);
void game_state_init(void);
void game_state_set_changed_callback(game_state_changed_fn callback, void *user);
bool game_state_validate(const GameState *state, char *error, int error_cap);
void game_state_mark_dirty(void);
bool game_state_is_dirty(void);
void game_state_clear_dirty(void);

cJSON *game_state_schema_json(void);
cJSON *game_state_to_json(const GameState *state);
cJSON *game_state_get_path_json(const GameState *state, const char *path, char *error, int error_cap);
bool game_state_set_path_json(GameState *state, const char *path, const cJSON *value, char *error, int error_cap);
bool game_state_patch_json(GameState *state, const cJSON *values, char *error, int error_cap);
bool game_state_from_json(GameState *state, const cJSON *json, char *error, int error_cap);

char *game_state_save_json_string(const GameState *state, char *error, int error_cap);
bool game_state_load_json_string(GameState *state, const char *data, char *error, int error_cap);
bool game_state_save(const GameState *state, const char *path, char *error, int error_cap);
bool game_state_load(GameState *state, const char *path, char *error, int error_cap);
bool game_state_reset(GameState *state, char *error, int error_cap);

#if NT_DEVAPI_ENABLED
void game_state_register_devapi(void);
#endif

#endif
"""


def render_schema_header(schema: dict[str, Any], schema_label: str = "state/game_state.schema.json") -> str:
    canonical = json.dumps(schema, ensure_ascii=False, separators=(",", ":"))
    chunks = [canonical[i : i + 1800] for i in range(0, len(canonical), 1800)]
    literal_lines = [f"    {json.dumps(chunk)}, \\" for chunk in chunks]
    return (
        "#ifndef GAME_STATE_SCHEMA_GEN_H\n"
        "#define GAME_STATE_SCHEMA_GEN_H\n\n"
        f"/* Generated by tools/state_codegen/generate_state.py from {schema_label}. */\n"
        "#define GAME_STATE_SCHEMA_JSON_CHUNKS \\\n"
        "    { \\\n"
        + "\n".join(literal_lines)
        + "\n"
        + '    "" \\\n'
        + "    }\n\n"
        "#endif\n"
    )


def render_devapi_source(schema: dict[str, Any], schema_label: str = "state/game_state.schema.json") -> str:
    doc = schema["document"]
    return f"""#include "game_state.h"

#if NT_DEVAPI_ENABLED

/* Generated by tools/state_codegen/generate_state.py from {schema_label}. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "devapi/nt_devapi.h"
#include "game_storage.h"

/* Dev-only, single-threaded: one static buffer for dynamic error messages the
   engine reads after the handler returns (err->message must outlive the call). */
static char s_state_err[256];

static bool state_fail(nt_devapi_error *err, const char *code, const char *message) {{
    (void)snprintf(s_state_err, sizeof(s_state_err), "%s", message);
    err->code = code;
    err->message = s_state_err;
    return false;
}}

/* err already carries a message snprintf'd into s_state_err by a game_state_* call. */
static bool state_fail_buf(nt_devapi_error *err, const char *code) {{
    err->code = code;
    err->message = s_state_err;
    return false;
}}

/* game_state_* helpers return freshly-built cJSON; the engine ABI fills a
   pre-created object, so transplant src's members into result_obj. */
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
    if (!doc || (cJSON_IsString(doc) && strcmp(doc->valuestring, "{doc}") == 0)) {{
        return true;
    }}
    return state_fail(err, "bad_params", "unsupported state document");
}}

static bool ep_game_state_schema(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {{
    (void)user;
    if (!check_doc(params, err)) {{
        return false;
    }}
    return state_emit(result_obj, game_state_schema_json(), err);
}}

static bool ep_game_state_get(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {{
    (void)user;
    if (!check_doc(params, err)) {{
        return false;
    }}
    const cJSON *path = cJSON_GetObjectItemCaseSensitive(params, "path");
    const char *state_path = cJSON_IsString(path) ? path->valuestring : "";
    cJSON *value = game_state_get_path_json(&g_game_state, state_path, s_state_err, (int)sizeof(s_state_err));
    if (!value) {{
        return state_fail_buf(err, "bad_params");
    }}
    /* result_obj is an object, but a path value can be any JSON, so wrap it. */
    cJSON_AddItemToObject(result_obj, "path", cJSON_CreateString(state_path));
    cJSON_AddItemToObject(result_obj, "value", value);
    return true;
}}

static bool ep_game_state_set(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {{
    (void)user;
    if (!check_doc(params, err)) {{
        return false;
    }}
    const cJSON *path = cJSON_GetObjectItemCaseSensitive(params, "path");
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(params, "value");
    if (!cJSON_IsString(path) || !value) {{
        return state_fail(err, "bad_params", "path and value are required");
    }}
    cJSON *values = cJSON_CreateObject();
    cJSON *copy = cJSON_Duplicate(value, true);
    if (!values || !copy) {{
        cJSON_Delete(values);
        cJSON_Delete(copy);
        return state_fail(err, "internal", "failed to copy state value");
    }}
    cJSON_AddItemToObject(values, path->valuestring, copy);
    bool ok = game_state_patch_json(&g_game_state, values, s_state_err, (int)sizeof(s_state_err));
    cJSON_Delete(values);
    if (!ok) {{
        return state_fail_buf(err, "bad_params");
    }}
    return state_emit(result_obj, game_state_to_json(&g_game_state), err);
}}

static bool ep_game_state_patch(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {{
    (void)user;
    if (!check_doc(params, err)) {{
        return false;
    }}
    const cJSON *values = cJSON_GetObjectItemCaseSensitive(params, "values");
    if (!cJSON_IsObject(values)) {{
        return state_fail(err, "bad_params", "values object is required");
    }}
    if (!game_state_patch_json(&g_game_state, values, s_state_err, (int)sizeof(s_state_err))) {{
        return state_fail_buf(err, "bad_params");
    }}
    return state_emit(result_obj, game_state_to_json(&g_game_state), err);
}}

static bool ep_game_state_save(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {{
    (void)user;
    if (!check_doc(params, err)) {{
        return false;
    }}
    cJSON *obj = cJSON_CreateObject();
    const cJSON *unsafe_path = cJSON_GetObjectItemCaseSensitive(params, "unsafe_path");
    if (cJSON_IsString(unsafe_path)) {{
        if (!game_state_save(&g_game_state, unsafe_path->valuestring, s_state_err, (int)sizeof(s_state_err))) {{
            cJSON_Delete(obj);
            return state_fail_buf(err, "bad_params");
        }}
        cJSON_AddStringToObject(obj, "unsafe_path", unsafe_path->valuestring);
        return state_emit(result_obj, obj, err);
    }}

    const cJSON *key = cJSON_GetObjectItemCaseSensitive(params, "key");
    if (!cJSON_IsString(key)) {{
        cJSON_Delete(obj);
        return state_fail(err, "bad_params", "key is required");
    }}
    char *data = game_state_save_json_string(&g_game_state, s_state_err, (int)sizeof(s_state_err));
    if (!data) {{
        cJSON_Delete(obj);
        return state_fail_buf(err, "internal");
    }}
    if (!game_storage_save_json(key->valuestring, GAME_STATE_DOCUMENT, data, s_state_err, (int)sizeof(s_state_err))) {{
        cJSON_free(data);
        cJSON_Delete(obj);
        return state_fail_buf(err, "internal");
    }}
    cJSON_free(data);
    char resolved[256];
    if (game_storage_resolve_key(key->valuestring, GAME_STATE_DOCUMENT, resolved, (int)sizeof(resolved), NULL, 0)) {{
        cJSON_AddStringToObject(obj, "resolved", resolved);
    }}
    cJSON_AddStringToObject(obj, "key", key->valuestring);
    cJSON_AddStringToObject(obj, "doc", GAME_STATE_DOCUMENT);
    return state_emit(result_obj, obj, err);
}}

static bool ep_game_state_load(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {{
    (void)user;
    if (!check_doc(params, err)) {{
        return false;
    }}
    const cJSON *unsafe_path = cJSON_GetObjectItemCaseSensitive(params, "unsafe_path");
    if (cJSON_IsString(unsafe_path)) {{
        if (!game_state_load(&g_game_state, unsafe_path->valuestring, s_state_err, (int)sizeof(s_state_err))) {{
            return state_fail_buf(err, "bad_params");
        }}
        return state_emit(result_obj, game_state_to_json(&g_game_state), err);
    }}

    const cJSON *key = cJSON_GetObjectItemCaseSensitive(params, "key");
    if (!cJSON_IsString(key)) {{
        return state_fail(err, "bad_params", "key is required");
    }}
    char *data = NULL;
    if (!game_storage_load_json(key->valuestring, GAME_STATE_DOCUMENT, &data, s_state_err, (int)sizeof(s_state_err))) {{
        return state_fail_buf(err, "bad_params");
    }}
    bool ok = game_state_load_json_string(&g_game_state, data, s_state_err, (int)sizeof(s_state_err));
    free(data);
    if (!ok) {{
        return state_fail_buf(err, "bad_params");
    }}
    return state_emit(result_obj, game_state_to_json(&g_game_state), err);
}}

static bool ep_game_state_reset(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {{
    (void)params;
    (void)user;
    if (!check_doc(params, err)) {{
        return false;
    }}
    if (!game_state_reset(&g_game_state, s_state_err, (int)sizeof(s_state_err))) {{
        return state_fail_buf(err, "internal");
    }}
    return state_emit(result_obj, game_state_to_json(&g_game_state), err);
}}

void game_state_register_devapi(void) {{
    static const nt_devapi_command_desc descs[] = {{
        {{"game.state.schema", "game", "Return the game state JSON schema.", "doc?", "schema object", "immediate", "none"}},
        {{"game.state.get", "game", "Get a state value by path.", "doc?, path", "path, value", "immediate", "none"}},
        {{"game.state.set", "game", "Set a state value by path.", "doc?, path, value", "state object", "immediate", "mutates state"}},
        {{"game.state.patch", "game", "Patch multiple state values.", "doc?, values", "state object", "immediate", "mutates state"}},
        {{"game.state.save", "game", "Save state to key or unsafe_path.", "doc?, key|unsafe_path", "key, doc, resolved", "immediate", "writes file"}},
        {{"game.state.load", "game", "Load state from key or unsafe_path.", "doc?, key|unsafe_path", "state object", "immediate", "mutates state"}},
        {{"game.state.reset", "game", "Reset state to defaults.", "doc?", "state object", "immediate", "mutates state"}},
    }};
    const nt_devapi_handler_fn fns[] = {{
        ep_game_state_schema, ep_game_state_get, ep_game_state_set, ep_game_state_patch,
        ep_game_state_save, ep_game_state_load, ep_game_state_reset,
    }};
    for (size_t i = 0; i < sizeof(fns) / sizeof(fns[0]); ++i) {{
        (void)nt_devapi_register(&descs[i], fns[i], NULL);
    }}
}}

#endif
"""


# Per-field code generation. Scalar fields (bool/int/float/string/enum,
# including dotted paths like settings.master_volume) are fully generated
# from the schema via /*@GEN:...@*/ markers in game_state.c.in. Only the
# structural patterns stay hand-written in the template: the items map,
# the inventory id list, and ref-checked optional strings (string?).

GENERATED_SCALAR_TYPES = {"bool", "int", "float", "string", "enum"}


def generated_fields(schema: dict[str, Any]) -> list[dict[str, Any]]:
    return [f for f in schema["fields"] if f["type"] in GENERATED_SCALAR_TYPES]


def path_group_key(path: str) -> tuple[str | None, str]:
    if "." in path:
        group, key = path.split(".", 1)
        return group, key
    return None, path


def enum_alias(field: dict[str, Any]) -> str | None:
    path = field["path"]
    return path[: -len("_index")] if path.endswith("_index") else None


def enum_table(field: dict[str, Any]) -> tuple[str, str]:
    """Return (names_table, count_macro) for an enum field."""
    enum_name = field["enum"]
    return f"k_{c_ident(enum_name)}_names", f"GAME_STATE_{c_macro(enum_name)}_COUNT"


def render_enum_name_decls(schema: dict[str, Any]) -> str:
    return "\n".join(
        f"const char *game_state_{c_ident(name)}_name(int value);" for name in schema["enums"]
    )


def render_enum_tables(schema: dict[str, Any]) -> str:
    blocks: list[str] = []
    for name, values in schema["enums"].items():
        ename = c_ident(name)
        lines = [f"static const char *const k_{ename}_names[GAME_STATE_{c_macro(name)}_COUNT] = {{"]
        lines.extend(f'    "{value}",' for value in values)
        lines.append("};")
        blocks.append("\n".join(lines))
    for name in schema["enums"]:
        ename = c_ident(name)
        blocks.append(
            f"const char *game_state_{ename}_name(int value) {{\n"
            f"    return (value >= 0 && value < GAME_STATE_{c_macro(name)}_COUNT) ? k_{ename}_names[value] : \"unknown\";\n"
            f"}}"
        )
    return "\n\n".join(blocks)


def render_defaults(schema: dict[str, Any]) -> str:
    lines: list[str] = []
    for field in generated_fields(schema):
        ident = c_ident(field["path"])
        macro = f"GAME_STATE_{c_macro(field['path'])}"
        if field["type"] == "string":
            lines.append(f"    (void)copy_text(state->{ident}, sizeof(state->{ident}), {macro}_DEFAULT);")
        else:
            lines.append(f"    state->{ident} = {macro}_DEFAULT;")
    return "\n".join(lines)


def render_validate(schema: dict[str, Any]) -> str:
    lines: list[str] = []
    for field in generated_fields(schema):
        path = field["path"]
        ident = c_ident(path)
        macro = f"GAME_STATE_{c_macro(path)}"
        if field["type"] == "enum":
            _, count_macro = enum_table(field)
            condition = f"state->{ident} < 0 || state->{ident} >= {count_macro}"
        elif field["type"] in {"int", "float"}:
            condition = f"state->{ident} < {macro}_MIN || state->{ident} > {macro}_MAX"
        else:
            continue
        lines.append(f"    if ({condition}) {{")
        lines.append(f'        set_error(error, error_cap, "{path} out of range");')
        lines.append("        return false;")
        lines.append("    }")
    return "\n".join(lines)


def render_to_json(schema: dict[str, Any]) -> str:
    lines: list[str] = []
    emitted_groups: set[str] = set()
    for field in generated_fields(schema):
        path = field["path"]
        ident = c_ident(path)
        group, key = path_group_key(path)
        target = "root"
        if group:
            if group not in emitted_groups:
                emitted_groups.add(group)
                lines.append("")
                lines.append(f'    cJSON *{group} = cJSON_AddObjectToObject(root, "{group}");')
            target = group
        if field["type"] == "enum":
            alias = enum_alias(field)
            lines.append(f'    cJSON_AddNumberToObject({target}, "{key}", state->{ident});')
            if alias:
                ename = c_ident(field["enum"])
                lines.append(f'    cJSON_AddStringToObject({target}, "{alias}", game_state_{ename}_name(state->{ident}));')
        elif field["type"] == "int":
            lines.append(f'    cJSON_AddNumberToObject({target}, "{key}", state->{ident});')
        elif field["type"] == "float":
            lines.append(f'    cJSON_AddNumberToObject({target}, "{key}", (double)state->{ident});')
        elif field["type"] == "bool":
            lines.append(f'    cJSON_AddBoolToObject({target}, "{key}", state->{ident});')
        elif field["type"] == "string":
            lines.append(f'    cJSON_AddStringToObject({target}, "{key}", state->{ident});')
    return "\n".join(lines)


def render_get_path(schema: dict[str, Any]) -> str:
    lines: list[str] = []

    def emit(path: str, expr: str) -> None:
        lines.append(f'    if (strcmp(path, "{path}") == 0) {{')
        lines.append(f"        return {expr};")
        lines.append("    }")

    for field in generated_fields(schema):
        path = field["path"]
        ident = c_ident(path)
        if field["type"] == "enum":
            emit(path, f"cJSON_CreateNumber(state->{ident})")
            alias = enum_alias(field)
            if alias:
                ename = c_ident(field["enum"])
                emit(alias, f"cJSON_CreateString(game_state_{ename}_name(state->{ident}))")
        elif field["type"] == "int":
            emit(path, f"cJSON_CreateNumber(state->{ident})")
        elif field["type"] == "float":
            emit(path, f"cJSON_CreateNumber((double)state->{ident})")
        elif field["type"] == "bool":
            emit(path, f"cJSON_CreateBool(state->{ident})")
        elif field["type"] == "string":
            emit(path, f"cJSON_CreateString(state->{ident})")
    return "\n".join(lines)


def render_set_path(schema: dict[str, Any]) -> str:
    chunks: list[str] = []
    for field in generated_fields(schema):
        path = field["path"]
        ident = c_ident(path)
        macro = f"GAME_STATE_{c_macro(path)}"
        typ = field["type"]
        if typ == "enum":
            names_table, count_macro = enum_table(field)
            chunks.append(
                f'    if (strcmp(path, "{path}") == 0) {{\n'
                f"        if (!parse_enum_value(value, {names_table}, {count_macro}, &state->{ident}, error, error_cap)) {{\n"
                f"            return false;\n"
                f"        }}\n"
                f"        return true;\n"
                f"    }}"
            )
            alias = enum_alias(field)
            if alias:
                chunks.append(
                    f'    if (strcmp(path, "{alias}") == 0) {{\n'
                    f"        if (!cJSON_IsString(value)) {{\n"
                    f'            set_error(error, error_cap, "{alias} expects string");\n'
                    f"            return false;\n"
                    f"        }}\n"
                    f"        int index = enum_index(value->valuestring, {names_table}, {count_macro});\n"
                    f"        if (index < 0) {{\n"
                    f'            set_error(error, error_cap, "unknown {alias}");\n'
                    f"            return false;\n"
                    f"        }}\n"
                    f"        state->{ident} = index;\n"
                    f"        return true;\n"
                    f"    }}"
                )
        elif typ == "int":
            chunks.append(
                f'    if (strcmp(path, "{path}") == 0) {{\n'
                f"        int parsed = 0;\n"
                f"        if (!parse_int_value(value, {macro}_MIN, {macro}_MAX, &parsed, error, error_cap)) {{\n"
                f"            return false;\n"
                f"        }}\n"
                f"        state->{ident} = parsed;\n"
                f"        return true;\n"
                f"    }}"
            )
        elif typ == "float":
            chunks.append(
                f'    if (strcmp(path, "{path}") == 0) {{\n'
                f"        if (!cJSON_IsNumber(value)) {{\n"
                f'            set_error(error, error_cap, "{path} expects number");\n'
                f"            return false;\n"
                f"        }}\n"
                f"        float parsed = (float)value->valuedouble;\n"
                f"        if (parsed < {macro}_MIN || parsed > {macro}_MAX) {{\n"
                f'            set_error(error, error_cap, "{path} out of range");\n'
                f"            return false;\n"
                f"        }}\n"
                f"        state->{ident} = parsed;\n"
                f"        return true;\n"
                f"    }}"
            )
        elif typ == "bool":
            chunks.append(
                f'    if (strcmp(path, "{path}") == 0) {{\n'
                f"        if (!cJSON_IsBool(value)) {{\n"
                f'            set_error(error, error_cap, "{path} expects bool");\n'
                f"            return false;\n"
                f"        }}\n"
                f"        state->{ident} = cJSON_IsTrue(value);\n"
                f"        return true;\n"
                f"    }}"
            )
        elif typ == "string":
            chunks.append(
                f'    if (strcmp(path, "{path}") == 0) {{\n'
                f"        if (!cJSON_IsString(value) || !copy_text(state->{ident}, sizeof(state->{ident}), value->valuestring)) {{\n"
                f'            set_error(error, error_cap, "{path} expects short string");\n'
                f"            return false;\n"
                f"        }}\n"
                f"        return true;\n"
                f"    }}"
            )
    return "\n".join(chunks)


def render_from_json(schema: dict[str, Any]) -> str:
    lines: list[str] = []
    emitted_groups: set[str] = set()

    def read_call(field: dict[str, Any], source: str, key: str) -> list[str]:
        ident = c_ident(field["path"])
        macro = f"GAME_STATE_{c_macro(field['path'])}"
        typ = field["type"]
        if typ == "enum":
            names_table, count_macro = enum_table(field)
            calls = [f'read_enum({source}, "{key}", {names_table}, {count_macro}, &next.{ident}, error, error_cap)']
            alias = enum_alias(field)
            if alias and source == "json":
                calls.append(f'read_enum({source}, "{alias}", {names_table}, {count_macro}, &next.{ident}, error, error_cap)')
            return calls
        if typ == "int":
            return [f'read_int_range({source}, "{key}", {macro}_MIN, {macro}_MAX, &next.{ident}, error, error_cap)']
        if typ == "float":
            return [f'read_float_range({source}, "{key}", {macro}_MIN, {macro}_MAX, &next.{ident}, error, error_cap)']
        if typ == "bool":
            return [f'read_bool({source}, "{key}", &next.{ident}, error, error_cap)']
        if typ == "string":
            return [f'read_string({source}, "{key}", next.{ident}, sizeof(next.{ident}), error, error_cap)']
        raise AssertionError(typ)

    for field in generated_fields(schema):
        group, key = path_group_key(field["path"])
        if group:
            if group not in emitted_groups:
                emitted_groups.add(group)
                lines.append(f'    const cJSON *{group} = object_item(json, "{group}");')
            for call in read_call(field, group, key):
                lines.append(f"    if ({group} && !{call}) {{")
                lines.append("        return false;")
                lines.append("    }")
        else:
            for call in read_call(field, "json", key):
                lines.append(f"    if (!{call}) {{")
                lines.append("        return false;")
                lines.append("    }")
    return "\n".join(lines)


SOURCE_MARKERS = {
    "/*@GEN:ENUM_TABLES@*/": render_enum_tables,
    "/*@GEN:DEFAULTS@*/": render_defaults,
    "/*@GEN:VALIDATE@*/": render_validate,
    "/*@GEN:TO_JSON@*/": render_to_json,
    "/*@GEN:GET_PATH@*/": render_get_path,
    "/*@GEN:SET_PATH@*/": render_set_path,
    "/*@GEN:FROM_JSON@*/": render_from_json,
}


def render_source(schema: dict[str, Any], schema_label: str = "state/game_state.schema.json") -> str:
    if not SOURCE_TEMPLATE_PATH.exists():
        raise SystemExit(f"missing source template: {SOURCE_TEMPLATE_PATH.relative_to(ROOT)}")
    text = SOURCE_TEMPLATE_PATH.read_text(encoding="utf-8")
    if "Generated by tools/state_codegen/generate_state.py" not in text:
        text = text.replace(
            '#include "game_state_schema.gen.h"\n',
            '#include "game_state_schema.gen.h"\n\n'
            f'/* Generated by tools/state_codegen/generate_state.py from {schema_label}. */\n',
            1,
        )
    if "GAME_STATE_SCHEMA_ID" not in text or "game_state_from_json" not in text:
        raise SystemExit("source template does not look like a game state implementation")
    for marker, render in SOURCE_MARKERS.items():
        if marker not in text:
            raise SystemExit(f"source template missing marker: {marker}")
        text = text.replace(marker, render(schema))
    for field in schema["fields"]:
        path = field["path"]
        if path not in text and c_ident(path) not in text:
            raise SystemExit(f"generated source does not cover schema field: {path}")
    return text


def write_if_changed(path: Path, text: str) -> bool:
    old = path.read_text(encoding="utf-8") if path.exists() else None
    if old == text:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return True


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--schema", default=str(SCHEMA_PATH), help="Schema JSON to generate from.")
    parser.add_argument("--out-dir", default=str(OUT_DIR), help="Directory for generated game_state files.")
    args = parser.parse_args(argv)

    schema_path = Path(args.schema).resolve()
    out_dir = Path(args.out_dir).resolve()
    header_path = out_dir / "game_state.h"
    source_path = out_dir / "game_state.c"
    devapi_source_path = out_dir / "game_state_devapi.c"
    schema_header_path = out_dir / "game_state_schema.gen.h"
    schema_label = relative_label(schema_path)

    schema = load_schema(schema_path)
    changed = []
    if write_if_changed(header_path, render_header(schema, schema_label)):
        changed.append(header_path)
    if write_if_changed(source_path, render_source(schema, schema_label)):
        changed.append(source_path)
    if write_if_changed(schema_header_path, render_schema_header(schema, schema_label)):
        changed.append(schema_header_path)
    if write_if_changed(devapi_source_path, render_devapi_source(schema, schema_label)):
        changed.append(devapi_source_path)
    if changed:
        for path in changed:
            print(f"generated {relative_label(path)}")
    else:
        print("state generated files are up to date")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
