from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


REQUIRED_TOP_LEVEL = [
    "schema",
    "version",
    "id",
    "task",
    "status",
    "gate",
    "selected_production_lane",
    "visual_identity",
    "body_and_gear_policy",
    "model_contract",
    "skeleton_contract",
    "required_source_files",
    "first_candidate_acceptance",
    "native_evidence_targets",
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate a Mine Cards character source pack contract.")
    parser.add_argument("--pack", required=True)
    return parser.parse_args()


def has_text(value: Any) -> bool:
    return isinstance(value, str) and bool(value.strip())


def require_text(data: dict[str, Any], key: str, problems: list[str], label: str = "") -> None:
    if not has_text(data.get(key)):
        problems.append(f"{label or 'object'} needs text field {key}")


def require_list(data: dict[str, Any], key: str, problems: list[str], *, min_items: int = 1) -> None:
    value = data.get(key)
    if not isinstance(value, list) or len(value) < min_items:
        problems.append(f"object needs list field {key} with at least {min_items} item(s)")


def main() -> int:
    args = parse_args()
    path = Path(args.pack)
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001 - report as validator problem.
        print(f"error: cannot read {path}: {exc}")
        return 1

    problems: list[str] = []
    for key in REQUIRED_TOP_LEVEL:
        if key not in data:
            problems.append(f"missing top-level field {key}")

    if data.get("schema") != "game.character_source_pack":
        problems.append("schema must be game.character_source_pack")
    if data.get("status") not in {"gated_prep", "active", "accepted", "rejected"}:
        problems.append("status must be gated_prep, active, accepted, or rejected")

    gate = data.get("gate")
    if isinstance(gate, dict):
        require_list(gate, "blocked_until", problems)
        must_not_change = gate.get("must_not_change")
        if not isinstance(must_not_change, list) or "external/neotolis-engine" not in must_not_change:
            problems.append("gate.must_not_change must include external/neotolis-engine")
    else:
        problems.append("gate must be an object")

    lane = data.get("selected_production_lane")
    if isinstance(lane, dict):
        require_text(lane, "id", problems, "selected_production_lane")
        require_text(lane, "decision", problems, "selected_production_lane")
        require_text(lane, "reason", problems, "selected_production_lane")
    else:
        problems.append("selected_production_lane must be an object")

    visual = data.get("visual_identity")
    if isinstance(visual, dict):
        for key in ["silhouette", "palette", "public_safety_distance"]:
            require_list(visual, key, problems, min_items=3)
    else:
        problems.append("visual_identity must be an object")

    gear = data.get("body_and_gear_policy")
    if isinstance(gear, dict):
        require_text(gear, "first_candidate", problems, "body_and_gear_policy")
        require_list(gear, "skinned", problems)
        require_list(gear, "rigid_socketed", problems)
    else:
        problems.append("body_and_gear_policy must be an object")

    model = data.get("model_contract")
    if isinstance(model, dict):
        if model.get("source_up_axis") != "Y":
            problems.append("model_contract.source_up_axis must be Y")
        require_text(model, "origin", problems, "model_contract")
        budget = model.get("target_budget")
        if not isinstance(budget, dict):
            problems.append("model_contract.target_budget must be an object")
        else:
            if budget.get("cpu_skin_ms_avg_max", 999) > 0.5:
                problems.append("cpu skin budget must be <= 0.5 ms average")
            if budget.get("skinned_vertices_max", 999999) > 5000:
                problems.append("skinned vertex budget must be <= 5000")
    else:
        problems.append("model_contract must be an object")

    skeleton = data.get("skeleton_contract")
    if isinstance(skeleton, dict):
        sockets = skeleton.get("sockets")
        socket_ids = {socket.get("id") for socket in sockets if isinstance(socket, dict)} if isinstance(sockets, list) else set()
        for socket_id in ["tool", "head", "chest"]:
            if socket_id not in socket_ids:
                problems.append(f"skeleton_contract.sockets must include {socket_id}")
    else:
        problems.append("skeleton_contract must be an object")

    source_files = data.get("required_source_files")
    if isinstance(source_files, list) and source_files:
        for index, entry in enumerate(source_files):
            if not isinstance(entry, dict):
                problems.append(f"required_source_files[{index}] must be an object")
                continue
            for key in ["id", "kind", "expected_path_pattern"]:
                require_text(entry, key, problems, f"required_source_files[{index}]")
    else:
        problems.append("required_source_files must be a non-empty list")

    acceptance = data.get("first_candidate_acceptance")
    if isinstance(acceptance, list):
        required_phrases = ["provenance", "native proof", "external/neotolis-engine"]
        joined = "\n".join(str(item) for item in acceptance).lower()
        for phrase in required_phrases:
            if phrase.lower() not in joined:
                problems.append(f"first_candidate_acceptance should mention {phrase}")
    else:
        problems.append("first_candidate_acceptance must be a list")

    if problems:
        print("character source pack validation failed:")
        for problem in problems:
            print(f"- {problem}")
        return 1

    print(f"ok: character source pack {data.get('id')} is valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
