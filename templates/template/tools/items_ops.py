#!/usr/bin/env python3
"""Read-only op-layer CLI for the items content catalog (T0327 И2c).

Single op-layer that the future T0316 web editor will sit on top of (tool
parity, design §7): subprocess + `--json` gives the Node editor the exact same
answers this CLI gives a human. read-v1 only (LEAN §3 -- upsert/deprecate are
editor-era, out of scope here).

    py -3.12 tools/items_ops.py list     [--catalog content/items.json] [--json]
    py -3.12 tools/items_ops.py validate [--catalog content/items.json] [--schema content/item_fields.schema.json] [--baseline content/items.lock.json] [--state-schema state/items.schema.json] [--json]
    py -3.12 tools/items_ops.py schema   [--schema content/item_fields.schema.json] [--json]

Shares its data model with tools/generate_items_catalog.py by IMPORTING it
(never re-parsing the catalog with a second, forkable set of rules): `list`
and `validate` reuse generate_items_catalog.load_json/validate_catalog/
render_header/render_source so the codegen's own sanity net (namespace
prefix, duplicate ids, required fields, container accept_policy, i64
integer-ness) always runs first, and this CLI only ADDS strictly stronger
checks on top (lock-file removal workflow, full namespace-pattern regex,
composite-key length, equip/unlimited sanity, display_name-keying lint) --
see §8.3 of templates/design/build_spec_t0327_i2_2026-07-07.md and
src/features/items/README.md ("Lock workflow" -- deleting/renaming a
SHIPPED def_id is a destructive action that must FORCE an explicit developer
reaction, lead-ratified 2026-07-07). Never contradicts the generator; only
tightens it.

`validate --json` errors/warnings are structured objects
`{rule, id, field, msg}` (stable kebab-case `rule` ids: "generator-check",
"namespace", "removed-without-reaction", "removed-version-not-shipped",
"lock-invalid", "lock-inconsistent", "removed-def-restored" (warning),
"composite-key-length", "equip-unlimited", "display-name-keying",
"rename-guard-skipped") -- not free strings, so the future web editor
(T0316) can key UI off `rule`/`id` instead of parsing text.

Exit codes: 0 = OK, 1 = validation FAIL, 2 = usage/IO error. A rename-guard
baseline that is MISSING because it was never given (default path absent) is
NOT an IO error -- it prints a warning (guard skipped, other checks still
run); an EXPLICITLY passed `--baseline` path that does not exist IS an IO
error (exit 2), consistent with `--catalog`/`--schema`/`--state-schema`.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any

# Same directory as generate_items_catalog.py -- import it directly instead of
# re-implementing catalog parsing/validation (spec §8.1: "shares the catalog
# schema/source of truth ... WITHOUT duplicating a second data model").
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
# §2.3/§8.3.4). Composite stack key is "<container>/<def_id>".
OWNED_KEY_MAX_LEN = 63

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


# ---------------------------------------------------------------------------
# list
# ---------------------------------------------------------------------------


def block_names(item: dict[str, Any]) -> list[str]:
    return [name for name in ("equip", "use", "currency") if item.get(name) is not None]


def item_json_record(item: dict[str, Any]) -> dict[str, Any]:
    """Full record for `list --json` (M4/T0316 parity): everything a web
    editor needs to render/edit an item without re-reading items.json itself.
    stack config is derived via generate_items_catalog.stack_fields -- the
    SAME defaulting (equip -> not stackable unless overridden) the compiled
    catalog actually uses, not a second guess at the defaults."""
    stackable, max_stack, unlimited = gen.stack_fields(item)
    record: dict[str, Any] = {
        "id": item.get("id"),
        "display_name": item.get("display_name"),
        "icon_asset_id": item.get("icon_asset_id"),
        "kind": item.get("kind"),
        "tags": item.get("tags", []),
        "base_value": item.get("base_value"),
        "stack": {"stackable": stackable, "max_stack": max_stack, "unlimited": unlimited},
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
    """§8.3.1: id matches `<namespace>.<slug>`, namespace == items.json.namespace.

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
    """Lead-ratified 2026-07-07 (build on top of §8.2/§8.3.2 + README "Lock
    workflow"): deleting/renaming a SHIPPED def_id is a destructive action
    and must FORCE an explicit developer reaction, not just a warning.

    items.lock.json v2 has two sections: `def_ids` (currently shipped and
    live -- append-only) and `removed` (deliberately removed after shipping,
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
    - `removed-def-restored` (warning): a `removed` id has reappeared in the
      catalog -- legal, but flagged so the lock file can be tidied up (move
      back to def_ids, drop the removed entry) once confirmed permanent.
    """
    errors: list[Issue] = []
    warnings: list[Issue] = []
    if baseline is None:
        return errors, warnings

    def_ids_raw = baseline.get("def_ids", [])
    def_ids = set(def_ids_raw) if isinstance(def_ids_raw, list) else set()
    removed_raw = baseline.get("removed", {})
    removed: dict[str, Any] = removed_raw if isinstance(removed_raw, dict) else {}
    current_ids = {item.get("id") for item in doc.get("items", [])}

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
        if locked_id in current_ids or locked_id in removed:
            continue  # still shipped, or already has a recorded reaction (lock-inconsistent covers "both")
        errors.append(
            issue(
                "removed-without-reaction",
                f"def_id {locked_id!r} is shipped (listed in items.lock.json def_ids) but missing from "
                "the catalog -- this is destructive and needs an explicit developer reaction: move it to "
                f"lock.removed with fragment_version={needed_version} (current items fragment version "
                f"{current_version} + 1), bump state/items.schema.json 'version' to {needed_version} to "
                "match, and add the corresponding migration step (a REAL step: delete or convert the "
                "records, e.g. convert a currency by base_value; or an explicit no-op step = a conscious "
                "decision that reconcile()'s quarantine is the handling). The generator enforces "
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
    """§8.3.4 (deep-review L2, HARD FAIL, И2c-only lint -- the runtime
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
                continue  # uniques key by instance_id, not "<container>/<def_id>" (§2.3)
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


def check_equip_unlimited(doc: dict[str, Any]) -> list[Issue]:
    """L1 (deep-review): a unique-instance def (has an `equip` block) must
    never also be `stack.unlimited` -- uniques are per-copy records
    (instance_id-keyed, count=1 each, §2.3); "unlimited stackable unique" is
    a contradiction in the data that would only surface confusingly at
    runtime (an unlimited currency-like stack that also carries an equip
    slot makes no sense)."""
    errors: list[Issue] = []
    for item in doc.get("items", []):
        item_id = item.get("id")
        if item.get("equip") is None:
            continue
        stack = item.get("stack") or {}
        if stack.get("unlimited") is True:
            errors.append(
                issue(
                    "equip-unlimited",
                    f"item {item_id!r} has an equip block (unique instance) but stack.unlimited=true -- "
                    "uniques are per-copy records, not an unlimited stack",
                    id=item_id,
                    field="stack.unlimited",
                )
            )
    return errors


DISPLAY_NAME_KEY_RE = re.compile(r"strcmp\s*\([^;]*display_name")


def lint_display_name_keying(src_dir: Path) -> list[Issue]:
    """§8.3.3: advisory duplicate of the greп-gate G11 -- code must never key
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


def resolve_baseline(args: argparse.Namespace) -> tuple[dict[str, Any] | None, Issue | None]:
    """An EXPLICIT --baseline that does not exist is an IO error (exit 2,
    consistent with --catalog/--schema/--state-schema); the DEFAULT baseline
    being absent is NOT an error -- the lock-file workflow is a convention
    that only kicks in once a lock file exists, so it degrades to a visible
    skip-warning instead of silently doing nothing."""
    if args.baseline is not None:
        baseline_path = Path(args.baseline)
        if not baseline_path.exists():
            raise OpsError(f"baseline not found: {baseline_path}")
        return load_json_or_die(baseline_path, "lock baseline"), None

    if DEFAULT_LOCK.exists():
        return load_json_or_die(DEFAULT_LOCK, "lock baseline"), None

    skip_warning = issue(
        "rename-guard-skipped",
        f"default baseline not found ({DEFAULT_LOCK}) -- rename-guard SKIPPED for this run",
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
    errors += check_composite_key_length(doc)
    errors += check_equip_unlimited(doc)

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
