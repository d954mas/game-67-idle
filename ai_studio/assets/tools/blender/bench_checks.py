"""Time the GLB checks over files or folders and print a table, so a speed change can be measured.

    python bench_checks.py assets/env/ hero.glb [--checks mesh contact glb clip] [--model hero.glb]
    python bench_checks.py clips/*.glb --checks clip --model hero.glb --profile 25
    python bench_checks.py assets/env/ --json-dir tmp/before --totals

contact_check runs with --floating (no contract needed). --json-dir writes one report per check and
file, so two runs can be diffed for unchanged findings. --profile prints the cProfile top functions
of the whole run by own time.
"""
import argparse
import cProfile
import json
import pstats
import sys
import time
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[4]))

from ai_studio.assets.tools.blender import (  # noqa: E402
    clip_check, contact_check, glb_check, mesh_check)
from ai_studio.assets.tools.blender.lib import glb as G  # noqa: E402

CHECKS = {
    "mesh": (mesh_check.check, {}),
    "contact": (contact_check.check, {"floating": True}),
    "glb": (glb_check.check, {}),
    "clip": (clip_check.check, {}),
}


def _files(paths):
    out = []
    for path in map(Path, paths):
        out.extend(sorted(path.glob("*.glb")) if path.is_dir() else [path])
    return out


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    parser.add_argument("paths", nargs="+", help="GLB files or folders of them")
    parser.add_argument("--checks", nargs="+", choices=sorted(CHECKS), default=["mesh", "contact", "glb", "clip"])
    parser.add_argument("--model", help="clip_check: the skinned model the clips play on")
    parser.add_argument("--json-dir", help="write each report to <dir>/<check>/<file stem>.json")
    parser.add_argument("--profile", type=int, metavar="N", help="print the top N functions by own time")
    parser.add_argument("--totals", action="store_true", help="print only per-check totals, not every file")
    args = parser.parse_args(argv)

    files = _files(args.paths)
    model = None
    if args.model:
        # Loaded once, as the clip_check CLI does, so the first clip pays the model's one-off decoding.
        model = G.Glb.load(args.model)
    profiler = cProfile.Profile() if args.profile else None
    rows, totals = [], {c: [0.0, 0, 0.0, ""] for c in args.checks}
    for path in files:
        for name in args.checks:
            fn, opts = CHECKS[name]
            opts = {**opts, **({"model": model} if name == "clip" and model else {})}
            if profiler:
                profiler.enable()
            start = time.perf_counter()
            report = fn(str(path), **opts)
            seconds = time.perf_counter() - start
            if profiler:
                profiler.disable()
            count = len(report["findings"])
            rows.append((path.name, name, seconds, count))
            total = totals[name]
            total[0] += seconds
            total[1] += 1
            if seconds > total[2]:
                total[2], total[3] = seconds, path.name
            if args.json_dir:
                out = Path(args.json_dir) / name / f"{path.stem}.json"
                out.parent.mkdir(parents=True, exist_ok=True)
                out.write_text(json.dumps(report, indent=1, ensure_ascii=False, default=str) + "\n", encoding="utf-8")

    width = max([len(r[0]) for r in rows] + [4])
    if not args.totals:
        print(f"{'file':<{width}}  {'check':<8} {'seconds':>8} {'findings':>8}")
        for file, name, seconds, count in rows:
            print(f"{file:<{width}}  {name:<8} {seconds:8.3f} {count:8d}")
        print()
    print(f"{'check':<8} {'files':>5} {'total s':>8} {'mean s':>8} {'max s':>8}  slowest")
    for name, (seconds, count, slowest, file) in totals.items():
        print(f"{name:<8} {count:5d} {seconds:8.2f} {seconds / max(count, 1):8.3f} {slowest:8.3f}  {file}")
    if profiler:
        pstats.Stats(profiler).sort_stats("tottime").print_stats(args.profile)
    return 0


if __name__ == "__main__":
    sys.exit(main())
