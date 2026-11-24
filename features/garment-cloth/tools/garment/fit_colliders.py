"""Fit an outfit's body shapes (garment.colliders.fit) on the body its script declares and the sidecar it builds, over
the outfit's clips, and write the set (garment.colliders.v1).

    python features/garment-cloth/tools/garment/fit_colliders.py content/garments/<outfit>.py <segments.json> <out.json>

The script runs into scratch first (as check.py does), so the fit sees the sim meshes and maps of the stage it builds;
the segments are the rig's body segments (joint pairs). Every reference sample that fails G4 is G4's, not a shape's."""
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from garment import check, colliders as C, model as M  # noqa: E402
from garment.sidecar import read  # noqa: E402


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) != 3:
        raise SystemExit(__doc__)
    script, segments_path, out = (Path(a) for a in argv)
    outfit, data = check.build(script.resolve())
    sc = read(data)
    rig = C.Rig.from_glb(outfit.model.glb)
    pieces = []
    for p, authored in zip(sc.pieces, outfit.pieces):
        w = np.zeros((len(p.points), len(rig.joints)))
        np.add.at(w, (np.arange(len(w))[:, None], p.joints.astype(int)), p.weights.astype(float))
        pieces.append(C.Piece(p.id, p.points.astype(float), p.faces, p.r.astype(float), w, authored.thickness))
    clips = C.clip_poses(outfit.model.glb, rig.joints, M.clips(outfit.clips))
    segments = C.segments_from(json.loads(segments_path.read_text(encoding="utf-8")))
    g4 = C.g4_failing(outfit.body, pieces, clips)
    shapes = C.fit(outfit.body, rig, segments, pieces, clips, g4)
    body_file = next((Path(outfit.root / k) for k in sc.build["inputs"] if k.endswith(".body.glb")), outfit.model_path)
    C.write(out, shapes, C.sha256(body_file))
    print(f"{out}: {len(shapes.spheres)} spheres, {len(shapes.cones)} cones on {body_file.name}")


if __name__ == "__main__":
    main()
