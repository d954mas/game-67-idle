"""Repair clips in a GLB without Blender. --ground lifts the root wherever the body sinks through the floor.

    python clip_fix.py clips.glb --ground --model body.glb --baseline Idle --clips Uppercut GetUp --out fixed.glb
    python clip_fix.py clips.glb --ground --in-place --report ground.json

The floor is --floor (metres, default 0) or, with --baseline, the median lowest point of that
animation. Keys sinking more than --tolerance below it are raised by the sink; the lift fades over
--ramp keys either side and is never downward. Untouched clips stay byte-identical (method:
lib/clip_ground.py). Prints per clip the max sink before and after, the largest lift and keys touched.
"""
import argparse
import json
import sys
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[4]))

from ai_studio.assets.tools.blender.lib import clip_ground as CG, glb as G  # noqa: E402


def parse(argv=None):
    p = argparse.ArgumentParser(prog="clip_fix.py", description=__doc__.split("\n\n")[0])
    p.add_argument("clip", help="GLB with the animations")
    p.add_argument("--ground", action="store_true", help="lift sinking clips onto the floor")
    p.add_argument("--model", help="skinned body the clips play on (default: the clip file)")
    p.add_argument("--clips", nargs="+", help="only these animations (default: all)")
    p.add_argument("--floor", type=float, default=0.0, help="floor height in metres (default 0)")
    p.add_argument("--baseline", help="animation whose median lowest point is the floor, e.g. Idle")
    p.add_argument("--baseline-file", help="GLB holding the --baseline animation (default: the clip file)")
    p.add_argument("--tolerance", type=float, default=0.01, help="sink in metres left alone (default 0.01)")
    p.add_argument("--ramp", type=int, default=3, help="keys either side the lift fades over (default 3)")
    p.add_argument("--loop", action="store_true", help="the clips loop: ramps wrap around")
    out = p.add_mutually_exclusive_group(required=True)
    out.add_argument("--out", help="write the fixed GLB here")
    out.add_argument("--in-place", action="store_true", help="overwrite the clip file")
    p.add_argument("--report", help="write the report JSON here")
    return p.parse_args(argv)


def main(argv=None):
    args = parse(argv)
    if not args.ground:
        raise SystemExit("nothing to do: pass --ground")
    clip = G.Glb.load(args.clip)
    model = G.Glb.load(args.model) if args.model else None
    source = G.Glb.load(args.baseline_file) if args.baseline_file else None
    document, binary, report = CG.ground(clip, model, args.clips, args.floor, args.baseline, args.tolerance,
                                         args.ramp, args.loop, baseline_source=source)
    target = Path(args.clip) if args.in_place else Path(args.out)
    G.write_glb(target, document, binary)
    print(f"floor {report['floor_y']:+.3f} m" + (f" (median of {args.baseline})" if args.baseline else ""))
    for name, row in report["clips"].items():
        print(f"{name:20} sink {row['max_sink_before']:.3f} -> {row['max_sink_after']:.3f} m  "
              f"lift {row['max_lift']:.3f} m on {row['frames_touched']}/{row['keys']} keys ({row['root']})")
    report.update(input=Path(args.clip).as_posix(), output=target.as_posix(), sha256=G.sha256(target))
    if args.report:
        Path(args.report).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {target}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
