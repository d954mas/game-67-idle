#!/usr/bin/env python3
"""Release-receipt checks for canonical Items Lua evaluation output."""

from __future__ import annotations

import json
import os
import re
from pathlib import Path
from typing import Any

MIN_REMOVED_FRAGMENT_VERSION = 2
DEF_ID_RE = re.compile(r"^[a-z][a-z0-9_]*\.[a-z][a-z0-9_]*$")
FIELD_ID_RE = re.compile(r"^[a-z][a-z0-9_]*(?:\.[a-z][a-z0-9_]*)+$")
RECEIPT_SCHEMA_VERSION = 4
RECEIPT_SCHEMA = "items.release_receipt.v2"
LEGACY_RECEIPT_SCHEMA_VERSION = 3
LEGACY_RECEIPT_SCHEMA = "items.release_receipt.v1"
ITEMS_CORE_VERSION = "1.14.0"

Issue = dict


def issue(rule: str, msg: str, *, id: str | None = None, field: str | None = None) -> Issue:  # noqa: A002
    return {"rule": rule, "id": id, "field": field, "msg": msg}


def format_issue(entry: Issue) -> str:
    where = "".join(
        f" {key}={value!r}"
        for key, value in (("id", entry.get("id")), ("field", entry.get("field")))
        if value
    )
    return f"[{entry['rule']}]{where}: {entry['msg']}"


class OpsError(Exception):
    """Usage or I/O problem reported as CLI exit code 2."""


def _load_json(path: Path, what: str) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise OpsError(f"{what} not found: {path}") from exc
    except json.JSONDecodeError as exc:
        raise OpsError(f"{what} is not valid JSON ({path}): {exc}") from exc


def write_utf8_if_changed(path: Path, text: str) -> bool:
    data = text.encode("utf-8")
    if path.exists() and path.read_bytes() == data:
        return False
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_bytes(data)
    os.replace(temporary, path)
    return True


def validate_baseline_shape(baseline: dict[str, Any], path: Path) -> None:
    schema_version = baseline.get("schema_version")
    if schema_version not in (2, LEGACY_RECEIPT_SCHEMA_VERSION, RECEIPT_SCHEMA_VERSION):
        raise OpsError(
            f"lock baseline 'schema_version' must be 2, 3, or 4, got {schema_version!r} ({path})"
        )
    def_ids = baseline.get("def_ids")
    expected = (list,) if schema_version == 2 else (dict,)
    if not isinstance(def_ids, expected):
        shape = "list" if schema_version == 2 else "object"
        raise OpsError(
            f"lock baseline 'def_ids' must be a {shape}, got {type(def_ids).__name__} ({path})"
        )
    removed = baseline.get("removed")
    if not isinstance(removed, dict):
        raise OpsError(
            f"lock baseline 'removed' must be an object, got {type(removed).__name__} ({path})"
        )
    if schema_version not in (LEGACY_RECEIPT_SCHEMA_VERSION, RECEIPT_SCHEMA_VERSION):
        return

    receipt = baseline.get("receipt")
    expected_receipt_schema = (
        LEGACY_RECEIPT_SCHEMA
        if schema_version == LEGACY_RECEIPT_SCHEMA_VERSION
        else RECEIPT_SCHEMA
    )
    if not isinstance(receipt, dict) or receipt.get("schema") != expected_receipt_schema:
        raise OpsError(
            f"lock baseline 'receipt' must be a {expected_receipt_schema} object ({path})"
        )
    required = {
        "schema", "items_core_version", "lua_evaluation_schema",
        "snapshot_schema", "state_schema", "field_ids",
    }
    if set(receipt) != required:
        raise OpsError(f"lock baseline 'receipt' keys are invalid ({path})")
    if not all(
        isinstance(receipt.get(key), str) and receipt[key]
        for key in ("items_core_version", "lua_evaluation_schema", "snapshot_schema")
    ):
        raise OpsError(f"lock baseline receipt tool/API versions must be strings ({path})")
    state = receipt.get("state_schema")
    if (
        not isinstance(state, dict)
        or set(state) != {"schema", "schema_version", "version"}
        or not isinstance(state.get("schema"), str)
        or type(state.get("schema_version")) is not int
        or type(state.get("version")) is not int
    ):
        raise OpsError(f"lock baseline receipt state_schema is invalid ({path})")
    field_ids = receipt.get("field_ids")
    if schema_version == LEGACY_RECEIPT_SCHEMA_VERSION:
        if (
            not isinstance(field_ids, list)
            or not all(
                isinstance(field_id, str) and FIELD_ID_RE.fullmatch(field_id)
                for field_id in field_ids
            )
            or len(field_ids) != len(set(field_ids))
        ):
            raise OpsError(
                f"lock baseline receipt field_ids must be unique strings ({path})"
            )
    else:
        if not isinstance(field_ids, dict) or set(field_ids) != {"active", "reserved"}:
            raise OpsError(
                f"lock baseline receipt field_ids must have active/reserved lists ({path})"
            )
        active, reserved = field_ids.get("active"), field_ids.get("reserved")
        if (
            not isinstance(active, list)
            or not isinstance(reserved, list)
            or not all(
                isinstance(field_id, str) and FIELD_ID_RE.fullmatch(field_id)
                for field_id in [*active, *reserved]
            )
            or len(active) != len(set(active))
            or len(reserved) != len(set(reserved))
            or set(active) & set(reserved)
        ):
            raise OpsError(
                f"lock baseline receipt field_ids must be unique disjoint strings ({path})"
            )
    for item_id, metadata in def_ids.items():
        if (
            not isinstance(item_id, str)
            or not isinstance(metadata, dict)
            or set(metadata) != {"storage", "level_count"}
            or metadata.get("storage") not in {"stack", "unique"}
            or type(metadata.get("level_count")) is not int
            or metadata["level_count"] < 0
        ):
            raise OpsError(
                f"lock baseline def_ids receipt for {item_id!r} is invalid ({path})"
            )
    for item_id, metadata in removed.items():
        if not isinstance(item_id, str) or not isinstance(metadata, dict):
            raise OpsError(
                f"lock baseline removed receipt for {item_id!r} is invalid ({path})"
            )
        storage = metadata.get("storage")
        level_count = metadata.get("level_count")
        if (
            storage not in {"stack", "unique", "unknown"}
            or (storage == "unknown" and level_count is not None)
            or (
                storage != "unknown"
                and (type(level_count) is not int or level_count < 0)
            )
        ):
            raise OpsError(
                f"lock baseline removed compatibility data for {item_id!r} is invalid ({path})"
            )


def _evaluation_receipt_view(
    evaluation: dict[str, Any],
) -> tuple[set[str], dict[str, dict[str, Any]]]:
    if not isinstance(evaluation, dict) or evaluation.get("schema") != "items.lua.evaluation.v1":
        raise OpsError("evaluation must be an items.lua.evaluation.v1 object")
    raw_fields = evaluation.get("fields")
    raw_items = evaluation.get("items")
    if not isinstance(raw_fields, list) or not isinstance(raw_items, list):
        raise OpsError("evaluation requires fields/items lists")

    field_ids: list[str] = []
    for field in raw_fields:
        field_id = field.get("id") if isinstance(field, dict) else None
        if not isinstance(field_id, str) or FIELD_ID_RE.fullmatch(field_id) is None:
            raise OpsError("evaluation fields require non-empty string ids")
        field_ids.append(field_id)
    if len(field_ids) != len(set(field_ids)):
        raise OpsError("evaluation field ids must be unique")

    items: dict[str, dict[str, Any]] = {}
    for item in raw_items:
        if (
            not isinstance(item, dict)
            or not isinstance(item.get("id"), str)
            or DEF_ID_RE.fullmatch(item["id"]) is None
        ):
            raise OpsError("evaluation items require non-empty string ids")
        item_id = item["id"]
        if item_id in items:
            raise OpsError(f"evaluation item id is duplicated: {item_id!r}")
        stack = item.get("stack")
        if type(stack) is not int or stack < 0:
            raise OpsError(f"evaluation item {item_id!r} requires an integer stack >= 0")
        levels = item.get("levels")
        if levels is None:
            level_count = 0
        elif not isinstance(levels, dict) or not isinstance(levels.get("rows"), list):
            raise OpsError(f"evaluation item {item_id!r} levels require a rows list")
        else:
            level_count = len(levels["rows"])
        storage = "unique" if stack == 1 else "stack"
        if storage == "stack" and level_count != 0:
            raise OpsError(f"evaluation stack item {item_id!r} cannot have levels")
        items[item_id] = {"storage": storage, "level_count": level_count}
    return set(field_ids), items


def _evaluation_receipt_checks(
    evaluation: dict[str, Any],
    baseline: dict[str, Any],
    state_schema: dict[str, Any],
) -> tuple[list[Issue], list[Issue], set[str], dict[str, dict[str, Any]]]:
    field_ids, items = _evaluation_receipt_view(evaluation)
    state_version = state_schema.get("version")
    if (
        not isinstance(state_schema.get("schema"), str)
        or type(state_schema.get("schema_version")) is not int
        or type(state_version) is not int
    ):
        raise OpsError(
            "items state schema requires schema plus integer schema_version/version"
        )

    errors: list[Issue] = []
    warnings: list[Issue] = []
    receipt = baseline["receipt"]
    frozen_state = receipt["state_schema"]
    active_fields = set(receipt["field_ids"]["active"])
    reserved_fields = set(receipt["field_ids"]["reserved"])
    if receipt["lua_evaluation_schema"] != evaluation["schema"]:
        errors.append(issue(
            "receipt-evaluation-schema",
            f"receipt expects {receipt['lua_evaluation_schema']!r}, got {evaluation['schema']!r}",
        ))
    if (
        frozen_state["schema"] != state_schema["schema"]
        or frozen_state["schema_version"] != state_schema["schema_version"]
    ):
        errors.append(issue(
            "state-schema-identity-change",
            "items state schema identity/version differs from the shipped receipt",
        ))
    elif state_version < frozen_state["version"]:
        errors.append(issue(
            "state-schema-regression",
            f"items state version regressed from {frozen_state['version']} to {state_version}",
        ))
    for field_id in sorted(active_fields - field_ids):
        errors.append(issue(
            "field-removed-without-reaction",
            f"shipped field_id {field_id!r} is absent; move it from field_ids.active to reserved explicitly",
            id=field_id,
        ))
    for field_id in sorted(reserved_fields & field_ids):
        errors.append(issue(
            "reserved-field-reused",
            f"reserved field_id {field_id!r} cannot be reused; restore it explicitly in the receipt or choose a new id",
            id=field_id,
        ))

    locked = baseline["def_ids"]
    removed = baseline["removed"]
    for item_id in sorted(set(locked) & set(removed)):
        errors.append(issue(
            "lock-inconsistent",
            f"def_id {item_id!r} is listed in both def_ids and removed",
            id=item_id,
        ))
    for item_id in sorted(locked):
        if item_id not in items:
            if item_id not in removed:
                errors.append(issue(
                    "removed-without-reaction",
                    f"shipped def_id {item_id!r} is absent; move it to removed with a delivered migration reaction",
                    id=item_id,
                ))
            continue
        current, shipped = items[item_id], locked[item_id]
        if current["storage"] != shipped["storage"]:
            errors.append(issue(
                "storage-change-without-reaction",
                f"def_id {item_id!r} shipped as {shipped['storage']!r} but is now {current['storage']!r}",
                id=item_id,
                field="storage",
            ))
        if current["level_count"] < shipped["level_count"]:
            errors.append(issue(
                "level-shrink-without-reaction",
                f"def_id {item_id!r} shipped with level_count={shipped['level_count']} but now has {current['level_count']}",
                id=item_id,
                field="level_count",
            ))
    for item_id in sorted(removed):
        entry = removed[item_id]
        fragment_version = entry.get("fragment_version")
        if type(fragment_version) is not int or fragment_version < MIN_REMOVED_FRAGMENT_VERSION:
            errors.append(issue(
                "lock-invalid",
                f"removed def_id {item_id!r} requires fragment_version >= {MIN_REMOVED_FRAGMENT_VERSION}",
                id=item_id,
                field="fragment_version",
            ))
        elif fragment_version > state_version:
            errors.append(issue(
                "removed-version-not-shipped",
                f"removed def_id {item_id!r} reaction version {fragment_version} exceeds state version {state_version}",
                id=item_id,
                field="fragment_version",
            ))
        if item_id in items:
            current = items[item_id]
            if entry.get("storage") in {"stack", "unique"} and current["storage"] != entry["storage"]:
                errors.append(issue(
                    "storage-change-without-reaction",
                    f"restored def_id {item_id!r} was {entry['storage']!r} but is now {current['storage']!r}",
                    id=item_id,
                    field="storage",
                ))
            removed_level_count = entry.get("level_count")
            if type(removed_level_count) is int and current["level_count"] < removed_level_count:
                errors.append(issue(
                    "level-shrink-without-reaction",
                    f"restored def_id {item_id!r} shipped with level_count={removed_level_count} but now has {current['level_count']}",
                    id=item_id,
                    field="level_count",
                ))
            warnings.append(issue(
                "removed-def-restored",
                f"removed def_id {item_id!r} is active again; move it back to def_ids if restoration is permanent",
                id=item_id,
            ))
    return errors, warnings, field_ids, items


def validate_evaluation_receipt(
    evaluation: dict[str, Any],
    baseline: dict[str, Any],
    state_schema: dict[str, Any],
    *,
    baseline_path: Path,
) -> dict[str, Any]:
    validate_baseline_shape(baseline, baseline_path)
    if baseline.get("schema_version") != RECEIPT_SCHEMA_VERSION:
        raise OpsError(
            "evaluation receipt requires schema_version 4; run upgrade-receipt first"
        )
    errors, warnings, _, _ = _evaluation_receipt_checks(
        evaluation, baseline, state_schema,
    )
    return {"ok": not errors, "errors": errors, "warnings": warnings}


def _load_evaluation_receipt_inputs(args: Any) -> tuple[dict[str, Any], dict[str, Any], dict[str, Any], Path]:
    evaluation = _load_json(Path(args.evaluation), "Items Lua evaluation")
    baseline_path = Path(args.baseline)
    baseline = _load_json(baseline_path, "lock baseline")
    validate_baseline_shape(baseline, baseline_path)
    if baseline.get("schema_version") != RECEIPT_SCHEMA_VERSION:
        raise OpsError(
            "evaluation receipt requires schema_version 4; run upgrade-receipt first"
        )
    state_schema = _load_json(
        Path(args.state_schema), "items state fragment schema",
    )
    return evaluation, baseline, state_schema, baseline_path


def cmd_validate_evaluation_receipt(args: Any) -> int:
    evaluation, baseline, state_schema, baseline_path = _load_evaluation_receipt_inputs(args)
    payload = validate_evaluation_receipt(
        evaluation, baseline, state_schema, baseline_path=baseline_path,
    )
    if args.json:
        print(json.dumps(payload, ensure_ascii=False, indent=2))
    else:
        print("evaluation receipt OK" if payload["ok"] else "evaluation receipt FAILED")
        for entry in [*payload["errors"], *payload["warnings"]]:
            print(f"  {format_issue(entry)}")
    return 0 if payload["ok"] else 1


def cmd_seal_evaluation_receipt(args: Any) -> int:
    evaluation, baseline, state_schema, baseline_path = _load_evaluation_receipt_inputs(args)
    errors, warnings, field_ids, items = _evaluation_receipt_checks(
        evaluation, baseline, state_schema,
    )
    if errors:
        payload = {
            "ok": False, "changed": False,
            "errors": errors, "warnings": warnings,
        }
        if args.json:
            print(json.dumps(payload, ensure_ascii=False, indent=2))
        else:
            print("evaluation receipt FAILED")
            for entry in errors:
                print(f"  {format_issue(entry)}")
        return 1

    sealed = json.loads(json.dumps(baseline))
    receipt = sealed["receipt"]
    reserved_fields = set(receipt["field_ids"]["reserved"])
    receipt["items_core_version"] = ITEMS_CORE_VERSION
    receipt["lua_evaluation_schema"] = evaluation["schema"]
    receipt["snapshot_schema"] = "items.snapshot.v1"
    receipt["state_schema"] = {
        "schema": state_schema["schema"],
        "schema_version": state_schema["schema_version"],
        "version": state_schema["version"],
    }
    receipt["field_ids"] = {
        "active": sorted(field_ids - reserved_fields),
        "reserved": sorted(reserved_fields),
    }
    removed_ids = set(sealed["removed"])
    sealed["def_ids"] = {
        item_id: items[item_id]
        for item_id in sorted(items)
        if item_id not in removed_ids
    }
    sealed["removed"] = {
        item_id: sealed["removed"][item_id]
        for item_id in sorted(sealed["removed"])
    }
    changed = write_utf8_if_changed(
        baseline_path,
        json.dumps(sealed, ensure_ascii=False, indent=2) + "\n",
    )
    payload = {
        "ok": True,
        "changed": changed,
        "errors": [],
        "warnings": warnings,
        "path": str(baseline_path),
    }
    if args.json:
        print(json.dumps(payload, ensure_ascii=False, indent=2))
    else:
        print(
            f"evaluation receipt {'sealed' if changed else 'unchanged'}: {baseline_path}"
        )
        for entry in warnings:
            print(f"  {format_issue(entry)}")
    return 0
