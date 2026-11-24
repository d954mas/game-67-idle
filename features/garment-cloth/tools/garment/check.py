"""Check an outfit script: rebuild its sidecar in scratch and compare the bytes, then run the gates on what it built.

    python features/garment-cloth/tools/garment/check.py content/garments/<outfit>.py
        [--sidecar PATH] [--gates G1,...,G9] [--stages N] [--clips walk,sit] [--cards DIR] [--report FILE]

Prints one JSON finding per gate and piece (see gates.py), then a summary line, and exits 1 when an enforced gate
or the rebuild fails. A failing gate never stops the sidecar: the findings are the content to-do. --sidecar is the
committed file to compare (default: beside the model); --cards writes a card of each failure's worst frame."""
import argparse
import json
import os
import runpy
import sys
import tempfile
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from garment import cards, gates as G, model as M  # noqa: E402
from garment.outfit import Outfit  # noqa: E402
from garment.sidecar import read  # noqa: E402

PER_FRAME = ("G4", "G5", "G6", "G7", "G8")
CHUNK = 16


def build(script):
    """Run the script with its sidecar written to scratch: (outfit, bytes)."""
    with tempfile.TemporaryDirectory() as scratch:
        previous = os.environ.get("GARMENT_SIDECAR_DIR")
        os.environ["GARMENT_SIDECAR_DIR"] = scratch
        try:
            runpy.run_path(str(script), run_name="__main__")
        finally:
            if previous is None:
                del os.environ["GARMENT_SIDECAR_DIR"]
            else:
                os.environ["GARMENT_SIDECAR_DIR"] = previous
        outfit = Outfit.instances[-1]
        written = sorted(Path(scratch).glob("*.cloth.glb"))
        if len(written) != 1:
            raise RuntimeError(f"{script}: the script must write one sidecar (o.write()), wrote {len(written)}")
        return outfit, written[0].read_bytes()


def rebuild(fresh, committed_path):
    finding = dict(gate="rebuild", piece=None, enforced=True, sidecar=str(committed_path))
    if not committed_path.is_file():
        return dict(finding, failed=True, reason="no committed sidecar")
    committed = committed_path.read_bytes()
    if committed == fresh:
        return dict(finding, failed=False)
    stale = read(committed).build != read(fresh).build
    return dict(finding, failed=True, reason="stale: the script or an input changed since the build" if stale
                else "same script and inputs, different bytes: the library changed or the build is not deterministic")


def gate_data(outfit, data):
    colliders = None if outfit.colliders is None else json.loads(outfit.colliders.read_text(encoding="utf-8"))
    clips = [(G.REST, None)] + (M.clips(outfit.clips) if outfit.clips else [])
    body = outfit.body
    return G.GateData(sidecar=data, model=str(outfit.model_path),
                      body=(body.points, body.faces, body.weights, body.complete), shapes=G.shapes_from(colliders),
                      fabrics={p.id: outfit.fabrics[p.fabric] for p in outfit.pieces}, clips=clips,
                      looks=[str(p) for p in outfit.looks])


def run_gates(outfit, data, wanted, clip_names=None, workers=None):
    gd = gate_data(outfit, data)
    sc = read(data)
    findings = []
    if "G1" in wanted:
        findings += G.g1_maps(sc)
    if "G2" in wanted or "G3" in wanted:
        findings += [f for f in G.g2_g3(outfit.model, sc) if f["gate"] in wanted]
    per_frame = tuple(g for g in PER_FRAME if g in wanted)
    if per_frame:
        clips = []
        for c in gd.clips:
            if clip_names is not None and c[0] not in clip_names and c[0] != G.REST:
                continue
            frames = 1 if c[0] == G.REST else len(M.load(c[1]).key_times())
            # Runs of frames, so a long clip spreads over the workers.
            clips += [c] if c[0] == G.REST else [(*c, s, s + CHUNK) for s in range(0, frames, CHUNK)]
        with ProcessPoolExecutor(max_workers=workers or min(8, os.cpu_count() or 1)) as pool:
            parts = list(pool.map(G.clip_gates, [gd] * len(clips), clips, [per_frame] * len(clips)))
        findings += list(G.merge(parts).values())
    if "G9" in wanted:
        findings += G.g9_looks(outfit.model, outfit.looks)
    findings += G.holes(sc, outfit.body)
    return findings


def pose_matrices(outfit, clip, t):
    if clip == G.REST:
        return None
    path = dict(M.clips(outfit.clips))[clip]
    glb = outfit.model.glb
    return M.skin_matrices(glb, outfit.model.joints, glb.inverse_binds(), M.clip_worlds(glb, M.load(path), [t]))[0]


def posed_shapes(outfit, matrices):
    if outfit.colliders is None:
        return None
    doc = json.loads(outfit.colliders.read_text(encoding="utf-8"))
    spheres = []
    for s in doc["spheres"]:
        centre = np.r_[s["center"], 1.0]
        if matrices is not None:
            centre = matrices[outfit.model.joints.index(s["bone"])] @ centre
        spheres.append((centre[:3], s["radius"]))
    return dict(spheres=spheres, cones=[(spheres[a], spheres[b]) for a, b in doc["cones"]])


def failure_card(outfit, finding, folder):
    worst = finding["worst"]
    piece_id = finding["piece"].split(">")[0].split("/")[0] if finding["gate"] != "G9" else None
    if not worst or piece_id is None:
        return None
    piece = outfit.get(piece_id)
    matrices = pose_matrices(outfit, worst["clip"], worst["t"])
    name = f"{finding['gate']}-{Path(outfit.model_path).stem}-{finding['piece'].replace('/', '-').replace('>', '-over-')}.png"
    value = f"{worst['value_mm']} mm" if worst.get("value_mm") is not None else ""
    if finding["frames"] > 1:
        title = (f"{finding['gate']} {finding['piece']}: worst at {worst['clip']} @ {worst['t']} s {value}; "
                 f"{finding['frames_failed']} of {finding['frames']} frames fail")
    else:
        counts = ", ".join(f"{k.replace('_', ' ')} {v}" for k, v in finding.get("counts", {}).items())
        title = f"{finding['gate']} {finding['piece']} at rest {value}: {counts}; marked: the sim vertices behind it"
    cards.piece_card(Path(folder) / name, piece, "fail", matrices=matrices, marks=worst["vertices"][:12],
                     shapes=posed_shapes(outfit, matrices) if finding["gate"] in ("G6", "G7") else None, title=title,
                     yaws=(0, 90, 180, 270))
    return str(Path(folder) / name)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("script", type=Path)
    parser.add_argument("--sidecar", type=Path)
    parser.add_argument("--gates", default="G1,G2,G3,G4,G5,G6,G7,G8,G9")
    parser.add_argument("--clips")
    parser.add_argument("--cards", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--workers", type=int)
    parser.add_argument("--stages", type=int, help="build the script's content up to this stage")
    args = parser.parse_args(argv)
    if args.stages is not None:
        os.environ["GARMENT_STAGES"] = str(args.stages)
    outfit, data = build(args.script.resolve())
    committed = args.sidecar or outfit.sidecar_path()
    findings = [rebuild(data, committed)]
    findings += run_gates(outfit, data, set(args.gates.split(",")), args.clips and set(args.clips.split(",")),
                          args.workers)
    if args.cards:
        args.cards.mkdir(parents=True, exist_ok=True)
        for f in findings:
            if f.get("failed") and f["gate"] != "rebuild":
                f["card"] = failure_card(outfit, f, args.cards)
    for f in findings:
        print(json.dumps(f))
    failed = [f for f in findings if f.get("failed") and f["enforced"]]
    reported = [f for f in findings if f.get("failed") and not f["enforced"]]
    print(f"{args.script.name}: {len(failed)} enforced failures, {len(reported)} reported", file=sys.stderr)
    if args.report:
        args.report.write_text(json.dumps(findings, indent=1) + "\n", encoding="utf-8")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
