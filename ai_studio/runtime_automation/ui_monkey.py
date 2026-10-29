#!/usr/bin/env python3
"""Seeded UI monkey for any Studio game with a native DevAPI build.

Static screenshots miss UI faults that live in transitions: a tab clicked
twice, a window opened over a half-built HUD, the first finger after keyboard
play. The monkey launches the game once per viewport x scenario and, for a
fixed number of steps, presses visible enabled buttons (by mouse or by touch),
opens and closes modal scenes, taps empty screen, taps keys and idles. A run
fails when the process exits, the DevAPI link drops, or the launch log gains a
line matching --fail-pattern (NT_ASSERT and ui-domain errors by default).

Every step is recorded with concrete coordinates; --replay re-runs a failed
trace from report.json without the random source.

    python ui_monkey.py --game <id> --steps 300 --seed 7 --in-hand-env UI_KIT_IN_HAND
"""

from __future__ import annotations

import argparse
import json
import random
import re
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable

sys.path.insert(0, str(Path(__file__).resolve().parent))
from devapi_client import DevApiError, running_game  # noqa: E402
from responsive_viewports import load_prepare, resolve_launch  # noqa: E402

OUT_SUBDIR = Path("build") / "quality" / "ui_monkey"
DEFAULT_FAIL_PATTERN = r"NT_ASSERT|ERROR \[ui\]"
DEFAULT_KEYS = ["ESCAPE", "ENTER", "SPACE", "E", "TAB"]
# Weights of the steps inside an episode; a button press dominates because buttons own most state changes.
WEIGHTS = {"button": 60, "touch_empty": 12, "mouse_empty": 5, "key": 10, "idle": 13}
# Steps per episode: enough presses in one window to come back to a tab or row it left.
EPISODE = (8, 16)
# How long the game runs between steps, seconds: long enough for a click's frame, short enough to stack transitions.
STEP_GAP = (0.05, 0.35)


@dataclass(frozen=True)
class Viewport:
    name: str
    width: int
    height: int
    in_hand: bool

    @property
    def window_size(self) -> str:
        return f"{self.width}x{self.height}"


DEFAULT_VIEWPORTS = (
    Viewport("phone-portrait", 390, 844, True),
    Viewport("phone-landscape", 844, 390, True),
    Viewport("desktop", 1280, 720, False),
)


def parse_viewport(value: str) -> Viewport:
    """NAME=WxH, with a trailing :hand for a screen held in a hand."""
    match = re.fullmatch(r"([A-Za-z0-9_-]+)=(\d+)x(\d+)(:hand)?", value)
    if not match:
        raise argparse.ArgumentTypeError("viewport must be NAME=WIDTHxHEIGHT[:hand]")
    return Viewport(match[1], int(match[2]), int(match[3]), bool(match[4]))


# --- choosing a step ------------------------------------------------------------

def buttons(tree: dict[str, Any]) -> list[dict[str, Any]]:
    """Visible enabled buttons with a non-empty box, in ui.tree space."""
    out = []
    for node in tree.get("nodes", []):
        box = node.get("bounds") or {}
        if node.get("role") == "nt_button" and node.get("visible") and node.get("enabled") and box.get("w", 0) > 0 and box.get("h", 0) > 0:
            out.append(node)
    return out


def ui_to_framebuffer(tree: dict[str, Any], x: float, y: float) -> tuple[float, float]:
    """ui.tree space -> framebuffer px; both are Y-up from the bottom-left."""
    view = tree.get("viewport") or {}
    scale_x = view.get("w", tree["width"]) / tree["width"]
    scale_y = view.get("h", tree["height"]) / tree["height"]
    return view.get("x", 0) + x * scale_x, view.get("y", 0) + y * scale_y


class Chooser:
    """Episodes over the game's windows: each opens the next modal scene of a seeded
    round (or returns to the bare screen) and then presses inside it, so every
    window is visited and its buttons are revisited in one stay."""

    def __init__(self, rng: random.Random, scenes: list[str], keys: list[str]):
        self.rng = rng
        self.scenes = scenes
        self.keys = keys
        self.round: list[str] = []
        self.left = 0

    def next(self, tree: dict[str, Any]) -> dict[str, Any]:
        rng = self.rng
        gap = round(rng.uniform(*STEP_GAP), 3)
        if self.left <= 0:
            self.left = rng.randint(*EPISODE)
            if not self.round:
                self.round = [*self.scenes, ""]
                rng.shuffle(self.round)
            scene = self.round.pop()
            return {"kind": "scene_show", "scene": scene, "gap": max(gap, 0.3)} if scene else {"kind": "close_modals", "gap": max(gap, 0.3)}
        self.left -= 1
        kinds = list(WEIGHTS)
        kind = rng.choices(kinds, weights=[WEIGHTS[k] for k in kinds])[0]
        pressable = buttons(tree)
        if kind == "button" and not pressable:
            kind = "touch_empty"
        if kind == "button":
            node = rng.choice(pressable)
            box = node["bounds"]
            x, y = box["x"] + box["w"] * rng.uniform(0.3, 0.7), box["y"] + box["h"] * rng.uniform(0.3, 0.7)
            via = rng.choice(["mouse", "touch"])
            step: dict[str, Any] = {"kind": "button", "via": via, "id": node.get("id_string") or node.get("id")}
            if via == "touch":
                step["fb"] = [round(v, 1) for v in ui_to_framebuffer(tree, x, y)]
            else:
                step["ui"] = [round(x, 1), round(y, 1)]
        elif kind in ("touch_empty", "mouse_empty"):
            x, y = rng.uniform(0.05, 0.95) * tree["width"], rng.uniform(0.05, 0.95) * tree["height"]
            step = {"kind": kind, "fb": [round(v, 1) for v in ui_to_framebuffer(tree, x, y)]}
        elif kind == "key":
            step = {"kind": kind, "key": rng.choice(self.keys), "hold": rng.choice([1, 2, 20])}
        else:
            step = {"kind": kind}
        step["gap"] = gap
        return step


# --- performing a step ----------------------------------------------------------

def finger(game: Any, fb: list[float], hold: float = 0.08) -> None:
    game.result("input.pointer", {"action": "down", "id": 7, "x": fb[0], "y": fb[1], "type": "touch"})
    time.sleep(hold)
    game.result("input.pointer", {"action": "up", "id": 7, "x": fb[0], "y": fb[1], "type": "touch"})


def perform(game: Any, step: dict[str, Any]) -> None:
    kind = step["kind"]
    if kind == "button" and step["via"] == "mouse":
        game.result("ui.click", {"id": {"x": step["ui"][0], "y": step["ui"][1]}})
    elif kind in ("button", "touch_empty"):
        finger(game, step["fb"])
    elif kind == "mouse_empty":
        game.result("input.click", {"x": step["fb"][0], "y": step["fb"][1]})
    elif kind == "scene_show":
        game.result("game.scene.show", {"scene": step["scene"]})
    elif kind == "close_modals":
        game.result("game.scene.close_modals")
    elif kind == "key":
        game.result("input.key", {"key": step["key"], "hold": step["hold"]})
    time.sleep(step["gap"])


class LogWatch:
    """New lines of the launch log since the last look, checked against the failure pattern."""

    def __init__(self, path: str | None, pattern: re.Pattern[str]):
        self.path = Path(path) if path else None
        self.pattern = pattern
        self.offset = 0

    def new_failures(self) -> list[str]:
        if self.path is None or not self.path.exists():
            return []
        with self.path.open("r", encoding="utf-8", errors="replace") as log:
            log.seek(self.offset)
            text = log.read()
            self.offset = log.tell()
        return [line for line in text.splitlines() if self.pattern.search(line)]


def modal_scenes(game: Any) -> list[str]:
    try:
        listing = game.result("game.scene.list")
    except DevApiError:
        return []
    return [s["id"] for s in listing.get("scenes", []) if s.get("kind") == "modal" and not s.get("debugOnly")]


def run_one(
    launch: Callable[[Viewport], Any],
    viewport: Viewport,
    *,
    steps: int,
    seed: int,
    keys: list[str],
    fail_pattern: re.Pattern[str],
    prepare: Callable[[Any, Viewport], Any] | None,
    replay: list[dict[str, Any]] | None = None,
    settle: float = 6.0,
    shot: Path | None = None,
) -> dict[str, Any]:
    rng = random.Random(seed)
    trace: list[dict[str, Any]] = []
    result: dict[str, Any] = {"viewport": viewport.name, "window_size": viewport.window_size, "in_hand": viewport.in_hand,
                              "seed": seed, "trace": trace, "failure": None}
    with launch(viewport) as game:
        watch = LogWatch(getattr(game, "launch_log_path", None), fail_pattern)

        def fail(index: int, reason: str, lines: list[str]) -> None:
            result["failure"] = {"step": index, "reason": reason, "lines": lines[-20:], "log_tail": game.launch_log_tail(60)}

        time.sleep(settle)
        try:
            if prepare is not None:
                result["scenario_result"] = prepare(game, viewport)
            chooser = Chooser(rng, modal_scenes(game), keys)
            plan = replay if replay is not None else [None] * steps
            for index, planned in enumerate(plan):
                step = planned if planned is not None else chooser.next(game.result("ui.tree"))
                trace.append(step)
                perform(game, step)
                exited = game.process.poll() if getattr(game, "process", None) else None
                lines = watch.new_failures()
                if exited is not None:
                    fail(index, f"process exited with {exited}", lines + watch.new_failures())
                    break
                if lines:
                    fail(index, "log matched the failure pattern", lines)
                    break
            if result["failure"] is None and shot is not None:
                game.capture_screenshot(str(shot), wait_frames=0)
        except (DevApiError, OSError) as error:
            time.sleep(0.5)
            exited = game.process.poll() if getattr(game, "process", None) else None
            fail(len(trace) - 1, f"DevAPI link lost ({error}); process exit {exited}", watch.new_failures())
    return result


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Seeded UI monkey: random button, scene, touch and key steps until a crash or a ui error.",
                                     epilog="Pass launch flags that start with a dash as --launch-arg=--no-vsync.")
    target = parser.add_argument_group("game")
    target.add_argument("--game", help="Game id from the workspace catalog; reads executable and launch from its capture/catalog.json.")
    target.add_argument("--exe", type=Path, help="Native DevAPI executable with asserts on; overrides the catalog executable.")
    target.add_argument("--cwd", type=Path, help="Launch directory (default: the game root, else the current directory).")
    target.add_argument("--launch-arg", action="append", default=[], help="Extra game argument; may be repeated.")
    target.add_argument("--fresh-state", action=argparse.BooleanOptionalAction, default=None)
    target.add_argument("--autosave", action=argparse.BooleanOptionalAction, default=None)
    target.add_argument("--startup-timeout", type=float, default=30.0)
    target.add_argument("--in-hand-env", help="Environment variable set to 1 for :hand viewports (the game's phone switch).")
    parser.add_argument("--out", type=Path, help=f"Report directory (default: <game root>/{OUT_SUBDIR.as_posix()}).")
    parser.add_argument("--viewport", type=parse_viewport, action="append", default=[], help="NAME=WxH[:hand]; may be repeated (default: phone portrait and landscape in hand, desktop).")
    parser.add_argument("--scenario", action="append", default=[], help="MODULE:FUNCTION or PATH.py:FUNCTION called as prepare(game, viewport) before the steps; each scenario is its own run. Repeatable.")
    parser.add_argument("--steps", type=int, default=200, help="Steps per run (default: 200).")
    parser.add_argument("--seed", type=int, default=1, help="First seed; --runs N uses seed..seed+N-1 (default: 1).")
    parser.add_argument("--runs", type=int, default=1, help="Seeds per viewport x scenario (default: 1).")
    parser.add_argument("--key", action="append", default=[], help=f"Key the monkey may tap; repeatable (default: {' '.join(DEFAULT_KEYS)}).")
    parser.add_argument("--fail-pattern", default=DEFAULT_FAIL_PATTERN, help=f"Regex over launch-log lines that fails a run (default: {DEFAULT_FAIL_PATTERN}).")
    parser.add_argument("--settle", type=float, default=6.0, help="Seconds after launch before the first step (default: 6).")
    parser.add_argument("--replay", type=Path, help="report.json of an earlier run; re-runs its first failed trace step for step.")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    plan = resolve_launch(args)
    out = args.out.resolve() if args.out else plan.out.parent / OUT_SUBDIR.name
    out.mkdir(parents=True, exist_ok=True)
    roots = [plan.game_root] if plan.game_root else []
    scenarios: list[tuple[str, Any]] = [(spec, load_prepare(spec, search_roots=roots)) for spec in args.scenario] or [("none", None)]
    viewports = args.viewport or list(DEFAULT_VIEWPORTS)
    keys = args.key or DEFAULT_KEYS
    pattern = re.compile(args.fail_pattern)

    def launch(viewport: Viewport) -> Any:
        env = {args.in_hand_env: "1"} if args.in_hand_env and viewport.in_hand else None
        return running_game(exe=str(plan.exe), cwd=str(plan.cwd), fresh_state=plan.fresh_state, autosave_enabled=plan.autosave_enabled,
                            window_size=viewport.window_size, extra_args=list(plan.extra_args), env_overrides=env,
                            startup_timeout=args.startup_timeout)

    jobs: list[tuple[Viewport, str, Any, int, list[dict[str, Any]] | None]] = []
    if args.replay:
        earlier = json.loads(args.replay.read_text(encoding="utf-8"))
        failed = next((run for run in earlier["runs"] if run["failure"]), None)
        if failed is None:
            print("replay: the report has no failed run")
            return 2
        view = next(v for v in [*viewports, *DEFAULT_VIEWPORTS] if v.name == failed["viewport"])
        spec = failed.get("scenario", "none")
        prepare = dict(scenarios).get(spec) or (load_prepare(spec, search_roots=roots) if spec != "none" else None)
        jobs.append((view, spec, prepare, failed["seed"], failed["trace"]))
    else:
        for view in viewports:
            for spec, prepare in scenarios:
                for seed in range(args.seed, args.seed + args.runs):
                    jobs.append((view, spec, prepare, seed, None))

    runs = []
    for view, spec, prepare, seed, replay in jobs:
        name = f"{view.name}-{Path(spec.split(':')[-1]).name if spec != 'none' else 'none'}-{seed}"
        print(f"run {name}: {'replay of ' + str(len(replay)) + ' steps' if replay else str(args.steps) + ' steps'}", flush=True)
        run = run_one(launch, view, steps=args.steps, seed=seed, keys=keys, fail_pattern=pattern, prepare=prepare, replay=replay,
                      settle=args.settle, shot=out / f"{name}.png")
        run["scenario"] = spec
        runs.append(run)
        failure = run["failure"]
        print(f"  {'FAIL at step ' + str(failure['step']) + ': ' + failure['reason'] if failure else 'ok'}", flush=True)
        for line in (failure or {}).get("lines", [])[:5]:
            print(f"    {line}", flush=True)
    report = out / "report.json"
    report.write_text(json.dumps({"runs": runs}, indent=1), encoding="utf-8")
    failures = sum(1 for run in runs if run["failure"])
    print(f"{len(runs) - failures}/{len(runs)} runs clean; report: {report}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
