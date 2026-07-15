#!/usr/bin/env python3
"""Focused semantic Items CLI over the canonical Lua evaluator and Snapshot."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
from typing import Any

import items_ops as receipt_api
import items_runtime_package as package_api
import items_snapshot as snapshot_api


RESULT_SCHEMA = "items.cli.result.v1"
ERROR_SCHEMA = "items.cli.error.v1"
DEFAULT_MAX_ITEMS = 1_000
DEFAULT_MAX_FIELDS = 256
DEFAULT_MAX_RELATED = 1_000
SCRIPT_DIR = Path(__file__).resolve().parent
SANDBOX = SCRIPT_DIR / "items_lua_sandbox.py"


class CliFailure(Exception):
    def __init__(self, code: str, message: str, **detail: Any) -> None:
        super().__init__(message)
        self.error = {"code": code, "message": message, **detail}


class CliArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        raise CliFailure("cli.arguments", message)


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
            result = {"item": args.item, "definition": query["source"]}
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
