#!/usr/bin/env python3
"""Capture QCLR_002 responsive viewport evidence for a copied game.

This script is intentionally game-local. It launches the game once per viewport,
captures engine-native screenshots, records ui.tree bounds metadata, and writes
review-ready evidence for ai_studio/quality/rules/player_clarity/QCLR_002.
"""

from __future__ import annotations

import argparse
import importlib
import importlib.util
import json
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable

import smoke_bot
from smoke_bot import DEFAULT_DEVAPI_PORT, DevApiError, REPO_ROOT, default_executable, running_game  # noqa: E402

from png_io import COLOR_TYPE_RGB, bytes_per_pixel, read_png, write_png_rgb  # noqa: E402


RULE_ID = "QCLR_002"
RULE_PATH = "ai_studio/quality/rules/player_clarity/checks/QCLR_002_responsive_viewports.md"


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


def ui_nodes(tree: Any) -> list[dict[str, Any]]:
    return smoke_bot.ui_nodes(tree)


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


PrepareFn = Callable[[Any, ViewportCase], Any]


def load_prepare(spec: str | None) -> PrepareFn | None:
    if not spec:
        return None
    if ":" not in spec:
        raise argparse.ArgumentTypeError("scenario must be MODULE:FUNCTION or PATH.py:FUNCTION")
    module_name, function_name = spec.rsplit(":", 1)
    if not function_name:
        raise argparse.ArgumentTypeError("scenario function name is empty")

    module_path = Path(module_name)
    if module_path.suffix == ".py" or module_path.exists():
        if not module_path.exists():
            raise argparse.ArgumentTypeError(f"scenario file does not exist: {module_path}")
        module_dir = str(module_path.resolve().parent)
        if module_dir not in sys.path:
            sys.path.insert(0, module_dir)
        loaded_name = f"_template_responsive_scenario_{module_path.stem}"
        module_spec = importlib.util.spec_from_file_location(loaded_name, module_path)
        if module_spec is None or module_spec.loader is None:
            raise argparse.ArgumentTypeError(f"could not load scenario file: {module_path}")
        module = importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(module)
    else:
        module = importlib.import_module(module_name)

    fn = getattr(module, function_name, None)
    if not callable(fn):
        raise argparse.ArgumentTypeError(f"scenario target is not callable: {spec}")
    return fn


def capture_viewport(
    game: Any,
    viewport: ViewportCase,
    out_dir: Path,
    *,
    audit: bool,
    warmup_frames: int,
    prepare: PrepareFn | None = None,
) -> dict[str, Any]:
    if warmup_frames > 0:
        game.wait_frames(warmup_frames)
    scenario_result = prepare(game, viewport) if prepare else None
    tree = wait_for_ui_tree(game)
    view = safe_result(game, "view")
    screenshot = game.capture_screenshot(str(out_dir / f"{viewport.name}.png"), wait_frames=2, audit=audit)
    nodes = ui_nodes(tree)
    return {
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
) -> dict[str, Any]:
    out_dir.mkdir(parents=True, exist_ok=True)
    entries = []
    for viewport in viewports:
        with game_factory(viewport) as game:
            entries.append(capture_viewport(game, viewport, out_dir, audit=audit, warmup_frames=warmup_frames, prepare=prepare))

    contact_sheet = make_contact_sheet(entries, out_dir / "contact_sheet.png")
    summary_path = out_dir / "summary.json"
    summary = {
        "schema": "template.qclr_002_responsive.v1",
        "quality_rule": RULE_ID,
        "rule_path": RULE_PATH,
        "evidence": "screenshots + runtime ui.tree bounds",
        "viewports": entries,
        "contact_sheet": contact_sheet,
        "summary": str(summary_path),
    }
    with summary_path.open("w", encoding="utf-8") as handle:
        json.dump(summary, handle, indent=2)
        handle.write("\n")
    return summary


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Capture QCLR_002 responsive viewport screenshots.")
    parser.add_argument("--exe", type=Path, default=default_executable(), help="Native debug executable to launch.")
    parser.add_argument("--port", type=int, default=DEFAULT_DEVAPI_PORT, help="DevAPI TCP port.")
    parser.add_argument("--out", type=Path, default=REPO_ROOT / "tmp" / "quality" / "qclr_002_responsive", help="Evidence output directory.")
    parser.add_argument("--viewport", type=parse_viewport, action="append", default=[], help="Viewport as NAME=WIDTHxHEIGHT; may be repeated.")
    parser.add_argument("--landscape-only", action="store_true", help="Use only landscape entries from the selected matrix.")
    parser.add_argument("--warmup-frames", type=int, default=5, help="Frames to wait before reading ui.tree in each viewport.")
    parser.add_argument(
        "--scenario",
        help="Optional MODULE:FUNCTION or PATH.py:FUNCTION hook called as prepare(game, viewport) before each screenshot.",
    )
    parser.add_argument("--audit", action="store_true", help="Run pixel-health audit for each captured screenshot.")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    exe = args.exe.resolve()
    if not exe.exists():
        print(f"build native Debug first or pass --exe: {exe}", file=sys.stderr)
        return 2

    try:
        viewports = selected_viewports(args.viewport, landscape_only=args.landscape_only)

        def game_factory(viewport: ViewportCase):
            return running_game(
                port=args.port,
                exe=str(exe),
                cwd=str(REPO_ROOT),
                reuse_existing=False,
                fresh_state=True,
                autosave_enabled=False,
                window_size=viewport.window_size,
            )

        prepare = load_prepare(args.scenario)
        summary = run_matrix(game_factory, viewports, args.out, audit=args.audit, warmup_frames=max(0, args.warmup_frames), prepare=prepare)
    except (DevApiError, ValueError, argparse.ArgumentTypeError, ImportError) as exc:
        print(f"responsive viewport capture failed: {exc}", file=sys.stderr)
        return 1

    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
