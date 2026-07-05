#!/usr/bin/env python3
"""Generate the supported game state C API from a state schema JSON file."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[3]
TOOL_LABEL = "features/game-state/scripts/generate_state.py"


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

/* Generated by {TOOL_LABEL} from {schema_label}. */

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
        f"/* Generated by {TOOL_LABEL} from {schema_label}. */\n"
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

/* Generated by {TOOL_LABEL} from {schema_label}. */

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
    if "Generated by " not in text:
        text = text.replace(
            '#include "game_state_schema.gen.h"\n',
            '#include "game_state_schema.gen.h"\n\n'
            f'/* Generated by {TOOL_LABEL} from {schema_label}. */\n',
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


# Generic schema-driven generator overrides. The earlier generator path is kept
# above for readability/history; these definitions are intentionally later in the
# module so main() uses the generic implementation.
def map_type_name(type_text: str) -> str | None:
    m = re.fullmatch(r"map<string,([A-Za-z_][A-Za-z0-9_]*)>", type_text)
    return m.group(1) if m else None


def is_map_type(type_text: str) -> bool:
    return map_type_name(type_text) is not None


def is_list_type(type_text: str) -> bool:
    return type_text == "list<string>"


def collection_macro(path: str) -> str:
    return f"GAME_STATE_MAX_{c_macro(path)}"


def object_type_c_name(type_name: str) -> str:
    return f"Game{type_name}"


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


def load_schema(schema_path: Path = SCHEMA_PATH) -> dict[str, Any]:
    with schema_path.open("r", encoding="utf-8") as f:
        schema = json.load(f)
    if not isinstance(schema.get("schema"), str) or not schema["schema"]:
        raise SystemExit("schema id must be a non-empty string")
    if not isinstance(schema.get("document"), str) or not schema["document"]:
        raise SystemExit("document must be a non-empty string")
    if not isinstance(schema.get("version"), int) or schema["version"] <= 0:
        raise SystemExit("version must be a positive integer")
    if not isinstance(schema.get("string_max"), int) or schema["string_max"] < 2:
        raise SystemExit("string_max must be an integer >= 2")
    if not isinstance(schema.get("fields"), list):
        raise SystemExit("fields must be a list")
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

    types = schema_types(schema)
    if not isinstance(types, dict):
        raise SystemExit("types must be an object")
    for type_name, type_def in types.items():
        if not isinstance(type_name, str) or not type_name or not c_ident(type_name):
            raise SystemExit("type names must be non-empty strings")
        if not isinstance(type_def, dict) or type_def.get("kind") != "object":
            raise SystemExit(f"types.{type_name} must be an object type")
        type_fields = type_def.get("fields")
        if not isinstance(type_fields, list):
            raise SystemExit(f"types.{type_name}.fields must be a list")
        validate_field_ids(f"types.{type_name}", type_fields, set(), set())
        for field in type_fields:
            validate_field_shape(schema, field, enums, types, allow_collections=False)

    validate_field_ids("fields", schema["fields"], reserved_ids, reserved_paths)
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
        elif typ == "string" and "default" in field:
            lines.append(f"#define GAME_STATE_{name}_DEFAULT {c_string(field['default'])}")
        elif typ == "string?" and isinstance(field.get("default"), str):
            lines.append(f"#define GAME_STATE_{name}_DEFAULT {c_string(field['default'])}")
        elif typ == "bool":
            lines.append(f"#define GAME_STATE_{name}_DEFAULT {1 if field['default'] else 0}")

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
        return [f"{indent}char {name}[GAME_STATE_STRING_MAX];"]
    if typ == "string?":
        return [f"{indent}bool has_{name};", f"{indent}char {name}[GAME_STATE_STRING_MAX];"]
    return [f"{indent}{field_c_type(field)} {name};"]


def render_object_structs(schema: dict[str, Any]) -> str:
    blocks: list[str] = []
    for type_name, type_def in schema_types(schema).items():
        lines = [f"typedef struct {object_type_c_name(type_name)} {{"]
        lines.append("    bool used;")
        lines.append("    char key[GAME_STATE_STRING_MAX];")
        for field in type_def["fields"]:
            lines.extend(render_struct_scalar_field(field))
        lines.append(f"}} {object_type_c_name(type_name)};")
        blocks.append("\n".join(lines))
    return "\n\n".join(blocks)


def render_state_struct(schema: dict[str, Any]) -> str:
    lines = ["typedef struct GameState {"]
    for field in schema["fields"]:
        typ = field["type"]
        name = c_ident(field["path"])
        if typ in SCALAR_TYPES:
            lines.extend(render_struct_scalar_field(field))
        elif is_list_type(typ):
            lines.append(f"    char {name}[{collection_macro(field['path'])}][GAME_STATE_STRING_MAX];")
            lines.append(f"    int {name}_count;")
        elif (type_name := map_type_name(typ)):
            lines.append(f"    {object_type_c_name(type_name)} {name}[{collection_macro(field['path'])}];")
    lines.append("} GameState;")
    return "\n".join(lines)


def render_header(schema: dict[str, Any], schema_label: str = "state/game_state.schema.json") -> str:
    enums = schema["enums"]
    enum_blocks = "\n\n".join(render_enum(name, values) for name, values in enums.items())
    return f"""#ifndef GAME_STATE_GENERATED_H
#define GAME_STATE_GENERATED_H

/* Generated by {TOOL_LABEL} from {schema_label}. */

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"

#define GAME_STATE_SCHEMA_ID "{schema["schema"]}"
#define GAME_STATE_DOCUMENT "{schema["document"]}"
#define GAME_STATE_VERSION {schema["version"]}
#define GAME_STATE_STRING_MAX {schema["string_max"]}

{render_state_constants(schema)}

{enum_blocks}

{render_object_structs(schema)}

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


def render_scalar_default_assignment(field: dict[str, Any], target: str, prefix: str = "") -> list[str]:
    ident = c_ident(field["path"])
    macro = f"GAME_STATE_{c_macro(prefix + field['path'])}"
    typ = field["type"]
    if typ == "string":
        if "default" in field:
            return [f"    (void)copy_text({target}->{ident}, sizeof({target}->{ident}), {macro}_DEFAULT);"]
        return []
    if typ == "string?":
        if isinstance(field.get("default"), str):
            return [
                f"    {target}->has_{ident} = true;",
                f"    (void)copy_text({target}->{ident}, sizeof({target}->{ident}), {macro}_DEFAULT);",
            ]
        return [f"    {target}->has_{ident} = false;"]
    return [f"    {target}->{ident} = {macro}_DEFAULT;"]


def render_scalar_validation(field: dict[str, Any], target: str, path: str, prefix: str = "") -> list[str]:
    ident = c_ident(field["path"])
    macro = f"GAME_STATE_{c_macro(prefix + field['path'])}"
    typ = field["type"]
    if typ == "enum":
        _, count_macro = enum_table(field)
        condition = f"{target}->{ident} < 0 || {target}->{ident} >= {count_macro}"
    elif typ in {"int", "float"}:
        condition = f"{target}->{ident} < {macro}_MIN || {target}->{ident} > {macro}_MAX"
    elif typ == "string":
        condition = f"{target}->{ident}[0] == '\\0'"
    else:
        return []
    return [
        f"    if ({condition}) {{",
        f'        set_error(error, error_cap, "{path} out of range");',
        "        return false;",
        "    }",
    ]


def render_object_helpers(schema: dict[str, Any]) -> str:
    blocks: list[str] = []
    for type_name, type_def in schema_types(schema).items():
        fname = object_type_func_name(type_name)
        cname = object_type_c_name(type_name)
        default_lines = [
            f"static void {fname}_init_defaults({cname} *obj, const char *key) {{",
            "    memset(obj, 0, sizeof(*obj));",
            "    obj->used = true;",
            "    (void)copy_text(obj->key, sizeof(obj->key), key);",
        ]
        for field in type_def["fields"]:
            default_lines.extend(render_scalar_default_assignment(field, "obj", f"{type_name}."))
        default_lines.append("}")

        validate_lines = [f"static bool {fname}_validate(const {cname} *obj, char *error, int error_cap) {{"]
        validate_lines.append("    if (!obj->used) { return true; }")
        validate_lines.append("    if (obj->key[0] == '\\0') { set_error(error, error_cap, \"object key is empty\"); return false; }")
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
        get_lines.append("    set_error(error, error_cap, \"unknown object field\");")
        get_lines.append("    return NULL;")
        get_lines.append("}")

        set_lines = [f"static bool {fname}_set_field_json({cname} *obj, const char *field, const cJSON *value, char *error, int error_cap) {{"]
        for field in type_def["fields"]:
            set_lines.extend(render_set_scalar_if(field, "obj", field["path"], f"{type_name}."))
        set_lines.append("    set_error(error, error_cap, \"unknown object field\");")
        set_lines.append("    return false;")
        set_lines.append("}")

        from_lines = [f"static bool {fname}_from_json({cname} *obj, const cJSON *json, char *error, int error_cap) {{"]
        from_lines.append("    if (!cJSON_IsObject(json)) { set_error(error, error_cap, \"object must be json object\"); return false; }")
        for field in type_def["fields"]:
            from_lines.extend(render_read_scalar(field, "json", "obj", field["path"], f"{type_name}."))
        from_lines.append(f"    return {fname}_validate(obj, error, error_cap);")
        from_lines.append("}")

        blocks.append("\n".join(default_lines + [""] + validate_lines + [""] + to_json_lines + [""] + get_lines + [""] + set_lines + [""] + from_lines))
    return "\n\n".join(blocks)


def render_cjson_add_scalar(field: dict[str, Any], target: str, state_expr: str, key: str) -> list[str]:
    ident = c_ident(field["path"])
    typ = field["type"]
    if typ == "enum":
        alias = enum_alias(field)
        ename = c_ident(field["enum"])
        if alias:
            return [
                f'    cJSON_AddNumberToObject({target}, "{key}", {state_expr}->{ident});',
                f'    cJSON_AddStringToObject({target}, "{alias}", game_state_{ename}_name({state_expr}->{ident}));',
            ]
        return [f'    cJSON_AddStringToObject({target}, "{key}", game_state_{ename}_name({state_expr}->{ident}));']
    if typ == "int":
        return [f'    cJSON_AddNumberToObject({target}, "{key}", {state_expr}->{ident});']
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
        return f"cJSON_CreateString(game_state_{ename}_name({state_expr}->{ident}))"
    if typ == "int":
        return f"cJSON_CreateNumber({state_expr}->{ident})"
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
            f"        return cJSON_CreateString(game_state_{ename}_name({state_expr}->{c_ident(field['path'])}));",
            "    }",
        ])
    return lines


def render_set_scalar_if(field: dict[str, Any], state_expr: str, path: str, prefix: str = "", compare_var: str = "field") -> list[str]:
    ident = c_ident(field["path"])
    macro = f"GAME_STATE_{c_macro(prefix + field['path'])}"
    typ = field["type"]
    head = [f'    if (strcmp({compare_var}, "{path}") == 0) {{']
    if typ == "enum":
        names_table, count_macro = enum_table(field)
        body = [
            f"        if (!parse_enum_value(value, {names_table}, {count_macro}, &{state_expr}->{ident}, error, error_cap)) {{ return false; }}",
            "        return true;",
        ]
    elif typ == "int":
        body = [
            "        int parsed = 0;",
            f"        if (!parse_int_value(value, {macro}_MIN, {macro}_MAX, &parsed, error, error_cap)) {{ return false; }}",
            f"        {state_expr}->{ident} = parsed;",
            "        return true;",
        ]
    elif typ == "float":
        body = [
            "        if (!cJSON_IsNumber(value)) { set_error(error, error_cap, \"expected number\"); return false; }",
            "        float parsed = (float)value->valuedouble;",
            f"        if (parsed < {macro}_MIN || parsed > {macro}_MAX) {{ set_error(error, error_cap, \"number out of range\"); return false; }}",
            f"        {state_expr}->{ident} = parsed;",
            "        return true;",
        ]
    elif typ == "bool":
        body = [
            "        if (!cJSON_IsBool(value)) { set_error(error, error_cap, \"expected bool\"); return false; }",
            f"        {state_expr}->{ident} = cJSON_IsTrue(value);",
            "        return true;",
        ]
    elif typ == "string":
        body = [
            f"        if (!cJSON_IsString(value) || !copy_text({state_expr}->{ident}, sizeof({state_expr}->{ident}), value->valuestring)) {{ set_error(error, error_cap, \"expected short string\"); return false; }}",
            "        return true;",
        ]
    elif typ == "string?":
        body = [
            "        if (cJSON_IsNull(value)) {",
            f"            {state_expr}->has_{ident} = false;",
            f"            {state_expr}->{ident}[0] = '\\0';",
            "            return true;",
            "        }",
            f"        if (!cJSON_IsString(value) || !copy_text({state_expr}->{ident}, sizeof({state_expr}->{ident}), value->valuestring)) {{ set_error(error, error_cap, \"expected short string or null\"); return false; }}",
            f"        {state_expr}->has_{ident} = true;",
            "        return true;",
        ]
    else:
        raise AssertionError(typ)
    return head + body + ["    }"]


def render_read_scalar(field: dict[str, Any], source: str, target: str, key: str, prefix: str = "") -> list[str]:
    ident = c_ident(field["path"])
    macro = f"GAME_STATE_{c_macro(prefix + field['path'])}"
    typ = field["type"]
    if typ == "enum":
        names_table, count_macro = enum_table(field)
        call = f'read_enum({source}, "{key}", {names_table}, {count_macro}, &{target}->{ident}, error, error_cap)'
    elif typ == "int":
        call = f'read_int_range({source}, "{key}", {macro}_MIN, {macro}_MAX, &{target}->{ident}, error, error_cap)'
    elif typ == "float":
        call = f'read_float_range({source}, "{key}", {macro}_MIN, {macro}_MAX, &{target}->{ident}, error, error_cap)'
    elif typ == "bool":
        call = f'read_bool({source}, "{key}", &{target}->{ident}, error, error_cap)'
    elif typ == "string":
        call = f'read_string({source}, "{key}", {target}->{ident}, sizeof({target}->{ident}), error, error_cap)'
    elif typ == "string?":
        return [
            f'    const cJSON *{c_ident(source + "_" + key)} = object_item({source}, "{key}");',
            f"    if ({c_ident(source + '_' + key)}) {{",
            f"        if (cJSON_IsNull({c_ident(source + '_' + key)})) {{",
            f"            {target}->has_{ident} = false;",
            f"        }} else if (cJSON_IsString({c_ident(source + '_' + key)}) && copy_text({target}->{ident}, sizeof({target}->{ident}), {c_ident(source + '_' + key)}->valuestring)) {{",
            f"            {target}->has_{ident} = true;",
            "        } else {",
            "            set_error(error, error_cap, \"expected short string or null\");",
            "            return false;",
            "        }",
            "    }",
        ]
    else:
        raise AssertionError(typ)
    return [f"    if (!{call}) {{ return false; }}"]


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
static {cname} *find_{ident}(GameState *state, const char *key) {{
    for (int i = 0; i < {max_macro}; i++) {{
        if (state->{ident}[i].used && strcmp(state->{ident}[i].key, key) == 0) {{
            return &state->{ident}[i];
        }}
    }}
    return NULL;
}}

static const {cname} *find_{ident}_const(const GameState *state, const char *key) {{
    for (int i = 0; i < {max_macro}; i++) {{
        if (state->{ident}[i].used && strcmp(state->{ident}[i].key, key) == 0) {{
            return &state->{ident}[i];
        }}
    }}
    return NULL;
}}

static {cname} *alloc_{ident}(GameState *state, const char *key) {{
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

static cJSON *{ident}_to_json(const GameState *state) {{
    cJSON *json = cJSON_CreateObject();
    for (int i = 0; i < {max_macro}; i++) {{
        if (state->{ident}[i].used) {{
            cJSON_AddItemToObject(json, state->{ident}[i].key, {fname}_to_json(&state->{ident}[i]));
        }}
    }}
    return json;
}}

static bool set_{ident}_from_json(GameState *state, const cJSON *json, char *error, int error_cap) {{
    if (!cJSON_IsObject(json)) {{ set_error(error, error_cap, \"map must be object\"); return false; }}
    for (int i = 0; i < {max_macro}; i++) {{ memset(&state->{ident}[i], 0, sizeof(state->{ident}[i])); }}
    const cJSON *child = NULL;
    cJSON_ArrayForEach(child, json) {{
        if (!child->string) {{ continue; }}
        {cname} *obj = alloc_{ident}(state, child->string);
        if (!obj) {{ set_error(error, error_cap, \"too many map entries or long key\"); return false; }}
        if (!{fname}_from_json(obj, child, error, error_cap)) {{ return false; }}
    }}
    return true;
}}
""")
    for field in list_fields(schema):
        ident = c_ident(field["path"])
        max_macro = collection_macro(field["path"])
        blocks.append(f"""
static cJSON *{ident}_to_json(const GameState *state) {{
    cJSON *json = cJSON_CreateArray();
    for (int i = 0; i < state->{ident}_count; i++) {{
        cJSON_AddItemToArray(json, cJSON_CreateString(state->{ident}[i]));
    }}
    return json;
}}

static bool set_{ident}_from_json(GameState *state, const cJSON *json, char *error, int error_cap) {{
    if (!cJSON_IsArray(json)) {{ set_error(error, error_cap, \"list must be array\"); return false; }}
    int count = cJSON_GetArraySize((cJSON *)json);
    if (count > {max_macro}) {{ set_error(error, error_cap, \"too many list entries\"); return false; }}
    state->{ident}_count = 0;
    for (int i = 0; i < count; i++) {{
        const cJSON *entry = cJSON_GetArrayItem((cJSON *)json, i);
        if (!cJSON_IsString(entry) || !copy_text(state->{ident}[i], sizeof(state->{ident}[i]), entry->valuestring)) {{
            set_error(error, error_cap, \"list entry must be short string\");
            return false;
        }}
        state->{ident}_count++;
    }}
    return true;
}}
""")
    return "\n\n".join(blocks)


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
            lines.append(f"    (void)copy_text(state->{ident}[{index}], sizeof(state->{ident}[{index}]), {c_string(value)});")
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
            f'        set_error(error, error_cap, "{field["path"]} count out of range");',
            "        return false;",
            "    }",
            f"    for (int i = 0; i < state->{ident}_count; i++) {{",
            f"        if (state->{ident}[i][0] == '\\0') {{",
            f'            set_error(error, error_cap, "{field["path"]} contains empty id");',
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
            "        char map_key[GAME_STATE_STRING_MAX];",
            "        if (field) {",
            "            size_t len = (size_t)(field - key);",
            "            if (len == 0 || len >= sizeof(map_key)) { set_error(error, error_cap, \"bad map key\"); return NULL; }",
            "            memcpy(map_key, key, len);",
            "            map_key[len] = '\\0';",
            "            field++;",
            "        } else if (!copy_text(map_key, sizeof(map_key), key)) {",
            "            set_error(error, error_cap, \"bad map key\");",
            "            return NULL;",
            "        }",
            f"        const {object_type_c_name(type_name)} *obj = find_{ident}_const(state, map_key);",
            "        if (!obj) { set_error(error, error_cap, \"unknown map key\"); return NULL; }",
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
            "        char map_key[GAME_STATE_STRING_MAX];",
            "        if (field) {",
            "            size_t len = (size_t)(field - key);",
            "            if (len == 0 || len >= sizeof(map_key)) { set_error(error, error_cap, \"bad map key\"); return false; }",
            "            memcpy(map_key, key, len);",
            "            map_key[len] = '\\0';",
            "            field++;",
            "        } else if (!copy_text(map_key, sizeof(map_key), key)) {",
            "            set_error(error, error_cap, \"bad map key\");",
            "            return false;",
            "        }",
            f"        {object_type_c_name(type_name)} *obj = alloc_{ident}(state, map_key);",
            "        if (!obj) { set_error(error, error_cap, \"too many map entries or long key\"); return false; }",
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
            lines.append(f'    const cJSON *{parent} = object_item(json, "{group}");')
        source = parent if group else "json"
        if field["type"] in SCALAR_TYPES:
            lines.extend(render_read_scalar(field, source, "(&next)", key))
        elif is_list_type(field["type"]) or is_map_type(field["type"]):
            item_var = c_ident(f"{field['path']}_json")
            lines.extend([
                f'    const cJSON *{item_var} = object_item({source}, "{key}");',
                f"    if ({item_var} && !set_{c_ident(field['path'])}_from_json(&next, {item_var}, error, error_cap)) {{",
                "        return false;",
                "    }",
            ])
    return "\n".join(lines)


def render_generic_source(schema: dict[str, Any], schema_label: str) -> str:
    return f"""#include "game_state.h"
#include "game_state_schema.gen.h"

/* Generated by {TOOL_LABEL} from {schema_label}. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#define GAME_STATE_MAX_FILE_BYTES (1024 * 1024)
#define GAME_STATE_PATH_MAX 512

#if defined(__GNUC__) || defined(__clang__)
#define GAME_STATE_MAYBE_UNUSED __attribute__((unused))
#else
#define GAME_STATE_MAYBE_UNUSED
#endif

{render_enum_tables(schema)}

GameState g_game_state;

static game_state_changed_fn s_changed;
static void *s_changed_user;
static bool s_dirty;

static void set_error(char *error, int error_cap, const char *message) {{
    if (error && error_cap > 0) {{
        (void)snprintf(error, (size_t)error_cap, "%s", message);
    }}
}}

static bool copy_text(char *dst, size_t dst_cap, const char *src) {{
    if (!dst || dst_cap == 0 || !src || strlen(src) >= dst_cap) {{
        return false;
    }}
    (void)snprintf(dst, dst_cap, "%s", src);
    return true;
}}

static const cJSON *object_item(const cJSON *obj, const char *name) {{
    return cJSON_IsObject(obj) ? cJSON_GetObjectItemCaseSensitive(obj, name) : NULL;
}}

static GAME_STATE_MAYBE_UNUSED bool read_bool(const cJSON *obj, const char *name, bool *out, char *error, int error_cap) {{
    const cJSON *item = object_item(obj, name);
    if (!item) {{ return true; }}
    if (!cJSON_IsBool(item)) {{ set_error(error, error_cap, "expected bool"); return false; }}
    *out = cJSON_IsTrue(item);
    return true;
}}

static GAME_STATE_MAYBE_UNUSED bool read_int_range(const cJSON *obj, const char *name, int min_value, int max_value, int *out, char *error, int error_cap) {{
    const cJSON *item = object_item(obj, name);
    if (!item) {{ return true; }}
    if (!cJSON_IsNumber(item)) {{ set_error(error, error_cap, "expected number"); return false; }}
    double number = item->valuedouble;
    if (number < (double)min_value || number > (double)max_value || number != (double)(int)number) {{
        set_error(error, error_cap, "number out of range");
        return false;
    }}
    *out = (int)number;
    return true;
}}

static GAME_STATE_MAYBE_UNUSED bool read_float_range(const cJSON *obj, const char *name, float min_value, float max_value, float *out, char *error, int error_cap) {{
    const cJSON *item = object_item(obj, name);
    if (!item) {{ return true; }}
    if (!cJSON_IsNumber(item)) {{ set_error(error, error_cap, "expected number"); return false; }}
    float value = (float)item->valuedouble;
    if (value < min_value || value > max_value) {{ set_error(error, error_cap, "number out of range"); return false; }}
    *out = value;
    return true;
}}

static GAME_STATE_MAYBE_UNUSED bool read_string(const cJSON *obj, const char *name, char *out, size_t out_cap, char *error, int error_cap) {{
    const cJSON *item = object_item(obj, name);
    if (!item) {{ return true; }}
    if (!cJSON_IsString(item) || !copy_text(out, out_cap, item->valuestring)) {{
        set_error(error, error_cap, "expected short string");
        return false;
    }}
    return true;
}}

static int enum_index(const char *value, const char *const *names, int count) {{
    if (!value) {{ return -1; }}
    for (int i = 0; i < count; i++) {{
        if (strcmp(value, names[i]) == 0) {{ return i; }}
    }}
    return -1;
}}

static GAME_STATE_MAYBE_UNUSED bool read_enum(const cJSON *obj, const char *name, const char *const *names, int count, int *out, char *error, int error_cap) {{
    const cJSON *item = object_item(obj, name);
    if (!item) {{ return true; }}
    int value = -1;
    if (cJSON_IsString(item)) {{
        value = enum_index(item->valuestring, names, count);
    }} else if (cJSON_IsNumber(item)) {{
        double number = item->valuedouble;
        if (number != (double)(int)number) {{ set_error(error, error_cap, "enum value must be an integer"); return false; }}
        value = (int)number;
    }} else {{
        set_error(error, error_cap, "expected enum string or number");
        return false;
    }}
    if (value < 0 || value >= count) {{ set_error(error, error_cap, "enum value out of range"); return false; }}
    *out = value;
    return true;
}}

static GAME_STATE_MAYBE_UNUSED bool parse_enum_value(const cJSON *item, const char *const *names, int count, int *out, char *error, int error_cap) {{
    int value = -1;
    if (cJSON_IsString(item)) {{
        value = enum_index(item->valuestring, names, count);
    }} else if (cJSON_IsNumber(item)) {{
        double number = item->valuedouble;
        if (number != (double)(int)number) {{ set_error(error, error_cap, "enum value must be an integer"); return false; }}
        value = (int)number;
    }} else {{
        set_error(error, error_cap, "expected enum string or number");
        return false;
    }}
    if (value < 0 || value >= count) {{ set_error(error, error_cap, "enum value out of range"); return false; }}
    *out = value;
    return true;
}}

static GAME_STATE_MAYBE_UNUSED bool parse_int_value(const cJSON *item, int min_value, int max_value, int *out, char *error, int error_cap) {{
    if (!cJSON_IsNumber(item)) {{ set_error(error, error_cap, "expected integer"); return false; }}
    double number = item->valuedouble;
    if (number < (double)min_value || number > (double)max_value || number != (double)(int)number) {{
        set_error(error, error_cap, "integer value out of range");
        return false;
    }}
    *out = (int)number;
    return true;
}}

static bool notify_changed(const char *path, char *error, int error_cap) {{
    if (!s_changed) {{ return true; }}
    return s_changed(path, s_changed_user, error, error_cap);
}}

{render_object_helpers(schema)}

{render_collection_helpers(schema)}

void game_state_init_defaults(GameState *state) {{
    memset(state, 0, sizeof(*state));
{render_defaults(schema)}
}}

void game_state_init(void) {{
    game_state_init_defaults(&g_game_state);
}}

void game_state_set_changed_callback(game_state_changed_fn callback, void *user) {{
    s_changed = callback;
    s_changed_user = user;
}}

void game_state_mark_dirty(void) {{ s_dirty = true; }}
bool game_state_is_dirty(void) {{ return s_dirty; }}
void game_state_clear_dirty(void) {{ s_dirty = false; }}

bool game_state_validate(const GameState *state, char *error, int error_cap) {{
    if (!state) {{ set_error(error, error_cap, "state is null"); return false; }}
{render_validate(schema)}
    return true;
}}

cJSON *game_state_schema_json(void) {{
    const char *chunks[] = GAME_STATE_SCHEMA_JSON_CHUNKS;
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

cJSON *game_state_to_json(const GameState *state) {{
    cJSON *root = cJSON_CreateObject();
{render_to_json(schema)}
    return root;
}}

cJSON *game_state_get_path_json(const GameState *state, const char *path, char *error, int error_cap) {{
    if (!path || path[0] == '\\0') {{ return game_state_to_json(state); }}
{render_get_path(schema)}
    set_error(error, error_cap, "unknown state path");
    return NULL;
}}

bool game_state_set_path_json(GameState *state, const char *path, const cJSON *value, char *error, int error_cap) {{
    if (!state || !path || !path[0] || !value) {{ set_error(error, error_cap, "path and value are required"); return false; }}
{render_set_path(schema)}
    set_error(error, error_cap, "unknown state path");
    return false;
}}

bool game_state_patch_json(GameState *state, const cJSON *values, char *error, int error_cap) {{
    if (!cJSON_IsObject(values)) {{ set_error(error, error_cap, "values must be an object"); return false; }}
    GameState next = *state;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, values) {{
        if (!item->string || !game_state_set_path_json(&next, item->string, item, error, error_cap)) {{ return false; }}
    }}
    if (!game_state_validate(&next, error, error_cap)) {{ return false; }}
    GameState old = *state;
    *state = next;
    if (!notify_changed("*", error, error_cap)) {{ *state = old; return false; }}
    if (state == &g_game_state) {{ game_state_mark_dirty(); }}
    return true;
}}

bool game_state_from_json(GameState *state, const cJSON *json, char *error, int error_cap) {{
    if (!cJSON_IsObject(json)) {{ set_error(error, error_cap, "state json must be object"); return false; }}
    GameState next;
    game_state_init_defaults(&next);
{render_from_json(schema)}
    if (!game_state_validate(&next, error, error_cap)) {{ return false; }}
    *state = next;
    return true;
}}

static cJSON *make_save_doc(const GameState *state) {{
    cJSON *doc = cJSON_CreateObject();
    cJSON_AddStringToObject(doc, "schema", GAME_STATE_SCHEMA_ID);
    cJSON_AddStringToObject(doc, "document", GAME_STATE_DOCUMENT);
    cJSON_AddNumberToObject(doc, "version", GAME_STATE_VERSION);
    cJSON_AddItemToObject(doc, "state", game_state_to_json(state));
    return doc;
}}

char *game_state_save_json_string(const GameState *state, char *error, int error_cap) {{
    if (!game_state_validate(state, error, error_cap)) {{ return NULL; }}
    cJSON *doc = make_save_doc(state);
    char *printed = cJSON_Print(doc);
    cJSON_Delete(doc);
    if (!printed) {{ set_error(error, error_cap, "failed to print state"); return NULL; }}
    return printed;
}}

static bool make_dir_if_needed(const char *path) {{
#ifdef _WIN32
    if (_mkdir(path) == 0) {{ return true; }}
    return errno == EEXIST;
#else
    if (mkdir(path, 0755) == 0) {{ return true; }}
    return errno == EEXIST;
#endif
}}

static bool ensure_parent_dirs(const char *path, char *error, int error_cap) {{
    char temp[GAME_STATE_PATH_MAX];
    if (!copy_text(temp, sizeof(temp), path)) {{ set_error(error, error_cap, "state path is too long"); return false; }}
    for (char *p = temp; *p; p++) {{
        if (*p != '/' && *p != '\\\\') {{ continue; }}
        if (p == temp || (*(p - 1) == ':')) {{ continue; }}
        char saved = *p;
        *p = '\\0';
        if (!make_dir_if_needed(temp)) {{ set_error(error, error_cap, "failed to create state directory"); return false; }}
        *p = saved;
    }}
    return true;
}}

static bool replace_file(const char *tmp_path, const char *path) {{
#ifdef _WIN32
    return MoveFileExA(tmp_path, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(tmp_path, path) == 0;
#endif
}}

bool game_state_save(const GameState *state, const char *path, char *error, int error_cap) {{
    if (!path || !path[0]) {{ set_error(error, error_cap, "path is required"); return false; }}
    if (!game_state_validate(state, error, error_cap)) {{ return false; }}
    if (!ensure_parent_dirs(path, error, error_cap)) {{ return false; }}
    char *printed = game_state_save_json_string(state, error, error_cap);
    if (!printed) {{ return false; }}
    char tmp_path[GAME_STATE_PATH_MAX];
    if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path) >= (int)sizeof(tmp_path)) {{
        cJSON_free(printed);
        set_error(error, error_cap, "state temp path is too long");
        return false;
    }}
    FILE *file = fopen(tmp_path, "wb");
    if (!file) {{ cJSON_free(printed); set_error(error, error_cap, "failed to open state file for write"); return false; }}
    size_t len = strlen(printed);
    bool ok = fwrite(printed, 1, len, file) == len;
    ok = fclose(file) == 0 && ok;
    cJSON_free(printed);
    if (!ok) {{ (void)remove(tmp_path); set_error(error, error_cap, "failed to write state file"); return false; }}
    if (!replace_file(tmp_path, path)) {{ (void)remove(tmp_path); set_error(error, error_cap, "failed to replace state file"); return false; }}
    return true;
}}

static bool read_file(const char *path, char **out, char *error, int error_cap) {{
    FILE *file = fopen(path, "rb");
    if (!file) {{ set_error(error, error_cap, "failed to open state file for read"); return false; }}
    if (fseek(file, 0, SEEK_END) != 0) {{ fclose(file); set_error(error, error_cap, "failed to seek state file"); return false; }}
    long size = ftell(file);
    if (size < 0 || size > GAME_STATE_MAX_FILE_BYTES) {{ fclose(file); set_error(error, error_cap, "state file too large"); return false; }}
    if (fseek(file, 0, SEEK_SET) != 0) {{ fclose(file); set_error(error, error_cap, "failed to rewind state file"); return false; }}
    char *data = (char *)malloc((size_t)size + 1U);
    if (!data) {{ fclose(file); set_error(error, error_cap, "failed to allocate state file buffer"); return false; }}
    size_t read_size = fread(data, 1, (size_t)size, file);
    fclose(file);
    if (read_size != (size_t)size) {{ free(data); set_error(error, error_cap, "failed to read state file"); return false; }}
    data[size] = '\\0';
    *out = data;
    return true;
}}

static bool game_state_load_doc(GameState *state, cJSON *doc, char *error, int error_cap) {{
    cJSON *state_json = doc;
    const cJSON *schema = object_item(doc, "schema");
    const cJSON *document = object_item(doc, "document");
    const cJSON *version_json = object_item(doc, "version");
    const cJSON *wrapped_state = object_item(doc, "state");
    if (schema || version_json || wrapped_state) {{
        if (!cJSON_IsString(schema) || strcmp(schema->valuestring, GAME_STATE_SCHEMA_ID) != 0) {{ set_error(error, error_cap, "state schema mismatch"); return false; }}
        if (document && (!cJSON_IsString(document) || strcmp(document->valuestring, GAME_STATE_DOCUMENT) != 0)) {{ set_error(error, error_cap, "state document mismatch"); return false; }}
        if (!cJSON_IsNumber(version_json) || version_json->valuedouble != (double)(int)version_json->valuedouble || (int)version_json->valuedouble != GAME_STATE_VERSION || !cJSON_IsObject(wrapped_state)) {{
            set_error(error, error_cap, "unsupported state version or envelope");
            return false;
        }}
        state_json = (cJSON *)wrapped_state;
    }}
    GameState next;
    if (!game_state_from_json(&next, state_json, error, error_cap)) {{ return false; }}
    GameState old = *state;
    *state = next;
    if (!notify_changed("*", error, error_cap)) {{ *state = old; return false; }}
    if (state == &g_game_state) {{ game_state_clear_dirty(); }}
    return true;
}}

bool game_state_load_json_string(GameState *state, const char *data, char *error, int error_cap) {{
    if (!data || !data[0]) {{ set_error(error, error_cap, "state json string is empty"); return false; }}
    cJSON *doc = cJSON_Parse(data);
    if (!doc) {{ set_error(error, error_cap, "invalid state json"); return false; }}
    bool ok = game_state_load_doc(state, doc, error, error_cap);
    cJSON_Delete(doc);
    return ok;
}}

bool game_state_load(GameState *state, const char *path, char *error, int error_cap) {{
    char *data = NULL;
    if (!read_file(path, &data, error, error_cap)) {{ return false; }}
    bool ok = game_state_load_json_string(state, data, error, error_cap);
    free(data);
    return ok;
}}

bool game_state_reset(GameState *state, char *error, int error_cap) {{
    GameState old = *state;
    GameState next;
    game_state_init_defaults(&next);
    if (!game_state_validate(&next, error, error_cap)) {{ return false; }}
    *state = next;
    if (!notify_changed("*", error, error_cap)) {{ *state = old; return false; }}
    if (state == &g_game_state) {{ game_state_mark_dirty(); }}
    return true;
}}
"""


def render_source(schema: dict[str, Any], schema_label: str = "state/game_state.schema.json") -> str:
    return render_generic_source(schema, schema_label)


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
    parser.add_argument("--out-dir", default=None, help="Directory for generated game_state files.")
    args = parser.parse_args(argv)

    schema_path = Path(args.schema).resolve() if args.schema else default_schema_path().resolve()
    out_dir = Path(args.out_dir).resolve() if args.out_dir else default_out_dir(schema_path).resolve()
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
