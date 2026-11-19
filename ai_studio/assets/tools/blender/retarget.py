"""Retarget humanoid clips onto another skeleton, no Blender needed (method: lib/retarget.py).

    python retarget.py --source clips/*.glb --target body.glb --out body_clips.glb [--foot-lock]
    python retarget.py --source walk.glb run.glb --source-rig rig.glb --target body.glb --out-dir out/ --output clip
    python retarget.py --list-maps [--target body.glb]

Without --source-rig every source file is its own rig (clip files that carry their skeleton). Maps are
detected from joint names (--map / --target-map pin one: an id from retarget_maps/ or a JSON path).
--out writes every clip into one file; --out-dir writes one file per source. --output full keeps the
target's meshes and skin; clip writes only its node tree (default: full when the target has a mesh).
Prints one line per clip: mapped joints, scale, foot slide before/after, check results, seconds.
"""
import argparse
import glob
import json
import sys
import time
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[4]))

from ai_studio.assets.tools.blender.lib import glb as G, retarget as RT  # noqa: E402


def expand(patterns):
    out = []
    for p in patterns:
        path = Path(p)
        if path.is_dir():
            out += sorted(path.glob("*.glb"))
        elif any(ch in p for ch in "*?["):
            out += sorted(Path(x) for x in glob.glob(p))
        else:
            out.append(path)
    unique = {}
    for path in out:
        unique.setdefault(path.resolve(), path)
    return list(unique.values())


def parse(argv=None):
    parser = argparse.ArgumentParser(prog="retarget.py", description=__doc__.split("\n\n")[0])
    parser.add_argument("--source", nargs="+", default=[], help="clip GLB files, folders or globs")
    parser.add_argument("--source-rig", help="skeleton the clip files are keyed against (by node name)")
    parser.add_argument("--target", help="target rig or skinned body GLB")
    parser.add_argument("--out", help="one GLB with every clip")
    parser.add_argument("--out-dir", help="one GLB per source file, named after it (parent folders "
                        "prefixed when two sources share a name)")
    parser.add_argument("--output", choices=("auto", "full", "clip"), default="auto")
    parser.add_argument("--map", default="auto", help="source map id or JSON (default: auto)")
    parser.add_argument("--target-map", default="auto", help="target map id or JSON (default: auto)")
    parser.add_argument("--clips", nargs="+", help="only these animation names")
    parser.add_argument("--fps", type=float, default=30.0)
    parser.add_argument("--in-place", action="store_true", help="remove the straight-line ground travel")
    parser.add_argument("--foot-lock", action="store_true", help="pin planted ankles with two-bone IK")
    parser.add_argument("--untwist", action="store_true", help="drop spin of the shins about their own axis")
    parser.add_argument("--ground", action="store_true", help="shift each clip so its feet meet the rest floor")
    parser.add_argument("--no-leg-ik", action="store_true",
                        help="copy leg rotations only (default: legs reach the source's mapped ankles)")
    parser.add_argument("--speed", type=float, default=0.0,
                        help="m/s ground speed of in-place source clips, so --foot-lock finds their plants")
    parser.add_argument("--no-fingers", action="store_true", help="leave finger joints at rest")
    parser.add_argument("--no-check", action="store_true", help="skip clip_check on the written files")
    parser.add_argument("--report", help="write the per-clip report JSON here")
    parser.add_argument("--list-maps", action="store_true", help="list maps (and their coverage of --target)")
    return parser.parse_args(argv)


def list_maps(args):
    maps = RT.load_maps()
    rows = {}
    for path in ([args.target] if args.target else []) + [str(p) for p in expand(args.source)]:
        _, _, cov = RT.detect_map(G.Glb.load(path), maps)
        rows[path] = cov
    for m in maps.values():
        print(f"{m.id:8} {len(m.joints):3} joints  {m.description}")
    for path, cov in rows.items():
        print(f"\n{path}:")
        for c in cov:
            miss = f"  missing {', '.join(c['missing_required'])}" if c["missing_required"] else ""
            print(f"  {c['map']:8} {c['joints']:3}/{c['of']}{miss}")


def fmt_feet(feet):
    parts = []
    for side, f in feet.items():
        after = f" -> {f['slide_after_m']:.3f}" if "slide_after_m" in f else ""
        parts.append(f"{side[0].upper()} {f['slide_before_m']:.3f}{after}")
    return "slide " + " ".join(parts) + " m" if parts else "no legs"


def check_rows(path, target, mode, names):
    from ai_studio.assets.tools.blender import clip_check
    report = clip_check.check(str(path), model=None if mode == "full" else target, animations=names)
    out = {}
    for row in report["metrics"].get("per_animation", []):
        feet = row.get("feet", {})
        out[row["animation"]] = {"max_drift_m": max((f["max_drift_m"] for f in feet.values()), default=None),
                                 "lowest_y": row.get("lowest_y")}
    return out, [f for f in report["findings"] if f["severity"] in ("medium", "high")]


def run(args):
    if not args.source or not args.target or not (args.out or args.out_dir):
        raise SystemExit("need --source, --target and --out or --out-dir")
    started = time.perf_counter()
    target = G.Glb.load(args.target)
    mode = args.output
    if mode == "auto":
        mode = "full" if any("mesh" in n for n in target.nodes) else "clip"
    maps = RT.load_maps()
    rig = G.Glb.load(args.source_rig) if args.source_rig else None
    shared = RT.Retargeter(rig, target, args.map, args.target_map, maps, not args.no_fingers) if rig else None
    options = dict(fps=args.fps, root="in_place" if args.in_place else "keep", foot_lock=args.foot_lock,
                   untwist=RT.HINGES if args.untwist else (), ground=args.ground, legs=not args.no_leg_ik,
                   speed=args.speed)
    groups = []
    for path in expand(args.source):
        clip = G.Glb.load(path)
        retargeter = shared or RT.Retargeter(clip, target, args.map, args.target_map, maps, not args.no_fingers)
        names = [n for n in clip.animation_names() if not args.clips or n in args.clips]
        results = []
        for name in names:
            result = retargeter.clip(clip, name, **options)
            result.report["source"] = path.as_posix()
            results.append(result)
        groups.append((path, results, retargeter))
    outputs = []
    if args.out:
        outputs.append((Path(args.out), [r for _, rs, _ in groups for r in rs]))
    else:
        from ai_studio.assets.tools.blender.review import output_names
        names = output_names([path for path, rs, _ in groups if rs])
        for path, rs, _ in groups:
            if rs:
                if names[path] != path.stem:
                    print(f"{path.as_posix()}: another source has the same name; writing {names[path]}.glb")
                outputs.append((Path(args.out_dir) / f"{names[path]}.glb", rs))
    every = []
    for out, results in outputs:
        RT.write(target, results, out, mode)
        checks, findings = ({}, []) if args.no_check else check_rows(out, target, mode, [r.name for r in results])
        for r in results:
            rep = r.report
            rep["out"] = out.as_posix()
            if r.name in checks:
                rep["check"] = checks[r.name]
            every.append(rep)
            check = rep.get("check") or {}
            drift = f"  check drift {check['max_drift_m']:.3f} m" if check.get("max_drift_m") is not None else ""
            low = f" lowest {check['lowest_y']:+.3f} m" if check.get("lowest_y") is not None else ""
            print(f"{r.name:24} {rep['mapped']:2}/{rep['target_joints']} joints ({rep['source_map']} -> "
                  f"{rep['target_map']})  scale {rep['scale']:.3f}  {fmt_feet(rep['feet'])}{drift}{low}  "
                  f"{rep['seconds']:.2f} s")
            for warning in rep.get("warnings", []):
                print(f"  [warning] {r.name}: {warning}")
        for f in findings:
            print(f"  [{f['severity']}] {f['message']}")
        print(f"wrote {out} ({mode}, {len(results)} clips)")
    first = groups[0][2] if groups else None
    summary = {"tool": "retarget", "target": Path(args.target).as_posix(), "mode": mode,
               "source_coverage": first.src.coverage if first else None,
               "target_coverage": first.tgt.coverage if first else None,
               "seconds": round(time.perf_counter() - started, 3), "clips": every}
    if args.report:
        Path(args.report).parent.mkdir(parents=True, exist_ok=True)
        Path(args.report).write_text(json.dumps(summary, indent=2), encoding="utf-8")
    if first:
        cov = first.tgt.coverage
        print(f"target map {cov['map']}: {cov['joints']}/{cov['of']} joints"
              + (f", missing {', '.join(cov['missing_required'])}" if cov["missing_required"] else ""))
    print(f"{len(every)} clips in {summary['seconds']:.2f} s")
    return summary


def main(argv=None):
    args = parse(argv)
    if args.list_maps:
        list_maps(args)
        return 0
    run(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
