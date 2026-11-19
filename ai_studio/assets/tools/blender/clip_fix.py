"""Repair clips in a GLB without Blender: --ground lifts sinking bodies, --foot-lock pins planted feet.

    python clip_fix.py clips.glb --ground --model body.glb --baseline Idle --clips Uppercut GetUp --out fixed.glb
    python clip_fix.py clips.glb --ground --in-place --report ground.json
    python clip_fix.py clips.glb --foot-lock --model body.glb --clips Laughing --out fixed.glb

The floor is --floor (metres, default 0) or, with --baseline, the median lowest point of that
animation. Keys sinking more than --tolerance below it are raised by the sink; the lift fades over
--ramp keys either side and is never downward. Untouched clips stay byte-identical (method:
lib/clip_ground.py). Prints per clip the max sink before and after, the largest lift and keys touched.
--foot-lock (after --ground when both) pins each planted ankle with two-bone IK and keeps the foot's
world rotation; only thigh, shin and foot keys change (lib/clip_lock.py).
"""
import argparse
import json
import sys
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[4]))

from ai_studio.assets.tools.blender.lib import clip_ground as CG, clip_lock as CL, glb as G  # noqa: E402


def parse(argv=None):
    p = argparse.ArgumentParser(prog="clip_fix.py", description=__doc__.split("\n\n")[0])
    p.add_argument("clip", help="GLB with the animations")
    p.add_argument("--ground", action="store_true", help="lift sinking clips onto the floor")
    p.add_argument("--foot-lock", action="store_true", help="pin planted feet with two-bone IK")
    p.add_argument("--map", default="auto", help="retarget map naming the legs for --foot-lock (default: auto)")
    p.add_argument("--lock-space", choices=("world", "in_place"), default="world",
                   help="in_place: pin feet where a game playing the clip in place shows them (it removes the "
                        "root's start-to-end horizontal travel linearly)")
    p.add_argument("--lock-pin", choices=("ankle", "toe"), default="ankle",
                   help="toe: the toe joint is the planted point (stance on the ball of the foot)")
    p.add_argument("--model", help="skinned body the clips play on (default: the clip file)")
    p.add_argument("--clips", nargs="+", help="only these animations (default: all)")
    p.add_argument("--floor", type=float, default=0.0, help="floor height in metres (default 0)")
    p.add_argument("--baseline", help="animation whose median lowest point is the floor, e.g. Idle")
    p.add_argument("--baseline-file", help="GLB holding the --baseline animation (default: the clip file)")
    p.add_argument("--tolerance", type=float, default=0.01, help="sink in metres left alone (default 0.01)")
    p.add_argument("--ramp", type=int, default=3, help="keys either side a lift or a lock fades over (default 3)")
    p.add_argument("--loop", action="store_true", help="the clips loop: lift ramps wrap, seam plants share an anchor")
    out = p.add_mutually_exclusive_group(required=True)
    out.add_argument("--out", help="write the fixed GLB here")
    out.add_argument("--in-place", action="store_true", help="overwrite the clip file")
    p.add_argument("--report", help="write the report JSON here")
    return p.parse_args(argv)


def one_pass(clip, model, args, source):
    """(document, binary, report) of one --ground then --foot-lock pass."""
    report, document, binary = {}, clip.json, clip.bin
    if args.ground:
        document, binary, report["ground"] = CG.ground(clip, model, args.clips, args.floor, args.baseline,
                                                       args.tolerance, args.ramp, args.loop, baseline_source=source)
        clip = G.Glb(document, binary, clip.path)
    if args.foot_lock:
        document, binary, report["foot_lock"] = CL.foot_lock(clip, model, args.clips, args.ramp, rig_map=args.map,
                                                             loop=args.loop, space=args.lock_space,
                                                             pin=args.lock_pin)
    return document, binary, report


def main(argv=None):
    args = parse(argv)
    if not (args.ground or args.foot_lock):
        raise SystemExit("nothing to do: pass --ground and/or --foot-lock")
    clip = G.Glb.load(args.clip)
    model = G.Glb.load(args.model) if args.model else None
    source = G.Glb.load(args.baseline_file) if args.baseline_file else None
    # Bent knees can move the lowest point and a lift changes which keys read as planted, so the passes
    # repeat until the file stops changing; running the command again then changes nothing.
    first, encoded = None, None
    for passes in range(1, 5):
        document, binary, report = one_pass(clip, model, args, source)
        first = first or report
        previous, encoded = encoded, G.encode_glb(document, binary)
        if encoded == previous:
            break
        clip = G.Glb(document, binary, clip.path)
    report = dict(first, passes=passes)
    if "ground" in report:
        ground = report["ground"]
        print(f"floor {ground['floor_y']:+.3f} m" + (f" (median of {args.baseline})" if args.baseline else ""))
        for name, row in ground["clips"].items():
            print(f"{name:20} sink {row['max_sink_before']:.3f} -> {row['max_sink_after']:.3f} m  "
                  f"lift {row['max_lift']:.3f} m on {row['frames_touched']}/{row['keys']} keys ({row['root']})")
    for name, row in report.get("foot_lock", {}).get("clips", {}).items():
        if "skipped" in row:
            print(f"{name:20} foot lock skipped: {row['skipped']}")
            continue
        feet = "  ".join(f"{side} {f['slide_before']:.3f} -> {f['slide_after']:.3f} m "
                         f"({f['planted_keys']} keys{'' if f['locked'] else ', left alone'})"
                         for side, f in row["feet"].items())
        print(f"{name:20} ankle slide {feet}")
    target = Path(args.clip) if args.in_place else Path(args.out)
    G.write_glb(target, document, binary)
    report.update(input=Path(args.clip).as_posix(), output=target.as_posix(), sha256=G.sha256(target))
    if args.report:
        Path(args.report).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {target} after {passes} pass{'es' if passes > 1 else ''}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
