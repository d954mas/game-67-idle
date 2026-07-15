#!/usr/bin/env python3
"""Focused semantic Items CLI over the canonical Lua evaluator and Snapshot."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from typing import Any

import items_lua_edit as edit_api
import items_ops as receipt_api
import items_runtime_package as package_api
import items_snapshot as snapshot_api


RESULT_SCHEMA = "items.cli.result.v1"
ERROR_SCHEMA = "items.cli.error.v1"
DEFAULT_MAX_ITEMS = 1_000
DEFAULT_MAX_FIELDS = 256
DEFAULT_MAX_RELATED = 1_000
MAX_PATCH_BYTES = 64 * 1024
MAX_BATCH_OPERATIONS = 100
SCRIPT_DIR = Path(__file__).resolve().parent
SANDBOX = SCRIPT_DIR / "items_lua_sandbox.py"


class CliFailure(Exception):
    def __init__(self, code: str, message: str, **detail: Any) -> None:
        super().__init__(message)
        self.error = {"code": code, "message": message, **detail}


class CliArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        raise CliFailure("cli.arguments", message)


def _source_hash(data: bytes) -> str:
    return f"sha256:{hashlib.sha256(data).hexdigest()}"


def _project_paths(root_value: str, manifest_value: str) -> tuple[Path, Path]:
    root = Path(root_value).resolve()
    if not root.is_dir():
        raise CliFailure("cli.project_root", f"project root is not a directory: {root}")
    manifest = (root / manifest_value).resolve()
    try:
        manifest.relative_to(root)
    except ValueError as error:
        raise CliFailure("cli.manifest", "manifest must stay inside project root") from error
    if not manifest.is_file():
        raise CliFailure("cli.manifest", f"manifest not found: {manifest}")
    return root, manifest


def _project_file(root: Path, value: str, code: str) -> Path:
    path = (root / value).resolve()
    try:
        path.relative_to(root)
    except ValueError as error:
        raise CliFailure(code, f"path must stay inside project root: {value}") from error
    if not path.is_file():
        raise CliFailure(code, f"file not found: {path}")
    return path


def _evaluate(root: Path, manifest: Path) -> dict[str, Any]:
    result = subprocess.run(
        [
            sys.executable, str(SANDBOX), "evaluate",
            "--root", str(root), "--manifest", str(manifest),
        ],
        text=True, capture_output=True, encoding="utf-8", timeout=20,
    )
    if result.returncode != 0:
        try:
            failure = json.loads(result.stderr)
            detail = failure.get("error")
        except json.JSONDecodeError:
            detail = None
        if isinstance(detail, dict) and isinstance(detail.get("code"), str):
            raise CliFailure(
                detail["code"], str(detail.get("message", "Items evaluation failed")),
                diagnostic=detail,
            )
        raise CliFailure("cli.evaluate", "Items evaluator failed without structured diagnostics")
    try:
        evaluation = json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise CliFailure("cli.evaluate", "Items evaluator returned invalid JSON") from error
    if not isinstance(evaluation, dict):
        raise CliFailure("cli.evaluate", "Items evaluator returned a non-object result")
    return evaluation


def _model(args: argparse.Namespace) -> tuple[Path, dict[str, Any], dict[str, Any]]:
    root, manifest = _project_paths(args.project_root, args.manifest)
    evaluation = _evaluate(root, manifest)
    return root, evaluation, snapshot_api.build_snapshot(evaluation)


def _load_json(path: Path, code: str) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise CliFailure(code, f"cannot read {path}: {error}") from error
    if not isinstance(value, dict):
        raise CliFailure(code, f"JSON root must be an object: {path}")
    return value


def _validation(
    root: Path, evaluation: dict[str, Any], snapshot: dict[str, Any], args: argparse.Namespace,
) -> dict[str, Any]:
    baseline_path = _project_file(root, args.baseline, "cli.baseline")
    state_path = _project_file(root, args.state_schema, "cli.state_schema")
    return _validation_paths(evaluation, snapshot, baseline_path, state_path)


def _validation_paths(
    evaluation: dict[str, Any], snapshot: dict[str, Any],
    baseline_path: Path, state_path: Path,
) -> dict[str, Any]:
    receipt = receipt_api.validate_evaluation_receipt(
        evaluation,
        _load_json(baseline_path, "cli.baseline"),
        _load_json(state_path, "cli.state_schema"),
        baseline_path=baseline_path,
    )
    diagnostics = snapshot_api.query_requirements(snapshot)["results"]
    requirements_ok = not any(
        entry["severity"] == "error" and entry["effective_status"] == "fail"
        for entry in diagnostics
    )
    return {
        "ok": receipt["ok"] and requirements_ok,
        "diagnostics": diagnostics,
        "receipt": receipt,
    }


def _list(snapshot: dict[str, Any], max_items: int) -> list[dict[str, Any]]:
    if max_items < 1 or max_items > DEFAULT_MAX_ITEMS:
        raise CliFailure("cli.max_items", f"max-items must be between 1 and {DEFAULT_MAX_ITEMS}")
    items = snapshot["items"]
    if len(items) > max_items:
        raise CliFailure("cli.result_limit", f"list exceeds {max_items} items; increase max-items")
    runtime = {entry["id"]: entry for entry in snapshot["runtime_export"]["items"]}
    return [
        {
            "id": item["id"],
            "kind": item["kind"],
            "runtime": {
                "storage": runtime[item["id"]]["storage"],
                "level_count": runtime[item["id"]]["level_count"],
            },
        }
        for item in items
    ]


def _copy_project_for_edit(
    root: Path, args: argparse.Namespace, source_path: Path, edited_bytes: bytes,
) -> tuple[dict[str, Any], dict[str, Any], dict[str, Any]]:
    _, manifest_path = _project_paths(args.project_root, args.manifest)
    baseline_path = _project_file(root, args.baseline, "cli.baseline")
    state_path = _project_file(root, args.state_schema, "cli.state_schema")
    manifest = _load_json(manifest_path, "cli.manifest")
    modules = manifest.get("modules")
    if not isinstance(modules, list):
        raise CliFailure("cli.manifest", "manifest modules must be a list")
    module_paths: set[Path] = set()
    for entry in modules:
        value = entry.get("file") if isinstance(entry, dict) else None
        if not isinstance(value, str) or not value:
            raise CliFailure("cli.manifest", "manifest module requires a file")
        module_paths.add(_project_file(root, value, "cli.manifest"))
    if source_path not in module_paths:
        raise CliFailure("edit.source", "definition source is not an allowlisted manifest module")

    with tempfile.TemporaryDirectory() as temporary:
        temp_root = Path(temporary)
        inputs = {*module_paths, manifest_path, baseline_path, state_path}
        for original in inputs:
            relative = original.relative_to(root)
            target = temp_root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(edited_bytes if original == source_path else original.read_bytes())
        temp_manifest = temp_root / manifest_path.relative_to(root)
        evaluation = _evaluate(temp_root, temp_manifest)
        snapshot = snapshot_api.build_snapshot(evaluation)
        validation = _validation_paths(
            evaluation, snapshot,
            temp_root / baseline_path.relative_to(root),
            temp_root / state_path.relative_to(root),
        )
        return evaluation, snapshot, validation


def _atomic_replace_expected(path: Path, expected_hash: str, content: bytes) -> None:
    lock_path = path.with_name(f".{path.name}.items-edit.lock")
    try:
        lock_descriptor = os.open(lock_path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    except FileExistsError as error:
        raise CliFailure(
            "edit.locked",
            f"writer lock exists: {lock_path}; stale locks are never removed automatically, "
            "confirm no writer is active before deleting it",
        ) from error
    temporary: str | None = None
    try:
        with os.fdopen(lock_descriptor, "w", encoding="utf-8") as lock_stream:
            lock_stream.write(json.dumps({"pid": os.getpid(), "source": path.name}) + "\n")
            lock_stream.flush()
            os.fsync(lock_stream.fileno())
        if _source_hash(path.read_bytes()) != expected_hash:
            raise CliFailure("edit.conflict", "source changed after preview; refresh and retry")
        file_mode = path.stat().st_mode
        descriptor, temporary = tempfile.mkstemp(
            prefix=f".{path.name}.", suffix=".tmp", dir=path.parent,
        )
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.chmod(temporary, file_mode)
        if _source_hash(path.read_bytes()) != expected_hash:
            raise CliFailure("edit.conflict", "source changed while applying; refresh and retry")
        os.replace(temporary, path)
        temporary = None
    finally:
        if temporary is not None:
            try:
                os.unlink(temporary)
            except FileNotFoundError:
                pass
        try:
            os.unlink(lock_path)
        except FileNotFoundError:
            pass


def _source_line(source: str, offset: int) -> tuple[int, str]:
    line = source.count("\n", 0, offset) + 1
    lines = source.splitlines()
    text = lines[line - 1] if line <= len(lines) else ""
    return line, text[:512]


def _primitive_edit(
    source: str, definition: dict[str, Any], operation: dict[str, Any],
) -> edit_api.EditResult:
    common = {
        "definition_line": definition["line"],
        "item_id": operation["item"],
        "field": operation["field"],
        "value": operation["value"],
    }
    if operation["operation"] == "level-set":
        return edit_api.level_set(source, level=operation["level"], **common)
    if operation["operation"] == "curve-set":
        return edit_api.curve_set(source, parameter=operation["parameter"], **common)
    return edit_api.override_set(source, level=operation["level"], **common)


def _load_batch_patch(path_value: str) -> dict[str, Any]:
    path = Path(path_value).resolve()
    try:
        if path.stat().st_size > MAX_PATCH_BYTES:
            raise CliFailure("edit.patch", f"patch exceeds {MAX_PATCH_BYTES} bytes")
        raw = path.read_bytes()
        if len(raw) > MAX_PATCH_BYTES:
            raise CliFailure("edit.patch", f"patch exceeds {MAX_PATCH_BYTES} bytes")
        patch = json.loads(raw.decode("utf-8"))
    except CliFailure:
        raise
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise CliFailure("edit.patch", f"cannot read patch {path}: {error}") from error
    if (
        not isinstance(patch, dict)
        or set(patch) != {"schema", "expected_source_hash", "operations"}
        or patch.get("schema") != "items.cli.patch_batch.v1"
        or not isinstance(patch.get("expected_source_hash"), str)
        or re.fullmatch(r"sha256:[0-9a-f]{64}", patch["expected_source_hash"]) is None
    ):
        raise CliFailure("edit.patch", "patch header/schema is invalid")
    operations = patch.get("operations")
    if not isinstance(operations, list) or not 1 <= len(operations) <= MAX_BATCH_OPERATIONS:
        raise CliFailure(
            "edit.patch", f"operations must contain 1..{MAX_BATCH_OPERATIONS} entries",
        )
    targets: set[tuple[Any, ...]] = set()
    for index, operation in enumerate(operations):
        if not isinstance(operation, dict):
            raise CliFailure("edit.patch", f"operation {index} must be an object")
        name = operation.get("operation")
        keys = (
            {"operation", "item", "field", "parameter", "value"}
            if name == "curve-set"
            else {"operation", "item", "field", "level", "value"}
        )
        if name not in {"level-set", "curve-set", "override-set"} or set(operation) != keys:
            raise CliFailure("edit.patch", f"operation {index} shape is invalid")
        if (
            not isinstance(operation["item"], str) or not operation["item"]
            or not isinstance(operation["field"], str) or not operation["field"]
            or type(operation["value"]) is not int
            or ("level" in operation and type(operation["level"]) is not int)
            or ("parameter" in operation and not isinstance(operation["parameter"], str))
        ):
            raise CliFailure("edit.patch", f"operation {index} values are invalid")
        target = (
            name, operation["item"], operation["field"],
            operation.get("level"), operation.get("parameter"),
        )
        if target in targets:
            raise CliFailure("edit.patch", f"operation {index} duplicates an earlier target")
        targets.add(target)
    return patch


def _edit(
    root: Path, evaluation: dict[str, Any], snapshot: dict[str, Any], args: argparse.Namespace,
) -> tuple[dict[str, Any], int]:
    del evaluation  # the edited model is evaluated afresh below
    query = snapshot_api.query_snapshot(snapshot, item_id=args.item)
    definition = query["source"]
    source_path = _project_file(root, definition["file"], "edit.source")
    original_bytes = source_path.read_bytes()
    actual_hash = _source_hash(original_bytes)
    if args.expected_source_hash != actual_hash:
        raise CliFailure(
            "edit.conflict", "expected source hash does not match current file",
            expected=args.expected_source_hash, actual=actual_hash,
        )
    try:
        source = original_bytes.decode("utf-8")
    except UnicodeDecodeError as error:
        raise CliFailure("edit.source", f"source is not UTF-8: {source_path}") from error
    operation = {
        "operation": args.command,
        "item": args.item,
        "field": args.field,
        "value": args.value,
    }
    if args.command in {"level-set", "override-set"}:
        operation["level"] = args.level
    else:
        operation["parameter"] = args.parameter
    edited = _primitive_edit(source, definition, operation)
    edited_bytes = edited.source.encode("utf-8")
    after_hash = _source_hash(edited_bytes)
    _, after_snapshot, validation = _copy_project_for_edit(
        root, args, source_path, edited_bytes,
    )
    semantic_diff = snapshot_api.diff_snapshots(snapshot, after_snapshot)
    line, before_line = _source_line(source, edited.start)
    _, after_line = _source_line(edited.source, edited.start)
    source_changed = edited_bytes != original_bytes
    applied = False
    if validation["ok"] and args.apply and source_changed:
        _atomic_replace_expected(source_path, actual_hash, edited_bytes)
        applied = True
    inverse = {
        "schema": "items.cli.patch.v1",
        "operation": args.command,
        "item": args.item,
        "field": args.field,
        "value": edited.old_value,
        "expected_source_hash": after_hash,
    }
    if args.command in {"level-set", "override-set"}:
        inverse["level"] = args.level
    else:
        inverse["parameter"] = args.parameter
    result = {
        **validation,
        "applied": applied,
        "source_diff": {
            "file": definition["file"],
            "line": line,
            "before_hash": actual_hash,
            "after_hash": after_hash,
            "old_value": edited.old_value,
            "new_value": args.value,
            "before_line": before_line,
            "after_line": after_line,
        },
        "semantic_diff": semantic_diff,
        "inverse_patch": inverse,
    }
    return result, 0 if validation["ok"] else 1


def _batch(
    root: Path, snapshot: dict[str, Any], args: argparse.Namespace,
) -> tuple[dict[str, Any], int]:
    patch = _load_batch_patch(args.patch_file)
    resolved: list[tuple[dict[str, Any], dict[str, Any], Path]] = []
    source_paths: set[Path] = set()
    for operation in patch["operations"]:
        definition = snapshot_api.query_snapshot(
            snapshot, item_id=operation["item"],
        )["source"]
        source_path = _project_file(root, definition["file"], "edit.source")
        source_paths.add(source_path)
        resolved.append((operation, definition, source_path))
    if len(source_paths) != 1:
        raise CliFailure("edit.multi_file", "v1 batch operations must share one Lua source file")
    source_path = next(iter(source_paths))
    original_bytes = source_path.read_bytes()
    actual_hash = _source_hash(original_bytes)
    if patch["expected_source_hash"] != actual_hash:
        raise CliFailure(
            "edit.conflict", "expected source hash does not match current file",
            expected=patch["expected_source_hash"], actual=actual_hash,
        )
    try:
        source = original_bytes.decode("utf-8")
    except UnicodeDecodeError as error:
        raise CliFailure("edit.source", f"source is not UTF-8: {source_path}") from error

    current = source
    edits: list[dict[str, Any]] = []
    inverse_operations: list[dict[str, Any]] = []
    for operation, definition, _ in resolved:
        edited = _primitive_edit(current, definition, operation)
        line, before_line = _source_line(current, edited.start)
        _, after_line = _source_line(edited.source, edited.start)
        edits.append({
            "operation": operation["operation"],
            "item": operation["item"],
            "field": operation["field"],
            "line": line,
            "old_value": edited.old_value,
            "new_value": operation["value"],
            "before_line": before_line,
            "after_line": after_line,
        })
        inverse = {**operation, "value": edited.old_value}
        inverse_operations.append(inverse)
        current = edited.source

    edited_bytes = current.encode("utf-8")
    after_hash = _source_hash(edited_bytes)
    _, after_snapshot, validation = _copy_project_for_edit(
        root, args, source_path, edited_bytes,
    )
    semantic_diff = snapshot_api.diff_snapshots(snapshot, after_snapshot)
    source_changed = edited_bytes != original_bytes
    applied = False
    if validation["ok"] and args.apply and source_changed:
        _atomic_replace_expected(source_path, actual_hash, edited_bytes)
        applied = True
    inverse_patch = {
        "schema": "items.cli.patch_batch.v1",
        "expected_source_hash": after_hash,
        "operations": list(reversed(inverse_operations)),
    }
    result = {
        **validation,
        "applied": applied,
        "source_diff": {
            "file": resolved[0][1]["file"],
            "before_hash": actual_hash,
            "after_hash": after_hash,
            "edits": edits,
        },
        "semantic_diff": semantic_diff,
        "inverse_patch": inverse_patch,
    }
    return result, 0 if validation["ok"] else 1


def _result(operation: str, snapshot: dict[str, Any], result: Any) -> dict[str, Any]:
    return {
        "schema": RESULT_SCHEMA,
        "operation": operation,
        "content_hash": snapshot["content_hash"],
        "result": result,
    }


def _parser() -> argparse.ArgumentParser:
    parser = CliArgumentParser(description=__doc__)
    parser.add_argument("--project-root", required=True)
    parser.add_argument("--manifest", default="items.lua.json")
    commands = parser.add_subparsers(dest="command", required=True)

    list_command = commands.add_parser("list")
    list_command.add_argument("--max-items", type=int, default=DEFAULT_MAX_ITEMS)

    inspect = commands.add_parser("inspect")
    inspect.add_argument("--item", required=True)
    inspect.add_argument("--field")
    inspect.add_argument("--level-from", type=int)
    inspect.add_argument("--level-to", type=int)

    dependencies = commands.add_parser("dependencies")
    dependencies.add_argument("--item", required=True)
    dependencies.add_argument("--max-related", type=int, default=DEFAULT_MAX_RELATED)

    source = commands.add_parser("source")
    source.add_argument("--item", required=True)
    source.add_argument("--field")

    chart = commands.add_parser("chart")
    chart.add_argument("--item", required=True)
    chart.add_argument("--field", required=True)
    chart.add_argument("--level-from", type=int)
    chart.add_argument("--level-to", type=int)
    chart.add_argument("--max-points", type=int, default=snapshot_api.DEFAULT_CHART_POINTS)

    requirements = commands.add_parser("requirements")
    requirements.add_argument("--item")
    requirements.add_argument("--severity", choices=["warning", "error"])
    requirements.add_argument(
        "--max-results", type=int, default=snapshot_api.DEFAULT_REQUIREMENT_RESULTS,
    )

    commands.add_parser("schema")
    validate = commands.add_parser("validate")
    validate.add_argument("--baseline", default="content/items.lock.json")
    validate.add_argument("--state-schema", default="state/items.schema.json")
    build = commands.add_parser("build")
    build.add_argument("--baseline", default="content/items.lock.json")
    build.add_argument("--state-schema", default="state/items.schema.json")
    build.add_argument("--out-dir", required=True)
    for name in ("level-set", "curve-set", "override-set"):
        edit = commands.add_parser(name)
        edit.add_argument("--item", required=True)
        edit.add_argument("--field", required=True)
        edit.add_argument("--value", type=int, required=True)
        edit.add_argument("--expected-source-hash", required=True)
        edit.add_argument("--baseline", default="content/items.lock.json")
        edit.add_argument("--state-schema", default="state/items.schema.json")
        edit.add_argument("--apply", action="store_true")
        if name in {"level-set", "override-set"}:
            edit.add_argument("--level", type=int, required=True)
        else:
            edit.add_argument("--parameter", required=True)
    batch = commands.add_parser("batch")
    batch.add_argument("--patch-file", required=True)
    batch.add_argument("--baseline", default="content/items.lock.json")
    batch.add_argument("--state-schema", default="state/items.schema.json")
    batch.add_argument("--apply", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    operation: str | None = None
    try:
        args = _parser().parse_args(argv)
        operation = args.command
        root, evaluation, snapshot = _model(args)
        exit_code = 0
        if operation == "list":
            result = _list(snapshot, args.max_items)
        elif operation == "inspect":
            result = snapshot_api.query_snapshot(
                snapshot, item_id=args.item, field=args.field,
                level_from=args.level_from, level_to=args.level_to,
            )
        elif operation == "dependencies":
            if args.max_related < 1 or args.max_related > DEFAULT_MAX_RELATED:
                raise CliFailure(
                    "cli.max_related",
                    f"max-related must be between 1 and {DEFAULT_MAX_RELATED}",
                )
            query = snapshot_api.query_snapshot(
                snapshot, item_id=args.item,
                include_inputs=True, include_dependents=True,
            )
            result = {
                "item": args.item,
                "inputs": query["inputs"],
                "dependents": query["dependents"],
            }
            if len(result["inputs"]) > args.max_related or len(result["dependents"]) > args.max_related:
                raise CliFailure(
                    "cli.result_limit",
                    f"dependency result exceeds {args.max_related}; use a more focused item",
                )
        elif operation == "source":
            query = snapshot_api.query_snapshot(snapshot, item_id=args.item, field=args.field)
            source_path = _project_file(root, query["source"]["file"], "edit.source")
            result = {
                "item": args.item,
                "definition": query["source"],
                "source_hash": _source_hash(source_path.read_bytes()),
            }
            if args.field is not None and "field" in query:
                result["field"] = query["field"]["source"]
        elif operation == "chart":
            result = snapshot_api.chart_snapshot(
                snapshot, item_id=args.item, field=args.field,
                level_from=args.level_from, level_to=args.level_to,
                max_points=args.max_points,
            )
        elif operation == "requirements":
            result = snapshot_api.query_requirements(
                snapshot, item_id=args.item, severity=args.severity,
                max_results=args.max_results,
            )
        elif operation == "schema":
            if len(snapshot["fields"]) > DEFAULT_MAX_FIELDS:
                raise CliFailure("cli.result_limit", f"schema exceeds {DEFAULT_MAX_FIELDS} fields")
            result = {
                "fields": snapshot["fields"],
                "kinds": sorted({item["kind"] for item in snapshot["items"]}),
            }
        elif operation == "validate":
            result = _validation(root, evaluation, snapshot, args)
            exit_code = 0 if result["ok"] else 1
        elif operation in {"level-set", "curve-set", "override-set"}:
            result, exit_code = _edit(root, evaluation, snapshot, args)
        elif operation == "batch":
            result, exit_code = _batch(root, snapshot, args)
        else:
            validation = _validation(root, evaluation, snapshot, args)
            if not validation["ok"]:
                result = {
                    **validation,
                    "changed": {"snapshot": False, "blob": False, "header": False},
                }
                exit_code = 1
            else:
                out_dir = Path(args.out_dir).resolve()
                snapshot_path = out_dir / "items.snapshot.json"
                blob_path = out_dir / "items.catalog"
                header_path = out_dir / "items_catalog_abi.gen.h"
                package = package_api.build_package(snapshot)
                header = package_api.render_abi_header(snapshot).encode("utf-8")
                inspected = package_api.inspect_package(package)
                changed = {
                    "snapshot": package_api.write_if_different(
                        snapshot_path, snapshot_api.snapshot_json_bytes(snapshot) + b"\n",
                    ),
                    "blob": package_api.write_if_different(blob_path, package),
                    "header": package_api.write_if_different(header_path, header),
                }
                result = {
                    **validation,
                    "changed": changed,
                    "outputs": {
                        "snapshot": str(snapshot_path),
                        "blob": str(blob_path),
                        "header": str(header_path),
                    },
                    "content_fingerprint": inspected["content_fingerprint"],
                    "schema_abi_fingerprint": inspected["schema_abi_fingerprint"],
                }
        print(json.dumps(_result(operation, snapshot, result), ensure_ascii=False, sort_keys=True))
        return exit_code
    except CliFailure as error:
        detail = error.error
    except snapshot_api.SnapshotFailure as error:
        detail = {"code": error.code, "message": error.message, "path": error.path}
    except receipt_api.OpsError as error:
        detail = {"code": "cli.receipt", "message": str(error)}
    except package_api.PackageFailure as error:
        detail = {"code": "cli.package", "message": str(error)}
    except edit_api.EditFailure as error:
        message = str(error)
        code, _, detail_message = message.partition(": ")
        detail = {"code": code, "message": detail_message or message}
    except (OSError, subprocess.TimeoutExpired) as error:
        detail = {"code": "cli.io", "message": str(error)}
    print(json.dumps({
        "schema": ERROR_SCHEMA,
        "operation": operation,
        "error": detail,
    }, ensure_ascii=False, sort_keys=True), file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
