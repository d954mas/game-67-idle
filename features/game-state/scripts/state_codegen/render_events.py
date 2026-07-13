from __future__ import annotations

from typing import Any

from .naming import Ns, c_ident, pascal
from .schema import EVENT_FIELD_C_TYPE, EVENT_FIELD_EMIT_ARG, EVENT_FIELD_FT_ENUM

_pascal = pascal
TOOL_LABEL = "features/game-state/scripts/generate_state.py"

class EventRenderer:
    def __init__(self, ns: Ns) -> None:
        self.ns = ns

    def schema_events(self, schema: dict[str, Any]) -> dict[str, dict[str, Any]]:
        events = schema.get("events", {})
        return events if isinstance(events, dict) else {}


    def event_struct_c_name(self, evt: str) -> str:
        return f"{self.ns.pascal}Ev{_pascal(evt)}"          # MiniEvCellSpawned


    def event_emit_fn(self, evt: str) -> str:
        return f"{self.ns.id}_emit_{evt}"                    # mini_emit_cell_spawned


    def event_type_fn(self, evt: str) -> str:
        return f"{self.ns.id}_ev_{evt}_type"                 # mini_ev_cell_spawned_type


    def event_desc_name(self, evt: str) -> str:
        return f"{self.ns.id}_ev_{evt}_desc"                 # mini_ev_cell_spawned_desc


    def event_full_name(self, evt: str) -> str:
        return f"{self.ns.id}.{evt}"                         # "mini.cell_spawned"


    def event_accessor(self, evt: str, field_name: str) -> str:
        return f"{self.ns.id}_ev_{evt}_{field_name}"         # mini_ev_cell_spawned_label


    def event_has_inline(self, fields: list[dict[str, Any]]) -> bool:
        return any(f["type"] in ("string", "bytes") for f in fields)


    def render_event_struct_fields(self, fields: list[dict[str, Any]]) -> list[str]:
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


    def render_event_accessors(self, evt: str, fields: list[dict[str, Any]]) -> list[str]:
        struct = self.event_struct_c_name(evt)
        lines: list[str] = []
        for f in fields:
            name = f["name"]
            acc = self.event_accessor(evt, name)
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


    def render_event_emit_args(self, fields: list[dict[str, Any]]) -> str:
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


    def render_event_emit_body(self, evt: str, fields: list[dict[str, Any]]) -> list[str]:
        struct = self.event_struct_c_name(evt)
        type_fn = self.event_type_fn(evt)
        emit_fn = self.event_emit_fn(evt)
        lines: list[str] = []
        if not self.event_has_inline(fields):
            # Scalar-only events use a direct local struct with no staging.
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


    def render_event_descriptor(self, evt: str, fields: list[dict[str, Any]]) -> list[str]:
        struct = self.event_struct_c_name(evt)
        fields_arr = f"{self.ns.id}_ev_{evt}_fields"
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
        lines.append(f"const game_event_desc_t {self.event_desc_name(evt)} = {{")
        lines.append(f'    "{self.event_full_name(evt)}",')
        lines.append(f"    (uint32_t)sizeof({struct}),")
        lines.append(f"    {fields_arr},")
        lines.append(f"    (int)(sizeof({fields_arr}) / sizeof({fields_arr}[0])),")
        lines.append("};")
        return lines


    def render_events_header(self, schema: dict[str, Any], schema_label: str) -> str:
        events = self.schema_events(schema)
        guard = f"{self.ns.macro}EVENTS_GEN_H"
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
            struct = self.event_struct_c_name(evt)
            parts.append(f"/* ---- {self.event_full_name(evt)} ---- */")
            parts.append(f"typedef struct {struct} {{")
            parts.extend(self.render_event_struct_fields(fields))
            parts.append(f"}} {struct};")
            parts.append("")
            parts.append(f'nt_hash64_t {self.event_type_fn(evt)}(void); /* nt_hash64_str("{self.event_full_name(evt)}"), cached */')
            parts.append("")
            parts.append(f"const void *{self.event_emit_fn(evt)}({self.render_event_emit_args(fields)});")
            accessors = self.render_event_accessors(evt, fields)
            if accessors:
                parts.append("")
                parts.extend(accessors)
            parts.append("")
            parts.append(f"extern const game_event_desc_t {self.event_desc_name(evt)};")
            parts.append("")
        parts.append("/* ---- fragment event table + label registration ---- */")
        parts.append(f"extern const game_event_desc_t *const {self.ns.id}_ev_descs[];")
        parts.append(f"extern const int {self.ns.id}_ev_desc_count;")
        parts.append("")
        parts.append(f"void {self.ns.id}_ev_register(void); /* register debug labels; call once after nt_hash_init */")
        parts.append("")
        parts.append(f"#endif /* {guard} */")
        parts.append("")
        return "\n".join(parts)


    def render_events_source(self, schema: dict[str, Any], schema_label: str) -> str:
        events = self.schema_events(schema)
        parts: list[str] = [
            f'#include "{self.ns.id}_state_events.gen.h"',
            "",
            f"/* Generated by {TOOL_LABEL} from {schema_label}. */",
            "",
        ]
        if not events:
            # Empty fragment (no events): a zero-length array is invalid in C, so emit a
            # 1-element NULL stub; consumers gate on count==0 and never dereference it.
            parts.append(f"const game_event_desc_t *const {self.ns.id}_ev_descs[1] = {{ NULL }};")
            parts.append(f"const int {self.ns.id}_ev_desc_count = 0;")
            parts.append(f"void {self.ns.id}_ev_register(void) {{ }}")
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
            struct = self.event_struct_c_name(evt)
            parts.append(f"_Static_assert(_Alignof({struct}) <= _Alignof(max_align_t),")
            parts.append(f'               "{struct} over-aligned for game_event_emit");')
        parts.append("")
        for evt, spec in events.items():
            fields = spec["fields"]
            parts.append(f"/* ---- {self.event_full_name(evt)} ---- */")
            parts.append(f"nt_hash64_t {self.event_type_fn(evt)}(void) {{")
            parts.append("    static nt_hash64_t h;")
            parts.append(f'    if (!h.value) {{ h = nt_hash64_str("{self.event_full_name(evt)}"); }}')
            parts.append("    return h;")
            parts.append("}")
            parts.append("")
            parts.append(f"const void *{self.event_emit_fn(evt)}({self.render_event_emit_args(fields)}) {{")
            parts.extend(self.render_event_emit_body(evt, fields))
            parts.append("}")
            parts.append("")
            parts.extend(self.render_event_descriptor(evt, fields))
            parts.append("")
        parts.append("/* ---- fragment event table ---- */")
        parts.append(f"const game_event_desc_t *const {self.ns.id}_ev_descs[] = {{")
        for evt in events:
            parts.append(f"    &{self.event_desc_name(evt)},")
        parts.append("};")
        parts.append(f"const int {self.ns.id}_ev_desc_count = {len(events)};")
        parts.append("")
        parts.append(f"void {self.ns.id}_ev_register(void) {{")
        for evt in events:
            parts.append(f'    game_event_register_type_name({self.event_type_fn(evt)}(), "{self.event_full_name(evt)}");')
        parts.append("}")
        parts.append("")
        return "\n".join(parts)
