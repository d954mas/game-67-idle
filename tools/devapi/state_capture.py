#!/usr/bin/env python3
"""Universal multi-state screenshot capture + acceptance-matrix writer.

Game-agnostic. A per-game capture script (tools/<game-id>/capture_states.py)
drives the game with its OWN DevAPI commands and calls `capture(tag)` at each key
state. This helper owns the universal parts: the capture transport, coverage
tracking, optional pixel-health, and writing the `game.live_state_acceptance_matrix`
JSON that the product gate (`review.mjs --state-matrix`) and the visual critic
(`visual_critic_run.mjs --state-matrix`) already read.

The taste of WHICH states matter is data (declared in the per-game art contract);
HOW to reach each state is code (the per-game capture script). This helper is the
seam between them.
"""
from __future__ import annotations

import json
import os

# Reusable state categories every game adapts to its own loop. See
# gamedesign/knowledge/live_state_acceptance_matrix.md.
STANDARD_STATES = [
    "first_screen",
    "primary_action_ready",
    "primary_action_feedback",
    "reward_active",
    "progression_panel_open",
    "modal_or_choice_open",
    "locked_or_disabled_state",
    "resume_or_reentry_state",
    "transient_stress_state",
    "small_viewport_state",
]


class StateCapture:
    """Capture one screenshot per named state and build the acceptance matrix.

    Usage (in a per-game capture script)::

        with running_game(fresh_state=True) as game:
            sc = StateCapture(game, "my-game").require("first_screen", "reward_active")
            sc.capture("first_screen")
            game.result("game.playtest.do_thing")      # game-specific routing
            sc.capture("reward_active")
            sc.write_matrix("gamedesign/projects/my-game/art/state_matrix.json")
    """

    def __init__(self, client, project: str, out_dir: str = "build/captures") -> None:
        self.client = client
        self.project = project
        self.out_dir = out_dir
        self.required: list[str] = []
        self.covered: list[dict] = []
        self.debt: list[dict] = []

    def require(self, *tags: str) -> "StateCapture":
        for tag in tags:
            if tag not in self.required:
                self.required.append(tag)
        return self

    def capture(self, tag: str, *, audit: bool = True, wait_frames: int = 2) -> str:
        path = f"{self.out_dir}/state_{tag}.png"
        self.client.capture_screenshot(path, wait_frames=wait_frames, audit=audit)
        self.covered = [entry for entry in self.covered if entry["tag"] != tag]
        self.covered.append({"tag": tag, "evidence": path})
        if tag not in self.required:
            self.required.append(tag)
        return path

    def mark_debt(self, tag: str, reason: str) -> "StateCapture":
        self.debt = [entry for entry in self.debt if entry["tag"] != tag]
        self.debt.append({"tag": tag, "reason": reason})
        if tag not in self.required:
            self.required.append(tag)
        return self

    def matrix(self) -> dict:
        states: dict[str, dict] = {}
        for entry in self.covered:
            states[entry["tag"]] = {"status": "covered", "evidence": entry["evidence"]}
        for entry in self.debt:
            states.setdefault(entry["tag"], {"status": "not_covered", "reason": entry["reason"]})
        return {
            "schema": "game.live_state_acceptance_matrix",
            "project": self.project,
            "required_states": list(self.required),
            "states": states,
        }

    def write_matrix(self, path: str) -> str:
        directory = os.path.dirname(os.path.abspath(path))
        os.makedirs(directory, exist_ok=True)
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(self.matrix(), handle, indent=2)
            handle.write("\n")
        return path

    def shots_args(self) -> list[str]:
        """Return ready `--shot tag:path` args for the visual critic runner."""
        return [f"--shot {entry['tag']}:{entry['evidence']}" for entry in self.covered]
