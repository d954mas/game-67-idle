#!/usr/bin/env python3
"""Items content op-layer CLI (T0327 И2c, T0381 receipt extension).

Single op-layer that the future T0316 web editor will sit on top of (tool
parity): subprocess + `--json` gives the Node editor the exact same
answers this CLI gives a human. Catalog operations stay read-only; the one write
is the atomic, idempotent legacy-lock to release-receipt upgrade.

Moved to features/items-core/scripts/ in T0337; its own argparse defaults are
now script-relative to features/items-core/, NOT the game/template root, so
every path below must be passed explicitly (never rely on the defaults).
Canonical invocation runs from the game/template root, e.g. templates/template/
(see src/features/items/README.md "Content workflow"):

    node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_ops.py list     --catalog <game-root>/content/items.json [--json]
    node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_ops.py validate --catalog <game-root>/content/items.json --schema <game-root>/content/item_fields.schema.json --baseline <game-root>/content/items.lock.json --state-schema <game-root>/state/items.schema.json --src-dir <game-root>/src/features/items [--json]
    node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_ops.py schema   --schema <game-root>/content/item_fields.schema.json [--json]

Shares its data model with features/items-core/scripts/generate_items_catalog.py by IMPORTING it
(never re-parsing the catalog with a second, forkable set of rules): `list`
and `validate` reuse generate_items_catalog.load_json/validate_catalog/
render_header/render_source so the codegen's own sanity net (namespace
prefix, duplicate ids, required fields, container accept_policy, i64
integer-ness) always runs first, and this CLI only ADDS strictly stronger
checks on top (lock-file removal workflow, full namespace-pattern regex,
composite-key length, equip/stack sanity, display_name-keying lint) --
see src/features/items/README.md ("Lock workflow" -- deleting/renaming a
SHIPPED def_id is a destructive action that must FORCE an explicit developer
reaction, lead-ratified 2026-07-07). Never contradicts the generator; only
tightens it.

`validate --json` errors/warnings are structured objects
`{rule, id, field, msg}` (stable kebab-case `rule` ids: "generator-check",
"namespace", "created-missing", "created-invalid", "removed-without-reaction",
"removed-version-not-shipped", "lock-invalid", "lock-inconsistent",
"storage-change-without-reaction", "level-shrink-without-reaction",
"removed-def-restored" (warning), "composite-key-length", "equip-stack",
"display-name-keying", "rename-guard-skipped") -- not free strings, so the
future web editor (T0316) can key UI off `rule`/`id` instead of parsing text.

Exit codes: 0 = OK, 1 = validation FAIL, 2 = usage/IO error. A lock baseline
that is MISSING because it was never given (default path absent) is NOT an
IO error -- it prints a warning (lock-file checks skipped, other checks
still run); an EXPLICITLY passed `--baseline` path that does not exist, OR a
baseline that exists but has the WRONG SHAPE (unsupported schema version,
invalid def_ids/receipt, or removed not an object), IS an IO error (exit 2, consistent with
`--catalog`/`--schema`/`--state-schema`) -- a broken lock file must never
SILENTLY disable the destructive-change guard.

This CLI is wired into `ctest` as `items_ops_validate` (CMakeLists.txt) --
deleting a shipped def_id without reacting now fails the build's test suite
automatically, not just a manual run. `items_ops_test.py` (unittest,
precedent features/game-state/scripts/generate_state_test.py) is a second
ctest target covering the lock-workflow rules directly against temp fixtures.
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import re
import sys
from pathlib import Path
from typing import Any

# Same directory as generate_items_catalog.py -- import it directly instead of
# re-implementing catalog parsing/validation: both surfaces share one catalog
# schema/source of truth without duplicating a second data model.
sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_items_catalog as gen  # noqa: E402

TEMPLATE_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_CATALOG = TEMPLATE_ROOT / "content" / "items.json"
DEFAULT_FIELD_SCHEMA = TEMPLATE_ROOT / "content" / "item_fields.schema.json"
DEFAULT_LOCK = TEMPLATE_ROOT / "content" / "items.lock.json"
DEFAULT_STATE_SCHEMA = TEMPLATE_ROOT / "state" / "items.schema.json"
DEFAULT_SRC_DIR = TEMPLATE_ROOT / "src" / "features" / "items"

# A removal always corresponds to a real state/items.schema.json version bump
# (fragment version 1 is the initial no-migrations skeleton -- there is no
# earlier version a removal could have shipped in).
MIN_REMOVED_FRAGMENT_VERSION = 2

# state/items.schema.json string_max=64 -> ITEMS_STATE_STRING_MAX=64 -> a
# NUL-terminated C string holds at most 63 payload chars (deep-review L2/L1;
# Composite stack key is "<container>/<def_id>".
OWNED_KEY_MAX_LEN = 63

# Lead reversal 2026-07-07: git history was rejected as an unreliable source
# of "when was this def created" (copy-then-own resets it, T0327 И2c) -- the
# creation date lives IN THE DATA instead. "YYYY-MM-DD", checked for both
# shape (regex) and calendar validity (datetime.date.fromisoformat below).
CREATED_DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
RECEIPT_SCHEMA_VERSION = 3
RECEIPT_SCHEMA = "items.release_receipt.v1"
ITEMS_CORE_VERSION = "1.6.0"

Issue = dict  # {"rule": str, "id": str | None, "field": str | None, "msg": str}


def issue(rule: str, msg: str, *, id: str | None = None, field: str | None = None) -> Issue:  # noqa: A002
    return {"rule": rule, "id": id, "field": field, "msg": msg}


def format_issue(entry: Issue) -> str:
    where = "".join(f" {k}={v!r}" for k, v in (("id", entry.get("id")), ("field", entry.get("field"))) if v)
    return f"[{entry['rule']}]{where}: {entry['msg']}"


class OpsError(Exception):
    """Usage/IO problem (bad path, malformed JSON) -- exit code 2."""


def load_json_or_die(path: Path, what: str) -> Any:
    try:
        return gen.load_json(path)
    except FileNotFoundError as exc:
        raise OpsError(f"{what} not found: {path}") from exc
    except json.JSONDecodeError as exc:
        raise OpsError(f"{what} is not valid JSON ({path}): {exc}") from exc


def _receipt_text(
    catalog: dict[str, Any],
    field_schema: dict[str, Any],
    state_schema: dict[str, Any],
    legacy: dict[str, Any],
) -> str:
    errors = run_generator_checks(catalog, field_schema)
    if errors:
        raise OpsError(format_issue(errors[0]))
    def_ids = legacy.get("def_ids")
    removed = legacy.get("removed")
    if legacy.get("schema_version") != 2 or not isinstance(def_ids, list) or not isinstance(removed, dict):
        raise OpsError("upgrade-receipt requires an items.lock.json schema_version 2 baseline")
    if not all(isinstance(item_id, str) for item_id in def_ids) or len(def_ids) != len(set(def_ids)):
        raise OpsError("legacy lock def_ids must be unique strings")
    items = {item.get("id"): item for item in catalog.get("items", []) if isinstance(item, dict)}
    missing = sorted(set(def_ids) - set(items))
    if missing:
        raise OpsError(f"cannot seed receipt for missing shipped def_id {missing[0]!r}")

    state_version = state_schema.get("version")
    state_schema_version = state_schema.get("schema_version")
    if type(state_version) is not int or type(state_schema_version) is not int:
        raise OpsError("items state schema requires integer schema_version and version")

    receipt: dict[str, Any] = {
        "schema": legacy.get("schema", "game_seed.items_lock"),
        "schema_version": RECEIPT_SCHEMA_VERSION,
    }
    if "comment" in legacy:
        receipt["comment"] = legacy["comment"]
    receipt["receipt"] = {
        "schema": RECEIPT_SCHEMA,
        "items_core_version": ITEMS_CORE_VERSION,
        "lua_evaluation_schema": "items.lua.evaluation.v1",
        "snapshot_schema": "items.snapshot.v1",
        "state_schema": {
            "schema": state_schema.get("schema"),
            "schema_version": state_schema_version,
            "version": state_version,
        },
        # The legacy JSON schema had no stable user field identities. Do not
        # invent them from member names; Lua schema extensions add real IDs.
        "field_ids": [],
    }
    receipt["def_ids"] = {
        item_id: {
            "storage": "unique" if items[item_id]["stack"] == 1 else "stack",
            "level_count": 1 if items[item_id]["stack"] == 1 else 0,
        }
        for item_id in sorted(def_ids)
    }
    receipt["removed"] = {}
    for item_id in sorted(removed):
        entry = removed[item_id]
        if not isinstance(item_id, str) or not isinstance(entry, dict):
            raise OpsError("legacy lock removed entries must be string/object pairs")
        receipt["removed"][item_id] = {
            "storage": entry.get("storage", "unknown"),
            "level_count": entry.get("level_count"),
            **entry,
        }
    return json.dumps(receipt, ensure_ascii=False, indent=2) + "\n"


def _write_utf8_if_changed(path: Path, text: str) -> bool:
    data = text.encode("utf-8")
    if path.exists() and path.read_bytes() == data:
        return False
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_bytes(data)
    os.replace(temporary, path)
    return True


def cmd_upgrade_receipt(args: argparse.Namespace) -> int:
    baseline_path = Path(args.baseline)
    baseline = load_json_or_die(baseline_path, "lock baseline")
    if baseline.get("schema_version") == RECEIPT_SCHEMA_VERSION:
        validate_baseline_shape(baseline, baseline_path)
        changed = False
    else:
        catalog = load_json_or_die(Path(args.catalog), "catalog")
        field_schema = load_json_or_die(Path(args.schema), "field schema")
        state_schema = load_json_or_die(Path(args.state_schema), "items state fragment schema")
        changed = _write_utf8_if_changed(
            baseline_path,
            _receipt_text(catalog, field_schema, state_schema, baseline),
        )
    if args.json:
        print(json.dumps({"ok": True, "changed": changed, "path": str(baseline_path)}))
    else:
        print(f"receipt {'upgraded' if changed else 'unchanged'}: {baseline_path}")
    return 0


# ---------------------------------------------------------------------------
# list
# ---------------------------------------------------------------------------


def block_names(item: dict[str, Any]) -> list[str]:
    return [name for name in ("equip", "use", "currency") if item.get(name) is not None]


def item_json_record(item: dict[str, Any]) -> dict[str, Any]:
    """Full record for `list --json` (M4/T0316 parity): everything a web
    editor needs to render/edit an item without re-reading items.json itself.
    `stack` is the raw authored int (0=unlimited, 1=unique/instance, N=cap) --
    the derived (stackable, max_stack, unlimited) triple lives ONLY in the
    compiled C catalog (single-sourced through generate_items_catalog.stack_fields),
    never in this read layer."""
    record: dict[str, Any] = {
        "id": item.get("id"),
        "display_name": item.get("display_name"),
        "icon_asset_id": item.get("icon_asset_id"),
        "kind": item.get("kind"),
        "created": item.get("created"),  # authoring metadata; never compiled into the C tables
        "tags": item.get("tags", []),
        "base_value": item.get("base_value"),
        "stack": item.get("stack"),   # raw authored int (0=unlimited, 1=unique/instance, N=cap)
        "blocks": block_names(item),
    }
    equip = item.get("equip")
    if equip is not None:
        record["equip"] = {"slot": equip.get("slot")}
    use = item.get("use")
    if use is not None:
        record["use"] = {"effect_id": use.get("effect_id"), "params": use.get("params")}
    currency = item.get("currency")
    if currency is not None:
        record["currency"] = {"hud_hint": currency.get("hud_hint"), "cap": currency.get("cap", 0)}
    return record


def cmd_list(args: argparse.Namespace) -> int:
    catalog_path = Path(args.catalog)
    doc = load_json_or_die(catalog_path, "catalog")

    items = doc.get("items", [])
    containers = doc.get("containers", [])
    item_kinds = doc.get("item_kinds", [])

    if args.json:
        payload = {
            "namespace": doc.get("namespace"),
            "items": [item_json_record(item) for item in items],
            "containers": [
                {
                    "id": c.get("id"),
                    "capacity": c.get("capacity"),
                    "accept_policy": c.get("accept_policy"),
                    "hidden": c.get("hidden", False),
                }
                for c in containers
            ],
            "item_kinds": [{"id": k.get("id"), "label": k.get("label")} for k in item_kinds],
        }
        print(json.dumps(payload, indent=2))
        return 0

    # L3: human summary output stays ASCII-safe on narrow consoles (e.g.
    # Windows cp1251) even if catalog content carries non-ASCII text -- see
    # the stdout/stderr reconfigure guard in main().
    print(f"items ({len(items)}):")
    for item in items:
        blocks = ",".join(block_names(item)) or "-"
        print(f"  {item.get('id', '?'):<20} {item.get('display_name', '?'):<24} kind={item.get('kind', '?'):<12} blocks={blocks}")
    print(f"containers ({len(containers)}):")
    for c in containers:
        cap = "unlimited" if not c.get("capacity") else str(c.get("capacity"))
        hidden = " hidden" if c.get("hidden") else ""
        print(f"  {c.get('id', '?'):<12} capacity={cap:<10} accept_policy={c.get('accept_policy', '?')}{hidden}")
    return 0


# ---------------------------------------------------------------------------
# schema
# ---------------------------------------------------------------------------


def cmd_schema(args: argparse.Namespace) -> int:
    schema_path = Path(args.schema)
    field_schema = load_json_or_die(schema_path, "field schema")

    if args.json:
        # Echo the exact source of truth validate() reads -- "нормализованная"
        # here means "round-tripped through the same JSON model", not a second
        # derived shape.
        print(json.dumps(field_schema, indent=2))
        return 0

    print(f"schema: {field_schema.get('schema', '?')} (schema_version {field_schema.get('schema_version', '?')})")
    print(f"namespace_pattern: {field_schema.get('namespace_pattern', '?')}")
    print("core fields:")
    for name, spec in field_schema.get("core", {}).items():
        req = "required" if spec.get("required") else "optional"
        print(f"  {name:<16} type={spec.get('type', '?'):<14} {req}")
    print("blocks:")
    for name, spec in field_schema.get("blocks", {}).items():
        fields = ", ".join(spec.get("fields", {}).keys())
        print(f"  {name}: {fields}")
    print("containers.fields:")
    for name, spec in field_schema.get("containers", {}).get("fields", {}).items():
        print(f"  {name:<16} type={spec.get('type', '?')}")
    return 0


# ---------------------------------------------------------------------------
# validate
# ---------------------------------------------------------------------------


def check_namespace_pattern(doc: dict[str, Any], field_schema: dict[str, Any]) -> list[Issue]:
    """Require `<namespace>.<slug>` ids matching items.json.namespace.

    Strictly stronger than the generator's own startswith(f"{namespace}.")
    check (run_generator_checks already catches missing/wrong namespace):
    this also validates slug/namespace CHARSET via the schema's
    namespace_pattern regex.
    """
    errors: list[Issue] = []
    pattern = field_schema.get("namespace_pattern")
    if not pattern:
        return errors
    regex = re.compile(pattern)
    namespace = doc.get("namespace")
    for item in doc.get("items", []):
        item_id = item.get("id")
        if not isinstance(item_id, str):
            continue
        if not regex.match(item_id):
            errors.append(issue("namespace", f"id does not match namespace_pattern {pattern!r}", id=item_id, field="id"))
            continue
        head = item_id.split(".", 1)[0]
        if head != namespace:
            errors.append(
                issue("namespace", f"namespace segment {head!r} != catalog namespace {namespace!r}", id=item_id, field="id")
            )
    return errors


def check_lock_workflow(
    doc: dict[str, Any], baseline: dict[str, Any] | None, current_version: int
) -> tuple[list[Issue], list[Issue]]:
    """Lead-ratified 2026-07-07 (see the game-side README "Lock
    workflow"): deleting/renaming a SHIPPED def_id is a destructive action
    and must FORCE an explicit developer reaction, not just a warning.

    items.lock.json keeps two history sections: `def_ids` (currently shipped
    and live) and `removed` (deliberately removed after shipping,
    each entry `{"fragment_version": N, "note": "..." }` recording which
    state/items.schema.json version bump + migration step handled it; `note`
    is OPTIONAL documentation, only `fragment_version` is checked). MANY ids
    can share the SAME fragment_version -- one release can batch several
    removals into one version bump + one migration step (README "Lock
    workflow": batching example); this function never requires distinct
    versions per entry.

    Rules (all HARD errors except removed-def-restored, which is a WARNING --
    restoration is legal, it un-quarantines matching saved records):
    - `removed-without-reaction`: id is in `def_ids` (shipped), missing from
      the catalog now, and NOT declared in `removed` -- the destructive
      change has no recorded reaction yet.
    - `removed-version-not-shipped`: a `removed` entry's fragment_version is
      AHEAD of the current items fragment version -- the reaction was
      declared but the version bump + migration step were not actually
      delivered.
    - `lock-invalid`: a `removed` entry's fragment_version is missing,
      non-integer, or < MIN_REMOVED_FRAGMENT_VERSION (2).
    - `lock-inconsistent`: an id is listed in BOTH `def_ids` and `removed` --
      it must be exactly one of "currently shipped" or "documented removal".
    - v3 receipt storage changes and shipped level shrink require an explicit
      migration plus receipt update.
    - `removed-def-restored` (warning): a `removed` id has reappeared in the
      catalog -- legal, but flagged so the lock file can be tidied up (move
      back to def_ids, drop the removed entry) once confirmed permanent.
    """
    errors: list[Issue] = []
    warnings: list[Issue] = []
    if baseline is None:
        return errors, warnings

    # F3 (deep-review): `baseline`'s shape (schema_version==2, def_ids is a
    # list, removed is an object) is already HARD-enforced by
    # validate_baseline_shape() before this function ever runs (resolve_baseline
    # raises OpsError / exit 2 on a malformed lock) -- so no isinstance-guarded
    # fallback-to-empty here. A silent "if not a list, treat as no ids" coercion
    # is exactly the loophole that let a broken lock file silently disable the
    # whole guard; it must never be reintroduced at this layer.
    def_ids = set(baseline.get("def_ids", []))
    removed: dict[str, Any] = baseline.get("removed", {})
    current_items = {
        item.get("id"): item for item in doc.get("items", []) if isinstance(item, dict)
    }
    current_ids = set(current_items)

    both = sorted(def_ids & removed.keys())
    for dup_id in both:
        errors.append(
            issue(
                "lock-inconsistent",
                f"def_id {dup_id!r} is listed in BOTH items.lock.json def_ids and removed -- inconsistent "
                "lock state: an id is either currently shipped (def_ids) or a documented removal "
                "(removed), never both. Fix items.lock.json by hand.",
                id=dup_id,
            )
        )

    needed_version = current_version + 1
    for locked_id in sorted(def_ids):
        if locked_id in current_ids and baseline.get("schema_version") == RECEIPT_SCHEMA_VERSION:
            metadata = baseline["def_ids"][locked_id]
            current_storage = "unique" if current_items[locked_id].get("stack") == 1 else "stack"
            current_level_count = 1 if current_storage == "unique" else 0
            if metadata["storage"] != current_storage:
                errors.append(issue(
                    "storage-change-without-reaction",
                    f"def_id {locked_id!r} shipped as {metadata['storage']!r} but is now "
                    f"{current_storage!r}; migrate saved entries and update the release receipt explicitly",
                    id=locked_id,
                    field="storage",
                ))
            if current_level_count < metadata["level_count"]:
                errors.append(issue(
                    "level-shrink-without-reaction",
                    f"def_id {locked_id!r} shipped with level_count={metadata['level_count']} but now "
                    f"has {current_level_count}; migrate out-of-range saved levels and update the receipt explicitly",
                    id=locked_id,
                    field="level_count",
                ))
        if locked_id in current_ids or locked_id in removed:
            continue  # still shipped, or already has a recorded reaction (lock-inconsistent covers "both")
        errors.append(
            issue(
                "removed-without-reaction",
                f"def_id {locked_id!r} is shipped (listed in items.lock.json def_ids) but missing from "
                "the catalog -- this is destructive and needs an explicit developer reaction: move it to "
                f"lock.removed with fragment_version={needed_version} (current items fragment version "
                f"{current_version} + 1 -- or just {current_version} if you already bumped "
                "state/items.schema.json's 'version' earlier in THIS SAME release for an unrelated shape "
                f"migration), bump 'version' to that same number if you have not already, and add the "
                "corresponding migration step (a REAL step: delete or convert the records, e.g. convert a "
                "currency by base_value; or an explicit no-op step = a conscious decision that "
                "reconcile()'s quarantine is the handling -- prefer the no-op/quarantine step over a real "
                "delete if this def might come back: dropping the removed receipt later does NOT undo a "
                "migration step that already shipped and ran). The generator enforces "
                "version == len(migrations)+1, so the version bump forces you to add that step. (Multiple "
                "removals in one release can share the SAME fragment_version/migration step -- batch "
                "them.) If this id never actually shipped in any released build, you may instead remove "
                "it from items.lock.json's def_ids directly -- but treat that as the exception, not the "
                "primary fix.",
                id=locked_id,
            )
        )

    for removed_id in sorted(removed):
        entry = removed[removed_id]
        fragment_version = entry.get("fragment_version") if isinstance(entry, dict) else None
        valid = (
            isinstance(fragment_version, int)
            and not isinstance(fragment_version, bool)
            and fragment_version >= MIN_REMOVED_FRAGMENT_VERSION
        )
        if not valid:
            errors.append(
                issue(
                    "lock-invalid",
                    f"lock.removed[{removed_id!r}].fragment_version={fragment_version!r} is invalid -- must "
                    f"be an integer >= {MIN_REMOVED_FRAGMENT_VERSION} (fragment version 1 is the initial "
                    "skeleton with no migrations yet; a removal always corresponds to a version bump "
                    "beyond that). 'note' is optional documentation, not checked.",
                    id=removed_id,
                    field="fragment_version",
                )
            )
        elif fragment_version > current_version:
            errors.append(
                issue(
                    "removed-version-not-shipped",
                    f"lock.removed[{removed_id!r}].fragment_version={fragment_version} is ahead of the "
                    f"current items fragment version ({current_version}, state/items.schema.json "
                    "'version') -- the removal was DECLARED in the lock but not actually DELIVERED: bump "
                    f"state/items.schema.json 'version' to {fragment_version} and add its migration step "
                    "before this can validate clean.",
                    id=removed_id,
                    field="fragment_version",
                )
            )

        if removed_id in current_ids:
            warnings.append(
                issue(
                    "removed-def-restored",
                    f"def_id {removed_id!r} is listed in items.lock.json removed but has reappeared in the "
                    "catalog -- this is legal (reconcile() un-quarantines matching saved records on "
                    "restoration); consider moving it back to def_ids and dropping the removed entry once "
                    "the restoration is confirmed intentional and permanent.",
                    id=removed_id,
                )
            )

    return errors, warnings


def check_composite_key_length(doc: dict[str, Any]) -> list[Issue]:
    """Hard-fail equip/stack consistency before runtime mutation checks.

    The runtime
    (items_containers.c) only rejects this at mutation time via NT_ASSERT/
    truncation-check, never at content-authoring time). For every (container,
    stackable item) pair whose accept_policy would actually let that item land
    there, the stack key "<container>/<def_id>" must fit ITEMS_STATE_STRING_MAX
    (=64, state/items.schema.json string_max) i.e. len(container)+1+len(def_id)
    <= 63. A silent overflow at runtime would truncate the key (silent save
    corruption), so this must be a hard fail here, at content-authoring time.
    """
    errors: list[Issue] = []
    for container in doc.get("containers", []):
        container_id = container.get("id")
        if not isinstance(container_id, str):
            continue
        policy = container.get("accept_policy")
        for item in doc.get("items", []):
            item_id = item.get("id")
            if not isinstance(item_id, str):
                continue
            stackable, _max_stack, _unlimited = gen.stack_fields(item)
            if not stackable:
                continue  # uniques key by instance_id, not "<container>/<def_id>"
            is_currency = item.get("currency") is not None
            # L6 (И2b items_containers.c): currency_only rejects non-currency;
            # any/slot_filter/capacity_1 are all treated as `any` in И2 (inert).
            if policy == "currency_only" and not is_currency:
                continue
            key_len = len(container_id) + 1 + len(item_id)
            if key_len > OWNED_KEY_MAX_LEN:
                errors.append(
                    issue(
                        "composite-key-length",
                        f"composite key '{container_id}/{item_id}' is {key_len} chars, exceeds "
                        f"{OWNED_KEY_MAX_LEN} (state/items.schema.json string_max=64 minus NUL) -- "
                        "shorten the container id or the item id",
                        id=f"{container_id}/{item_id}",
                        field="owned.key",
                    )
                )
    return errors


def check_equip_stack(doc: dict[str, Any]) -> list[Issue]:
    """L1 (adapted for the single-int stack model): an item with an `equip` block IS
    a unique instance (per-copy record, instance_id-keyed, count=1), which under
    the collapsed model means stack == 1 exactly. 0 (unlimited) or N>1 (capped stack)
    contradicts per-copy ownership."""
    errors: list[Issue] = []
    for item in doc.get("items", []):
        if item.get("equip") is None:
            continue
        stack = item.get("stack")
        if not isinstance(stack, int) or isinstance(stack, bool):
            continue  # generator-check already reported the bad type
        if stack != 1:
            errors.append(
                issue(
                    "equip-stack",
                    f"item {item.get('id')!r} has an equip block (unique instance) but stack={stack} -- "
                    "uniques are single-instance per-copy records, so stack must be 1",
                    id=item.get("id"),
                    field="stack",
                )
            )
    return errors


def check_created_field(doc: dict[str, Any]) -> list[Issue]:
    """Lead reversal 2026-07-07: every item def carries a `created` (ISO
    date) authoring field IN THE DATA -- git history was rejected as
    unreliable for this (copy-then-own resets it when a game is spun up from
    the template, games/new_game.mjs). `created` is REQUIRED on items only
    (container defs do not need one); `item_fields.schema.json`'s generic
    core-required-field check (run_generator_checks) also catches a MISSING
    `created` as a side effect of it being `"required": true` there, but
    this function is the dedicated, always-evaluated, stably-ruled check
    (not gated behind the generator's fail-fast on some unrelated field) --
    same pattern as check_equip_stack/check_composite_key_length. Never
    compiled into the C tables (generate_items_catalog.py) -- authoring-only,
    read by tooling/editors (T0316), not runtime."""
    errors: list[Issue] = []
    for item in doc.get("items", []):
        item_id = item.get("id")
        created = item.get("created")
        if created is None:
            errors.append(
                issue(
                    "created-missing",
                    f"item {item_id!r} is missing required field 'created' (ISO date, e.g. \"2026-07-07\") -- "
                    "set it when authoring a new item (the T0316 web editor will set it automatically).",
                    id=item_id,
                    field="created",
                )
            )
            continue
        if not isinstance(created, str) or not CREATED_DATE_RE.match(created):
            errors.append(
                issue(
                    "created-invalid",
                    f"item {item_id!r} 'created'={created!r} is not an ISO date string (\"YYYY-MM-DD\")",
                    id=item_id,
                    field="created",
                )
            )
            continue
        try:
            datetime.date.fromisoformat(created)
        except ValueError:
            errors.append(
                issue(
                    "created-invalid",
                    f"item {item_id!r} 'created'={created!r} matches YYYY-MM-DD shape but is not a real "
                    "calendar date",
                    id=item_id,
                    field="created",
                )
            )
    return errors


DISPLAY_NAME_KEY_RE = re.compile(r"strcmp\s*\([^;]*display_name")


def lint_display_name_keying(src_dir: Path) -> list[Issue]:
    """Advisory duplicate of grep gate G11: code must never key
    behavior off display_name (only ever read/emit it). Formally G11 is a
    manual grep-gate reviewed at PR time; this is the op-layer's own advisory
    scan so `validate` surfaces the same smell without gating exit code on it
    (a legitimate strcmp elsewhere in the same statement can still false-
    positive, hence advisory, not a hard FAIL). L3: source read tolerates
    non-UTF-8 bytes (errors="replace") instead of crashing the whole
    `validate` run over an unrelated encoding glitch in a source file."""
    warnings: list[Issue] = []
    if not src_dir.exists():
        return warnings
    for path in sorted(src_dir.rglob("*.c")):
        text = path.read_text(encoding="utf-8", errors="replace")
        for lineno, line in enumerate(text.splitlines(), start=1):
            if DISPLAY_NAME_KEY_RE.search(line):
                warnings.append(
                    issue(
                        "display-name-keying",
                        f"possible branch/compare keyed on display_name: {line.strip()}",
                        field=f"{path}:{lineno}",
                    )
                )
    return warnings


def run_generator_checks(doc: dict[str, Any], field_schema: dict[str, Any]) -> list[Issue]:
    """Reuse (never re-derive) the generator's own sanity net: namespace
    prefix, duplicate item/container/kind ids, required fields, container
    accept_policy membership (gen.validate_catalog), PLUS i64 integer-ness for
    base_value/max_stack/currency.cap/capacity (only checked inside
    gen.render_source's c_i64 calls -- exercised here without writing any
    files, since render_header/render_source are pure string builders)."""
    errors: list[Issue] = []
    try:
        gen.validate_catalog(doc, field_schema)
    except SystemExit as exc:
        errors.append(issue("generator-check", str(exc.code)))
        return errors  # require() fails fast; further generator checks would compound on bad data

    try:
        gen.render_header(len(doc.get("items", [])), len(doc.get("containers", [])))
        gen.render_source(doc)
    except SystemExit as exc:
        errors.append(issue("generator-check", str(exc.code)))
    except (KeyError, TypeError, ValueError, AttributeError) as exc:
        errors.append(issue("generator-check", f"items catalog validation: {exc}"))
    return errors


def validate_baseline_shape(baseline: dict[str, Any], path: Path) -> None:
    """F3 (deep-review): a broken lock file must NEVER silently disable the
    destructive-change guard. Before this check existed, check_lock_workflow
    coerced a malformed `def_ids`/`removed` to an empty set/dict and moved on
    -- a typo'd lock file (e.g. `def_ids` accidentally overwritten with a
    string, or a stray `"removed": null`) would validate GREEN while quietly
    skipping every lock-workflow rule. Same severity as a missing/unparsable
    file: OpsError, exit 2, not a warning."""
    schema_version = baseline.get("schema_version")
    if schema_version not in (2, RECEIPT_SCHEMA_VERSION):
        raise OpsError(f"lock baseline 'schema_version' must be 2 or 3, got {schema_version!r} ({path})")
    def_ids = baseline.get("def_ids")
    expected = (list,) if schema_version == 2 else (dict,)
    if not isinstance(def_ids, expected):
        shape = "list" if schema_version == 2 else "object"
        raise OpsError(f"lock baseline 'def_ids' must be a {shape}, got {type(def_ids).__name__} ({path})")
    removed = baseline.get("removed")
    if not isinstance(removed, dict):
        raise OpsError(f"lock baseline 'removed' must be an object, got {type(removed).__name__} ({path})")
    if schema_version == RECEIPT_SCHEMA_VERSION:
        receipt = baseline.get("receipt")
        if not isinstance(receipt, dict) or receipt.get("schema") != RECEIPT_SCHEMA:
            raise OpsError(f"lock baseline 'receipt' must be a {RECEIPT_SCHEMA} object ({path})")
        required = {
            "schema", "items_core_version", "lua_evaluation_schema",
            "snapshot_schema", "state_schema", "field_ids",
        }
        if set(receipt) != required:
            raise OpsError(f"lock baseline 'receipt' keys are invalid ({path})")
        if not all(isinstance(receipt.get(key), str) and receipt[key] for key in (
            "items_core_version", "lua_evaluation_schema", "snapshot_schema",
        )):
            raise OpsError(f"lock baseline receipt tool/API versions must be strings ({path})")
        state = receipt.get("state_schema")
        if (not isinstance(state, dict) or set(state) != {"schema", "schema_version", "version"}
                or not isinstance(state.get("schema"), str)
                or type(state.get("schema_version")) is not int
                or type(state.get("version")) is not int):
            raise OpsError(f"lock baseline receipt state_schema is invalid ({path})")
        field_ids = receipt.get("field_ids")
        if (not isinstance(field_ids, list) or not all(isinstance(field_id, str) for field_id in field_ids)
                or len(field_ids) != len(set(field_ids))):
            raise OpsError(f"lock baseline receipt field_ids must be unique strings ({path})")
        for item_id, metadata in def_ids.items():
            if (not isinstance(item_id, str) or not isinstance(metadata, dict)
                    or set(metadata) != {"storage", "level_count"}
                    or metadata.get("storage") not in {"stack", "unique"}
                    or type(metadata.get("level_count")) is not int
                    or metadata["level_count"] < 0):
                raise OpsError(f"lock baseline def_ids receipt for {item_id!r} is invalid ({path})")
        for item_id, metadata in removed.items():
            if not isinstance(item_id, str) or not isinstance(metadata, dict):
                raise OpsError(f"lock baseline removed receipt for {item_id!r} is invalid ({path})")
            storage = metadata.get("storage")
            level_count = metadata.get("level_count")
            if (storage not in {"stack", "unique", "unknown"}
                    or (storage == "unknown" and level_count is not None)
                    or (storage != "unknown" and (type(level_count) is not int or level_count < 0))):
                raise OpsError(f"lock baseline removed compatibility data for {item_id!r} is invalid ({path})")


def resolve_baseline(args: argparse.Namespace) -> tuple[dict[str, Any] | None, Issue | None]:
    """An EXPLICIT --baseline that does not exist (or exists but has the
    wrong shape, F3) is an IO error (exit 2, consistent with
    --catalog/--schema/--state-schema); the DEFAULT baseline being absent is
    NOT an error -- the lock-file workflow is a convention that only kicks in
    once a lock file exists, so it degrades to a visible skip-warning instead
    of silently doing nothing. A PRESENT default baseline is held to the same
    shape check as an explicit one -- "it happened to exist" is not a reason
    to accept a malformed lock file."""
    if args.baseline is not None:
        baseline_path = Path(args.baseline)
        if not baseline_path.exists():
            raise OpsError(f"baseline not found: {baseline_path}")
        baseline = load_json_or_die(baseline_path, "lock baseline")
        validate_baseline_shape(baseline, baseline_path)
        return baseline, None

    if DEFAULT_LOCK.exists():
        baseline = load_json_or_die(DEFAULT_LOCK, "lock baseline")
        validate_baseline_shape(baseline, DEFAULT_LOCK)
        return baseline, None

    skip_warning = issue(
        "rename-guard-skipped",
        f"default baseline not found ({DEFAULT_LOCK}) -- lock-file checks SKIPPED for this run",
    )
    return None, skip_warning


def cmd_validate(args: argparse.Namespace) -> int:
    catalog_path = Path(args.catalog)
    schema_path = Path(args.schema)
    state_schema_path = Path(args.state_schema)
    doc = load_json_or_die(catalog_path, "catalog")
    field_schema = load_json_or_die(schema_path, "field schema")
    state_schema = load_json_or_die(state_schema_path, "items state fragment schema")

    current_version = state_schema.get("version")
    if not isinstance(current_version, int) or isinstance(current_version, bool):
        raise OpsError(f"items state fragment schema 'version' is missing/invalid ({state_schema_path})")

    baseline, baseline_skip_warning = resolve_baseline(args)
    if baseline_skip_warning is not None:
        print(f"items_ops: warning: {baseline_skip_warning['msg']}", file=sys.stderr)

    errors: list[Issue] = []
    errors += run_generator_checks(doc, field_schema)
    errors += check_namespace_pattern(doc, field_schema)
    errors += check_created_field(doc)
    errors += check_composite_key_length(doc)
    errors += check_equip_stack(doc)

    lock_errors, lock_warnings = check_lock_workflow(doc, baseline, current_version)
    errors += lock_errors

    warnings = lint_display_name_keying(Path(args.src_dir))
    warnings += lock_warnings
    if baseline_skip_warning is not None:
        warnings = [baseline_skip_warning, *warnings]

    ok = not errors
    if args.json:
        print(json.dumps({"ok": ok, "errors": errors, "warnings": warnings}, indent=2))
    else:
        if ok:
            print(f"validate OK: {catalog_path}")
        else:
            print(f"validate FAILED: {catalog_path}", file=sys.stderr)
            for err in errors:
                print(f"  error {format_issue(err)}", file=sys.stderr)
        for warn in warnings:
            print(f"  warning {format_issue(warn)}")
    return 0 if ok else 1


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="items_ops.py", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    p_list = sub.add_parser("list", help="List catalog items/containers.")
    p_list.add_argument("--catalog", default=str(DEFAULT_CATALOG))
    p_list.add_argument("--json", action="store_true")
    p_list.set_defaults(func=cmd_list)

    p_validate = sub.add_parser("validate", help="Validate the catalog (lock-file removal workflow + content rules).")
    p_validate.add_argument("--catalog", default=str(DEFAULT_CATALOG))
    p_validate.add_argument("--schema", default=str(DEFAULT_FIELD_SCHEMA))
    p_validate.add_argument(
        "--baseline",
        default=None,
        help=f"Path to items.lock.json baseline (default: {DEFAULT_LOCK}; missing DEFAULT warns+skips the "
        "lock-file workflow checks, missing EXPLICIT path is an error).",
    )
    p_validate.add_argument(
        "--state-schema",
        default=str(DEFAULT_STATE_SCHEMA),
        help="Path to the items save-fragment schema (default: %(default)s) -- its 'version' is the "
        "CURRENT items fragment version used to judge lock.removed[*].fragment_version.",
    )
    p_validate.add_argument("--src-dir", default=str(DEFAULT_SRC_DIR), help=argparse.SUPPRESS)
    p_validate.add_argument("--json", action="store_true")
    p_validate.set_defaults(func=cmd_validate)

    p_schema = sub.add_parser("schema", help="Echo the normalized item-fields schema.")
    p_schema.add_argument("--schema", default=str(DEFAULT_FIELD_SCHEMA))
    p_schema.add_argument("--json", action="store_true")
    p_schema.set_defaults(func=cmd_schema)

    p_upgrade = sub.add_parser("upgrade-receipt", help="Upgrade items.lock.json v2 to the extended release receipt.")
    p_upgrade.add_argument("--catalog", required=True)
    p_upgrade.add_argument("--schema", required=True)
    p_upgrade.add_argument("--baseline", required=True)
    p_upgrade.add_argument("--state-schema", required=True)
    p_upgrade.add_argument("--json", action="store_true")
    p_upgrade.set_defaults(func=cmd_upgrade_receipt)

    return parser


def main(argv: list[str] | None = None) -> int:
    # L3: keep this CLI usable on narrow-codepage consoles (e.g. Windows
    # cp1251/cp1252) even though its own docstrings/comments carry non-ASCII
    # (Cyrillic §-references) -- never crash on an encode error, degrade to
    # '?'-replacement instead. Not all stream wrappers support reconfigure()
    # (e.g. when stdout is replaced by a test harness), so this is best-effort.
    for stream in (sys.stdout, sys.stderr):
        reconfigure = getattr(stream, "reconfigure", None)
        if reconfigure is not None:
            try:
                reconfigure(errors="replace")
            except (ValueError, OSError):
                pass

    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except OpsError as exc:
        print(f"items_ops: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
