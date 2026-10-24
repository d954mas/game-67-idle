"""Check that one game window keeps its composition across screen shapes."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any


SCROLL_CALL = re.compile(
    r"\b(?:nt_ui_scroll_begin|nt_ui_vlist_begin|hud_content_begin|CLAY_SCROLL_CONFIG)\s*\(|"
    r"\bscrollContainer\b|\.scroll(?:_id)?\s*=|\bCLAY_SCROLL_\w+\b"
)


def _clean_c(source: str) -> str:
    """Blank comments and literals while preserving braces and source offsets."""
    pattern = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
    return pattern.sub(lambda match: re.sub(r"[^\n]", " ", match.group()), source)


def function_body(source: str, name: str) -> str:
    clean = _clean_c(source)
    match = re.search(r"\b" + re.escape(name) + r"\s*\([^;{}]*\)\s*\{", clean)
    if match is None:
        raise ValueError(f"function {name} not found")
    start = match.end()
    depth = 1
    for index in range(start, len(clean)):
        if clean[index] == "{":
            depth += 1
        elif clean[index] == "}":
            depth -= 1
            if depth == 0:
                return clean[start:index]
    raise ValueError(f"function {name} has no closing brace")


def _rect(node: dict[str, Any]) -> tuple[float, float, float, float]:
    bounds = node.get("bounds")
    if not isinstance(bounds, dict):
        raise ValueError(f"node {node.get('id_string')!r} has no bounds")
    return tuple(float(bounds[key]) for key in ("x", "y", "w", "h"))


def window_snapshot(
    tree: dict[str, Any], root_id: str, scroll_ids: set[str] | None = None
) -> dict[str, tuple[float, float, float, float]]:
    nodes = tree.get("nodes")
    if not isinstance(nodes, list) or tree.get("truncated"):
        raise ValueError("ui.tree must contain a complete nodes array")
    roots = [node for node in nodes if node.get("id_string") == root_id and node.get("visible", True)]
    if len(roots) != 1:
        raise ValueError(f"expected one visible {root_id}, found {len(roots)}")
    root = roots[0]
    rx, ry, rw, rh = _rect(root)
    if rw <= 0 or rh <= 0:
        raise ValueError(f"{root_id} has zero size")
    width, height = float(tree["width"]), float(tree["height"])
    margin = min(rw, rh) * 0.01
    if rx < -margin or ry < -margin or rx + rw > width + margin or ry + rh > height + margin:
        raise ValueError(f"{root_id} extends outside the UI viewport")
    by_id = {node.get("id"): node for node in nodes}
    result: dict[str, tuple[float, float, float, float]] = {}
    scroll_ids = scroll_ids or set()
    found_scroll: set[str] = set()
    for node in nodes:
        name = node.get("id_string")
        if not name or name == root_id or not node.get("visible", True):
            continue
        parent = node.get("parent")
        seen: set[Any] = set()
        under_scroll = False
        while parent in by_id and parent not in seen:
            if parent == root.get("id"):
                break
            seen.add(parent)
            if by_id[parent].get("id_string") in scroll_ids:
                under_scroll = True
            parent = by_id[parent].get("parent")
        else:
            continue
        x, y, w, h = _rect(node)
        if w <= 0 or h <= 0:
            raise ValueError(f"{name} has zero size")
        if re.search(r"(?:^|/)scroll(?:/|$)", name) and name not in scroll_ids:
            raise ValueError(f"undeclared scroll region {name} in {root_id}")
        if name in scroll_ids:
            found_scroll.add(name)
        if not under_scroll and (x < rx - margin or y < ry - margin or x + w > rx + rw + margin or y + h > ry + rh + margin):
            raise ValueError(f"{name} is clipped by {root_id}")
        if name in result:
            raise ValueError(f"duplicate semantic id {name}")
        result[name] = ((x - rx) / rw, (y - ry) / rh, w / rw, h / rh)
    if not result:
        raise ValueError(f"{root_id} has no named descendants")
    if scroll_ids - found_scroll:
        raise ValueError(f"missing declared scroll regions: {sorted(scroll_ids - found_scroll)}")
    return result


def compare_windows(
    portrait: dict[str, Any], landscape: dict[str, Any], root_id: str, tolerance: float = 0.06,
    scroll_ids: set[str] | None = None, layout: str = "single",
) -> list[str]:
    errors: list[str] = []
    try:
        first = window_snapshot(portrait, root_id, scroll_ids)
        second = window_snapshot(landscape, root_id, scroll_ids)
    except (KeyError, TypeError, ValueError) as exc:
        return [str(exc)]
    missing = sorted(first.keys() - second.keys())
    added = sorted(second.keys() - first.keys())
    if missing or added:
        errors.append(f"semantic IDs differ: portrait-only={missing}, landscape-only={added}")
    for name in sorted(first.keys() & second.keys()):
        if layout == "adaptive":
            continue
        delta = max(abs(a - b) for a, b in zip(first[name], second[name]))
        if delta > tolerance:
            errors.append(f"{name} changes normalized layout by {delta:.3f} (> {tolerance:.3f})")
    return errors


def _descendant_names(tree: dict[str, Any], ancestor_id: str) -> set[str]:
    nodes = tree["nodes"]
    by_id = {node.get("id"): node for node in nodes}
    ancestor = next((node for node in nodes if node.get("id_string") == ancestor_id), None)
    if ancestor is None:
        raise ValueError(f"scroll target {ancestor_id} is absent")
    names: set[str] = set()
    for node in nodes:
        parent = node.get("parent")
        seen: set[Any] = set()
        while parent in by_id and parent not in seen:
            if parent == ancestor.get("id"):
                if node.get("id_string"):
                    names.add(node["id_string"])
                break
            seen.add(parent)
            parent = by_id[parent].get("parent")
    return names


def check_scroll_motion(
    before: dict[str, Any], after: dict[str, Any], root_id: str, allowed_target: str | None,
    scroll_ids: set[str], tolerance: float = 0.01,
) -> list[str]:
    try:
        first = window_snapshot(before, root_id, scroll_ids)
        second = window_snapshot(after, root_id, scroll_ids)
        allowed = _descendant_names(before, allowed_target) if allowed_target else set()
    except (KeyError, TypeError, ValueError) as exc:
        return [str(exc)]
    if first.keys() != second.keys():
        return [f"{root_id}: semantic IDs changed after wheel input"]
    moved = {name for name in first if max(abs(a - b) for a, b in zip(first[name], second[name])) > tolerance}
    unexpected = moved - allowed
    if unexpected:
        return [f"{root_id}: wheel moved controls outside {allowed_target or 'no-scroll window'}: {sorted(unexpected)}"]
    return []


WINDOW_LITERAL = re.compile(r'"([A-Za-z0-9_./-]+/window)"')


def discovered_windows(source_root: Path) -> set[str]:
    found: set[str] = set()
    for path in source_root.rglob("*"):
        if path.suffix not in (".c", ".h") or not path.is_file():
            continue
        found.update(WINDOW_LITERAL.findall(path.read_text(encoding="utf-8", errors="replace")))
    return found


def check_contract(
    contract: dict[str, Any], game_root: Path, proof: dict[str, Any] | None = None,
    scene_list: dict[str, Any] | None = None,
) -> list[str]:
    errors: list[str] = []
    if contract.get("schema") != "ai_studio.ui_window_contracts.v1":
        return ["unsupported window contract schema"]
    entries = contract.get("windows")
    if not isinstance(entries, list):
        return ["contract must declare a windows array"]
    if not entries and not contract.get("no_windows_reason"):
        errors.append("empty windows array needs no_windows_reason")
    ids: set[str] = set()
    scenes: set[str] = set()
    for entry in entries:
        if not isinstance(entry, dict) or not isinstance(entry.get("id"), str) or not entry["id"]:
            errors.append("window entry needs a semantic id")
            continue
        root_id = entry["id"]
        if root_id in ids:
            errors.append(f"duplicate window {root_id}")
        ids.add(root_id)
        layout = entry.get("layout", "single")
        scroll = entry.get("scroll", [])
        if layout not in ("single", "adaptive"):
            errors.append(f"{root_id}: layout must be single or adaptive")
            continue
        if not isinstance(scroll, list) or any(not isinstance(item, str) or not item for item in scroll):
            errors.append(f"{root_id}: scroll must be a list of semantic IDs")
            continue
        if (layout == "adaptive" or scroll) and not entry.get("reason"):
            errors.append(f"{root_id}: layout/scroll exception needs a reason")
        scene = entry.get("scene")
        settings_window = root_id == "settings/window" or entry.get("purpose") == "settings"
        if settings_window and (layout != "single" or scroll):
            errors.append(f"{root_id}: settings must use one composition without scroll")
        if scene is not None:
            if not isinstance(scene, str) or not scene:
                errors.append(f"{root_id}: scene must be a nonempty string")
            else:
                scenes.add(scene)
        opener = entry.get("open")
        if scene is None and not isinstance(opener, list):
            errors.append(f"{root_id}: non-scene window needs open commands")
        if opener is not None and (not isinstance(opener, list) or not opener or any(
            not isinstance(command, dict) or not isinstance(command.get("method"), str)
            or not isinstance(command.get("params", {}), dict)
            or ("waitFor" in command and (not isinstance(command["waitFor"], str) or not command["waitFor"]))
            for command in opener
        )):
            errors.append(f"{root_id}: open must contain DevAPI method/params commands")
        source = entry.get("source")
        if not isinstance(source, dict) or not isinstance(source.get("file"), str) or not isinstance(source.get("function"), str):
            errors.append(f"{root_id}: source file and draw function are required")
        else:
            source_path = (game_root / source["file"]).resolve()
            if not source_path.is_relative_to(game_root.resolve()):
                errors.append(f"{root_id}: source file must stay inside the game")
            else:
                try:
                    source_text = source_path.read_text(encoding="utf-8")
                    function_body(source_text, source["function"])
                except (OSError, ValueError) as exc:
                    errors.append(f"{root_id}: {exc}")
                else:
                    if f'"{root_id}"' not in source_text:
                        errors.append(f"{root_id}: root ID literal is absent from {source_path}")
                    if not scroll:
                        errors.extend(check_source_no_scroll(source_path, source["function"]))
    source_root = game_root / "src"
    if not source_root.is_dir():
        errors.append(f"missing game source directory: {source_root}")
    else:
        ignored_roots = contract.get("non_window_roots", [])
        if not isinstance(ignored_roots, list) or any(
            not isinstance(item, dict) or not item.get("id") or not item.get("reason") for item in ignored_roots
        ):
            errors.append("non_window_roots entries need id and reason")
            ignored_roots = []
        excluded_roots = {item["id"] for item in ignored_roots}
        if ids & excluded_roots:
            errors.append(f"window roots cannot also be excluded: {sorted(ids & excluded_roots)}")
        undeclared = discovered_windows(source_root) - ids - excluded_roots
        if undeclared:
            errors.append(f"window roots without contracts: {sorted(undeclared)}")
    if scene_list is not None:
        raw_scenes = scene_list.get("scenes")
        if not isinstance(raw_scenes, list):
            errors.append("scene list has no scenes array")
        else:
            modal = {item.get("id") for item in raw_scenes if isinstance(item, dict) and item.get("kind") == "modal"}
            ignored = contract.get("non_window_scenes", [])
            if not isinstance(ignored, list) or any(not isinstance(item, dict) or not item.get("id") or not item.get("reason") for item in ignored):
                errors.append("non_window_scenes entries need id and reason")
                ignored = []
            excluded = {item["id"] for item in ignored}
            if modal - scenes - excluded:
                errors.append(f"modal scenes without window contracts: {sorted(modal - scenes - excluded)}")
            if scenes - modal:
                errors.append(f"contract scenes absent from scene catalog: {sorted(scenes - modal)}")
            for entry in entries:
                scene = entry.get("scene")
                descriptor = next((item for item in raw_scenes if isinstance(item, dict) and item.get("id") == scene), None)
                if descriptor and descriptor.get("argsSize", 0) and not entry.get("open"):
                    errors.append(f"{entry['id']}: scene {scene} needs explicit open commands because it takes arguments")
    if errors:
        return errors
    if proof is not None:
        if proof.get("schema") != "ai_studio.ui_window_proof.v1" or not isinstance(proof.get("windows"), list):
            errors.append("unsupported window proof schema")
        else:
            captures = {item.get("id"): item for item in proof["windows"] if isinstance(item, dict)}
            if set(captures) != ids:
                errors.append(f"proof coverage differs: missing={sorted(ids - set(captures))}, extra={sorted(set(captures) - ids)}")
            for entry in entries:
                root_id = entry.get("id")
                capture = captures.get(root_id)
                if capture is None:
                    continue
                if not isinstance(capture.get("portrait"), dict) or not isinstance(capture.get("landscape"), dict):
                    errors.append(f"{root_id}: proof needs portrait and landscape ui.tree")
                    continue
                errors.extend(compare_windows(capture["portrait"], capture["landscape"], root_id,
                                              scroll_ids=set(entry.get("scroll", [])), layout=entry.get("layout", "single")))
                checks = capture.get("after_scroll")
                targets = entry.get("scroll", []) or [root_id]
                if not isinstance(checks, dict):
                    errors.append(f"{root_id}: proof needs after_scroll for both orientations")
                    continue
                for orientation in ("portrait", "landscape"):
                    after = checks.get(orientation)
                    if not isinstance(after, dict):
                        errors.append(f"{root_id}: missing {orientation} after_scroll")
                        continue
                    for target in targets:
                        pair = after.get(target)
                        if not isinstance(pair, dict) or not isinstance(pair.get("before"), dict) or not isinstance(pair.get("after"), dict):
                            errors.append(f"{root_id}: missing {orientation} wheel proof for {target}")
                            continue
                        errors.extend(check_scroll_motion(pair["before"], pair["after"], root_id,
                                                          target if entry.get("scroll") else None,
                                                          set(entry.get("scroll", []))))
    return errors


def check_studio_games(studio_root: Path) -> list[str]:
    errors: list[str] = []
    games_root = studio_root / "games"
    if not games_root.is_dir():
        return [f"missing games directory: {games_root}"]
    candidates = [*games_root.iterdir()]
    private_root = games_root / "private"
    if private_root.is_dir():
        candidates.extend(private_root.iterdir())
    for game_root in sorted(candidates):
        identity_path = game_root / "game.json"
        if not game_root.is_dir() or not (game_root / "src").is_dir() or not identity_path.is_file():
            continue
        try:
            identity = json.loads(identity_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            errors.append(f"{game_root.name}: {exc}")
            continue
        if identity.get("lifecycle", {}).get("state") == "frozen":
            continue
        contract_path = game_root / "design" / "ui_window_contracts.json"
        if not contract_path.is_file():
            errors.append(f"{game_root.name}: missing {contract_path}")
            continue
        try:
            contract = json.loads(contract_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            errors.append(f"{game_root.name}: {exc}")
            continue
        errors.extend(f"{game_root.name}: {error}" for error in check_contract(contract, game_root))
    return errors


def check_source_no_scroll(path: Path, function: str) -> list[str]:
    try:
        body = function_body(path.read_text(encoding="utf-8"), function)
    except (OSError, ValueError) as exc:
        return [str(exc)]
    if SCROLL_CALL.search(body):
        return [f"{path}:{function} uses a scroll layout primitive"]
    return []


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--portrait", type=Path, help="ui.tree JSON with the window open")
    parser.add_argument("--landscape", type=Path, help="same state and locale in landscape")
    parser.add_argument("--root", help="semantic ID of one window root")
    parser.add_argument("--tolerance", type=float, default=0.06, help="maximum normalized geometry delta")
    parser.add_argument("--no-scroll-source", type=Path, help="source file containing the window draw function")
    parser.add_argument("--no-scroll-function", help="window draw function to inspect")
    parser.add_argument("--contract", type=Path, help="game-owned contract for every window")
    parser.add_argument("--game-root", type=Path, help="game root for contract source paths")
    parser.add_argument("--proof", type=Path, help="optional complete portrait/landscape ui.tree proof")
    parser.add_argument("--scene-list", type=Path, help="optional game.scene.list result for modal coverage")
    parser.add_argument("--studio-root", type=Path, help="check contracts for every active game with src/")
    args = parser.parse_args()
    if args.studio_root:
        if args.contract or args.root or args.portrait or args.landscape:
            parser.error("--studio-root excludes single-game and single-window arguments")
        errors = check_studio_games(args.studio_root)
        if errors:
            for error in errors:
                print(f"FAIL: {error}", file=sys.stderr)
            return 1
        print("PASS: every active game with src/ has a valid window contract")
        return 0
    if args.contract:
        if not args.game_root or args.root or args.portrait or args.landscape:
            parser.error("--contract needs --game-root and excludes single-window arguments")
        try:
            contract = json.loads(args.contract.read_text(encoding="utf-8"))
            proof = json.loads(args.proof.read_text(encoding="utf-8")) if args.proof else None
            scene_list = json.loads(args.scene_list.read_text(encoding="utf-8")) if args.scene_list else None
        except (OSError, json.JSONDecodeError) as exc:
            print(f"FAIL: {exc}", file=sys.stderr)
            return 1
        errors = check_contract(contract, args.game_root, proof, scene_list)
        if errors:
            for error in errors:
                print(f"FAIL: {error}", file=sys.stderr)
            return 1
        scope = "source, scene catalog and runtime proof" if proof and scene_list else "source and exception declarations"
        print(f"PASS: {len(contract['windows'])} declared windows; {scope} agree")
        return 0
    if not args.root or not args.portrait or not args.landscape:
        parser.error("pass --contract with --game-root, or --portrait, --landscape and --root")
    if bool(args.no_scroll_source) != bool(args.no_scroll_function):
        parser.error("--no-scroll-source and --no-scroll-function must be used together")
    try:
        portrait = json.loads(args.portrait.read_text(encoding="utf-8"))
        landscape = json.loads(args.landscape.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1
    errors = compare_windows(portrait, landscape, args.root, args.tolerance)
    if args.no_scroll_source:
        errors.extend(check_source_no_scroll(args.no_scroll_source, args.no_scroll_function))
    if errors:
        for error in errors:
            print(f"FAIL: {error}", file=sys.stderr)
        return 1
    print(f"PASS: {args.root} retains one fitted layout across portrait and landscape")
    if args.no_scroll_source:
        print(f"PASS: {args.no_scroll_function} has no scroll layout primitive")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
