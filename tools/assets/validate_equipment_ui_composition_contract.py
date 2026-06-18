from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate a Mine Cards equipment UI composition contract.")
    parser.add_argument("--contract", required=True)
    return parser.parse_args()


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def has_text(value: Any) -> bool:
    return isinstance(value, str) and bool(value.strip())


def add(problems: list[str], message: str) -> None:
    problems.append(message)


def require_list(value: Any, label: str, problems: list[str], *, min_items: int = 1) -> list[Any]:
    if not isinstance(value, list) or len(value) < min_items:
        add(problems, f"{label} must be a list with at least {min_items} item(s)")
        return []
    return value


def main() -> int:
    args = parse_args()
    root = Path.cwd()
    contract_path = Path(args.contract)
    contract = load_json(contract_path)
    problems: list[str] = []

    if contract.get("schema") != "game.equipment_ui_composition_contract":
        add(problems, "schema must be game.equipment_ui_composition_contract")
    if contract.get("status") != "gated_prep":
        add(problems, "status must remain gated_prep before acceptance")

    gate = contract.get("gate")
    if not isinstance(gate, dict):
        add(problems, "gate must be an object")
    else:
        blocked = require_list(gate.get("blocked_until"), "gate.blocked_until", problems, min_items=2)
        if not any("T0001" in str(item) for item in blocked):
            add(problems, "gate.blocked_until must mention T0001")
        if not any("T0008" in str(item) for item in blocked):
            add(problems, "gate.blocked_until must mention T0008")
        must_not_change = require_list(gate.get("must_not_change_before_gate"), "gate.must_not_change_before_gate", problems)
        if "external/neotolis-engine" not in must_not_change:
            add(problems, "gate.must_not_change_before_gate must include external/neotolis-engine")

    first_surface = contract.get("first_surface")
    if not isinstance(first_surface, dict) or first_surface.get("id") != "compact_item_compare_panel":
        add(problems, "first_surface.id must be compact_item_compare_panel")
    elif not has_text(first_surface.get("reason")):
        add(problems, "first_surface.reason must be text")

    item_manifest_rel = contract.get("input_item_manifest")
    if not has_text(item_manifest_rel):
        add(problems, "input_item_manifest is required")
        item_manifest = None
    else:
        manifest_path = root / item_manifest_rel
        if not manifest_path.exists():
            add(problems, f"input item manifest missing: {item_manifest_rel}")
            item_manifest = None
        else:
            item_manifest = load_json(manifest_path)

    scope = contract.get("input_item_scope")
    if not isinstance(scope, dict):
        add(problems, "input_item_scope must be an object")
        scope = {}
    atlas_ids = require_list(scope.get("atlas_item_ids"), "input_item_scope.atlas_item_ids", problems, min_items=12)
    proof_ids = require_list(scope.get("first_proof_item_ids"), "input_item_scope.first_proof_item_ids", problems, min_items=3)
    if item_manifest:
        manifest_ids = {asset.get("id") for asset in item_manifest.get("assets", []) if isinstance(asset, dict)}
        for item_id in atlas_ids:
            if item_id not in manifest_ids:
                add(problems, f"atlas item id missing from runtime item manifest: {item_id}")
        for item_id in proof_ids:
            if item_id not in atlas_ids:
                add(problems, f"first proof item id must also be in atlas scope: {item_id}")

    families = contract.get("required_source_families")
    families_list = require_list(families, "required_source_families", problems, min_items=4)
    family_ids = {entry.get("id") for entry in families_list if isinstance(entry, dict)}
    for family_id in ["equipment_slot_frames", "equipment_panel_frame", "equipment_state_overlays", "equipment_action_buttons"]:
        if family_id not in family_ids:
            add(problems, f"required_source_families must include {family_id}")
    for entry in families_list:
        if not isinstance(entry, dict):
            add(problems, "required_source_families entries must be objects")
            continue
        if not has_text(entry.get("kind")):
            add(problems, f"source family {entry.get('id', '<unknown>')} needs kind")
        require_list(entry.get("required_states"), f"source family {entry.get('id', '<unknown>')}.required_states", problems)
        rules = require_list(entry.get("rules"), f"source family {entry.get('id', '<unknown>')}.rules", problems)
        joined_rules = " ".join(str(rule).lower() for rule in rules)
        if "text" not in joined_rules and entry.get("id") in {"equipment_slot_frames", "equipment_action_buttons"}:
            add(problems, f"source family {entry.get('id')} rules should forbid baked text")

    runtime = contract.get("runtime_composition")
    if not isinstance(runtime, dict):
        add(problems, "runtime_composition must be an object")
    else:
        layers = require_list(runtime.get("must_compose_runtime_layers"), "runtime_composition.must_compose_runtime_layers", problems, min_items=5)
        for layer in ["item_sprite", "runtime_item_name", "runtime_stat_delta", "runtime_action_label"]:
            if layer not in layers:
                add(problems, f"runtime composition layer missing {layer}")
        must_not_bake = " ".join(str(item).lower() for item in require_list(runtime.get("must_not_bake"), "runtime_composition.must_not_bake", problems))
        for phrase in ["item names", "stat values", "button labels"]:
            if phrase not in must_not_bake:
                add(problems, f"runtime_composition.must_not_bake should include {phrase}")

    proof_targets = require_list(contract.get("composition_proof_targets"), "composition_proof_targets", problems, min_items=2)
    proof_ids_set = {entry.get("id") for entry in proof_targets if isinstance(entry, dict)}
    for proof_id in ["desktop_compact_compare", "portrait_compact_compare"]:
        if proof_id not in proof_ids_set:
            add(problems, f"composition_proof_targets must include {proof_id}")
    for entry in proof_targets:
        if not isinstance(entry, dict):
            add(problems, "composition_proof_targets entries must be objects")
            continue
        size = entry.get("size")
        if not isinstance(size, list) or len(size) != 2 or not all(isinstance(v, int) and v > 0 for v in size):
            add(problems, f"composition proof {entry.get('id', '<unknown>')} needs positive [w,h] size")

    gates = set(require_list(contract.get("required_gate_outputs"), "required_gate_outputs", problems, min_items=8))
    for gate_name in [
        "composition_proof",
        "atlas_metadata_audit",
        "review_atlas_audit",
        "source_family_coverage_audit",
        "runtime_usage_audit_after_native_integration",
        "native_product_gate"
    ]:
        if gate_name not in gates:
            add(problems, f"required_gate_outputs must include {gate_name}")

    boundary = contract.get("native_proof_boundary")
    if not isinstance(boundary, dict):
        add(problems, "native_proof_boundary must be an object")
    elif boundary.get("external_engine_diff_allowed") is not False:
        add(problems, "native_proof_boundary.external_engine_diff_allowed must be false")

    if problems:
        print("equipment UI composition contract validation failed:")
        for problem in problems:
            print(f"- {problem}")
        return 1

    print(f"ok: equipment UI composition contract {contract.get('id')} is valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
