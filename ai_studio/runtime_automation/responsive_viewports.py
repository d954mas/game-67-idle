#!/usr/bin/env python3
"""Capture QCLR_002 responsive viewport evidence for any Studio game.

Launches the game's native DevAPI build once per viewport, captures an
engine-native screenshot, records ui.tree bounds, optionally checks visible ui
leaves against a portal's reserved zones (--portal), and writes review-ready
evidence for ai_studio/quality/rules/player_clarity/checks/QCLR_002.

`--game <id>` resolves the game through the workspace catalog and takes the
executable and launch flags from the game's capture/catalog.json, the same
contract capture_game.py reads; `--exe` (+ `--cwd`) runs any other build.
"""

from __future__ import annotations

import argparse
import importlib
import importlib.util
import json
import math
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable

HERE = Path(__file__).resolve().parent
STUDIO_ROOT = HERE.parents[1]
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from devapi_client import DevApiError, running_game  # noqa: E402
from native_capture_preflight import canonical_native_executable  # noqa: E402
from png_io import COLOR_TYPE_RGB, bytes_per_pixel, read_png, write_png_rgb  # noqa: E402
import portal_zones  # noqa: E402


RULE_ID = "QCLR_002"
RULE_PATH = "ai_studio/quality/rules/player_clarity/checks/QCLR_002_responsive_viewports.md"
OUT_SUBDIR = Path("build") / "quality" / "qclr_002_responsive"


@dataclass(frozen=True)
class ViewportCase:
    name: str
    width: int
    height: int

    @property
    def window_size(self) -> str:
        return f"{self.width}x{self.height}"

    @property
    def orientation(self) -> str:
        return "landscape" if self.width >= self.height else "portrait"


DEFAULT_VIEWPORTS = [
    ViewportCase("4x3_landscape", 640, 480),
    ViewportCase("4x3_portrait", 480, 640),
    ViewportCase("16x9_landscape", 640, 360),
    ViewportCase("16x9_portrait", 360, 640),
    ViewportCase("phone_19_5x9_portrait", 390, 844),
    ViewportCase("phone_19_5x9_landscape", 844, 390),
]


def parse_viewport(value: str) -> ViewportCase:
    raw_name: str | None = None
    raw_size = value
    if "=" in value:
        raw_name, raw_size = value.split("=", 1)
    match = re.fullmatch(r"([1-9][0-9]{1,4})x([1-9][0-9]{1,4})", raw_size.strip(), re.IGNORECASE)
    if not match:
        raise argparse.ArgumentTypeError("viewport must be NAME=WIDTHxHEIGHT or WIDTHxHEIGHT")
    width = int(match.group(1))
    height = int(match.group(2))
    if width > 16384 or height > 16384:
        raise argparse.ArgumentTypeError("viewport dimensions must be <= 16384")
    name = raw_name.strip() if raw_name else f"{width}x{height}"
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", name):
        raise argparse.ArgumentTypeError("viewport name may contain only letters, numbers, dot, underscore, or dash")
    return ViewportCase(name, width, height)


def selected_viewports(custom: list[ViewportCase], *, landscape_only: bool) -> list[ViewportCase]:
    viewports = custom if custom else DEFAULT_VIEWPORTS
    if landscape_only:
        viewports = [case for case in viewports if case.orientation == "landscape"]
    if not viewports:
        raise ValueError("no viewports selected")
    return viewports


# --- launch resolution -------------------------------------------------------

@dataclass(frozen=True)
class LaunchPlan:
    exe: Path
    cwd: Path
    out: Path
    fresh_state: bool = True
    autosave_enabled: bool = False
    extra_args: tuple[str, ...] = field(default_factory=tuple)
    game_root: Path | None = None


def list_game_mounts(studio_root: Path = STUDIO_ROOT) -> list[dict[str, Any]]:
    """Game mounts from the workspace catalog (public and private games)."""
    proc = subprocess.run(
        ["node", str(studio_root / "ai_studio" / "workspace" / "games.mjs"), "list",
         "--root", str(studio_root), "--include-private", "--json"],
        capture_output=True, text=True, encoding="utf-8", errors="replace", check=False,
    )
    if proc.returncode != 0:
        raise ValueError(f"workspace game catalog failed: {proc.stderr.strip() or proc.stdout.strip()}")
    mounts = json.loads(proc.stdout).get("mounts")
    if not isinstance(mounts, list):
        raise ValueError("workspace game catalog returned no mounts list")
    return mounts


def resolve_game_root(
    game_id: str,
    studio_root: Path = STUDIO_ROOT,
    list_mounts: Callable[[Path], list[dict[str, Any]]] = list_game_mounts,
) -> Path:
    matches = [
        mount for mount in list_mounts(studio_root)
        if mount.get("gameId") == game_id or game_id in (mount.get("aliases") or [])
    ]
    if len(matches) != 1:
        raise ValueError(f"game {game_id} has no unique catalog mount")
    return (studio_root / matches[0]["root"]).resolve()


def read_launch_catalog(game_root: Path) -> dict[str, Any]:
    """The executable/launch part of capture/catalog.json; {} when the game has none."""
    path = game_root / "capture" / "catalog.json"
    if not path.is_file():
        return {}
    catalog = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(catalog, dict):
        raise ValueError(f"{path}: catalog must be an object")
    executable = catalog.get("executable")
    if executable is not None and (not isinstance(executable, str) or not executable):
        raise ValueError(f"{path}: executable must be a non-empty string")
    launch = catalog.get("launch", {})
    if not isinstance(launch, dict) or set(launch) - {"fresh_state", "autosave_enabled", "args"}:
        raise ValueError(f"{path}: launch must contain only fresh_state, autosave_enabled, and args")
    for key in ("fresh_state", "autosave_enabled"):
        if key in launch and not isinstance(launch[key], bool):
            raise ValueError(f"{path}: launch {key} must be a boolean")
    if "args" in launch and (not isinstance(launch["args"], list)
                             or any(not isinstance(arg, str) or not arg for arg in launch["args"])):
        raise ValueError(f"{path}: launch args must be a list of non-empty strings")
    return {"executable": executable, "launch": launch}


def resolve_launch(
    args: argparse.Namespace,
    *,
    resolve_game: Callable[[str], Path] = resolve_game_root,
    invocation_dir: Path | None = None,
) -> LaunchPlan:
    here = (invocation_dir or Path.cwd()).resolve()
    game_root = resolve_game(args.game) if args.game else None
    catalog = read_launch_catalog(game_root) if game_root else {}
    launch = catalog.get("launch", {})

    if args.exe is not None:
        exe = args.exe if args.exe.is_absolute() else here / args.exe
    elif game_root is not None:
        relative = catalog.get("executable")
        exe = game_root / relative if relative else canonical_native_executable(game_root)
    else:
        raise ValueError("pass --game <id> or --exe <path>")

    cwd = args.cwd if args.cwd is not None else (game_root or here)
    if not cwd.is_absolute():
        cwd = here / cwd
    out = args.out if args.out is not None else (game_root or cwd) / OUT_SUBDIR
    if not out.is_absolute():
        out = here / out

    return LaunchPlan(
        exe=exe.resolve(),
        cwd=cwd.resolve(),
        out=out.resolve(),
        fresh_state=args.fresh_state if args.fresh_state is not None else launch.get("fresh_state", True),
        autosave_enabled=args.autosave if args.autosave is not None else launch.get("autosave_enabled", False),
        extra_args=(*launch.get("args", []), *args.launch_arg),
        game_root=game_root,
    )


# --- scenario hook -------------------------------------------------------------

PrepareFn = Callable[[Any, ViewportCase], Any]


def load_prepare(spec: str | None, *, search_roots: list[Path] | None = None) -> PrepareFn | None:
    """Load MODULE:FUNCTION or PATH.py:FUNCTION; relative paths and modules also
    resolve against search_roots (the game root), so game-owned hooks stay in the game."""
    if not spec:
        return None
    if ":" not in spec:
        raise argparse.ArgumentTypeError("scenario must be MODULE:FUNCTION or PATH.py:FUNCTION")
    module_name, function_name = spec.rsplit(":", 1)
    if not function_name:
        raise argparse.ArgumentTypeError("scenario function name is empty")
    roots = list(search_roots or [])

    module_path = Path(module_name)
    if module_path.suffix == ".py":
        candidates = [module_path] if module_path.is_absolute() else [Path.cwd() / module_path, *(root / module_path for root in roots)]
        found = next((candidate for candidate in candidates if candidate.is_file()), None)
        if found is None:
            raise argparse.ArgumentTypeError(f"scenario file does not exist: {module_path}")
        module_dir = str(found.resolve().parent)
        if module_dir not in sys.path:
            sys.path.insert(0, module_dir)
        module_spec = importlib.util.spec_from_file_location(f"_responsive_scenario_{found.stem}", found)
        if module_spec is None or module_spec.loader is None:
            raise argparse.ArgumentTypeError(f"could not load scenario file: {found}")
        module = importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(module)
    else:
        for root in reversed(roots):
            if str(root) not in sys.path:
                sys.path.insert(0, str(root))
        module = importlib.import_module(module_name)

    fn = getattr(module, function_name, None)
    if not callable(fn):
        raise argparse.ArgumentTypeError(f"scenario target is not callable: {spec}")
    return fn


# --- capture -------------------------------------------------------------------

def ui_nodes(tree: Any) -> list[dict[str, Any]]:
    if isinstance(tree, dict) and isinstance(tree.get("nodes"), list):
        return [node for node in tree["nodes"] if isinstance(node, dict)]
    return []


def wait_for_ui_tree(game: Any, *, max_frames: int = 90, stride: int = 3) -> dict[str, Any]:
    last_error: Exception | None = None
    for _ in range(max(1, max_frames // max(1, stride))):
        try:
            tree = game.result("ui.tree")
        except DevApiError as exc:
            last_error = exc
        else:
            if isinstance(tree, dict):
                return tree
        game.wait_frames(stride)
    detail = f"; last error: {last_error}" if last_error else ""
    raise DevApiError(f"ui.tree did not become available after {max_frames} frames{detail}")


def safe_result(game: Any, method: str) -> Any:
    try:
        return game.result(method)
    except DevApiError as exc:
        return {"error": str(exc)}


def capture_viewport(
    game: Any,
    viewport: ViewportCase,
    out_dir: Path,
    *,
    audit: bool,
    warmup_frames: int,
    prepare: PrepareFn | None = None,
    portal: str | None = None,
    zones: list[dict[str, Any]] | None = None,
    zone_ignore: list[str] | None = None,
) -> dict[str, Any]:
    if warmup_frames > 0:
        game.wait_frames(warmup_frames)
    scenario_result = prepare(game, viewport) if prepare else None
    tree = wait_for_ui_tree(game)
    view = safe_result(game, "view")
    screenshot = game.capture_screenshot(str(out_dir / f"{viewport.name}.png"), wait_frames=2, audit=audit)
    nodes = ui_nodes(tree)
    entry: dict[str, Any] = {
        "name": viewport.name,
        "window_size": viewport.window_size,
        "orientation": viewport.orientation,
        "view": view,
        "ui": {
            "width": tree.get("width"),
            "height": tree.get("height"),
            "viewport": tree.get("viewport"),
            "node_count": len(nodes),
            "stable_ids": sorted(node.get("id_string") for node in nodes if isinstance(node.get("id_string"), str) and node.get("id_string")),
        },
        "scenario_result": scenario_result,
        "screenshot": screenshot,
    }
    if portal:
        check = portal_zones.check_tree(tree, zones or [], viewport.width, viewport.height, ignore=zone_ignore or [])
        entry["portal_zones"] = {"target": portal, **check}
    return entry


def portal_zone_failures(entries: list[dict[str, Any]]) -> int:
    """Violations plus viewports whose tree could not be mapped onto the window."""
    return sum(len(entry["portal_zones"]["violations"]) + (1 if "error" in entry["portal_zones"] else 0) for entry in entries if "portal_zones" in entry)


def png_to_rgb(path: str) -> tuple[int, int, bytes]:
    width, height, color_type, pixels = read_png(path)
    if color_type == COLOR_TYPE_RGB:
        return width, height, pixels
    bpp = bytes_per_pixel(color_type)
    rgb = bytearray(width * height * 3)
    dst = 0
    for src in range(0, len(pixels), bpp):
        if color_type == 0:
            r = g = b = pixels[src]
        else:
            r, g, b = pixels[src], pixels[src + 1], pixels[src + 2]
        rgb[dst : dst + 3] = bytes((r, g, b))
        dst += 3
    return width, height, bytes(rgb)


def make_contact_sheet(entries: list[dict[str, Any]], output: Path, *, columns: int = 3, padding: int = 8) -> str:
    if not entries:
        raise ValueError("contact sheet requires at least one screenshot")
    images = [(*png_to_rgb(entry["screenshot"]), entry["name"]) for entry in entries]
    cell_w = max(width for width, _height, _rgb, _name in images)
    cell_h = max(height for _width, height, _rgb, _name in images)
    cols = max(1, columns)
    rows = math.ceil(len(images) / cols)
    sheet_w = (cols * cell_w) + ((cols + 1) * padding)
    sheet_h = (rows * cell_h) + ((rows + 1) * padding)
    sheet = bytearray([22, 28, 38] * sheet_w * sheet_h)

    for index, (width, height, rgb, _name) in enumerate(images):
        col = index % cols
        row = index // cols
        dst_x = padding + (col * (cell_w + padding)) + ((cell_w - width) // 2)
        dst_y = padding + (row * (cell_h + padding)) + ((cell_h - height) // 2)
        for y in range(height):
            src_off = y * width * 3
            dst_off = ((dst_y + y) * sheet_w + dst_x) * 3
            sheet[dst_off : dst_off + (width * 3)] = rgb[src_off : src_off + (width * 3)]

    write_png_rgb(str(output), sheet_w, sheet_h, bytes(sheet))
    return str(output)


def run_matrix(
    game_factory: Callable[[ViewportCase], Any],
    viewports: list[ViewportCase],
    out_dir: Path,
    *,
    audit: bool = True,
    warmup_frames: int = 5,
    prepare: PrepareFn | None = None,
    portal: str | None = None,
    zone_ignore: list[str] | None = None,
) -> dict[str, Any]:
    out_dir.mkdir(parents=True, exist_ok=True)
    zones = portal_zones.load_target_zones(portal) if portal else None
    entries = []
    for viewport in viewports:
        with game_factory(viewport) as game:
            entries.append(capture_viewport(
                game, viewport, out_dir, audit=audit, warmup_frames=warmup_frames, prepare=prepare,
                portal=portal, zones=zones, zone_ignore=zone_ignore,
            ))

    contact_sheet = make_contact_sheet(entries, out_dir / "contact_sheet.png")
    summary_path = out_dir / "summary.json"
    summary = {
        "schema": "ai_studio.qclr_002_responsive.v1",
        "quality_rule": RULE_ID,
        "rule_path": RULE_PATH,
        "evidence": "screenshots + runtime ui.tree bounds",
        "viewports": entries,
        "contact_sheet": contact_sheet,
        "summary": str(summary_path),
    }
    if portal:
        summary["portal_zones"] = {
            "target": portal,
            "zone_ids": [zone.get("id") for zone in zones or []],
            "ignore": list(zone_ignore or []),
            "failures": portal_zone_failures(entries),
        }
    with summary_path.open("w", encoding="utf-8") as handle:
        json.dump(summary, handle, indent=2)
        handle.write("\n")
    return summary


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Capture QCLR_002 responsive viewport screenshots for a Studio game.",
        epilog="Pass launch flags that start with a dash as --launch-arg=--no-vsync.",
    )
    target = parser.add_argument_group("game")
    target.add_argument("--game", help="Game id from the workspace catalog; reads executable and launch from its capture/catalog.json.")
    target.add_argument("--exe", type=Path, help="Native DevAPI executable; overrides the catalog executable.")
    target.add_argument("--cwd", type=Path, help="Launch directory (default: the game root, else the current directory).")
    target.add_argument("--launch-arg", action="append", default=[], help="Extra game argument appended after the catalog's launch args; may be repeated.")
    target.add_argument("--fresh-state", action=argparse.BooleanOptionalAction, default=None,
                        help="Append --fresh-state (default: catalog launch.fresh_state, else on). Off for games whose CLI rejects it.")
    target.add_argument("--autosave", action=argparse.BooleanOptionalAction, default=None,
                        help="Leave autosave on; off appends --disable-autosave (default: catalog launch.autosave_enabled, else off).")
    target.add_argument("--port", type=int, help="DevAPI TCP port (default: a free port per launch).")
    target.add_argument("--startup-timeout", type=float, default=8.0, help="Seconds to wait for each native launch (default: 8).")
    parser.add_argument("--out", type=Path, help=f"Evidence directory (default: <game root>/{OUT_SUBDIR.as_posix()}).")
    parser.add_argument("--viewport", type=parse_viewport, action="append", default=[], help="Viewport as NAME=WIDTHxHEIGHT; may be repeated.")
    parser.add_argument("--landscape-only", action="store_true", help="Use only landscape entries from the selected matrix.")
    parser.add_argument("--warmup-frames", type=int, default=5, help="Frames to wait before the scenario hook and ui.tree in each viewport.")
    parser.add_argument(
        "--scenario",
        help="MODULE:FUNCTION or PATH.py:FUNCTION called as prepare(game, viewport) before each screenshot; relative paths also resolve against the game root.",
    )
    parser.add_argument("--audit", action="store_true", help="Run pixel-health audit for each captured screenshot.")
    parser.add_argument("--portal", help="Publish target whose reserved zones (e.g. the Poki Pill) visible ui leaves must avoid.")
    parser.add_argument(
        "--zone-ignore",
        action="append",
        default=[],
        help="Glob over ui id_string exempt from the --portal check (full-screen decorative leaves); may be repeated.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        plan = resolve_launch(args)
    except (ValueError, json.JSONDecodeError) as exc:
        print(f"responsive viewport capture failed: {exc}", file=sys.stderr)
        return 2
    if not plan.exe.exists():
        print(f"build the native DevAPI executable first or pass --exe: {plan.exe}", file=sys.stderr)
        return 2

    try:
        viewports = selected_viewports(args.viewport, landscape_only=args.landscape_only)

        def game_factory(viewport: ViewportCase):
            return running_game(
                port=args.port,
                exe=str(plan.exe),
                cwd=str(plan.cwd),
                reuse_existing=False,
                fresh_state=plan.fresh_state,
                autosave_enabled=plan.autosave_enabled,
                window_size=viewport.window_size,
                extra_args=list(plan.extra_args),
                startup_timeout=args.startup_timeout,
            )

        prepare = load_prepare(args.scenario, search_roots=[plan.game_root] if plan.game_root else [])
        summary = run_matrix(
            game_factory, viewports, plan.out, audit=args.audit, warmup_frames=max(0, args.warmup_frames), prepare=prepare,
            portal=args.portal, zone_ignore=args.zone_ignore,
        )
    except (DevApiError, ValueError, argparse.ArgumentTypeError, ImportError) as exc:
        print(f"responsive viewport capture failed: {exc}", file=sys.stderr)
        return 1

    print(json.dumps(summary, indent=2))
    failures = summary.get("portal_zones", {}).get("failures", 0)
    if failures:
        print(f"portal reserved zones of {args.portal} are covered: {failures} failure(s); see portal_zones in {summary['summary']}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
