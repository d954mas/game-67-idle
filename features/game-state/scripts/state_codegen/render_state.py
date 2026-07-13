from __future__ import annotations

import json
import re
from typing import Any

from .naming import Ns, c_ident, c_macro
from .schema import SCALAR_TYPES, is_list_type, map_type_name

TOOL_LABEL = "features/game-state/scripts/generate_state.py"

def is_map_type(type_text: str) -> bool:
    return map_type_name(type_text) is not None

class StateRenderer:
    def __init__(self, ns: Ns) -> None:
        self.ns = ns

    def field_c_type(self, field: dict[str, Any]) -> str:
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


    def default_enum_macro(self, enum_name: str, value: str) -> str:
        return f"{self.ns.macro}{c_macro(enum_name)}_{c_macro(value)}"


    def c_float(self, value: Any) -> str:
        text = f"{float(value):.8g}"
        if "." not in text and "e" not in text and "E" not in text:
            text += ".0"
        return f"{text}F"


    def c_int(self, value: Any) -> str:
        if not isinstance(value, int) or isinstance(value, bool):
            raise SystemExit(f"expected integer value, got {value!r}")
        return str(value)


    def c_i64(self, value: Any) -> str:
        if not isinstance(value, int) or isinstance(value, bool):
            raise SystemExit(f"expected i64 integer value, got {value!r}")
        return f"{value}LL"


    def c_string(self, value: Any) -> str:
        if not isinstance(value, str):
            raise SystemExit(f"expected string value, got {value!r}")
        return json.dumps(value)

    def collection_macro(self, path: str) -> str:
        return f"{self.ns.macro}MAX_{c_macro(path)}"


    def object_type_c_name(self, type_name: str) -> str:
        return f"{self.ns.pascal}{type_name}"


    def object_type_func_name(self, type_name: str) -> str:
        return c_ident(type_name)


    def schema_types(self, schema: dict[str, Any]) -> dict[str, dict[str, Any]]:
        types = schema.get("types", {})
        return types if isinstance(types, dict) else {}


    def map_fields(self, schema: dict[str, Any]) -> list[dict[str, Any]]:
        return [f for f in schema["fields"] if is_map_type(f["type"])]


    def list_fields(self, schema: dict[str, Any]) -> list[dict[str, Any]]:
        return [f for f in schema["fields"] if is_list_type(f["type"])]


    def scalar_fields(self, schema: dict[str, Any]) -> list[dict[str, Any]]:
        return [f for f in schema["fields"] if f["type"] in SCALAR_TYPES]


    def scalar_type_fields(self, fields: list[dict[str, Any]]) -> list[dict[str, Any]]:
        return [f for f in fields if f["type"] in SCALAR_TYPES]


    def enum_alias(self, field: dict[str, Any]) -> str | None:
        path = field["path"]
        return path[: -len("_index")] if path.endswith("_index") else None


    def enum_table(self, field: dict[str, Any]) -> tuple[str, str]:
        """Return (names_table, count_macro) for an enum field."""
        enum_name = field["enum"]
        return f"k_{c_ident(enum_name)}_names", f"{self.ns.macro}{c_macro(enum_name)}_COUNT"


    # ---------------------------------------------------------------------------
    # Enums
    # ---------------------------------------------------------------------------


    def render_enum(self, enum_name: str, values: list[str]) -> str:
        type_name = f"{self.ns.type}{enum_name}"
        lines = [f"typedef enum {type_name} {{"]
        for i, value in enumerate(values):
            suffix = f" = {i}" if i == 0 else ""
            lines.append(f"    {self.ns.macro}{c_macro(enum_name)}_{c_macro(value)}{suffix},")
        lines.append(f"    {self.ns.macro}{c_macro(enum_name)}_COUNT,")
        lines.append(f"}} {type_name};")
        return "\n".join(lines)


    def render_enum_name_decls(self, schema: dict[str, Any]) -> str:
        return "\n".join(
            f"const char *{self.ns.fn}{c_ident(name)}_name(int value);" for name in schema["enums"]
        )


    def render_enum_tables(self, schema: dict[str, Any]) -> str:
        blocks: list[str] = []
        for name, values in schema["enums"].items():
            ename = c_ident(name)
            lines = [f"static const char *const k_{ename}_names[{self.ns.macro}{c_macro(name)}_COUNT] = {{"]
            lines.extend(f'    "{value}",' for value in values)
            lines.append("};")
            blocks.append("\n".join(lines))
        for name in schema["enums"]:
            ename = c_ident(name)
            blocks.append(
                f"const char *{self.ns.fn}{ename}_name(int value) {{\n"
                f"    return (value >= 0 && value < {self.ns.macro}{c_macro(name)}_COUNT) ? k_{ename}_names[value] : \"unknown\";\n"
                f"}}"
            )
        return "\n\n".join(blocks)


    # ---------------------------------------------------------------------------
    # Constants + structs
    # ---------------------------------------------------------------------------


    def render_state_constants(self, schema: dict[str, Any]) -> str:
        lines: list[str] = []

        def emit_scalar(field: dict[str, Any], prefix: str = "") -> None:
            typ = field["type"]
            name = c_macro(f"{prefix}{field['path']}")
            if typ == "enum":
                lines.append(f"#define {self.ns.macro}{name}_DEFAULT {self.default_enum_macro(field['enum'], field['default'])}")
            elif typ == "int":
                lines.append(f"#define {self.ns.macro}{name}_DEFAULT {self.c_int(field['default'])}")
                lines.append(f"#define {self.ns.macro}{name}_MIN {self.c_int(field['min'])}")
                lines.append(f"#define {self.ns.macro}{name}_MAX {self.c_int(field['max'])}")
            elif typ == "i64":
                lines.append(f"#define {self.ns.macro}{name}_DEFAULT {self.c_i64(field['default'])}")
                lines.append(f"#define {self.ns.macro}{name}_MIN {self.c_i64(field['min'])}")
                lines.append(f"#define {self.ns.macro}{name}_MAX {self.c_i64(field['max'])}")
            elif typ == "float":
                lines.append(f"#define {self.ns.macro}{name}_DEFAULT {self.c_float(field['default'])}")
                lines.append(f"#define {self.ns.macro}{name}_MIN {self.c_float(field['min'])}")
                lines.append(f"#define {self.ns.macro}{name}_MAX {self.c_float(field['max'])}")
            elif typ == "string" and "default" in field:
                lines.append(f"#define {self.ns.macro}{name}_DEFAULT {self.c_string(field['default'])}")
            elif typ == "string?" and isinstance(field.get("default"), str):
                lines.append(f"#define {self.ns.macro}{name}_DEFAULT {self.c_string(field['default'])}")
            elif typ == "bool":
                lines.append(f"#define {self.ns.macro}{name}_DEFAULT {1 if field['default'] else 0}")

        for field in self.scalar_fields(schema):
            emit_scalar(field)
        for type_name, type_def in self.schema_types(schema).items():
            for field in self.scalar_type_fields(type_def["fields"]):
                emit_scalar(field, f"{type_name}.")
        for field in self.map_fields(schema) + self.list_fields(schema):
            lines.append(f"#define {self.collection_macro(field['path'])} {self.c_int(field['max_count'])}")
        return "\n".join(lines)


    def render_struct_scalar_field(self, field: dict[str, Any], indent: str = "    ") -> list[str]:
        name = c_ident(field["path"])
        typ = field["type"]
        if typ == "string":
            return [f"{indent}char {name}[{self.ns.macro}STRING_MAX];"]
        if typ == "string?":
            return [f"{indent}bool has_{name};", f"{indent}char {name}[{self.ns.macro}STRING_MAX];"]
        return [f"{indent}{self.field_c_type(field)} {name};"]


    def render_object_structs(self, schema: dict[str, Any]) -> str:
        blocks: list[str] = []
        for type_name, type_def in self.schema_types(schema).items():
            lines = [f"typedef struct {self.object_type_c_name(type_name)} {{"]
            lines.append("    bool used;")
            lines.append(f"    char key[{self.ns.macro}STRING_MAX];")
            for field in type_def["fields"]:
                lines.extend(self.render_struct_scalar_field(field))
            lines.append(f"}} {self.object_type_c_name(type_name)};")
            blocks.append("\n".join(lines))
        return "\n\n".join(blocks)


    def render_state_struct(self, schema: dict[str, Any]) -> str:
        lines = [f"typedef struct {self.ns.type} {{"]
        for field in schema["fields"]:
            typ = field["type"]
            name = c_ident(field["path"])
            if typ in SCALAR_TYPES:
                lines.extend(self.render_struct_scalar_field(field))
            elif is_list_type(typ):
                lines.append(f"    char {name}[{self.collection_macro(field['path'])}][{self.ns.macro}STRING_MAX];")
                lines.append(f"    int {name}_count;")
            elif (type_name := map_type_name(typ)):
                lines.append(f"    {self.object_type_c_name(type_name)} {name}[{self.collection_macro(field['path'])}];")
        lines.append(f"}} {self.ns.type};")
        return "\n".join(lines)


    def render_header(self, schema: dict[str, Any], schema_label: str) -> str:
        enums = schema["enums"]
        enum_blocks = "\n\n".join(self.render_enum(name, values) for name, values in enums.items())
        guard = f"{self.ns.macro}GENERATED_H"
        return f"""#ifndef {guard}
#define {guard}

/* Generated by {TOOL_LABEL} from {schema_label}. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cJSON.h"
#include "game_save.h"

#define {self.ns.macro}SCHEMA_ID "{schema["schema"]}"
#define {self.ns.macro}FRAGMENT_ID "{schema["fragment"]}"
#define {self.ns.macro}VERSION {schema["version"]}
#define {self.ns.macro}STRING_MAX {schema["string_max"]}

{self.render_state_constants(schema)}

{enum_blocks}

{self.render_object_structs(schema)}

{self.render_state_struct(schema)}

/* Instance owned by this fragment TU (the shared global-state monolith is gone).
   Feature LOGIC works with it directly or through its own API. */
extern {self.ns.type} {self.ns.inst};

{self.render_enum_name_decls(schema)}

void   {self.ns.fn}init_defaults({self.ns.type} *state);
bool   {self.ns.fn}validate(const {self.ns.type} *state, char *error, int error_cap);
cJSON *{self.ns.fn}schema_json(void);
cJSON *{self.ns.fn}to_json(const {self.ns.type} *state);
cJSON *{self.ns.fn}get_path_json(const {self.ns.type} *state, const char *path, char *error, int error_cap);
bool   {self.ns.fn}set_path_json({self.ns.type} *state, const char *path, const cJSON *value, char *error, int error_cap);
bool   {self.ns.fn}patch_json({self.ns.type} *state, const cJSON *values, char *error, int error_cap);
bool   {self.ns.fn}from_json({self.ns.type} *state, const cJSON *json, char *error, int error_cap);

/* Generated descriptor — replaces the hand-written fragment adapter. */
extern const GameSaveFragment {self.ns.frag};

#endif
"""


    # ---------------------------------------------------------------------------
    # Scalar codegen
    # ---------------------------------------------------------------------------


    def render_scalar_default_assignment(self, field: dict[str, Any], target: str, prefix: str = "") -> list[str]:
        ident = c_ident(field["path"])
        macro = f"{self.ns.macro}{c_macro(prefix + field['path'])}"
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


    def render_scalar_validation(self, field: dict[str, Any], target: str, path: str, prefix: str = "") -> list[str]:
        ident = c_ident(field["path"])
        macro = f"{self.ns.macro}{c_macro(prefix + field['path'])}"
        typ = field["type"]
        if typ == "enum":
            _, count_macro = self.enum_table(field)
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


    def render_cjson_add_scalar(self, field: dict[str, Any], target: str, state_expr: str, key: str) -> list[str]:
        ident = c_ident(field["path"])
        typ = field["type"]
        if typ == "enum":
            alias = self.enum_alias(field)
            ename = c_ident(field["enum"])
            if alias:
                return [
                    f'    cJSON_AddNumberToObject({target}, "{key}", {state_expr}->{ident});',
                    f'    cJSON_AddStringToObject({target}, "{alias}", {self.ns.fn}{ename}_name({state_expr}->{ident}));',
                ]
            return [f'    cJSON_AddStringToObject({target}, "{key}", {self.ns.fn}{ename}_name({state_expr}->{ident}));']
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


    def render_get_scalar_expr(self, field: dict[str, Any], state_expr: str) -> str:
        ident = c_ident(field["path"])
        typ = field["type"]
        if typ == "enum":
            ename = c_ident(field["enum"])
            return f"cJSON_CreateString({self.ns.fn}{ename}_name({state_expr}->{ident}))"
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


    def render_get_scalar_if(self, field: dict[str, Any], state_expr: str, path: str) -> list[str]:
        lines = [
            f'    if (strcmp(field, "{path}") == 0) {{',
            f"        return {self.render_get_scalar_expr(field, state_expr)};",
            "    }",
        ]
        alias = self.enum_alias(field) if field["type"] == "enum" else None
        if alias:
            ename = c_ident(field["enum"])
            lines.extend([
                f'    if (strcmp(field, "{alias}") == 0) {{',
                f"        return cJSON_CreateString({self.ns.fn}{ename}_name({state_expr}->{c_ident(field['path'])}));",
                "    }",
            ])
        return lines


    def render_set_scalar_if(self, field: dict[str, Any], state_expr: str, path: str, prefix: str = "", compare_var: str = "field") -> list[str]:
        ident = c_ident(field["path"])
        macro = f"{self.ns.macro}{c_macro(prefix + field['path'])}"
        typ = field["type"]
        head = [f'    if (strcmp({compare_var}, "{path}") == 0) {{']
        if typ == "enum":
            names_table, count_macro = self.enum_table(field)
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


    def render_read_scalar(self, field: dict[str, Any], source: str, target: str, key: str, prefix: str = "") -> list[str]:
        ident = c_ident(field["path"])
        macro = f"{self.ns.macro}{c_macro(prefix + field['path'])}"
        typ = field["type"]
        if typ == "enum":
            names_table, count_macro = self.enum_table(field)
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


    def render_object_helpers(self, schema: dict[str, Any]) -> str:
        blocks: list[str] = []
        for type_name, type_def in self.schema_types(schema).items():
            fname = self.object_type_func_name(type_name)
            cname = self.object_type_c_name(type_name)
            default_lines = [
                f"static void {fname}_init_defaults({cname} *obj, const char *key) {{",
                "    memset(obj, 0, sizeof(*obj));",
                "    obj->used = true;",
                "    (void)gsj_copy_text(obj->key, sizeof(obj->key), key);",
            ]
            for field in type_def["fields"]:
                default_lines.extend(self.render_scalar_default_assignment(field, "obj", f"{type_name}."))
            default_lines.append("}")

            validate_lines = [f"static bool {fname}_validate(const {cname} *obj, char *error, int error_cap) {{"]
            validate_lines.append("    if (!obj->used) { return true; }")
            validate_lines.append("    if (obj->key[0] == '\\0') { gsj_set_error(error, error_cap, \"object key is empty\"); return false; }")
            for field in type_def["fields"]:
                validate_lines.extend(self.render_scalar_validation(field, "obj", field["path"], f"{type_name}."))
            validate_lines.append("    return true;")
            validate_lines.append("}")

            to_json_lines = [f"static cJSON *{fname}_to_json(const {cname} *obj) {{"]
            to_json_lines.append("    cJSON *json = cJSON_CreateObject();")
            for field in type_def["fields"]:
                to_json_lines.extend(self.render_cjson_add_scalar(field, "json", "obj", field["path"]))
            to_json_lines.append("    return json;")
            to_json_lines.append("}")

            get_lines = [f"static cJSON *{fname}_get_field_json(const {cname} *obj, const char *field, char *error, int error_cap) {{"]
            get_lines.append("    if (!field || field[0] == '\\0') { return " + f"{fname}_to_json(obj); }}")
            for field in type_def["fields"]:
                get_lines.extend(self.render_get_scalar_if(field, "obj", field["path"]))
            get_lines.append("    gsj_set_error(error, error_cap, \"unknown object field\");")
            get_lines.append("    return NULL;")
            get_lines.append("}")

            set_lines = [f"static bool {fname}_set_field_json({cname} *obj, const char *field, const cJSON *value, char *error, int error_cap) {{"]
            for field in type_def["fields"]:
                set_lines.extend(self.render_set_scalar_if(field, "obj", field["path"], f"{type_name}."))
            set_lines.append("    gsj_set_error(error, error_cap, \"unknown object field\");")
            set_lines.append("    return false;")
            set_lines.append("}")

            from_lines = [f"static bool {fname}_from_json({cname} *obj, const cJSON *json, char *error, int error_cap) {{"]
            from_lines.append("    if (!cJSON_IsObject(json)) { gsj_set_error(error, error_cap, \"object must be json object\"); return false; }")
            for field in type_def["fields"]:
                from_lines.extend(self.render_read_scalar(field, "json", "obj", field["path"], f"{type_name}."))
            from_lines.append(f"    return {fname}_validate(obj, error, error_cap);")
            from_lines.append("}")

            blocks.append("\n".join(default_lines + [""] + validate_lines + [""] + to_json_lines + [""] + get_lines + [""] + set_lines + [""] + from_lines))
        return "\n\n".join(blocks)


    def render_collection_helpers(self, schema: dict[str, Any]) -> str:
        blocks: list[str] = []
        for field in self.map_fields(schema):
            ident = c_ident(field["path"])
            type_name = map_type_name(field["type"])
            assert type_name is not None
            fname = self.object_type_func_name(type_name)
            cname = self.object_type_c_name(type_name)
            max_macro = self.collection_macro(field["path"])
            blocks.append(f"""
static {cname} *find_{ident}({self.ns.type} *state, const char *key) {{
    for (int i = 0; i < {max_macro}; i++) {{
        if (state->{ident}[i].used && strcmp(state->{ident}[i].key, key) == 0) {{
            return &state->{ident}[i];
        }}
    }}
    return NULL;
}}

static const {cname} *find_{ident}_const(const {self.ns.type} *state, const char *key) {{
    for (int i = 0; i < {max_macro}; i++) {{
        if (state->{ident}[i].used && strcmp(state->{ident}[i].key, key) == 0) {{
            return &state->{ident}[i];
        }}
    }}
    return NULL;
}}

static {cname} *alloc_{ident}({self.ns.type} *state, const char *key) {{
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

static cJSON *{ident}_to_json(const {self.ns.type} *state) {{
    cJSON *json = cJSON_CreateObject();
    for (int i = 0; i < {max_macro}; i++) {{
        if (state->{ident}[i].used) {{
            cJSON_AddItemToObject(json, state->{ident}[i].key, {fname}_to_json(&state->{ident}[i]));
        }}
    }}
    return json;
}}

static bool set_{ident}_from_json({self.ns.type} *state, const cJSON *json, char *error, int error_cap) {{
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
        for field in self.list_fields(schema):
            ident = c_ident(field["path"])
            max_macro = self.collection_macro(field["path"])
            blocks.append(f"""
static cJSON *{ident}_to_json(const {self.ns.type} *state) {{
    cJSON *json = cJSON_CreateArray();
    for (int i = 0; i < state->{ident}_count; i++) {{
        cJSON_AddItemToArray(json, cJSON_CreateString(state->{ident}[i]));
    }}
    return json;
}}

static bool set_{ident}_from_json({self.ns.type} *state, const cJSON *json, char *error, int error_cap) {{
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


    def parent_var_for(self, path: str) -> tuple[str, str, str]:
        if "." not in path:
            return "root", path, ""
        group, key = path.split(".", 1)
        return c_ident(group), key, group


    def render_to_json(self, schema: dict[str, Any]) -> str:
        lines: list[str] = []
        emitted_groups: set[str] = set()
        for field in schema["fields"]:
            parent, key, group = self.parent_var_for(field["path"])
            if group and group not in emitted_groups:
                emitted_groups.add(group)
                lines.append(f'    cJSON *{parent} = cJSON_AddObjectToObject(root, "{group}");')
            if field["type"] in SCALAR_TYPES:
                lines.extend(self.render_cjson_add_scalar(field, parent, "state", key))
            elif is_map_type(field["type"]) or is_list_type(field["type"]):
                lines.append(f'    cJSON_AddItemToObject({parent}, "{key}", {c_ident(field["path"])}_to_json(state));')
        return "\n".join(lines)


    def render_defaults(self, schema: dict[str, Any]) -> str:
        lines: list[str] = []
        for field in self.scalar_fields(schema):
            lines.extend(self.render_scalar_default_assignment(field, "state"))
        for field in self.list_fields(schema):
            default = field.get("default")
            if not default:
                continue
            ident = c_ident(field["path"])
            for index, value in enumerate(default):
                lines.append(f"    (void)gsj_copy_text(state->{ident}[{index}], sizeof(state->{ident}[{index}]), {self.c_string(value)});")
            lines.append(f"    state->{ident}_count = {len(default)};")
        return "\n".join(lines)


    def render_validate(self, schema: dict[str, Any]) -> str:
        lines: list[str] = []
        for field in self.scalar_fields(schema):
            lines.extend(self.render_scalar_validation(field, "state", field["path"]))
        for field in self.list_fields(schema):
            ident = c_ident(field["path"])
            max_macro = self.collection_macro(field["path"])
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
        for field in self.map_fields(schema):
            ident = c_ident(field["path"])
            type_name = map_type_name(field["type"])
            assert type_name is not None
            fname = self.object_type_func_name(type_name)
            max_macro = self.collection_macro(field["path"])
            lines.extend([
                f"    for (int i = 0; i < {max_macro}; i++) {{",
                f"        if (state->{ident}[i].used && !{fname}_validate(&state->{ident}[i], error, error_cap)) {{",
                "            return false;",
                "        }",
                "    }",
            ])
        return "\n".join(lines)


    def render_get_path(self, schema: dict[str, Any]) -> str:
        lines: list[str] = []
        for field in self.scalar_fields(schema):
            path = field["path"]
            lines.append(f'    if (strcmp(path, "{path}") == 0) {{')
            lines.append(f"        return {self.render_get_scalar_expr(field, 'state')};")
            lines.append("    }")
        for field in self.list_fields(schema):
            path = field["path"]
            ident = c_ident(path)
            lines.extend([
                f'    if (strcmp(path, "{path}") == 0) {{',
                f"        return {ident}_to_json(state);",
                "    }",
            ])
        for field in self.map_fields(schema):
            path = field["path"]
            ident = c_ident(path)
            type_name = map_type_name(field["type"])
            assert type_name is not None
            fname = self.object_type_func_name(type_name)
            prefix_len = len(path) + 1
            lines.extend([
                f'    if (strcmp(path, "{path}") == 0) {{',
                f"        return {ident}_to_json(state);",
                "    }",
                f'    if (strncmp(path, "{path}.", {prefix_len}) == 0) {{',
                f"        const char *key = path + {prefix_len};",
                "        const char *field = strchr(key, '.');",
                f"        char map_key[{self.ns.macro}STRING_MAX];",
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
                f"        const {self.object_type_c_name(type_name)} *obj = find_{ident}_const(state, map_key);",
                "        if (!obj) { gsj_set_error(error, error_cap, \"unknown map key\"); return NULL; }",
                f"        return {fname}_get_field_json(obj, field, error, error_cap);",
                "    }",
            ])
        return "\n".join(lines)


    def render_set_path(self, schema: dict[str, Any]) -> str:
        lines: list[str] = []
        for field in self.scalar_fields(schema):
            lines.extend(self.render_set_scalar_if(field, "state", field["path"], compare_var="path"))
        for field in self.list_fields(schema):
            path = field["path"]
            ident = c_ident(path)
            lines.extend([
                f'    if (strcmp(path, "{path}") == 0) {{',
                f"        return set_{ident}_from_json(state, value, error, error_cap);",
                "    }",
            ])
        for field in self.map_fields(schema):
            path = field["path"]
            ident = c_ident(path)
            type_name = map_type_name(field["type"])
            assert type_name is not None
            fname = self.object_type_func_name(type_name)
            prefix_len = len(path) + 1
            lines.extend([
                f'    if (strcmp(path, "{path}") == 0) {{',
                f"        return set_{ident}_from_json(state, value, error, error_cap);",
                "    }",
                f'    if (strncmp(path, "{path}.", {prefix_len}) == 0) {{',
                f"        const char *key = path + {prefix_len};",
                "        const char *field = strchr(key, '.');",
                f"        char map_key[{self.ns.macro}STRING_MAX];",
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
                f"        {self.object_type_c_name(type_name)} *obj = alloc_{ident}(state, map_key);",
                "        if (!obj) { gsj_set_error(error, error_cap, \"too many map entries or long key\"); return false; }",
                f"        return field ? {fname}_set_field_json(obj, field, value, error, error_cap) : {fname}_from_json(obj, value, error, error_cap);",
                "    }",
            ])
        return "\n".join(lines)


    def render_from_json(self, schema: dict[str, Any]) -> str:
        lines: list[str] = []
        emitted_groups: set[str] = set()
        for field in schema["fields"]:
            parent, key, group = self.parent_var_for(field["path"])
            if group and group not in emitted_groups:
                emitted_groups.add(group)
                lines.append(f'    const cJSON *{parent} = gsj_object_item(json, "{group}");')
            source = parent if group else "json"
            if field["type"] in SCALAR_TYPES:
                lines.extend(self.render_read_scalar(field, source, "(&next)", key))
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


    def render_fragment_descriptor(self, schema: dict[str, Any]) -> str:
        migrations = schema["migrations"]
        hooks = schema["hooks"]
        pre: list[str] = []

        steps_field = "NULL"
        if migrations:
            for entry in migrations:
                pre.append(f'extern bool {entry["fn"]}(cJSON *frag, char *err, int cap);')
            pre.append(f"static const GameSaveMigrateFn {self.ns.inst}_migration_steps[] = {{")
            for entry in migrations:
                pre.append(f'    {entry["fn"]},')
            pre.append("};")
            steps_field = f"{self.ns.inst}_migration_steps"

        on_new_game = "NULL"
        reconcile = "NULL"
        if hooks.get("on_new_game"):
            pre.append(f"extern void {self.ns.ident}_on_new_game(void);")
            on_new_game = f"{self.ns.ident}_on_new_game"
        if hooks.get("reconcile"):
            pre.append(f"extern void {self.ns.ident}_reconcile(void);")
            reconcile = f"{self.ns.ident}_reconcile"

        wrappers = (
            f"static void   frag_reset(void)                                             {{ {self.ns.fn}init_defaults(&{self.ns.inst}); }}\n"
            f"static cJSON *frag_to_json(void)                                           {{ return {self.ns.fn}to_json(&{self.ns.inst}); }}\n"
            f"static bool   frag_from_json(const cJSON *j, char *e, int c)               {{ return {self.ns.fn}from_json(&{self.ns.inst}, j, e, c); }}\n"
            f"static cJSON *frag_get_path(const char *s, char *e, int c)                 {{ return {self.ns.fn}get_path_json(&{self.ns.inst}, s, e, c); }}\n"
            f"static bool   frag_set_path(const char *s, const cJSON *v, char *e, int c) {{ return {self.ns.fn}set_path_json(&{self.ns.inst}, s, v, e, c); }}\n"
            f"static cJSON *frag_schema(void)                                            {{ return {self.ns.fn}schema_json(); }}"
        )

        descriptor = (
            f"const GameSaveFragment {self.ns.frag} = {{\n"
            f"    .id            = {self.ns.macro}FRAGMENT_ID,\n"
            f"    .version       = {self.ns.macro}VERSION,\n"
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


    def normalized_schema_for_embed(self, schema: dict[str, Any]) -> dict[str, Any]:
        """Canonical form the runtime embeds and the editor/smoke bot reads:
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


    def render_schema_header(self, schema: dict[str, Any], schema_label: str) -> str:
        canonical = json.dumps(self.normalized_schema_for_embed(schema), ensure_ascii=False, separators=(",", ":"))
        chunks = [canonical[i : i + 1800] for i in range(0, len(canonical), 1800)]
        literal_lines = [f"    {json.dumps(chunk)}, \\" for chunk in chunks]
        guard = f"{self.ns.macro}SCHEMA_GEN_H"
        return (
            f"#ifndef {guard}\n"
            f"#define {guard}\n\n"
            f"/* Generated by {TOOL_LABEL} from {schema_label}. */\n"
            f"#define {self.ns.macro}SCHEMA_JSON_CHUNKS \\\n"
            "    { \\\n"
            + "\n".join(literal_lines)
            + "\n"
            + '    "" \\\n'
            + "    }\n\n"
            "#endif\n"
        )


    # ---------------------------------------------------------------------------
    # Source
    # ---------------------------------------------------------------------------


    def render_generic_source(self, schema: dict[str, Any], schema_label: str) -> str:
        return f"""#include "{self.ns.id}_state.h"
#include "{self.ns.id}_state_schema.gen.h"
#include "game_state_json.h"

/* Generated by {TOOL_LABEL} from {schema_label}. */

#include <stdlib.h>
#include <string.h>

{self.render_enum_tables(schema)}

{self.ns.type} {self.ns.inst};   /* fragment instance (ownership lives here) */

{self.render_object_helpers(schema)}

{self.render_collection_helpers(schema)}

void {self.ns.fn}init_defaults({self.ns.type} *state) {{
    memset(state, 0, sizeof(*state));
{self.render_defaults(schema)}
}}

bool {self.ns.fn}validate(const {self.ns.type} *state, char *error, int error_cap) {{
    if (!state) {{ gsj_set_error(error, error_cap, "state is null"); return false; }}
{self.render_validate(schema)}
    return true;
}}

cJSON *{self.ns.fn}schema_json(void) {{
    const char *chunks[] = {self.ns.macro}SCHEMA_JSON_CHUNKS;
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

cJSON *{self.ns.fn}to_json(const {self.ns.type} *state) {{
    cJSON *root = cJSON_CreateObject();
{self.render_to_json(schema)}
    return root;
}}

cJSON *{self.ns.fn}get_path_json(const {self.ns.type} *state, const char *path, char *error, int error_cap) {{
    if (!path || path[0] == '\\0') {{ return {self.ns.fn}to_json(state); }}
{self.render_get_path(schema)}
    gsj_set_error(error, error_cap, "unknown state path");
    return NULL;
}}

bool {self.ns.fn}set_path_json({self.ns.type} *state, const char *path, const cJSON *value, char *error, int error_cap) {{
    if (!state || !path || !path[0] || !value) {{ gsj_set_error(error, error_cap, "path and value are required"); return false; }}
{self.render_set_path(schema)}
    gsj_set_error(error, error_cap, "unknown state path");
    return false;
}}

bool {self.ns.fn}patch_json({self.ns.type} *state, const cJSON *values, char *error, int error_cap) {{
    if (!cJSON_IsObject(values)) {{ gsj_set_error(error, error_cap, "values must be an object"); return false; }}
    {self.ns.type} next = *state;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, values) {{
        if (!item->string || !{self.ns.fn}set_path_json(&next, item->string, item, error, error_cap)) {{ return false; }}
    }}
    if (!{self.ns.fn}validate(&next, error, error_cap)) {{ return false; }}
    *state = next;
    return true;
}}

bool {self.ns.fn}from_json({self.ns.type} *state, const cJSON *json, char *error, int error_cap) {{
    if (!cJSON_IsObject(json)) {{ gsj_set_error(error, error_cap, "state json must be object"); return false; }}
    {self.ns.type} next;
    {self.ns.fn}init_defaults(&next);
{self.render_from_json(schema)}
    if (!{self.ns.fn}validate(&next, error, error_cap)) {{ return false; }}
    *state = next;
    return true;
}}

{self.render_fragment_descriptor(schema)}
"""


    def render_source(self, schema: dict[str, Any], schema_label: str = "state/game_state.schema.json") -> str:
        return self.render_generic_source(schema, schema_label)


    # ---------------------------------------------------------------------------
    # Events (typed event structs + emit helpers + descriptors)  [E2]
    # ---------------------------------------------------------------------------
