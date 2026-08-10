#!/usr/bin/env python3
"""Generate const C progression-track tables from the Items Snapshot.

Tracks are authored in Lua beside items (`studio.tracks`) and reach this
generator through the `tracks` section of `items.snapshot.v1`. This script bakes
each track's per-level price, grants, and column values into compile-time const
tables so the runtime never evaluates a formula.

    node ai_studio/dev_environment/python_run.mjs \
        features/progression-core/scripts/generate_progression_tracks.py \
        --snapshot build/items/items.snapshot.json \
        --state-schema state/progression.schema.json \
        --out-dir <dir>

Emits <out-dir>/progression_tracks.gen.{h,c}. Idempotent (write_if_changed);
table order is the Snapshot's sorted order, so the output is round-trip stable.

Level bases differ on purpose and are converted here once: the Snapshot is
1-based (row 1 is the un-upgraded state), the runtime is 0-based (`steps[L]`
leaves level L). `max_level` is therefore `len(rows) - 1`.
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

# Names the generated header defines itself; a column claiming one would redefine it.
RESERVED_VALUE_IDENTS = {"COUNT"}

# Width of progression_track_def_t.owned_values, the per-track column ownership mask.
OWNED_VALUE_BITS = 64

# Mirrors ITEMS_PAYMENT_MAX_REQUIREMENTS in features/items-core (a C macro this
# script cannot include). A longer price bakes fine and then refuses to pay, so
# the build is where it must be caught.
ITEMS_PAYMENT_MAX_REQUIREMENTS = 16

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
    escaped = (
        str(value)
        .replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )
    return f'"{escaped}"'


def c_double(value: float) -> str:
    """Round-trip precision: the value is computed once here and read back exactly."""
    return repr(float(value))


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"progression tracks validation: {message}")


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def validate_state_schema(doc: Any) -> tuple[int, int, int]:
    """Return the storage bounds the owning save schema imposes.

    (max track id bytes, max track count, max level). Every one of them silently
    truncates or drops on save if the catalog exceeds it, so all three are read
    from the game-owned schema rather than duplicated here."""
    prefix = "progression state schema validation"

    def schema_require(condition: bool, message: str) -> None:
        if not condition:
            raise SystemExit(f"{prefix}: {message}")

    schema_require(isinstance(doc, dict), "document must be an object")
    schema_require(doc.get("schema") == "game_seed.progression", "schema must be 'game_seed.progression'")
    schema_require(doc.get("fragment") == "progression", "fragment must be 'progression'")
    string_max = doc.get("string_max")
    schema_require(
        isinstance(string_max, int) and not isinstance(string_max, bool) and string_max >= 2,
        "string_max must be an integer >= 2",
    )
    track_state = doc.get("types", {}).get("TrackState") if isinstance(doc.get("types"), dict) else None
    schema_require(
        isinstance(track_state, dict) and track_state.get("kind") == "object",
        "TrackState must be an object type",
    )
    tracks = doc.get("fields", {}).get("tracks") if isinstance(doc.get("fields"), dict) else None
    schema_require(
        isinstance(tracks, dict) and tracks.get("type") == "map<string,TrackState>",
        "fields.tracks must have type 'map<string,TrackState>'",
    )
    max_count = tracks.get("max_count")
    schema_require(
        isinstance(max_count, int) and not isinstance(max_count, bool) and max_count >= 1,
        "fields.tracks.max_count must be an integer >= 1",
    )
    level = track_state.get("fields", {}).get("level") if isinstance(track_state.get("fields"), dict) else None
    level_max = level.get("max") if isinstance(level, dict) else None
    schema_require(
        isinstance(level_max, int) and not isinstance(level_max, bool) and level_max >= 1,
        "TrackState.level must declare an integer max >= 1",
    )
    return string_max - 1, max_count, level_max


def track_columns(snapshot: dict[str, Any], tracks: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """The column dictionary: fields at least one track kind requires, in id order.

    A field only item kinds require belongs to the item catalog and is not a
    progression column, exactly as a track column is absent from that catalog."""
    kinds = {track["kind"] for track in tracks}
    fields = snapshot.get("fields")
    require(isinstance(fields, list), "snapshot 'fields' must be an array")
    columns = [
        field for field in fields
        if isinstance(field, dict) and any(kind in field.get("required_for", []) for kind in kinds)
    ]
    # One dictionary serves every track and one row carries a slot per column, so a
    # column no track kind declares is a slot every track pays for and nobody reads.
    require(
        len(columns) <= OWNED_VALUE_BITS,
        f"a game may declare at most {OWNED_VALUE_BITS} progression columns "
        f"(progression_track_def_t.owned_values is a {OWNED_VALUE_BITS}-bit mask); "
        f"{len(columns)} are required by track kinds",
    )
    members: set[str] = set()
    for column in columns:
        member = column.get("member")
        require(
            isinstance(member, str) and member not in members,
            f"progression column member is missing or duplicated: {member!r}",
        )
        require(
            column.get("type") in ("i64", "f64"),
            f"progression column {member} must be i64 or f64",
        )
        require(
            c_ident(member) not in RESERVED_VALUE_IDENTS,
            f"progression column {member!r} would redefine PROGRESSION_VALUE_{c_ident(member)}",
        )
        members.add(member)
    return sorted(columns, key=lambda column: column["id"])


def validate_tracks(
    tracks: list[dict[str, Any]], max_track_id_len: int, max_track_count: int, max_level_cap: int,
) -> None:
    require(
        len(tracks) > 0,
        "the snapshot declares no tracks; a catalog with none would emit a zero-length "
        "k_tracks[], which is not valid C",
    )
    require(
        len(tracks) <= max_track_count,
        f"the catalog has {len(tracks)} tracks but the save schema stores at most "
        f"{max_track_count}; the tracks past that would silently never persist",
    )
    seen_symbols: dict[str, str] = {}
    for track in tracks:
        track_id = track["id"]
        # The generated state key is a NUL-terminated char[string_max]; read the
        # authoritative game-owned schema instead of duplicating that bound.
        id_bytes = len(track_id.encode("utf-8"))
        require(
            id_bytes <= max_track_id_len,
            f"track id {track_id!r} is {id_bytes} UTF-8 bytes, exceeds the maximum "
            f"{max_track_id_len} derived from state schema string_max={max_track_id_len + 1}",
        )
        # Two ids that sanitize to one C identifier would define one table twice.
        symbol = c_ident(track_id)
        require(
            symbol not in seen_symbols,
            f"track ids {seen_symbols.get(symbol)!r} and {track_id!r} both sanitize to "
            f"the C identifier {symbol!r} -- rename one",
        )
        seen_symbols[symbol] = track_id

        max_level = len(track["levels"]["rows"]) - 1
        require(
            1 <= max_level <= max_level_cap,
            f"track {track_id!r} reaches level {max_level}; the save schema caps 'level' at "
            f"{max_level_cap}, and a higher one would silently clamp on save",
        )
        for index, row in enumerate(track["levels"]["rows"][1:]):
            entries = quantity_entries(row.get("cost_to_reach"))
            require(
                len(entries) <= ITEMS_PAYMENT_MAX_REQUIREMENTS,
                f"track {track_id!r} prices level {index + 1} in {len(entries)} items; a payment "
                f"carries at most {ITEMS_PAYMENT_MAX_REQUIREMENTS}, so the level would be "
                "unbuyable at runtime",
            )


def quantity_entries(value: Any) -> list[tuple[str, int]]:
    """A normalized cost/grant list as (def_id, amount), in its declared order."""
    if not isinstance(value, dict) or value.get("__studio_kind") == "free":
        return []
    entries = [value] if value.get("__studio_kind") == "cost" else value["entries"]
    result = []
    for entry in entries:
        amount = entry["count"]
        require(
            isinstance(amount, int) and not isinstance(amount, bool) and 0 < amount <= INT64_MAX,
            "quantity amount must be a positive int64",
        )
        result.append((entry["item"]["id"], amount))
    return result


def render_header(columns: list[dict[str, Any]], track_count: int, provenance: str) -> str:
    lines = [
        "#ifndef PROGRESSION_TRACKS_GEN_H",
        "#define PROGRESSION_TRACKS_GEN_H",
        "",
        "/* Generated by features/progression-core/scripts/generate_progression_tracks.py",
        f"   from {provenance}. Do not edit by hand. */",
        "",
        '#include "features/progression/progression.h"',
        "",
        f"#define PROGRESSION_TRACK_COUNT {track_count}",
        f"#define PROGRESSION_VALUE_COUNT {len(columns)}",
        "",
    ]
    for index, column in enumerate(columns):
        lines.append(
            f"#define PROGRESSION_VALUE_{c_ident(column['member'])} ((progression_value_t){index}u)"
        )
    lines.extend([
        "",
        "/* Which read a column answers; the mismatched one traps in every build. */",
        "extern const bool k_progression_value_exact[];",
        "",
        "extern const progression_track_def_t k_tracks[];",
        "extern const int k_tracks_count;",
        "",
        "#endif /* PROGRESSION_TRACKS_GEN_H */",
        "",
    ])
    return "\n".join(lines)


def render_source(
    tracks: list[dict[str, Any]], columns: list[dict[str, Any]], provenance: str,
) -> str:
    lines = [
        '#include "progression_tracks.gen.h"',
        "",
        "#include <stddef.h> /* NULL */",
        "#include <stdint.h> /* UINT64_C */",
        "",
        "/* Generated by features/progression-core/scripts/generate_progression_tracks.py",
        f"   from {provenance}. Do not edit by hand. */",
        "",
    ]
    if columns:
        lines.append("const bool k_progression_value_exact[] = {")
        lines.extend(f"    {'true' if column['type'] == 'i64' else 'false'}," for column in columns)
        lines.append("};")
    else:
        # A zero-length array is invalid C; nothing indexes it when the count is 0.
        lines.append("const bool k_progression_value_exact[1] = { false };")
    lines.append("")

    members = [column["member"] for column in columns]
    exact_members = [column["member"] for column in columns if column["type"] == "i64"]

    for track in tracks:
        symbol = c_ident(track["id"])
        rows = track["levels"]["rows"]
        steps = rows[1:]
        for index, row in enumerate(steps):
            for kind, member in (("cost", "cost_to_reach"), ("grants", "grants")):
                entries = quantity_entries(row.get(member))
                if not entries:
                    continue
                lines.append(f"static const progression_amount_t {kind.upper()}_{symbol}_{index}[] = {{")
                lines.extend(
                    f"    {{ {c_str(def_id)}, {amount}LL }}," for def_id, amount in entries
                )
                lines.append("};")
        lines.append(f"static const progression_step_t STEPS_{symbol}[] = {{")
        for index, row in enumerate(steps):
            cost = quantity_entries(row.get("cost_to_reach"))
            grants = quantity_entries(row.get("grants"))
            cost_sym = f"COST_{symbol}_{index}" if cost else "NULL"
            grant_sym = f"GRANTS_{symbol}_{index}" if grants else "NULL"
            xp_cost = row.get("xp_to_reach", 0)
            lines.append(
                f"    {{ {cost_sym}, {len(cost)}, {xp_cost}LL, {grant_sym}, {len(grants)} }},"
            )
        lines.append("};")
        if exact_members:
            lines.append(f"static const int64_t EXACT_{symbol}[] = {{")
            for row in rows:
                values = ", ".join(
                    f"{row.get(member, 0)}LL" if member in exact_members else "0LL"
                    for member in members
                )
                lines.append(f"    {values},")
            lines.append("};")
        if len(exact_members) != len(members):
            lines.append(f"static const double FRACTIONAL_{symbol}[] = {{")
            for row in rows:
                values = ", ".join(
                    c_double(row.get(member, 0.0)) if member not in exact_members else "0.0"
                    for member in members
                )
                lines.append(f"    {values},")
            lines.append("};")
        lines.append("")

    lines.append("const progression_track_def_t k_tracks[] = {")
    for track in tracks:
        symbol = c_ident(track["id"])
        max_level = len(track["levels"]["rows"]) - 1
        exact = f"EXACT_{symbol}" if exact_members else "NULL"
        fractional = f"FRACTIONAL_{symbol}" if len(exact_members) != len(members) else "NULL"
        owned = [
            f"(UINT64_C(1) << PROGRESSION_VALUE_{c_ident(column['member'])})"
            for column in columns
            if track["kind"] in column["required_for"]
        ]
        lines.extend([
            "    {",
            f"        .id = {c_str(track['id'])},",
            f"        .mode = {MODE_ENUM[track['mode']]},",
            f"        .max_level = {max_level},",
            f"        .steps = STEPS_{symbol},",
            f"        .exact = {exact},",
            f"        .fractional = {fractional},",
            f"        .value_count = {len(members)}u,",
            f"        .owned_values = {' | '.join(owned) if owned else '0u'},",
            "    },",
        ])
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
    tmp_path = path.with_name(path.name + ".tmp")  # atomic: a crash leaves the old file
    tmp_path.write_text(text, encoding="utf-8")
    os.replace(tmp_path, path)
    return True


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--snapshot", required=True, help="Path to the items.snapshot.v1 build output.")
    parser.add_argument(
        "--state-schema", required=True,
        help="Path to the authoritative game-owned state/progression.schema.json.",
    )
    parser.add_argument("--out-dir", required=True, help="Directory for progression_tracks.gen.{h,c}.")
    args = parser.parse_args(argv)

    snapshot_path = Path(args.snapshot).resolve()
    state_schema_path = Path(args.state_schema).resolve()
    out_dir = Path(args.out_dir).resolve()

    try:
        snapshot = load_json(snapshot_path)
        require(
            isinstance(snapshot, dict) and snapshot.get("schema") == "items.snapshot.v1",
            "snapshot schema must be 'items.snapshot.v1'",
        )
        tracks = snapshot.get("tracks")
        require(isinstance(tracks, list), "snapshot 'tracks' must be an array")
        max_track_id_len, max_track_count, max_level_cap = validate_state_schema(
            load_json(state_schema_path))
        validate_tracks(tracks, max_track_id_len, max_track_count, max_level_cap)
        columns = track_columns(snapshot, tracks)

        provenance = "the Items Snapshot tracks section"
        header_text = render_header(columns, len(tracks), provenance)
        source_text = render_source(tracks, columns, provenance)
    except SystemExit:
        raise
    except (TypeError, KeyError, ValueError, AttributeError, IndexError) as exc:
        raise SystemExit(f"progression tracks validation: {exc}") from exc

    changed = []
    for path, text in (
        (out_dir / "progression_tracks.gen.h", header_text),
        (out_dir / "progression_tracks.gen.c", source_text),
    ):
        if write_if_changed(path, text):
            changed.append(path)
    if changed:
        print(f"generate_progression_tracks: wrote {', '.join(str(p) for p in changed)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
