"""Look at and check a model in one command: renders, every applicable check, one review card.

    python review.py tmp/crate.glb
    python review.py tmp/hero.glb --game-camera '{"height": 1.6, "behind": 4, "pitch": 12, "fov_y": 40}'
    python review.py assets/models/props --out tmp/review --contacts contacts.json --pivot floor

From a game repo call it with any Python that has numpy and Pillow, e.g.
`python ../../ai_studio/assets/tools/blender/review.py assets/models/crate.glb`; paths resolve
against the current folder. Without Blender (agent sandbox) add --no-render for checks only.

All models render in one Blender start (about 1 s per extra model). Per model it writes
<out>/<stem>/ (default ./tmp/review): PNG views, views.json, one JSON report per check, card.png and
review.json. Checks advise: the command exits 0 unless --strict is given. Skinned models get the
rig checks and models with animations get the clip checks automatically.
"""
import argparse
import glob
import json
import os
import sys
import time
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[4]))

from ai_studio.assets.tools.blender import clip_check, contact_check, glb_check, mesh_check  # noqa: E402
from ai_studio.assets.tools.blender import review_card, run_blender  # noqa: E402
from ai_studio.assets.tools.blender.lib import report as R  # noqa: E402
from ai_studio.assets.tools.blender.lib.glb import Glb, GlbError  # noqa: E402

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


def validate_config(config):
    """A flat key no tool knows is a typo: refuse it rather than silently keep the default."""
    known = set().union(*(module.DEFAULTS for module in TOOLS.values()))
    unknown = sorted(k for k, v in config.items() if k not in TOOLS and k not in known)
    if unknown:
        raise SystemExit(f"--config: unknown option(s) {unknown}; known: {sorted(known)}")


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
    """Reports for one model; a file that cannot be read yields one `unreadable` report."""
    try:
        glb = Glb.load(model)
    except (GlbError, OSError, ValueError, KeyError, IndexError) as error:
        return [R.make_report("glb_check", model, [R.finding(
            "unreadable", "high", f"cannot read the file: {error}",
            "re-export the GLB; Draco/meshopt compression is not supported by these checks")])]
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
    renderable = [m for m in models if not any(
        f["check"] == "unreadable" for r in checked[m] for f in r["findings"])]
    rendered, render_result, render_error = {}, None, None
    if not args.no_render and renderable:
        try:
            rendered, render_result = render(renderable, root, args)
        except (RuntimeError, FileNotFoundError) as error:
            render_error = str(error)
    results = []
    for model in models:
        out, reports, views = root / model.stem, checked[model], rendered.get(model)
        card = review_card.compose(out / "views.json", reports, out / "card.png", args.game_crop) if views else None
        summary = {"asset": model.as_posix(), "out": out.as_posix(), "card": card.as_posix() if card else None,
                   "render_error": render_error if model in renderable else "file not readable",
                   "blender": render_result["version"] if render_result else None,
                   "stats": views.get("stats") if views else None,
                   "checks": {r["tool"]: {"status": r["status"], "counts": r["counts"]} for r in reports}}
        write_json(out / "review.json", summary)
        results.append((summary, reports))
    seconds = round(time.monotonic() - started, 2)
    return results, seconds


def caller_path(value):
    """Paths on the command line are relative to where the caller stands, even when a wrapper
    (python_run.mjs) starts Python from the Studio root."""
    if value is None:
        return None
    path = Path(value)
    return path if path.is_absolute() else Path(os.environ.get("NT_CALLER_CWD", os.getcwd())) / path


def expand_models(items):
    """Files, folders (their *.glb) and globs; PowerShell and cmd pass `*.glb` through unexpanded."""
    models = []
    for item in items:
        path = caller_path(item)
        if path.is_dir():
            models += sorted(path.glob("*.glb"))
        elif any(ch in item for ch in "*?["):
            models += sorted(Path(p) for p in glob.glob(str(path)))
        else:
            models.append(path)
    if not models:
        raise SystemExit(f"no GLB files match {items}")
    return models


def print_report(report):
    """Summary line, then every finding above info with its fix hint; info only counted."""
    counts = report["counts"]
    print(f"{report['tool']}: {report['status']} (high {counts['high']}, medium {counts['medium']}, "
          f"low {counts['low']}, info {counts['info']})")
    for f in report["findings"]:
        if f["severity"] == "info":
            continue
        print(f"  [{f['severity']}] {f['check']}: {f['message']}")
        if f.get("hint"):
            print(f"      fix: {f['hint']}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    parser.add_argument("models", nargs="+", help="GLB files, folders or globs")
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
    for name in ("out", "config", "contacts", "clip_model", "game_crop"):
        if getattr(args, name):
            setattr(args, name, str(caller_path(getattr(args, name))))
    if args.out is None:
        args.out = str(caller_path("tmp/review"))
    config = R.load_config(args.config)
    validate_config(config)
    results, seconds = review(expand_models(args.models), args, config)
    failed = False
    for summary, reports in results:
        print(f"== {Path(summary['asset']).name}")
        for report in reports:
            print_report(report)
        if summary["card"]:
            print(f"card: {summary['card']}  (open it to see the views)")
        elif not args.no_render:
            print(f"card: not rendered ({summary['render_error']})")
        failed |= args.strict and any(R.fails(r, args.fail_on) for r in reports)
    print(f"{len(results)} model(s) in {seconds} s")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
