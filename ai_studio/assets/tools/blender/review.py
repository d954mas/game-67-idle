"""Look at and check a model in one command: renders, every applicable check, one review card.

    python review.py tmp/crate.glb
    python review.py tmp/hero.glb --game-camera '{"height": 1.6, "behind": 4, "pitch": 12, "fov_y": 40}'
    python review.py tmp/*.glb --out tmp/review --contacts contacts.json --pivot floor

All models render in one Blender start (about 1 s per extra model). Per model it writes
<out>/<stem>/ (default ./tmp/review): PNG views, views.json, one JSON report per check, card.png and
review.json. Checks advise: the command exits 0 unless --strict is given. Skinned models get the
rig checks and models with animations get the clip checks automatically.
"""
import argparse
import json
import sys
import time
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[4]))

from ai_studio.assets.tools.blender import clip_check, contact_check, glb_check, mesh_check  # noqa: E402
from ai_studio.assets.tools.blender import review_card, run_blender  # noqa: E402
from ai_studio.assets.tools.blender.lib import report as R  # noqa: E402
from ai_studio.assets.tools.blender.lib.glb import Glb  # noqa: E402

HERE = Path(__file__).resolve().parent
DEFAULT_VIEWS = ("front", "right", "three_quarter")


def render(models, root, args):
    """Render every model in one Blender start; returns ({model: views}, runner result)."""
    out = root if len(models) > 1 else root / models[0].stem
    command = ["--model", *map(str, models), "--out", str(out), "--size", str(args.size),
               "--light", args.light, "--no-sheet"]
    views = list(args.views or DEFAULT_VIEWS)
    if args.game_camera and "game" not in views:
        views.append("game")
    if args.turntable and "turntable" not in views:
        views.append("turntable")
    command += ["--views", *views]
    if args.passes:
        command += ["--passes", *args.passes]
    if args.game_camera:
        command += ["--game", args.game_camera]
    if args.turntable:
        command += ["--turntable-frames", str(args.turntable)]
    if args.animation:
        command += ["--animation", args.animation, "--time", str(args.time)]
    root.mkdir(parents=True, exist_ok=True)
    result = run_blender.run(HERE / "render_views.py", command, args.blender, root / "render.log",
                             timeout=args.timeout)
    rendered = {}
    for model in models:
        views_file = root / model.stem / "views.json"
        if views_file.is_file():
            rendered[model] = json.loads(views_file.read_text(encoding="utf-8"))
    if result["code"] != 0 or len(rendered) != len(models):
        missing = [m.name for m in models if m not in rendered]
        raise RuntimeError(f"render failed (exit {result['code']}, missing {missing}), see {result['log']}")
    return rendered, result


TOOLS = {"mesh_check": mesh_check, "glb_check": glb_check, "contact_check": contact_check,
         "clip_check": clip_check}


def tool_options(tool, config, **flags):
    """--config (nested under the tool name, or flat keys this tool knows) first, then the flags
    given on this command line."""
    nested = config.get(tool) if isinstance(config.get(tool), dict) else None
    known = TOOLS[tool].DEFAULTS
    base = dict(nested) if nested is not None else {
        k: v for k, v in config.items() if k in known and not isinstance(v, dict)}
    base.update({k: v for k, v in flags.items() if v not in (None, False)})
    return base


def run_checks(model, args, config):
    glb = Glb.load(model)
    skinned = bool(glb.json.get("skins"))
    animated = bool(glb.json.get("animations"))
    has_mesh = any("mesh" in node for node in glb.nodes)
    reports = []
    if has_mesh:
        reports.append(mesh_check.check(glb, **tool_options("mesh_check", config, profile=args.profile)))
    reports.append(glb_check.check(glb, **tool_options("glb_check", config, pivot=args.pivot)))
    if animated and not has_mesh and not args.clip_model:
        reports.append(R.make_report("clip_check", model, [R.finding(
            "clip_only", "info", "animation-only file: nothing to pose or render",
            hint="pass --clip-model <model.glb> to check these clips on a body")]))
    if args.contacts or args.floating:
        reports.append(contact_check.check(glb, **tool_options(
            "contact_check", config, contract=args.contacts, floating=args.floating)))
    if animated and (skinned or args.clip_model):
        reports.append(clip_check.check(glb, **tool_options("clip_check", config, model=args.clip_model)))
    return reports


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def review(models, args, config):
    """Checks per model, one shared render, one card per model. Returns [(summary, reports)]."""
    started = time.monotonic()
    models = [Path(m).resolve() for m in models]
    stems = [m.stem for m in models]
    clashes = sorted({s for s in stems if stems.count(s) > 1})
    if clashes:
        raise SystemExit(f"several models share the name {clashes}; review them in separate calls")
    root = Path(args.out).resolve() if args.out else Path.cwd() / "tmp" / "review"
    checked = {}
    for model in models:
        out = root / model.stem
        out.mkdir(parents=True, exist_ok=True)
        checked[model] = run_checks(model, args, config)
        for report in checked[model]:
            write_json(out / f"{report['tool']}.json", report)
    rendered, render_result = ({}, None) if args.no_render else render(models, root, args)
    results = []
    for model in models:
        out, reports, views = root / model.stem, checked[model], rendered.get(model)
        card = review_card.compose(out / "views.json", reports, out / "card.png", args.game_crop) if views else None
        summary = {"asset": model.as_posix(), "out": out.as_posix(), "card": card.as_posix() if card else None,
                   "blender": render_result["version"] if render_result else None,
                   "stats": views.get("stats") if views else None,
                   "checks": {r["tool"]: {"status": r["status"], "counts": r["counts"]} for r in reports}}
        write_json(out / "review.json", summary)
        results.append((summary, reports))
    seconds = round(time.monotonic() - started, 2)
    return results, seconds


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    parser.add_argument("models", nargs="+", help="GLB files")
    parser.add_argument("--out", help="output folder (default: ./tmp/review)")
    parser.add_argument("--views", nargs="+", help="front back left right top three_quarter game")
    parser.add_argument("--passes", nargs="+", help="beauty clay wire silhouette")
    parser.add_argument("--size", type=int, default=768)
    parser.add_argument("--light", default="studio_hdr", choices=("studio_hdr", "flat"))
    parser.add_argument("--game-camera", metavar="JSON",
                        help='add the game view: {"height":1.8,"behind":2,"offset":0,"pitch":16,"fov_y":55,'
                             '"yaw":0,"target":[0,0,0]} (any subset; pitch looks down)')
    parser.add_argument("--turntable", type=int, default=0, help="add N turntable frames")
    parser.add_argument("--animation", help="render posed at this clip")
    parser.add_argument("--time", type=float, default=0.0, help="seconds into --animation")
    parser.add_argument("--game-crop", help="screenshot crop of the asset in the game, shown on the card")
    parser.add_argument("--profile", help="mesh_check profile: auto (default), volume or surface")
    parser.add_argument("--pivot", help="glb_check pivot rule: floor, center, top or any (default)")
    parser.add_argument("--contacts", help="contact_check contract JSON")
    parser.add_argument("--floating", action="store_true", help="look for parts hanging in the air")
    parser.add_argument("--clip-model", help="model GLB to play this clip-only GLB on")
    parser.add_argument("--config", help="JSON option overrides, flat or {tool name: {option: value}}; flags win")
    parser.add_argument("--no-render", action="store_true", help="checks only, no Blender")
    parser.add_argument("--blender")
    parser.add_argument("--timeout", type=float, default=900)
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--fail-on", choices=R.SEVERITIES[1:], default="high")
    args = parser.parse_args(argv)
    config = R.load_config(args.config)
    results, seconds = review(args.models, args, config)
    failed = False
    for summary, reports in results:
        for report in reports:
            print(R.format_report(report, quiet=True))
        print(f"card: {summary['card'] or '(no render)'}")
        failed |= args.strict and any(R.fails(r, args.fail_on) for r in reports)
    print(f"{len(results)} model(s) in {seconds} s")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
