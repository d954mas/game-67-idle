#!/usr/bin/env python3
"""Generate const C item/container tables from the items content catalog.

Second codegen of the template (deliberately separate from
features/game-state/scripts/generate_state.py, design doc §9 "не смешивать"):
this one is compile-time content embed (const tables), not a save-state
fragment generator. Pattern mirrors games/rb-dark-rpg/tools/generate_dialogue_content.py.

    py -3.12 tools/generate_items_catalog.py \
        --catalog content/items.json --schema content/item_fields.schema.json \
        --out-dir <dir>

Emits <out-dir>/items_catalog.gen.h + items_catalog.gen.c. Idempotent
(write_if_changed); table order == document order (round-trip determinism).
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any

ACCEPT_POLICY_ENUM = {
    "any": "ITEM_ACCEPT_ANY",
    "currency_only": "ITEM_ACCEPT_CURRENCY_ONLY",
    "slot_filter": "ITEM_ACCEPT_SLOT_FILTER",
    "capacity_1": "ITEM_ACCEPT_CAPACITY_1",
}


def c_ident(value: str) -> str:
    ident = re.sub(r"[^A-Za-z0-9_]", "_", value).upper()
    if not ident or ident[0].isdigit():
        ident = f"_{ident}"
    return ident


def c_str(value: Any) -> str:
    if value is None:
        return "NULL"
    text = str(value)
    escaped = (
        text.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )
    return f'"{escaped}"'


def c_bool(value: Any) -> str:
    return "true" if bool(value) else "false"


def c_i64(value: Any) -> str:
    return f"{int(value)}LL"


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


# ---------------------------------------------------------------------------
# Validation against content/item_fields.schema.json (lightweight: required-field
# presence + block shape; NOT a generic JSON-schema engine -- op-slop `items_ops.py
# validate` (И2c) is the strict full-catalog gate; this is the generator's own
# sanity net so a broken catalog fails the build, not the runtime).
# ---------------------------------------------------------------------------


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"items catalog validation: {message}")


def validate_catalog(doc: dict[str, Any], field_schema: dict[str, Any]) -> None:
    require(isinstance(doc.get("namespace"), str) and doc["namespace"], "missing top-level 'namespace'")
    namespace = doc["namespace"]
    core = field_schema.get("core", {})
    blocks = field_schema.get("blocks", {})
    seen_ids: set[str] = set()

    for item in doc.get("items", []):
        item_id = item.get("id")
        require(isinstance(item_id, str) and item_id, f"item missing 'id': {item!r}")
        require(item_id not in seen_ids, f"duplicate item id {item_id!r}")
        seen_ids.add(item_id)
        require(
            item_id.startswith(f"{namespace}."),
            f"item id {item_id!r} does not match namespace {namespace!r} (expected '<namespace>.<slug>')",
        )
        for field_name, spec in core.items():
            if spec.get("required") and field_name != "stack":
                require(field_name in item, f"item {item_id!r} missing required field '{field_name}'")
        stack = item.get("stack")
        if core.get("stack", {}).get("required"):
            require(isinstance(stack, dict), f"item {item_id!r} missing required 'stack' block")
            require("stackable" in stack, f"item {item_id!r} stack block missing 'stackable'")
        for block_name, block_spec in blocks.items():
            block = item.get(block_name)
            if block is None:
                continue
            require(isinstance(block, dict), f"item {item_id!r} block '{block_name}' must be an object")
            for field_name, spec in block_spec.get("fields", {}).items():
                if spec.get("required"):
                    require(field_name in block, f"item {item_id!r} block '{block_name}' missing '{field_name}'")

    seen_containers: set[str] = set()
    accept_enum = field_schema.get("containers", {}).get("fields", {}).get("accept_policy", {}).get("enum", [])
    for container in doc.get("containers", []):
        cid = container.get("id")
        require(isinstance(cid, str) and cid, f"container missing 'id': {container!r}")
        require(cid not in seen_containers, f"duplicate container id {cid!r}")
        seen_containers.add(cid)
        policy = container.get("accept_policy")
        require(policy in accept_enum, f"container {cid!r} has unknown accept_policy {policy!r}")

    seen_kinds: set[str] = set()
    for kind in doc.get("item_kinds", []):
        kid = kind.get("id")
        require(isinstance(kid, str) and kid, f"item_kind missing 'id': {kind!r}")
        require(kid not in seen_kinds, f"duplicate item_kind id {kid!r}")
        seen_kinds.add(kid)
        require("label" in kind, f"item_kind {kid!r} missing 'label'")


# ---------------------------------------------------------------------------
# Codegen
# ---------------------------------------------------------------------------


def stack_fields(item: dict[str, Any]) -> tuple[bool, int, bool]:
    stack = item.get("stack") or {}
    has_equip = "equip" in item
    stackable = bool(stack.get("stackable", not has_equip))
    unlimited = bool(stack.get("unlimited", False))
    max_stack = int(stack.get("max_stack", 0))
    return stackable, max_stack, unlimited


def render_header(item_count: int, container_count: int) -> str:
    return "\n".join(
        [
            "#ifndef ITEMS_CATALOG_GEN_H",
            "#define ITEMS_CATALOG_GEN_H",
            "",
            "/* Generated by templates/template/tools/generate_items_catalog.py",
            "   from templates/template/content/items.json. Do not edit by hand. */",
            "",
            '#include "features/items/items.h"',
            "",
            f"#define ITEMS_CATALOG_ITEM_COUNT {item_count}",
            f"#define ITEMS_CATALOG_CONTAINER_COUNT {container_count}",
            "",
            "extern const game_item_def_t k_items[];",
            "extern const int k_items_count;",
            "extern const game_container_def_t k_containers[];",
            "extern const int k_containers_count;",
            "",
            "#endif /* ITEMS_CATALOG_GEN_H */",
            "",
        ]
    )


def render_source(doc: dict[str, Any]) -> str:
    items = doc.get("items", [])
    containers = doc.get("containers", [])
    lines: list[str] = [
        '#include "items_catalog.gen.h"',
        "",
        "#include <stddef.h> /* NULL */",
        "",
        "/* Generated by templates/template/tools/generate_items_catalog.py",
        "   from templates/template/content/items.json. Do not edit by hand. */",
        "",
    ]

    # Per-item static sub-tables (tags array + equip/use/currency blocks), only
    # when present -- mirrors the rb-dark game_content generator's NULL-when-absent style.
    block_refs: dict[str, dict[str, str]] = {}
    for item in items:
        sym = c_ident(item["id"])
        refs: dict[str, str] = {}
        tags = item.get("tags") or []
        if tags:
            tags_sym = f"TAGS_{sym}"
            lines.append(f"static const char *const {tags_sym}[] = {{")
            for tag in tags:
                lines.append(f"    {c_str(tag)},")
            lines.append("};")
            lines.append("")
            refs["tags"] = tags_sym
        equip = item.get("equip")
        if equip is not None:
            equip_sym = f"EQUIP_{sym}"
            lines.append(f"static const item_equip_block_t {equip_sym} = {{ .slot = {c_str(equip.get('slot'))} }};")
            lines.append("")
            refs["equip"] = equip_sym
        use = item.get("use")
        if use is not None:
            # L2-нота (§6.3): use.params лежит в JSON как документация будущего
            # (эпоха эффектов); C-структ несёт только effect_id.
            use_sym = f"USE_{sym}"
            lines.append(f"static const item_use_block_t {use_sym} = {{ .effect_id = {c_str(use.get('effect_id'))} }};")
            lines.append("")
            refs["use"] = use_sym
        currency = item.get("currency")
        if currency is not None:
            currency_sym = f"CURRENCY_{sym}"
            lines.append(
                f"static const item_currency_block_t {currency_sym} = "
                f"{{ .hud_hint = {c_str(currency.get('hud_hint'))}, .cap = {c_i64(currency.get('cap', 0))} }};"
            )
            lines.append("")
            refs["currency"] = currency_sym
        block_refs[item["id"]] = refs

    lines.append("const game_item_def_t k_items[] = {")
    for item in items:
        refs = block_refs[item["id"]]
        stackable, max_stack, unlimited = stack_fields(item)
        tags = item.get("tags") or []
        lines.append("    {")
        lines.append(f"        .id = {c_str(item.get('id'))},")
        lines.append(f"        .display_name = {c_str(item.get('display_name'))},")
        lines.append(f"        .icon_asset_id = {c_str(item.get('icon_asset_id'))},")
        lines.append(f"        .kind = {c_str(item.get('kind'))},")
        lines.append(f"        .tags = {refs.get('tags', 'NULL')},")
        lines.append(f"        .tag_count = {len(tags)},")
        lines.append(f"        .base_value = {c_i64(item.get('base_value', 0))},")
        lines.append(f"        .stackable = {c_bool(stackable)},")
        lines.append(f"        .max_stack = {c_i64(max_stack)},")
        lines.append(f"        .unlimited = {c_bool(unlimited)},")
        lines.append(f"        .equip = {('&' + refs['equip']) if 'equip' in refs else 'NULL'},")
        lines.append(f"        .use = {('&' + refs['use']) if 'use' in refs else 'NULL'},")
        lines.append(f"        .currency = {('&' + refs['currency']) if 'currency' in refs else 'NULL'},")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append("const int k_items_count = (int)(sizeof k_items / sizeof k_items[0]);")
    lines.append("")

    lines.append("const game_container_def_t k_containers[] = {")
    for container in containers:
        lines.append("    {")
        lines.append(f"        .id = {c_str(container.get('id'))},")
        lines.append(f"        .capacity = {c_i64(container.get('capacity', 0))},")
        lines.append(f"        .accept_policy = {ACCEPT_POLICY_ENUM[container['accept_policy']]},")
        lines.append(f"        .hidden = {c_bool(container.get('hidden', False))},")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append("const int k_containers_count = (int)(sizeof k_containers / sizeof k_containers[0]);")
    lines.append("")
    return "\n".join(lines)


def write_if_changed(path: Path, text: str) -> bool:
    old = path.read_text(encoding="utf-8") if path.exists() else None
    if old == text:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return True


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", required=True, help="Path to content/items.json.")
    parser.add_argument("--schema", required=True, help="Path to content/item_fields.schema.json.")
    parser.add_argument("--out-dir", required=True, help="Directory for generated items_catalog.gen.{h,c}.")
    args = parser.parse_args(argv)

    catalog_path = Path(args.catalog).resolve()
    schema_path = Path(args.schema).resolve()
    out_dir = Path(args.out_dir).resolve()

    doc = load_json(catalog_path)
    field_schema = load_json(schema_path)
    validate_catalog(doc, field_schema)

    header_path = out_dir / "items_catalog.gen.h"
    source_path = out_dir / "items_catalog.gen.c"

    changed = []
    if write_if_changed(header_path, render_header(len(doc.get("items", [])), len(doc.get("containers", [])))):
        changed.append(header_path)
    if write_if_changed(source_path, render_source(doc)):
        changed.append(source_path)

    if changed:
        print(f"generate_items_catalog: wrote {', '.join(str(p) for p in changed)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
