"""Corrective shapes of an outfit (spec B.5, C.2, E.1): the runtime's own drape in a key pose, stored as rest-space
deltas the game weights while the pose holds.

A bake runs the feature's runtime through the garment_host library: reset at rest and settle, ramp the joints into
the key, raise the convexes (a seat) from below, settle with the pose frozen, and take the drape. One drape is not yet
an equilibrium of the runtime around itself: where max distance held cloth during the bake, the drape leans on the
old reference. So the bake repeats until it is one: reset onto the corrected reference, hold, take the drape again,
until the cloth stays within its fabric's thickness of the corrected reference (the drift). The residual drift and
the pass count are recorded with the corrective; a bake that does not converge within `max_passes` stops the build
(the record would describe a drape the game cannot hold).

What an author needs: the garment_host library built from the current runtime (`node
ai_studio/feature_testing/run.mjs`), the outfit's collider set and clip manifest declared on the Outfit."""
import hashlib
import json
from pathlib import Path

import numpy as np

from garment_blob import decode
from . import body as body_maps
from . import cook
from . import host as H
from . import model as M

MAX_PASSES = 24
FLOOR = np.array([[0.0, 1.0, 0.0, 0.0]])  # the actor stands on y = 0 of its model space
DRIFT_PERCENTILE = 99


def state_digest(pieces):
    """Hash of everything a bake reads from the sidecar pieces except their correctives: a corrective baked before a
    later edit of a piece is stale, and the outfit refuses to write it."""
    h = hashlib.sha256()
    for p in pieces:
        h.update(p.id.encode())
        h.update(p.fabric.encode())
        for array in (p.points, p.faces, p.joints, p.weights, p.r, p.d, p.R, p.b):
            h.update(np.ascontiguousarray(array).tobytes())
    return h.hexdigest()


def posed_body(body, glb, joints, worlds):
    """The outfit's body skinned by a pose ({node: (1, 4, 4)} worlds): a Body the direction rule can read."""
    matrices = M.skin_matrices(glb, joints, glb.inverse_binds(), worlds)
    points = M.skin_points(body.points, body.weights, matrices)[0]
    normals = M.skin_directions(body.normals, body.weights, matrices)[0]
    return body_maps.Body(points, body.faces, normals, body.weights, body.joints, body.complete)


def drift(host, decoded):
    """Per piece the p99 distance (m) of the free vertices from their reference, after the last update."""
    out = []
    for p, piece in enumerate(decoded["pieces"]):
        free = np.asarray(piece["mesh"]["max_distance"]) > 0
        x, (ref, _) = host.positions(p), host.reference(p)
        gap = np.linalg.norm(x - ref, axis=1)[free]
        out.append(float(np.percentile(gap, DRIFT_PERCENTILE)) if len(gap) else 0.0)
    return out


def bake(blob, index, skeleton, rest, key, convexes, body, thicknesses, max_passes=MAX_PASSES, hold_s=2.0,
         procedure=None, floor=FLOOR, library=None):
    """Bake corrective `index` of a blob in the key pose (both model-space poses in the skeleton's order) among the
    convexes (plane rows, actor space; the floor stays put, the others rise from below): ([(delta, direction delta)
    per piece], record). body: the outfit's body posed in the key; thicknesses: each piece's fabric thickness, the
    drift the corrective must reach; procedure: the spec's bake times (C.2), each overridable."""
    procedure = {**dict(settle_in_s=0.5, ramp_s=1.0, rise_s=0.5, settle_s=2.0, hz=H.POSE_HZ), **(procedure or {})}
    decoded = decode(blob)
    rising = [np.asarray(c, float).reshape(-1, 4) for c in convexes]
    fixed = [np.asarray(floor, float).reshape(-1, 4)]
    weights = np.zeros(len(decoded["correctives"]))
    weights[index] = 1.0
    faces = [np.asarray(p["mesh"]["faces"], int) for p in decoded["pieces"]]
    thickness = np.asarray(thicknesses, float)

    def shapes_of(drape):
        directions = [body_maps.directions(x, f, body) for x, f in zip(drape, faces)]
        return H.deltas(decoded, key, skeleton, drape, directions)

    passes = []
    with H.Host(blob, skeleton, library) as host:
        drape = H.bake(host, rest, key, rising=rising, fixed=fixed, thickness=float(thickness.max()),
                       settle_in_s=procedure["settle_in_s"], ramp_s=procedure["ramp_s"], rise_s=procedure["rise_s"],
                       settle_s=procedure["settle_s"], hz=procedure["hz"])
        shapes = shapes_of(drape)
        best = None
        for k in range(max_passes):
            for p, (delta, turn) in enumerate(shapes):
                host.set_corrective(p, index, delta, turn)
            # The hold the game gives a seated placement: a reset onto the corrected reference, the pose frozen.
            host.step(key, 0.0, reset=True, convexes=fixed + rising, weights=weights)
            for _ in range(int(round(hold_s * H.POSE_HZ))):
                host.step(key, 1 / H.POSE_HZ, convexes=fixed + rising, weights=weights)
            measured = np.asarray(drift(host, decoded))
            passes.append([round(float(v) * 1000, 2) for v in measured])
            score = float(np.max(measured / thickness))
            if best is None or score < best[0]:
                best = (score, shapes, measured)
            if score <= 1.0:
                break
            shapes = shapes_of([host.positions(p) for p in range(len(host.vertices))])
    score, shapes, measured = best
    record = dict(passes=len(passes), converged=score <= 1.0,
                  drift_mm={p["id"]: round(float(v) * 1000, 2) for p, v in zip(decoded["pieces"], measured)},
                  thickness_mm={p["id"]: round(float(t) * 1000, 2) for p, t in zip(decoded["pieces"], thickness)},
                  drift_by_pass_mm=passes, procedure=procedure, hold_s=hold_s)
    return shapes, record


def key_of(outfit, record):
    """(key pose in the host skeleton's order, {node: (1, 4, 4)} worlds) of a corrective record on its outfit."""
    glb = outfit.model.glb
    skeleton = H.Skeleton.from_glb(glb)
    if "pose" in record:
        key = np.asarray(record["pose"]["matrices"], float)
        return key, {glb.node_index(n): m[None] for n, m in zip(record["pose"]["joints"], key)}
    if outfit.clips is None:
        raise ValueError(f"corrective {record['name']!r}: a key clip needs the outfit's clip manifest")
    paths = dict(M.clips(outfit.clips))
    if record["clip"] not in paths:
        raise ValueError(f"corrective {record['name']!r}: no clip {record['clip']!r} in {outfit.clips}")
    worlds = M.clip_worlds(glb, M.load(paths[record["clip"]]), [float(record["t"])])
    return skeleton.pose({n: m[0] for n, m in worlds.items()}), worlds


def bake_inputs(outfit, request):
    """{relative path: sha256} of the files a bake reads besides the pieces: the colliders, and for a clip key the clip
    manifest and the clip."""
    outfit._input(outfit.colliders)
    if "clip" in request:
        outfit._input(dict(outfit.clip_paths())[request["clip"]])
    names = [Path(outfit.colliders).resolve().relative_to(outfit.root).as_posix()]
    if "clip" in request:
        names += [Path(outfit.clips).resolve().relative_to(outfit.root).as_posix(),
                  Path(dict(outfit.clip_paths())[request["clip"]]).resolve().relative_to(outfit.root).as_posix()]
    return {n: outfit.inputs[n] for n in names}


def resolve(outfit, pieces, records, request, committed):
    """Outfit.resolve's last step for one declared corrective: store it on the resolved pieces and return its record.
    Its key is everything the bake reads: the pieces as built without correctives, the runtime sources, the files
    (bake_inputs) and the request. A committed sidecar holding a corrective of that name with the same key gives its
    deltas as they are, so a check needs no host and no re-bake; otherwise the bake runs, and one that does not
    converge stops the build."""
    name = request["name"]
    for p in pieces:
        p.correctives[name] = (np.zeros_like(p.x), np.zeros_like(p.x))
    sc = outfit.sidecar(pieces, records + [dict(name=name)])
    key = json.loads(json.dumps(dict(pieces_sha256=state_digest(sc.pieces), runtime_sha256=H.runtime_sha256(),
                                      inputs=bake_inputs(outfit, request),
                                      request={k: v for k, v in request.items() if k != "name"})))
    previous = next((c for c in committed.correctives if c["name"] == name), None) if committed else None
    if previous is not None and previous.get("key") == key and             [p.id for p in committed.pieces] == [p.id for p in pieces]:
        for p in pieces:
            dx, db = committed.piece(p.id).correctives[name]
            p.correctives[name] = (dx.astype(float), db.astype(float))
        return previous
    model, glb = outfit.model, outfit.model.glb
    skeleton = H.Skeleton.from_glb(glb)
    if "clip" in request:
        source = dict(clip=request["clip"], t=request["t"])
    else:
        source = dict(pose=dict(joints=skeleton.names, matrices=request["pose"]["matrices"]))
    key_pose, worlds = key_of(outfit, dict(name=name, **source))
    convexes = [np.asarray(c, float).reshape(-1, 4) for c in request["convexes"]]
    blob = outfit_blob(outfit, sc)
    body = posed_body(outfit.body, glb, model.joints, worlds)
    thicknesses = [outfit.fabrics[p.fabric]["thickness"] for p in sc.pieces]
    shapes, record = bake(blob, sc.corrective_names.index(name), skeleton, H.rest_pose(glb, skeleton), key_pose, convexes,
                          body, thicknesses, max_passes=request["max_passes"])
    if not record["converged"]:
        raise ValueError(f"corrective {name!r} did not converge in {record['passes']} passes: drift "
                         f"{record['drift_mm']} mm against fabric thickness {record['thickness_mm']} mm; the drape is "
                         "not an equilibrium of the runtime (see the shapes and the seat, G7 and G10)")
    for p, (delta, turn) in zip(pieces, shapes):
        p.correctives[name] = (delta, turn)
    return json.loads(json.dumps(dict(name=name, **source, convexes=request["convexes"], floor=FLOOR.tolist(), **record,
                                      key=key)))


def outfit_blob(outfit, sc):
    """The blob the runtime would load for a sidecar of the outfit, for the host."""
    colliders = json.loads(outfit.colliders.read_text(encoding="utf-8"))
    # Inertia moves cloth only when the root moves; the host holds the actor still.
    return cook.cook_blob(outfit.model_path.stem, sc, outfit.model, outfit.fabrics, colliders, (0.0, 0.0))[0]


def hold(blob, index, skeleton, key, convexes, seconds, library=None):
    """Reset at the key onto corrective `index` (weight 1) and hold the pose: per piece (cloth, reference)."""
    weights = np.zeros(len(decode(blob)["correctives"]))
    if index is not None:
        weights[index] = 1.0
    with H.Host(blob, skeleton, library) as host:
        host.step(key, 0.0, reset=True, convexes=convexes, weights=weights)
        for _ in range(int(round(seconds * H.POSE_HZ))):
            host.step(key, 1 / H.POSE_HZ, convexes=convexes, weights=weights)
        return [(host.positions(p), host.reference(p)[0]) for p in range(len(host.vertices))]


def card(outfit, sc, record, path, yaws=(0, 45, 90)):
    """The corrective's card, three rows per piece over the body posed in its key: the plain reference, the corrected
    reference (heat: how far the corrective moves it, mm) and the cloth the runtime holds on it after a reset there
    (heat: distance from the corrected reference, mm, up to the fabric thickness)."""
    from PIL import Image
    from . import cards
    glb = outfit.model.glb
    skeleton = H.Skeleton.from_glb(glb)
    key, worlds = key_of(outfit, record)
    matrices = M.skin_matrices(glb, outfit.model.joints, glb.inverse_binds(), worlds)
    body = cards.body_triangles(outfit.body, matrices[0])
    blob = outfit_blob(outfit, sc)
    convexes = [np.asarray(c, float) for c in record.get("floor", FLOOR.tolist())[:1]] + record.get("convexes", [])
    convexes = [np.asarray(c, float).reshape(-1, 4) for c in convexes]
    held = hold(blob, sc.corrective_names.index(record["name"]), skeleton, key, convexes, record.get("hold_s", 2.0))
    rows = []
    for p, (x, ref) in zip(sc.pieces, held):
        dense = np.zeros((len(p.points), len(sc.joint_names)))
        np.add.at(dense, (np.arange(len(dense))[:, None], p.joints.astype(int)), p.weights.astype(float))
        plain = M.skin_points(p.points.astype(float), dense, matrices)[0]
        delta = p.correctives[record["name"]][0]
        moved = np.linalg.norm(delta, axis=1) * 1000
        gap = np.linalg.norm(x - ref, axis=1) * 1000
        source = f"{record.get('clip', 'pose')} @ {record.get('t', '')} s"
        rows.append(cards.draw(None, body, plain, p.faces, pins=p.r == 0, yaws=yaws,
                               title=f"{p.id}: reference at the key of {record['name']} ({source}), without the corrective"))
        rows.append(cards.draw(None, body, ref, p.faces, values=moved, low=0.0, unit="corrective mm", yaws=yaws,
                               title=f"{p.id}: corrected reference at the key"))
        rows.append(cards.draw(None, body, x, p.faces, values=gap, low=0.0, high=max(record["thickness_mm"][p.id], 1e-3),
                               unit="mm from the corrected reference", yaws=yaws,
                               title=f"{p.id}: cloth after a reset there and a {record.get('hold_s', 2.0)} s hold; bake "
                                     f"drift p99 {record['drift_mm'][p.id]} mm (fabric {record['thickness_mm'][p.id]} mm), "
                                     f"{record['passes']} passes, {'converged' if record['converged'] else 'NOT converged'}"))
    width = max(r.size[0] for r in rows)
    sheet = Image.new("RGB", (width, sum(r.size[1] for r in rows)), cards.BACKGROUND)
    y = 0
    for r in rows:
        sheet.paste(r, (0, y))
        y += r.size[1]
    sheet.save(path)
    return path
