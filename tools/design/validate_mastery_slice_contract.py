from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate a Mine Cards mastery slice contract.")
    parser.add_argument("--contract", required=True)
    parser.add_argument("--parameters", required=True)
    return parser.parse_args()


def problem(message: str, problems: list[str]) -> None:
    problems.append(message)


def has_text(value: Any) -> bool:
    return isinstance(value, str) and bool(value.strip())


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def find_node(parameters: dict[str, Any], node_id: str) -> dict[str, Any] | None:
    for node in parameters.get("mining_nodes", []):
        if node.get("id") == node_id:
            return node
    return None


def find_mastery_tier(parameters: dict[str, Any], tier_id: int) -> dict[str, Any] | None:
    for tier in parameters.get("mastery_tiers", []):
        if tier.get("tier") == tier_id:
            return tier
    return None


def main() -> int:
    args = parse_args()
    contract_path = Path(args.contract)
    parameters_path = Path(args.parameters)
    contract = load_json(contract_path)
    parameters = load_json(parameters_path)

    problems: list[str] = []
    if contract.get("schema") != "game.mastery_slice_contract":
        problem("schema must be game.mastery_slice_contract", problems)
    if contract.get("status") != "gated_prep":
        problem("status must remain gated_prep before T0001 acceptance", problems)

    gate = contract.get("gate")
    if not isinstance(gate, dict):
        problem("gate must be an object", problems)
    else:
        must_not_change = gate.get("must_not_change_before_gate")
        if not isinstance(must_not_change, list) or "external/neotolis-engine" not in must_not_change:
            problem("gate.must_not_change_before_gate must include external/neotolis-engine", problems)
        if not isinstance(gate.get("blocked_until"), list) or not gate["blocked_until"]:
            problem("gate.blocked_until must be a non-empty list", problems)

    source = contract.get("source_parameters")
    if not isinstance(source, dict):
        problem("source_parameters must be an object", problems)
        source = {}

    node_id = source.get("node")
    tier_id = source.get("tier")
    node = find_node(parameters, node_id)
    tier = find_mastery_tier(parameters, tier_id)
    if not node:
        problem(f"source node {node_id!r} not found in parameters", problems)
    if not tier:
        problem(f"source mastery tier {tier_id!r} not found in parameters", problems)

    if node:
        base_interval = node.get("base_interval_seconds")
        mastery_reward = node.get("rewards", {}).get("mastery_xp")
        if source.get("base_interval_seconds") != base_interval:
            problem("source base_interval_seconds must match parameters", problems)
        if source.get("mastery_xp_per_completed_tick") != mastery_reward:
            problem("mastery_xp_per_completed_tick must match node reward mastery_xp", problems)

    if tier:
        threshold = tier.get("mastery_xp")
        delta = tier.get("effects", {}).get("node_interval_multiplier_delta")
        if source.get("threshold_mastery_xp") != threshold:
            problem("threshold_mastery_xp must match mastery tier parameters", problems)
        if source.get("node_interval_multiplier_delta") != delta:
            problem("node_interval_multiplier_delta must match mastery tier parameters", problems)
        base_interval = source.get("base_interval_seconds")
        expected_interval = round(base_interval * (1.0 + delta), 2) if isinstance(base_interval, (int, float)) and isinstance(delta, (int, float)) else None
        if expected_interval is not None and source.get("tier1_interval_seconds") != expected_interval:
            problem(f"tier1_interval_seconds must be {expected_interval}", problems)

    state = contract.get("state_contract")
    if not isinstance(state, dict):
        problem("state_contract must be an object", problems)
    else:
        if state.get("domain_action_required") is not True:
            problem("state_contract.domain_action_required must be true", problems)
        if state.get("raw_state_mutation_allowed") is not False:
            problem("state_contract.raw_state_mutation_allowed must be false", problems)
        minimum = state.get("minimum_runtime_fields")
        for field in ["node_id", "mastery_xp", "mastery_tier", "last_tier_up_node_id"]:
            if not isinstance(minimum, list) or field not in minimum:
                problem(f"state_contract.minimum_runtime_fields must include {field}", problems)

    order = contract.get("reward_resolution_order")
    required_order = [
        "grant node resources",
        "grant mining XP",
        "grant node mastery XP",
        "check mining level-up",
        "check node mastery tier-up"
    ]
    if not isinstance(order, list):
        problem("reward_resolution_order must be a list", problems)
    else:
        for expected in required_order:
            if expected not in order:
                problem(f"reward_resolution_order missing {expected}", problems)

    matrix = contract.get("live_state_matrix_additions")
    matrix_ids = {entry.get("id") for entry in matrix if isinstance(entry, dict)} if isinstance(matrix, list) else set()
    for state_id in ["mastery_near_tier", "mastery_tier_up", "mastery_post_tier"]:
        if state_id not in matrix_ids:
            problem(f"live_state_matrix_additions must include {state_id}", problems)

    scenario = contract.get("devapi_scenario_contract")
    if not isinstance(scenario, dict):
        problem("devapi_scenario_contract must be an object", problems)
    else:
        expected = scenario.get("expected_state_after_tick")
        if not isinstance(expected, dict):
            problem("devapi_scenario_contract.expected_state_after_tick must be an object", problems)
        else:
            if expected.get("mastery_tier") != 1:
                problem("expected_state_after_tick.mastery_tier must be 1", problems)
            if not has_text(expected.get("feedback_contains")):
                problem("expected_state_after_tick.feedback_contains must be text", problems)

    if problems:
        print("mastery slice contract validation failed:")
        for item in problems:
            print(f"- {item}")
        return 1

    print(f"ok: mastery slice contract {contract.get('id')} is valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
