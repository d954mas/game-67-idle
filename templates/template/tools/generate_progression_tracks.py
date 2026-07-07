#!/usr/bin/env python3
"""Generate const C progression-track tables from the progression content catalog.

Content-codegen sibling of tools/generate_items_catalog.py (same helpers,
same write_if_changed/SystemExit discipline) -- this one bakes each track's
`curve` PRESET into a compile-time `int64_t cost[]` table so the runtime
never runs float/formula math for level-ups (build_spec_t0327_i3 §5.2/§2.3).

    py -3.12 tools/generate_progression_tracks.py \
        --catalog content/progression.json \
        --items content/items.json \
        --out-dir <dir>

Emits <out-dir>/progression_tracks.gen.h + progression_tracks.gen.c. Idempotent
(write_if_changed); table order == document order (round-trip determinism).

LEAN-порезы (build_spec §5.2, ратифицировано лидом):
- порез B: curve.type поддержан РОВНО один -- "exp". Любой другой тип -- SystemExit
  громко (linear/table/poly отвергаются, а не тихо игнорятся).
- порез A: `on_level_up` в JSON -- SystemExit громко (codegen его НЕ печёт; наличие
  в JSON -- ошибка авторства, не тихый no-op). Каждый испечённый трек ВСЕГДА несёт
  `.on_level_up = NULL, .on_level_up_count = 0`.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path
from typing import Any

INT64_MAX = 2**63 - 1

# state/progression.schema.json: string_max=64 -> char key[64] (NUL-terminated).
# Kept as a documented constant here (this codegen deliberately never reads the
# state schema, §9 "не смешивать") -- deep-review #2.
MAX_TRACK_ID_LEN = 63

MODE_ENUM = {
    "manual": "PROGRESSION_MODE_MANUAL",
    "auto": "PROGRESSION_MODE_AUTO",
    "threshold": "PROGRESSION_MODE_THRESHOLD",
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


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"progression tracks validation: {message}")


# ---------------------------------------------------------------------------
# Validation (lightweight generator sanity net, mirrors generate_items_catalog.py)
# ---------------------------------------------------------------------------


def index_currency_items(items_doc: dict[str, Any]) -> dict[str, bool]:
    """def_id -> is_currency (has a `currency` block), from content/items.json."""
    index: dict[str, bool] = {}
    for item in items_doc.get("items", []):
        item_id = item.get("id")
        if isinstance(item_id, str) and item_id:
            index[item_id] = item.get("currency") is not None
    return index


def validate_catalog(doc: dict[str, Any], currency_items: dict[str, bool]) -> None:
    require(isinstance(doc.get("namespace"), str) and doc["namespace"], "missing top-level 'namespace'")
    tracks = doc.get("tracks")
    require(isinstance(tracks, list), "'tracks' must be an array")

    seen_ids: set[str] = set()
    seen_symbols: dict[str, str] = {}  # c_ident(track_id) -> first track_id that claimed it
    for track in tracks:
        require(isinstance(track, dict), f"track entry must be an object: {track!r}")
        track_id = track.get("id")
        require(isinstance(track_id, str) and track_id, f"track missing 'id': {track!r}")
        require(track_id not in seen_ids, f"duplicate track id {track_id!r}")
        seen_ids.add(track_id)

        # M-fix (deep-review #2): a track_id must fit the progression state
        # fragment's save key (state/progression.schema.json string_max=64,
        # NUL-terminated char[64] -- MAX_TRACK_ID_LEN mirrors that -1 for the
        # terminator, same convention as items' ITEMS_STATE_STRING_MAX). This
        # codegen tool does not read the state schema (deliberately separate
        # codegen, §9 "не смешивать"), so the bound is a documented constant
        # here, not derived. A truncated key would desync from progression.c's
        # find_track lookup by full id -- reject loudly at authoring time
        # instead of relying only on the runtime defense-in-depth guard.
        require(
            len(track_id) <= MAX_TRACK_ID_LEN,
            f"track id {track_id!r} is {len(track_id)} chars, exceeds MAX_TRACK_ID_LEN={MAX_TRACK_ID_LEN} "
            "(must fit the progression state fragment's string_max=64 save key)",
        )

        # L-fix (deep-review #7): two distinct ids that sanitize to the SAME C
        # identifier (e.g. "hero-1" and "hero.1" both -> HERO_1) would emit two
        # `static const int64_t COST_HERO_1[]` definitions -- a silent
        # redefinition the C compiler either rejects with a confusing error or
        # (worse, if only one made it into the translation unit some other way)
        # resolves to the WRONG track's cost table. Reject at authoring time.
        symbol = c_ident(track_id)
        require(
            symbol not in seen_symbols,
            f"track ids {seen_symbols.get(symbol)!r} and {track_id!r} both sanitize to the C identifier "
            f"{symbol!r} (COST_{symbol} would be defined twice) -- rename one",
        )
        seen_symbols[symbol] = track_id

        mode = track.get("mode")
        require(mode in MODE_ENUM, f"track {track_id!r} has unknown mode {mode!r} (expected manual/auto/threshold)")

        currency_def = track.get("currency_def")
        if mode in ("manual", "auto"):
            require(
                isinstance(currency_def, str) and currency_def,
                f"track {track_id!r} (mode {mode!r}) requires 'currency_def'",
            )
            require(
                currency_def in currency_items,
                f"track {track_id!r} currency_def {currency_def!r} not found in items catalog",
            )
            require(
                currency_items[currency_def],
                f"track {track_id!r} currency_def {currency_def!r} is not a currency item",
            )
        # threshold: currency_def is absent/ignored -- no existence check (§5.1).

        max_level = track.get("max_level")
        require(
            isinstance(max_level, int) and not isinstance(max_level, bool) and 1 <= max_level <= 9999,
            f"track {track_id!r} max_level must be an integer in [1, 9999] (got {max_level!r}); "
            "schema-level 'level' is capped at 9999, a higher max_level would silently clamp on save",
        )

        require("on_level_up" not in track, f"track {track_id!r} declares 'on_level_up' -- codegen does not bake it in И3 (LEAN-порез A); remove it from content/progression.json")

        curve = track.get("curve")
        require(isinstance(curve, dict), f"track {track_id!r} missing 'curve' object")
        curve_type = curve.get("type")
        require(
            curve_type == "exp",
            f"unknown curve type {curve_type!r} for track {track_id!r} (only 'exp' supported in И3)",
        )
        for key in ("base", "growth_num", "growth_den"):
            value = curve.get(key)
            # L-fix (deep-review #6): base was allowed to be 0, which bakes
            # cost[L]==0 for every level -- an auto-track with a free (0-cost)
            # curve levels to its cap in one progression_update() call, for
            # free, every frame it is below max_level. base must be >= 1, same
            # as growth_num/growth_den (a curve with a real, positive floor).
            require(
                isinstance(value, int) and not isinstance(value, bool) and value >= 1,
                f"track {track_id!r} curve.{key} must be a positive integer (>=1), got {value!r}",
            )


# ---------------------------------------------------------------------------
# Curve baking (§2.3/§5.2: FLOOR, pure-int arithmetic, int64 overflow -> SystemExit)
# ---------------------------------------------------------------------------


def bake_exp_cost(track_id: str, base: int, growth_num: int, growth_den: int, max_level: int) -> list[int]:
    costs: list[int] = []
    for level in range(max_level):
        # cost[L] = floor(base * (growth_num/growth_den) ** L), computed as pure
        # int arithmetic (base * growth_num**L) // (growth_den**L) -- FLOOR by
        # construction, zero float-rounding risk.
        value = (base * (growth_num**level)) // (growth_den**level)
        if value > INT64_MAX:
            raise SystemExit(
                f"progression tracks validation: track {track_id!r} cost overflow at level {level} "
                f"(value {value} exceeds int64 max {INT64_MAX})"
            )
        costs.append(value)
    return costs


# ---------------------------------------------------------------------------
# Codegen
# ---------------------------------------------------------------------------


def render_header(track_count: int) -> str:
    return "\n".join(
        [
            "#ifndef PROGRESSION_TRACKS_GEN_H",
            "#define PROGRESSION_TRACKS_GEN_H",
            "",
            "/* Generated by templates/template/tools/generate_progression_tracks.py",
            "   from templates/template/content/progression.json. Do not edit by hand. */",
            "",
            '#include "features/progression/progression.h"',
            "",
            f"#define PROGRESSION_TRACK_COUNT {track_count}",
            "",
            "extern const progression_track_def_t k_tracks[];",
            "extern const int k_tracks_count;",
            "",
            "#endif /* PROGRESSION_TRACKS_GEN_H */",
            "",
        ]
    )


def render_source(doc: dict[str, Any]) -> str:
    tracks = doc.get("tracks", [])
    lines: list[str] = [
        '#include "progression_tracks.gen.h"',
        "",
        "#include <stddef.h> /* NULL */",
        "",
        "/* Generated by templates/template/tools/generate_progression_tracks.py",
        "   from templates/template/content/progression.json. Do not edit by hand. */",
        "",
    ]

    cost_syms: dict[str, str] = {}
    for track in tracks:
        track_id = track["id"]
        sym = c_ident(track_id)
        curve = track["curve"]
        max_level = track["max_level"]
        costs = bake_exp_cost(track_id, curve["base"], curve["growth_num"], curve["growth_den"], max_level)
        cost_sym = f"COST_{sym}"
        lines.append(f"static const int64_t {cost_sym}[] = {{")
        for value in costs:
            lines.append(f"    {value}LL,")
        lines.append("};")
        lines.append("")
        cost_syms[track_id] = cost_sym

    lines.append("const progression_track_def_t k_tracks[] = {")
    for track in tracks:
        track_id = track["id"]
        mode = track["mode"]
        currency_def = track.get("currency_def") if mode in ("manual", "auto") else None
        lines.append("    {")
        lines.append(f"        .id = {c_str(track_id)},")
        lines.append(f"        .mode = {MODE_ENUM[mode]},")
        lines.append(f"        .currency_def = {c_str(currency_def)},")
        lines.append(f"        .max_level = {track['max_level']},")
        lines.append(f"        .cost = {cost_syms[track_id]},")
        lines.append(f"        .cost_count = {track['max_level']},")
        lines.append("        .on_level_up = NULL,")  # LEAN-порез A: codegen never bakes on_level_up
        lines.append("        .on_level_up_count = 0,")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append("const int k_tracks_count = (int)(sizeof k_tracks / sizeof k_tracks[0]);")
    lines.append("")
    return "\n".join(lines)


def write_if_changed(path: Path, text: str) -> bool:
    old = path.read_text(encoding="utf-8") if path.exists() else None
    if old == text:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp_path = path.with_name(path.name + ".tmp")  # atomic write (crash mid-write leaves the old file intact)
    tmp_path.write_text(text, encoding="utf-8")
    os.replace(tmp_path, path)
    return True


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", required=True, help="Path to content/progression.json.")
    parser.add_argument("--items", required=True, help="Path to content/items.json (currency_def cross-check).")
    parser.add_argument("--out-dir", required=True, help="Directory for generated progression_tracks.gen.{h,c}.")
    args = parser.parse_args(argv)

    catalog_path = Path(args.catalog).resolve()
    items_path = Path(args.items).resolve()
    out_dir = Path(args.out_dir).resolve()

    try:
        doc = load_json(catalog_path)
        items_doc = load_json(items_path)
        currency_items = index_currency_items(items_doc)
        validate_catalog(doc, currency_items)

        header_path = out_dir / "progression_tracks.gen.h"
        source_path = out_dir / "progression_tracks.gen.c"

        header_text = render_header(len(doc.get("tracks", [])))
        source_text = render_source(doc)
    except SystemExit:
        raise
    except (TypeError, KeyError, ValueError, AttributeError) as exc:
        raise SystemExit(f"progression tracks validation: {exc}") from exc

    changed = []
    if write_if_changed(header_path, header_text):
        changed.append(header_path)
    if write_if_changed(source_path, source_text):
        changed.append(source_path)

    if changed:
        print(f"generate_progression_tracks: wrote {', '.join(str(p) for p in changed)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
