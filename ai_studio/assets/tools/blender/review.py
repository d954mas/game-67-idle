"""Look at and check a model in one command: renders, every applicable check, one review card.

    python review.py tmp/crate.glb
    python review.py tmp/hero.glb --game-camera '{"height": 1.6, "behind": 4, "pitch": 12, "fov_y": 40}'
    python review.py assets/models/props --beside assets/models/props/approved_crate.glb --pivot floor

From a game repo call it with any Python that has numpy and Pillow, e.g.
`python ../../ai_studio/assets/tools/blender/review.py assets/models/crate.glb`; paths resolve
against the current folder. Without Blender (agent sandbox) add --no-render for checks only.

Game presets: a `blender_review.json` in the model's folder or any parent up to the game root
(the folder with game.json) sets defaults for every option below and for the checks, e.g.
{"game_camera": {...}, "beside": ["../approved/crate.glb"], "pivot": "floor",
"mesh_check": {"profile": "volume"}}. Nearer files win, --config wins over presets, flags win over
both. Paths inside a preset are relative to that preset file.

Clip findings (foot slide, pops, soles under the floor) are rendered as pose frames on the card,
so the problem is visible, not only described. All models sharing the same render settings
render in one Blender run. The first render starts a warm Blender server that later reviews reuse
(about 1 s instead of 4 s); it exits after 30 idle minutes, and --cold skips it.
Per model it writes <out>/<stem>/: PNG views, views.json, one JSON report per check, card.png
and review.json. Checks advise: the command exits 0 unless --strict is given.
"""
import argparse
import fnmatch
import glob
import json
import os
import sys
import time
from pathlib import Path
from types import SimpleNamespace

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[4]))

from ai_studio.assets.tools.blender import clip_check, contact_check, glb_check, mesh_check  # noqa: E402
from ai_studio.assets.tools.blender import review_card, run_blender  # noqa: E402
from ai_studio.assets.tools.blender.lib import report as R  # noqa: E402
from ai_studio.assets.tools.blender.lib.glb import Glb, GlbError  # noqa: E402

HERE = Path(__file__).resolve().parent
PRESET_NAME = "blender_review.json"
DEFAULT_VIEWS = ("front", "right", "three_quarter")
# Every option a preset, --config or a flag can set; None/False/[] mean "not set here".
OPTIONS = {"views": None, "passes": None, "size": 768, "light": "studio_hdr", "game_camera": None,
           "turntable": 0, "beside": [], "lineup_views": None, "animation": None, "time": 0.0,
           "pose_views": None, "max_poses": 4, "filmstrip": 8, "max_filmstrips": 3, "pose_size": 384, "profile": None, "pivot": None, "contacts": None,
           "floating": False, "clip_model": None, "game_crop": None}
PATH_OPTIONS = ("contacts", "clip_model", "game_crop")
RENDER_OPTIONS = ("views", "passes", "size", "light", "game_camera", "turntable", "beside",
                  "lineup_views", "animation", "time", "pose_views", "pose_size")
FACING_CHECKS = ("inverted_shell", "inside_out", "miswound")
TOOLS = {"mesh_check": mesh_check, "glb_check": glb_check, "contact_check": contact_check,
         "clip_check": clip_check}


# --- options ----------------------------------------------------------------------------------

def is_set(value):
    """Absent means None, an unset switch (False) or an empty list; 0 is a real value."""
    if value is None or value is False:
        return False
    return not (isinstance(value, (list, tuple, str)) and len(value) == 0)


def find_presets(model):
    """Preset files from the game root down to the model's folder (nearest last, so it wins)."""
    found = []
    for folder in Path(model).resolve().parents:
        candidate = folder / PRESET_NAME
        if candidate.is_file():
            found.append(candidate)
        if (folder / "game.json").is_file() or (folder / ".git").exists():
            break
    return list(reversed(found))


def parse_layer(data, base):
    """(options, tool config) of one preset layer, its paths made absolute against `base`."""
    options, tools = {}, {}
    for key, value in data.items():
        if key in ("files", "match"):
            continue
        if key in OPTIONS:
            if key in PATH_OPTIONS and value:
                value = str((base / value).resolve())
            elif key == "beside":
                value = [str((base / v).resolve()) for v in ([value] if isinstance(value, str) else value)]
            options[key] = value
        else:
            tools[key] = value
    return options, tools


_PRESET_CACHE = {}


def read_preset(path):
    """Cached per file and modification time: a batch of hundreds of models shares one parse."""
    path = Path(path).resolve()
    key = (path, path.stat().st_mtime_ns)
    if key not in _PRESET_CACHE:
        _PRESET_CACHE[key] = _read_preset(path)
    return _PRESET_CACHE[key]


def _read_preset(path):
    """(options, tool config, file rules) of one preset file. `files` holds per-glob overrides,
    e.g. {"match": "*_glow*.glb", "floating": false}, matched against the file name or its path
    relative to the preset; later rules win."""
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    validate_config(data, f"{path}: ")
    base = Path(path).parent
    rules = []
    for k, rule in enumerate(data.get("files", [])):
        if not isinstance(rule, dict) or not rule.get("match"):
            raise SystemExit(f"{path}: files[{k}] needs a \"match\" glob")
        validate_config(rule, f"{path}: files[{k}]: ")
        matches = [rule["match"]] if isinstance(rule["match"], str) else list(rule["match"])
        rules.append((matches, *parse_layer(rule, base)))
    return (*parse_layer(data, base), rules)


def rule_applies(matches, model, base):
    """`model` and `base` are already resolved: this runs once per rule per model."""
    try:
        relative = model.relative_to(base).as_posix()
    except ValueError:
        relative = None
    return any(fnmatch.fnmatch(model.name, m) or (relative and fnmatch.fnmatch(relative, m)) for m in matches)


def layer_tools(layer):
    """One config layer as {tool: options}: flat keys go to every tool that knows them, a
    nested `{tool: {...}}` section overrides them for that tool."""
    out = {}
    for tool, module in TOOLS.items():
        options = {k: v for k, v in layer.items() if k in module.DEFAULTS and not isinstance(v, dict)}
        if isinstance(layer.get(tool), dict):
            options.update(layer[tool])
        if options:
            out[tool] = options
    return out


def merge_tools(into, layer):
    """Merge one layer on top, per tool, so a nearer flat key beats a farther nested one."""
    for tool, options in layer_tools(layer).items():
        into[tool] = {**into.get(tool, {}), **options}
    return into


def model_options(model, args, config):
    """Effective options and tool config for one model: defaults < presets < --config < flags."""
    options, tools = dict(OPTIONS), {}
    presets = [] if getattr(args, "no_preset", False) else (
        [Path(args.preset)] if getattr(args, "preset", None) else find_presets(model))
    resolved = Path(model).resolve()
    for path in presets:
        preset_options, preset_tools, rules = read_preset(path)
        options.update(preset_options)
        merge_tools(tools, preset_tools)
        base = Path(path).resolve().parent
        for matches, rule_options, rule_tools in rules:
            if rule_applies(matches, resolved, base):
                options.update(rule_options)
                merge_tools(tools, rule_tools)
    merge_tools(tools, config)
    options.update({k: v for k, v in config.items() if k in OPTIONS})
    for key in OPTIONS:
        value = getattr(args, key, None)
        if is_set(value):
            options[key] = value
    options["presets"] = [p.as_posix() for p in presets]
    return SimpleNamespace(**options), tools


def validate_config(config, where=""):
    """A key no tool and no option knows is a typo: refuse it rather than keep the default."""
    known = set(OPTIONS).union(*(module.DEFAULTS for module in TOOLS.values()))
    unknown = sorted(k for k in config if k not in TOOLS and k not in known and k not in ("files", "match"))
    for tool in TOOLS:
        if isinstance(config.get(tool), dict):
            unknown += sorted(f"{tool}.{k}" for k in config[tool] if k not in TOOLS[tool].DEFAULTS)
    if unknown:
        raise SystemExit(f"{where}unknown option(s) {unknown}; known: {sorted(known)}")


def tool_options(tool, config, **flags):
    """This tool's options from a config layer (flat keys it knows, then its nested section),
    then the flags given on the command line."""
    base = layer_tools(config).get(tool, {})
    base.update({k: v for k, v in flags.items() if is_set(v)})
    return base


# --- checks -----------------------------------------------------------------------------------

def run_checks(model, opts, config):
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
    clip_model = getattr(opts, "clip_model", None)
    reports = []
    if has_mesh:
        reports.append(mesh_check.check(glb, **tool_options("mesh_check", config, profile=opts.profile)))
    reports.append(glb_check.check(glb, **tool_options("glb_check", config, pivot=opts.pivot)))
    if animated and not has_mesh and not clip_model:
        reports.append(R.make_report("clip_check", model, [R.finding(
            "clip_only", "info", "animation-only file: nothing to pose or render",
            hint="pass --clip-model <model.glb> to check these clips on a body")]))
    if opts.contacts or opts.floating:
        reports.append(contact_check.check(glb, **tool_options(
            "contact_check", config, contract=opts.contacts, floating=opts.floating)))
    if animated and (skinned or clip_model):
        reports.append(clip_check.check(glb, **tool_options("clip_check", config, model=clip_model)))
    return reports


def clip_moments(reports):
    """(animation, seconds, check) moments named by clip findings above info, in report order."""
    moments = []

    def walk(value, anim=None):
        # clip_check nests moments (pops, clips) under the finding's animation.
        if isinstance(value, dict):
            anim = value.get("animation", anim)
            if anim is not None and "time" in value:
                yield anim, float(value["time"])
            for child in value.values():
                yield from walk(child, anim)
        elif isinstance(value, list):
            for child in value:
                yield from walk(child, anim)

    for report in reports:
        if report["tool"] != "clip_check":
            continue
        for f in report["findings"]:
            if f["severity"] != "info":
                moments += [(anim, seconds, f["check"]) for anim, seconds in walk(f.get("where"))]
    return moments


def pose_key(anim, seconds):
    return f"{anim}@{round(float(seconds), 3):g}"


def finding_poses(reports, limit):
    """Distinct `anim@seconds` moments named by clip findings, worst first."""
    poses = []
    for anim, seconds, _ in clip_moments(reports):
        key = pose_key(anim, seconds)
        if key not in poses:
            poses.append(key)
    return poses[:limit]


def pose_plan(model, opts, reports):
    """Pose frames for the card: every finding moment (marked with its checks) plus an even
    filmstrip over each animation that has findings, or over the first one when all are clean,
    so a clip is always seen moving and on the floor."""
    marks = {}
    for anim, seconds, check in clip_moments(reports):
        key = pose_key(anim, seconds)
        if key in marks or len(marks) < int(opts.max_poses or 0):
            marks.setdefault(key, [])
            if check not in marks[key]:
                marks[key].append(check)
    poses = list(marks)
    frames = int(opts.filmstrip or 0)
    # Only files the clip check looked at have animations worth a filmstrip.
    if frames and any(r["tool"] == "clip_check" for r in reports):
        try:
            glb = Glb.load(model)
            names = glb.animation_names()
        except (GlbError, OSError, ValueError, KeyError, IndexError):
            names = []
        flagged = [a for a in names if any(k.startswith(f"{a}@") for k in marks)]
        for anim in (flagged or names[:1])[:int(opts.max_filmstrips or 0)]:
            duration = glb.duration(anim)
            times = [duration * i / (frames - 1) for i in range(frames)] if frames > 1 else [0.0]
            poses += [k for k in (pose_key(anim, s) for s in times) if k not in poses]
    return poses, marks


# --- render -----------------------------------------------------------------------------------

def render_command(models, out, opts, poses, clip_source=None):
    command = ["--model", *map(str, models), "--out", str(out), "--size", str(opts.size),
               "--light", opts.light, "--no-sheet"]
    # A clip-only file needs the motion, not twelve views of the body at rest.
    views = list(opts.views or (["three_quarter"] if clip_source else DEFAULT_VIEWS))
    if opts.game_camera and "game" not in views and not clip_source:
        views.append("game")
    if opts.turntable and "turntable" not in views:
        views.append("turntable")
    command += ["--views", *views]
    passes = list(opts.passes or (["beauty"] if clip_source else []))
    if getattr(opts, "facing", False) and passes and "facing" not in passes:
        passes.append("facing")
    elif getattr(opts, "facing", False) and not passes:
        passes = ["beauty", "clay", "wire", "facing"]
    if passes:
        command += ["--passes", *passes]
    if opts.game_camera:
        game = opts.game_camera if isinstance(opts.game_camera, str) else json.dumps(opts.game_camera)
        command += ["--game", game]
    if opts.turntable:
        command += ["--turntable-frames", str(opts.turntable)]
    if opts.animation:
        command += ["--animation", opts.animation, "--time", str(opts.time)]
    if opts.beside:
        command += ["--beside", *map(str, opts.beside)]
        if opts.lineup_views:
            command += ["--lineup-views", *opts.lineup_views]
    if poses:
        command += ["--poses", *poses, "--floor", "--pose-size", str(opts.pose_size)]
        if opts.pose_views:
            command += ["--pose-views", *opts.pose_views]
    if clip_source:
        command += ["--clip-source", str(clip_source)]
    return command


def render(models, root, opts, poses, blender=None, timeout=900, server=True):
    """Render models that share settings in one Blender run; returns ({model: views}, result).
    A clip-only file is rendered on its body (`clip_model`), one file per run."""
    out = root if len(models) > 1 else root / models[0].stem
    if opts.clip_model:
        command = render_command([opts.clip_model], out, opts, poses, clip_source=models[0])
    else:
        command = render_command(models, out, opts, poses)
    root.mkdir(parents=True, exist_ok=True)
    result = run_blender.run(HERE / "render_views.py", command, blender, root / f"render_{models[0].stem}.log",
                             timeout=timeout, server=server)
    rendered = {}
    for model in models:
        views_file = root / model.stem / "views.json"
        if views_file.is_file():
            rendered[model] = json.loads(views_file.read_text(encoding="utf-8"))
    if result["code"] != 0 or len(rendered) != len(models):
        missing = [m.name for m in models if m not in rendered]
        raise RuntimeError(f"render failed (exit {result['code']}, missing {missing}), see {result['log']}")
    return rendered, result


def render_groups(models, settings, poses):
    """Models whose render settings and pose lists match share one Blender run."""
    groups = {}
    for model in models:
        opts = settings[model]
        key = json.dumps([[getattr(opts, k) for k in RENDER_OPTIONS], opts.facing, poses[model],
                          str(model) if opts.clip_model else None],
                         sort_keys=True, default=str)
        groups.setdefault(key, []).append(model)
    return list(groups.values())


# --- review -----------------------------------------------------------------------------------

def write_json(path, data):
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def review(models, args, config):
    """Checks per model, batched renders, one card per model. Returns ([(summary, reports)], s)."""
    started = time.monotonic()
    models = [Path(m).resolve() for m in models]
    stems = [m.stem for m in models]
    clashes = sorted({s for s in stems if stems.count(s) > 1})
    if clashes:
        raise SystemExit(f"several models share the name {clashes}; review them in separate calls")
    root = Path(args.out).resolve() if getattr(args, "out", None) else Path.cwd() / "tmp" / "review"
    settings, checked, poses, marks = {}, {}, {}, {}
    for model in models:
        opts, tools = model_options(model, args, config)
        settings[model] = opts
        out = root / model.stem
        out.mkdir(parents=True, exist_ok=True)
        checked[model] = run_checks(model, opts, tools)
        poses[model], marks[model] = pose_plan(model, opts, checked[model])
        # Inverted shells look fine in a double-sided render; the facing pass shows them red.
        opts.facing = any(f["check"] in FACING_CHECKS for r in checked[model] for f in r["findings"])
        for report in checked[model]:
            write_json(out / f"{report['tool']}.json", report)
    renderable = [m for m in models if not any(
        f["check"] == "unreadable" for r in checked[m] for f in r["findings"])]
    rendered, results_by_model, errors = {}, {}, {}
    if not getattr(args, "no_render", False):
        for group in render_groups(renderable, settings, poses):
            try:
                views, result = render(group, root, settings[group[0]], poses[group[0]],
                                       getattr(args, "blender", None), getattr(args, "timeout", 900),
                                       server=False if getattr(args, "cold", False) else True)
                rendered.update(views)
                results_by_model.update({m: result for m in group})
            except (RuntimeError, FileNotFoundError) as error:
                errors.update({m: str(error) for m in group})
    results = []
    for model in models:
        out, reports, views, opts = root / model.stem, checked[model], rendered.get(model), settings[model]
        card = None
        if views:
            title = f"{model.name} on {Path(opts.clip_model).name}" if opts.clip_model else None
            card = review_card.compose(out / "views.json", reports, out / "card.png", opts.game_crop, title,
                                       marks=marks[model])
        result = results_by_model.get(model)
        summary = {"asset": model.as_posix(), "out": out.as_posix(), "card": card.as_posix() if card else None,
                   "render_error": errors.get(model) if model in renderable else "file not readable",
                   "blender": result["version"] if result else None, "via": result.get("via") if result else None,
                   "presets": opts.presets, "poses": poses[model],
                   "stats": views.get("stats") if views else None,
                   "checks": {r["tool"]: {"status": r["status"], "counts": r["counts"]} for r in reports}}
        write_json(out / "review.json", summary)
        results.append((summary, reports))
    return results, round(time.monotonic() - started, 2)


# --- command line -----------------------------------------------------------------------------

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


def parser():
    p = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    p.add_argument("models", nargs="+", help="GLB files, folders or globs")
    p.add_argument("--out", help="output folder (default: ./tmp/review)")
    p.add_argument("--views", nargs="+", help="front back left right top three_quarter game")
    p.add_argument("--passes", nargs="+", help="beauty clay wire silhouette")
    p.add_argument("--size", type=int, help="image height in px (default 768)")
    p.add_argument("--light", choices=("studio_hdr", "flat"))
    p.add_argument("--game-camera", metavar="JSON",
                   help='add the game view: {"height":1.8,"behind":2,"offset":0,"pitch":16,"fov_y":55,'
                        '"yaw":0,"target":[0,0,0]} (any subset; pitch looks down)')
    p.add_argument("--beside", nargs="+", help="reference GLBs rendered next to the model at true scale")
    p.add_argument("--lineup-views", nargs="+", help="views of the side-by-side lineup")
    p.add_argument("--turntable", type=int, help="add N turntable frames")
    p.add_argument("--animation", help="render posed at this clip")
    p.add_argument("--time", type=float, help="seconds into --animation")
    p.add_argument("--max-poses", type=int, help="pose frames taken from clip findings (default 4, 0 = none)")
    p.add_argument("--pose-views", nargs="+", help="views for pose frames (default three_quarter)")
    p.add_argument("--filmstrip", type=int, help="even frames per animation on the card (default 8, 0 = none)")
    p.add_argument("--max-filmstrips", type=int, help="animations that get a filmstrip (default 3)")
    p.add_argument("--pose-size", type=int, help="pose frame height in px (default 384)")
    p.add_argument("--game-crop", help="screenshot crop of the asset in the game, shown on the card")
    p.add_argument("--profile", help="mesh_check profile: auto (default), volume or surface")
    p.add_argument("--pivot", help="glb_check pivot rule: floor, floor-center, center, top or any")
    p.add_argument("--contacts", help="contact_check contract JSON")
    p.add_argument("--floating", action="store_true", help="look for parts hanging in the air")
    p.add_argument("--clip-model", help="model GLB to play this clip-only GLB on")
    p.add_argument("--config", help="JSON option overrides, flat or {tool name: {option: value}}")
    p.add_argument("--preset", help=f"use this preset file instead of the nearest {PRESET_NAME}")
    p.add_argument("--no-preset", action="store_true", help=f"ignore {PRESET_NAME} files")
    p.add_argument("--no-render", action="store_true", help="checks only, no Blender")
    p.add_argument("--cold", action="store_true",
                   help="start a fresh Blender instead of the warm server (which review starts on first "
                        "use and which exits after 30 idle minutes)")
    p.add_argument("--blender")
    p.add_argument("--timeout", type=float, default=900)
    p.add_argument("--strict", action="store_true")
    p.add_argument("--fail-on", choices=R.SEVERITIES[1:], default="high")
    return p


def main(argv=None):
    args = parser().parse_args(argv)
    for name in ("out", "config", "contacts", "clip_model", "game_crop", "preset"):
        if getattr(args, name):
            setattr(args, name, str(caller_path(getattr(args, name))))
    if args.beside:
        args.beside = [str(caller_path(p)) for p in args.beside]
    if args.out is None:
        args.out = str(caller_path("tmp/review"))
    config = R.load_config(args.config)
    validate_config(config)
    results, seconds = review(expand_models(args.models), args, config)
    failed = False
    for summary, reports in results:
        print(f"== {Path(summary['asset']).name}" + (f"  (preset: {', '.join(Path(p).parent.name + '/' + Path(p).name for p in summary['presets'])})"
                                                    if summary["presets"] else ""))
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
