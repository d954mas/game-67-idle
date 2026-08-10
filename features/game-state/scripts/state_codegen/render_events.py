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


    def event_records(self, spec: dict[str, Any]) -> list[dict[str, Any]]:
        records = spec.get("repeated", [])
        return records if isinstance(records, list) else []


    def record_struct_c_name(self, evt: str, record: str) -> str:
        return f"{self.event_struct_c_name(evt)}{_pascal(record)}"   # MiniEvTickedSpent


    def record_input_c_name(self, evt: str, record: str) -> str:
        return f"{self.record_struct_c_name(evt, record)}In"         # MiniEvTickedSpentIn


    def event_has_inline(self, fields: list[dict[str, Any]], records: list[dict[str, Any]]) -> bool:
        return bool(records) or any(f["type"] in ("string", "bytes") for f in fields)


    def render_event_struct_fields(self, fields: list[dict[str, Any]], records: list[dict[str, Any]]) -> list[str]:
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
        for record in records:
            name = record["name"]
            lines.append(f"    uint32_t {name}; /* byte offset -> inline record array (read via accessor) */")
            lines.append(f"    uint32_t {name}_count; /* number of inline records */")
        return lines


    def render_record_structs(self, evt: str, records: list[dict[str, Any]]) -> list[str]:
        """Two structs per section: the packed wire record (strings as payload-relative
        offsets) and the caller-facing input record the emit helper reads."""
        lines: list[str] = []
        for record in records:
            wire = self.record_struct_c_name(evt, record["name"])
            given = self.record_input_c_name(evt, record["name"])
            lines.append(f"typedef struct {wire} {{")
            for member in record["fields"]:
                name, typ = member["name"], member["type"]
                if typ == "float":
                    lines.append(f"    double {name}; /* schema 'float' == C double (f64) */")
                elif typ == "string":
                    lines.append(f"    uint32_t {name}; /* byte offset -> inline NUL string (read via accessor) */")
                else:
                    lines.append(f"    {EVENT_FIELD_C_TYPE[typ]} {name};")
            lines.append(f"}} {wire};")
            lines.append("")
            lines.append(f"typedef struct {given} {{")
            for member in record["fields"]:
                name, typ = member["name"], member["type"]
                if typ == "float":
                    lines.append(f"    double {name};")
                elif typ == "string":
                    lines.append(f"    const char *{name};")
                else:
                    lines.append(f"    {EVENT_FIELD_C_TYPE[typ]} {name};")
            lines.append(f"}} {given};")
            lines.append("")
        return lines


    def render_event_accessors(self, evt: str, fields: list[dict[str, Any]], records: list[dict[str, Any]]) -> list[str]:
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
        for record in records:
            name = record["name"]
            wire = self.record_struct_c_name(evt, name)
            acc = self.event_accessor(evt, name)
            lines.append(f"static inline uint32_t {acc}_count(const {struct} *e) {{")
            lines.append(f"    return e->{name}_count;")
            lines.append("}")
            lines.append(f"static inline const {wire} *{acc}_at(const {struct} *e, uint32_t i) {{")
            lines.append(f"    return (const {wire} *)((const uint8_t *)e + e->{name}) + i;")
            lines.append("}")
            for member in record["fields"]:
                if member["type"] != "string":
                    continue
                lines.append(f"static inline const char *{acc}_{member['name']}(const {struct} *e, uint32_t i) {{")
                lines.append(f"    return (const char *)e + {acc}_at(e, i)->{member['name']};")
                lines.append("}")
        return lines


    def render_event_emit_args(self, evt: str, fields: list[dict[str, Any]], records: list[dict[str, Any]]) -> str:
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
        for record in records:
            name = record["name"]
            args.append(f"const {self.record_input_c_name(evt, name)} *{name}, uint32_t {name}_count")
        return ", ".join(args) if args else "void"


    def emit_alignment_expr(self, evt: str, records: list[dict[str, Any]]) -> str:
        """game_event_emit must see the strongest alignment in the payload: a record
        with an int64 member outranks an event struct built from 32-bit fields."""
        expr = f"_Alignof({self.event_struct_c_name(evt)})"
        for record in records:
            other = f"_Alignof({self.record_struct_c_name(evt, record['name'])})"
            expr = f"(({expr} > {other}) ? {expr} : {other})"
        return expr


    def render_record_overflow_guard(self, emit_fn: str, condition: str, indent: str = "    ") -> list[str]:
        return [
            f"{indent}if ({condition}) {{",
            f'{indent}    NT_ASSERT(0 && "{emit_fn} payload exceeds GAME_EVENT_EMIT_MAX");',
            f'{indent}    nt_log_warn("{emit_fn}: payload exceeds GAME_EVENT_EMIT_MAX (%u B) -> dropped", (unsigned)GAME_EVENT_EMIT_MAX);',
            f"{indent}    return NULL;",
            f"{indent}}}",
        ]


    def render_record_packing(self, evt: str, record: dict[str, Any], emit_fn: str) -> list[str]:
        """Reserve the record array at its own alignment, then fill records one by one,
        appending each record's strings after the array. Strings keep the payload base
        as their origin, so the array stays a flat POD run.

        The alignment gap is zeroed rather than skipped: the whole payload is copied
        into the log, so a skipped byte would make two identical emits differ."""
        name = record["name"]
        wire = self.record_struct_c_name(evt, name)
        lines: list[str] = [
            "",
            f"    const uint32_t {name}_n = ({name} != NULL) ? {name}_count : 0u;",
            f"    const uint32_t {name}_align = (uint32_t)_Alignof({wire});",
            f"    const uint32_t {name}_pad = (uint32_t)(((off + {name}_align - 1u) & ~({name}_align - 1u)) - off);",
            f"    memset(u.bytes + off, 0, {name}_pad);",
            f"    off += {name}_pad;",
        ]
        lines.extend(self.render_record_overflow_guard(
            emit_fn, f"(size_t)off + ((size_t){name}_n * sizeof({wire})) > sizeof(u.bytes)"
        ))
        lines.extend([
            f"    u.ev.{name} = off;",
            f"    u.ev.{name}_count = {name}_n;",
            f"    const uint32_t {name}_base = off;",
            f"    off += (uint32_t)((size_t){name}_n * sizeof({wire}));",
            f"    for (uint32_t {name}_i = 0; {name}_i < {name}_n; ++{name}_i) {{",
            f"        {wire} {name}_rec;",
            f"        memset(&{name}_rec, 0, sizeof {name}_rec);",
        ])
        for member in record["fields"]:
            member_name = member["name"]
            if member["type"] != "string":
                lines.append(f"        {name}_rec.{member_name} = {name}[{name}_i].{member_name};")
        for member in record["fields"]:
            if member["type"] != "string":
                continue
            member_name = member["name"]
            local = f"{name}_{member_name}"
            lines.extend([
                f"        const char *{local}_s = {name}[{name}_i].{member_name} ? {name}[{name}_i].{member_name} : \"\";",
                f"        size_t {local}_n = strlen({local}_s) + 1u; /* incl. NUL */",
            ])
            lines.extend(self.render_record_overflow_guard(
                emit_fn, f"(size_t)off + {local}_n > sizeof(u.bytes)", indent="        "
            ))
            lines.extend([
                f"        {name}_rec.{member_name} = off;",
                f"        memcpy(u.bytes + off, {local}_s, {local}_n);",
                f"        off += (uint32_t){local}_n;",
            ])
        lines.extend([
            f"        memcpy(u.bytes + {name}_base + ((size_t){name}_i * sizeof({wire})), &{name}_rec, sizeof {name}_rec);",
            "    }",
        ])
        return lines


    def render_event_emit_body(self, evt: str, fields: list[dict[str, Any]], records: list[dict[str, Any]]) -> list[str]:
        struct = self.event_struct_c_name(evt)
        type_fn = self.event_type_fn(evt)
        emit_fn = self.event_emit_fn(evt)
        lines: list[str] = []
        if not self.event_has_inline(fields, records):
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
        if terms:
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
        for record in records:
            lines.extend(self.render_record_packing(evt, record, emit_fn))
        align = self.emit_alignment_expr(evt, records)
        lines.append(f"    return game_event_emit({type_fn}(), &u, off, {align});")
        return lines


    def render_event_descriptor(self, evt: str, fields: list[dict[str, Any]], records: list[dict[str, Any]]) -> list[str]:
        struct = self.event_struct_c_name(evt)
        fields_arr = f"{self.ns.id}_ev_{evt}_fields"
        records_arr = f"{self.ns.id}_ev_{evt}_records"
        lines: list[str] = []
        for record in records:
            name = record["name"]
            wire = self.record_struct_c_name(evt, name)
            members_arr = f"{self.ns.id}_ev_{evt}_{name}_record_fields"
            lines.append(f"static const game_event_field_t {members_arr}[] = {{")
            for member in record["fields"]:
                ft = EVENT_FIELD_FT_ENUM[member["type"]]
                lines.append(
                    f'    {{ "{member["name"]}", {ft}, (uint32_t)offsetof({wire}, {member["name"]}), 0u }},'
                )
            lines.append("};")
        if records:
            lines.append(f"static const game_event_record_t {records_arr}[] = {{")
            for record in records:
                name = record["name"]
                wire = self.record_struct_c_name(evt, name)
                members_arr = f"{self.ns.id}_ev_{evt}_{name}_record_fields"
                lines.append(
                    f'    {{ "{name}", (uint32_t)offsetof({struct}, {name}), '
                    f"(uint32_t)offsetof({struct}, {name}_count), (uint32_t)sizeof({wire}), "
                    f"{members_arr}, (int)(sizeof({members_arr}) / sizeof({members_arr}[0])) }},"
                )
            lines.append("};")
        if fields:
            lines.append(f"static const game_event_field_t {fields_arr}[] = {{")
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
        if fields:
            lines.append(f"    {fields_arr},")
            lines.append(f"    (int)(sizeof({fields_arr}) / sizeof({fields_arr}[0])),")
        else:
            lines.append("    NULL,")
            lines.append("    0,")
        if records:
            lines.append(f"    {records_arr},")
            lines.append(f"    (int)(sizeof({records_arr}) / sizeof({records_arr}[0])),")
        else:
            lines.append("    NULL,")
            lines.append("    0,")
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
            records = self.event_records(spec)
            struct = self.event_struct_c_name(evt)
            parts.append(f"/* ---- {self.event_full_name(evt)} ---- */")
            parts.extend(self.render_record_structs(evt, records))
            parts.append(f"typedef struct {struct} {{")
            parts.extend(self.render_event_struct_fields(fields, records))
            parts.append(f"}} {struct};")
            parts.append("")
            parts.append(f'nt_hash64_t {self.event_type_fn(evt)}(void); /* nt_hash64_str("{self.event_full_name(evt)}"), cached */')
            parts.append("")
            parts.append(f"const void *{self.event_emit_fn(evt)}({self.render_event_emit_args(evt, fields, records)});")
            accessors = self.render_event_accessors(evt, fields, records)
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
        for evt, spec in events.items():
            structs = [self.event_struct_c_name(evt)]
            structs += [self.record_struct_c_name(evt, r["name"]) for r in self.event_records(spec)]
            for struct in structs:
                parts.append(f"_Static_assert(_Alignof({struct}) <= _Alignof(max_align_t),")
                parts.append(f'               "{struct} over-aligned for game_event_emit");')
        parts.append("")
        for evt, spec in events.items():
            fields = spec["fields"]
            records = self.event_records(spec)
            parts.append(f"/* ---- {self.event_full_name(evt)} ---- */")
            parts.append(f"nt_hash64_t {self.event_type_fn(evt)}(void) {{")
            parts.append("    static nt_hash64_t h;")
            parts.append(f'    if (!h.value) {{ h = nt_hash64_str("{self.event_full_name(evt)}"); }}')
            parts.append("    return h;")
            parts.append("}")
            parts.append("")
            parts.append(f"const void *{self.event_emit_fn(evt)}({self.render_event_emit_args(evt, fields, records)}) {{")
            parts.extend(self.render_event_emit_body(evt, fields, records))
            parts.append("}")
            parts.append("")
            parts.extend(self.render_event_descriptor(evt, fields, records))
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
