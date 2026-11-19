"""Export contract check of a GLB: finite data, unit normals, triangle lists, embedded payloads, image
formats, vertex colour encoding, node transforms, pivot, joint count and skin weights; optionally the
rig against a reference rig.

    python glb_check.py model.glb [--pivot floor|floor-center|center|top|any] [--textures any|none|png]
        [--color-encoding any|float|u8|u16|normalized] [--color-components any|3|4] [--identity-nodes] [--max-joints N] [--reference-rig rig.glb [--reference-rig-mode exact|subset|superset]]

The bounds and the bounds centre are reported separately from the origin: a consumer that places a
model by its bounds or collider centre instead of its origin floats or sinks it.
"""
import sys
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[4]))

import numpy as np  # noqa: E402

from ai_studio.assets.tools.blender.lib import glb as G, report as R  # noqa: E402

TOOL = "glb_check"
DEFAULTS = {"normal_tolerance": 0.01, "jpeg": "low", "textures": "any", "color_encoding": "any", "color_components": "any",
            "identity_nodes": False, "pivot": "any", "pivot_tolerance": 0.001, "max_joints": None,
            "max_influences": 4, "influence_epsilon": 1e-3, "weight_sum_tolerance": 1e-3,
            "bind_tolerance": 1e-4, "seam_weight_tolerance": 1e-3, "weld": 1e-6, "reference_rig": None, "reference_rig_mode": "exact",
            "rig_translation_tolerance": 1e-4, "rig_rotation_tolerance_deg": 0.01}
SAMPLES = 5
PIVOTS = ("floor", "floor-center", "center", "top", "any")
VISIBLE_OFFSET = 0.01  # m
# glTF allows COLOR_0 as float or normalized unsigned byte/short; `normalized` accepts either integer form.
COLOR_ENCODINGS = {"float": {(G.FLOAT, False)}, "u8": {(G.UNSIGNED_BYTE, True)}, "u16": {(G.UNSIGNED_SHORT, True)},
                   "normalized": {(G.UNSIGNED_BYTE, True), (G.UNSIGNED_SHORT, True)}}


def _r(values, digits=4):
    return [round(float(v), digits) for v in np.ravel(values)]


def _accessor_uses(doc):
    uses = {}
    for m, mesh in enumerate(doc.get("meshes", [])):
        for p, prim in enumerate(mesh.get("primitives", [])):
            for key, acc in prim.get("attributes", {}).items():
                uses.setdefault(acc, f"mesh {mesh.get('name', m)} primitive {p} {key}")
            if "indices" in prim:
                uses.setdefault(prim["indices"], f"mesh {mesh.get('name', m)} primitive {p} indices")
    for s, skin in enumerate(doc.get("skins", [])):
        if "inverseBindMatrices" in skin:
            uses.setdefault(skin["inverseBindMatrices"], f"skin {s} inverseBindMatrices")
    for a, anim in enumerate(doc.get("animations", [])):
        for k, sampler in enumerate(anim.get("samplers", [])):
            uses.setdefault(sampler["input"], f"animation {anim.get('name', a)} sampler {k} input")
            uses.setdefault(sampler["output"], f"animation {anim.get('name', a)} sampler {k} output")
    return uses


def check_data(glb, opts, out):
    doc = glb.json
    uses = _accessor_uses(doc)
    bad, unreadable = [], set()
    for index, acc in enumerate(doc.get("accessors", [])):
        try:
            data = glb.accessor(index)
        except (G.GlbError, ValueError, KeyError, IndexError, TypeError) as error:
            unreadable.add(index)
            out.append(R.finding("accessor", "high", f"accessor {index} unreadable: {error}",
                                 "re-export the file", {"accessor": uses.get(index, index)}))
            continue
        if acc.get("componentType") == G.FLOAT and not np.isfinite(data).all():
            bad.append(uses.get(index, f"accessor {index}"))
    for i, node in enumerate(glb.nodes):
        values = [v for key in ("translation", "rotation", "scale", "matrix") for v in node.get(key, [])]
        if not np.isfinite(np.array(values, float)).all():
            bad.append(f"node {glb.name(i)}")
    if bad:
        out.append(R.finding("finite", "high", f"{len(bad)} data streams hold NaN or infinity",
                             "re-export; find the NaN source (zero-length normals, broken keys)",
                             {"streams": bad[:SAMPLES]}, len(bad), 0))
    for group in ("buffers", "images"):
        for k, item in enumerate(doc.get(group, [])):
            uri = item.get("uri")
            if uri is None:
                continue
            embedded = uri.startswith("data:")
            out.append(R.finding("uri", "low" if embedded else "high",
                                 f"{group[:-1]} {k} is {'a base64 data URI' if embedded else 'an external file ' + uri}",
                                 "export as binary glTF with everything packed into the GLB",
                                 {group[:-1]: k}))
    return unreadable


def _image_format(glb, image):
    mime = image.get("mimeType")
    if mime:
        return mime.split("/")[-1].replace("jpg", "jpeg")
    if "bufferView" in image:
        view = glb.json["bufferViews"][image["bufferView"]]
        head = glb.bin[view.get("byteOffset", 0):view.get("byteOffset", 0) + 4]
        if head.startswith(b"\x89PNG"):
            return "png"
        if head.startswith(b"\xff\xd8"):
            return "jpeg"
    uri = image.get("uri", "")
    return "jpeg" if uri.lower().endswith((".jpg", ".jpeg")) else "png" if uri.lower().endswith(".png") else "unknown"


def check_images(glb, opts, out):
    doc = glb.json
    images = doc.get("images", [])
    if opts["textures"] == "none" and (images or doc.get("textures")):
        out.append(R.finding("textures", "medium", f"{len(images)} images, {len(doc.get('textures', []))} "
                             "textures where the contract wants none", "bake to vertex colour or drop the "
                             "texture slots before export", value=len(images), limit=0))
    for k, image in enumerate(images):
        fmt = _image_format(glb, image)
        where = {"image": image.get("name", k)}
        if fmt == "jpeg" and opts["jpeg"] != "off":
            out.append(R.finding("jpeg", opts["jpeg"], f"image {image.get('name', k)} is JPEG",
                                 "re-export as PNG (JPEG blocks alpha and adds ringing on flat colour); "
                                 "--jpeg info accepts it", where))
        elif opts["textures"] == "png" and fmt != "png":
            out.append(R.finding("textures", "medium", f"image {image.get('name', k)} is {fmt}, not PNG",
                                 "re-export the image as PNG", where))


def check_primitives(glb, opts, out, unreadable=frozenset()):
    """Primitives whose accessors were already reported unreadable are skipped here."""
    doc = glb.json
    for m, mesh in enumerate(doc.get("meshes", [])):
        for p, prim in enumerate(mesh.get("primitives", [])):
            where = {"mesh": mesh.get("name", m), "primitive": p}
            attrs = prim.get("attributes", {})
            if unreadable & (set(attrs.values()) | {prim.get("indices")}):
                continue
            mode = prim.get("mode", G.TRIANGLES)
            if mode != G.TRIANGLES:
                out.append(R.finding("mode", "medium", f"primitive mode {mode} is not a triangle list",
                                     "triangulate and export as triangles", where, mode, G.TRIANGLES))
            if "POSITION" not in attrs:
                out.append(R.finding("position", "high", "primitive has no POSITION", "re-export", where))
                continue
            count = doc["accessors"][attrs["POSITION"]]["count"]
            if "indices" in prim:
                try:
                    flat = glb.accessor(prim["indices"], normalize=False)[:, 0]
                except (G.GlbError, ValueError):
                    continue
                if mode == G.TRIANGLES and len(flat) % 3:
                    out.append(R.finding("indices", "high", f"{len(flat)} indices is not a multiple of 3",
                                         "re-export", where))
                if len(flat) and (flat.max() >= count or flat.min() < 0):
                    out.append(R.finding("indices", "high", f"index {int(flat.max())} past {count} vertices",
                                         "re-export", where))
            if "NORMAL" in attrs:
                lengths = np.linalg.norm(glb.accessor(attrs["NORMAL"]), axis=1)
                off = np.abs(lengths - 1.0) > opts["normal_tolerance"]
                if off.any():
                    out.append(R.finding("normals", "medium", f"{int(off.sum())} normals are not unit length "
                                         f"(range {lengths.min():.3f}..{lengths.max():.3f})",
                                         "recompute normals, or normalise on export", where,
                                         int(off.sum()), 0))
            if "COLOR_0" in attrs and opts["color_encoding"] != "any":
                acc = doc["accessors"][attrs["COLOR_0"]]
                want = COLOR_ENCODINGS[opts["color_encoding"]]
                if (acc["componentType"], bool(acc.get("normalized"))) not in want:
                    out.append(R.finding("color_encoding", "medium",
                                         f"COLOR_0 is componentType {acc['componentType']}, the contract wants "
                                         f"{opts['color_encoding']}", "re-export the colour attribute in the "
                                         "contract's encoding", where))
            if "COLOR_0" in attrs and str(opts["color_components"]) != "any":
                kind = doc["accessors"][attrs["COLOR_0"]]["type"]
                if kind != f"VEC{opts['color_components']}":
                    out.append(R.finding("color_components", "medium",
                                         f"COLOR_0 is {kind}, the contract wants {opts['color_components']} components",
                                         "re-export the colour attribute with alpha (RGBA)"
                                         if str(opts["color_components"]) == "4" else
                                         "re-export the colour attribute as RGB", where))


def check_nodes(glb, opts, out):
    moved, flipped, collapsed = [], [], []
    joints = {j for skin in glb.json.get("skins", []) for j in skin.get("joints", [])}
    for i, node in enumerate(glb.nodes):
        t, r, s = glb.local_trs(i)
        # Sockets and anchors (nodes without a mesh) exist to carry a transform.
        if opts["identity_nodes"] and i not in joints and "mesh" in node and not (
                np.allclose(t, 0, atol=1e-7) and np.allclose(s, 1, atol=1e-7) and G.quat_angle_deg(r, [0, 0, 0, 1]) < 1e-4):
            moved.append(glb.name(i))
        if np.any(np.abs(s) < 1e-12):
            collapsed.append(glb.name(i))
        elif np.prod(np.sign(s)) < 0:  # two negative axes are a rotation, not a mirror
            flipped.append(glb.name(i))
    if moved:
        out.append(R.finding("node_transform", "low", f"{len(moved)} mesh nodes carry a transform; many engines "
                             "drop node transforms when they load a static mesh, so check it draws where intended",
                             "apply transforms before export (Blender: Ctrl+A > All Transforms)",
                             {"nodes": moved[:SAMPLES]}, len(moved), 0))
    if collapsed:
        out.append(R.finding("node_scale", "high", f"{len(collapsed)} nodes have zero scale",
                             "fix the scale or remove the node", {"nodes": collapsed[:SAMPLES]}))
    if flipped:
        out.append(R.finding("node_scale", "medium", f"{len(flipped)} nodes are mirrored (odd number of negative scale axes)",
                             "apply the scale so the mesh winding is baked, then recalculate normals",
                             {"nodes": flipped[:SAMPLES]}))


def bounds(glb, points=None):
    points = np.vstack([p.positions for p in glb.parts()] or [np.zeros((0, 3))]) if points is None else points
    if not len(points):
        return None
    lo, hi = points.min(axis=0), points.max(axis=0)
    return lo, hi, (lo + hi) / 2


def check_pivot(glb, opts, out, metrics):
    points = np.vstack([p.positions for p in glb.parts()] or [np.zeros((0, 3))])
    box = bounds(glb, points)
    if box is None:
        return
    lo, hi, centre = box
    tol = opts["pivot_tolerance"]
    metrics.update(bounds_min=_r(lo), bounds_max=_r(hi), bounds_centre=_r(centre), size=_r(hi - lo))
    rule = opts["pivot"]
    horizontal = float(np.hypot(centre[0], centre[2]))
    vertical = {"floor": lo[1], "floor-center": lo[1], "top": hi[1], "center": centre[1]}.get(rule)
    hint_h = ("centre the origin under the model, or place it by its origin everywhere: never by the "
              "bounds or collider centre")
    if rule != "any" and abs(vertical) > tol:
        edge = {"floor": "bottom", "floor-center": "bottom", "top": "top", "center": "bounds centre"}[rule]
        # Under a centimetre a prop floats or sinks by a pixel or two at game distance. Past its mount
        # (below the floor, above the ceiling) only a buried share of the model shows: a plug prong
        # under the floor or a hook in the ceiling does not.
        into = {"floor": -1, "floor-center": -1, "top": 1}.get(rule, 0) * vertical > 0
        buried = float(np.mean(points[:, 1] * (1 if rule == "top" else -1) > VISIBLE_OFFSET / 2)) if into else 0.0
        seen = abs(vertical) >= VISIBLE_OFFSET and (not into or buried > 0.1)
        out.append(R.finding("pivot", "medium" if seen else "low",
                             f"{edge} is at y {vertical:+.4f}, the {rule} pivot wants 0",
                             f"move the origin to the {rule} point (set origin, apply) or move the mesh",
                             {"bounds_min": _r(lo), "bounds_max": _r(hi)}, round(float(vertical), 4), tol))
    # Only floor-center and center promise a centred origin; an asymmetric model (a cart, a wall
    # lamp) is centred by its maker's choice, so elsewhere the offset is a placement note.
    judged = rule in ("floor-center", "center")
    if horizontal > tol and judged:
        out.append(R.finding("pivot_horizontal", "info",
                             f"origin is {centre[0]:+.4f} x, {centre[2]:+.4f} z from the bounds centre",
                             hint_h, {"bounds_centre": _r(centre)}, round(horizontal, 4), tol))
    elif horizontal > tol or rule == "any" and abs(lo[1]) > tol:
        out.append(R.finding("origin_offset", "info",
                             f"bounds centre sits at ({centre[0]:+.3f}, {centre[1]:+.3f}, {centre[2]:+.3f}) "
                             f"from the origin; bottom at y {lo[1]:+.3f}", hint_h,
                             {"bounds_centre": _r(centre), "bounds_min": _r(lo)}))


def _seam_tears(positions, joints, weights, weld, tolerance):
    """Vertices that share a position with another but carry different weights: the seam opens under motion."""
    keys = np.round(positions / weld)
    _, group, counts = np.unique(keys, axis=0, return_inverse=True, return_counts=True)
    group = group.reshape(-1)
    shared = np.flatnonzero(counts[group] > 1)
    if not len(shared):
        return 0, 0.0
    j, w = joints[shared], weights[shared]
    width = int(j.max()) + 1
    dense = np.zeros((len(shared), width))
    np.add.at(dense, (np.repeat(np.arange(len(shared)), j.shape[1]), j.reshape(-1)), w.reshape(-1))
    order = np.argsort(group[shared], kind="stable")
    g = group[shared][order]
    dense = dense[order]
    first = np.r_[0, np.flatnonzero(np.diff(g)) + 1]
    reference = dense[np.repeat(first, np.diff(np.r_[first, len(g)]))]
    diff = np.abs(dense - reference).max(axis=1)
    return int(np.sum(diff > tolerance)), float(diff.max())


def _bind_spread(glb, skin, world):
    """(metres, joint) of the largest disagreement between joints about the mesh-to-world transform
    at rest. Rest equals bind when every joint world x inverse bind is one and the same matrix; that
    matrix need not be identity (a centimetre mesh under a 0.01 armature carries the scale in it)."""
    palette = glb.skin_palette(world, skin)
    points = [glb.accessor(prim["attributes"]["POSITION"])
              for node in glb.nodes if node.get("skin") == skin and "mesh" in node
              for prim in glb.json["meshes"][node["mesh"]]["primitives"] if "POSITION" in prim["attributes"]]
    points = np.vstack(points) if points else np.zeros((1, 3))
    lo, hi = points.min(axis=0), points.max(axis=0)
    corners = np.array([[x, y, z, 1.0] for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])])
    placed = np.einsum("jab,cb->jca", palette, corners)[..., :3]
    spread = np.linalg.norm(placed - np.median(placed, axis=0), axis=2).max(axis=1)
    worst = int(np.argmax(spread))
    return float(spread[worst]), glb.name(glb.json["skins"][skin]["joints"][worst])


def check_skins(glb, opts, out, metrics):
    doc = glb.json
    skins = doc.get("skins", [])
    if not skins:
        return
    world = glb.world_rest()
    metrics["joints"] = [len(s["joints"]) for s in skins]
    for k, skin in enumerate(skins):
        if opts["max_joints"] is not None and len(skin["joints"]) > opts["max_joints"]:
            out.append(R.finding("joint_count", "medium", f"skin {k} has {len(skin['joints'])} joints",
                                 "remove helper/leaf bones the engine does not need", {"skin": k},
                                 len(skin["joints"]), opts["max_joints"]))
        missing = [glb.name(j) for j in skin["joints"] if j not in world]
        if missing:
            out.append(R.finding("joints_in_scene", "high", f"skin {k}: {len(missing)} joints not in the scene",
                                 "export the armature with the mesh", {"joints": missing[:SAMPLES]}))
            continue
        error, joint = _bind_spread(glb, k, world)
        metrics.setdefault("bind_error_m", []).append(round(error, 7))
        if error > opts["bind_tolerance"]:
            out.append(R.finding("bind_pose", "medium",
                                 f"skin {k}: at rest the joints disagree on where the mesh is by {error:.2e} m "
                                 f"(worst {joint}); the unanimated model deforms",
                                 "apply the rest pose as bind pose before export", {"skin": k, "joint": joint},
                                 round(error, 7), opts["bind_tolerance"]))
    for i, node in enumerate(glb.nodes):
        if "skin" not in node or "mesh" not in node:
            continue
        joint_total = len(skins[node["skin"]]["joints"])
        streams = []
        for p, prim in enumerate(doc["meshes"][node["mesh"]]["primitives"]):
            where = {"node": glb.name(i), "primitive": p}
            joints, weights = glb.skin_streams(prim)
            if joints is None:
                out.append(R.finding("skin_streams", "high", "skinned primitive has no JOINTS_0/WEIGHTS_0",
                                     "weight the mesh to the armature before export", where))
                continue
            live = weights > opts["influence_epsilon"]
            outside = live & ((joints >= joint_total) | (joints < 0))
            if outside.any():
                out.append(R.finding("joint_index", "high", f"{int(outside.any(axis=1).sum())} vertices "
                                     f"reference joints past the skin's {joint_total}", "re-export the skin", where))
            ordered = -np.sort(-weights, axis=1)
            dropped = ordered[:, opts["max_influences"]:]
            over = (dropped > opts["influence_epsilon"]).any(axis=1) if dropped.shape[1] else np.zeros(len(weights), bool)
            if over.any():
                out.append(R.finding("influences", "medium",
                                     f"{int(over.sum())} vertices have more than {opts['max_influences']} "
                                     f"influences (largest dropped weight {dropped.max():.3f})",
                                     "limit total weights to 4 and normalise (Blender: Weights > Limit Total, "
                                     "Normalize All)", where, int(over.sum()), 0))
            sums = weights.sum(axis=1)
            unweighted = sums <= opts["influence_epsilon"]
            if unweighted.any():
                out.append(R.finding("unweighted", "high", f"{int(unweighted.sum())} vertices have no weight "
                                     "(they collapse to the origin when skinned)",
                                     "weight every vertex (Blender: select ungrouped verts, assign)", where,
                                     int(unweighted.sum()), 0))
            off = ~unweighted & (np.abs(sums - 1.0) > opts["weight_sum_tolerance"])
            if off.any():
                out.append(R.finding("weight_sum", "medium", f"{int(off.sum())} vertices have weights summing "
                                     f"to {sums[off].min():.3f}..{sums[off].max():.3f}",
                                     "normalise all weights before export", where, int(off.sum()), 0))
            streams.append((glb.accessor(prim["attributes"]["POSITION"]), joints, weights))
        if streams:
            width = max(j.shape[1] for _, j, _ in streams)
            pad = [np.pad(a, ((0, 0), (0, width - a.shape[1]))) for _, j, w in streams for a in (j, w)]
            torn, worst = _seam_tears(np.vstack([p for p, _, _ in streams]), np.vstack(pad[0::2]),
                                      np.vstack(pad[1::2]), opts["weld"], opts["seam_weight_tolerance"])
            if torn:
                out.append(R.finding("seam_weights", "medium", f"{torn} seam vertices differ in weight from their "
                                     f"twin at the same position (up to {worst:.3f}); the seam opens under motion",
                                     "weld, transfer weights across the seam, then split again",
                                     {"node": glb.name(i)}, torn, 0))


def _rig_nodes(glb):
    keep = set()
    for skin in glb.json.get("skins", []):
        keep.update(skin["joints"])
    return keep or set(range(len(glb.nodes)))


def check_reference_rig(glb, opts, out, metrics):
    ref = opts["reference_rig"]
    ref = ref if isinstance(ref, G.Glb) else G.Glb.load(ref)
    missing, parents, moved = [], [], []
    worst_t = worst_r = 0.0
    # exact and superset: every reference joint must be in the model; subset: every model joint must
    # be in the reference (a look that skins fewer joints). Shared joints must match either way.
    subset = opts["reference_rig_mode"] == "subset"
    for k in sorted(_rig_nodes(glb if subset else ref)):
        name = (glb if subset else ref).name(k)
        if name not in (ref if subset else glb).by_name:
            missing.append(name)
            continue
        i, j = (ref.by_name[name], k) if subset else (k, glb.by_name[name])
        parent_ref = ref.name(ref.parent[i]) if i in ref.parent else None
        parent_own = glb.name(glb.parent[j]) if j in glb.parent else None
        if parent_ref != parent_own:
            parents.append(f"{name}: {parent_own} != {parent_ref}")
        (ta, ra, _), (tb, rb, _) = ref.local_trs(i), glb.local_trs(j)
        dt, dr = float(np.linalg.norm(ta - tb)), G.quat_angle_deg(ra, rb)
        worst_t, worst_r = max(worst_t, dt), max(worst_r, dr)
        if dt > opts["rig_translation_tolerance"] or dr > opts["rig_rotation_tolerance_deg"]:
            moved.append(f"{name}: {dt * 1000:.3f} mm, {dr:.4f} deg")
    metrics["reference_rig"] = {"asset": ref.label, "max_translation_m": round(worst_t, 7),
                                "max_rotation_deg": round(worst_r, 5)}
    for check, items, message, hint in (
            ("rig_missing", missing, "joints not in the reference rig" if subset else "reference joints missing",
             "use the reference armature's joints only" if subset else "export the full reference armature"),
            ("rig_parent", parents, "joints hang from a different parent", "match the reference hierarchy"),
            ("rig_rest", moved, "joints differ from the reference rest pose",
             "rebuild on the reference armature; clips made for it will not fit otherwise")):
        if items:
            out.append(R.finding(check, "high", f"{len(items)} {message}", hint, {"joints": items[:SAMPLES]},
                                 len(items), 0))


def check(path_or_glb, **options):
    opts = R.resolve_options(TOOL, DEFAULTS, overrides=options)
    for key, allowed in (("pivot", PIVOTS), ("textures", ("any", "none", "png")),
                         ("color_encoding", ("any",) + tuple(COLOR_ENCODINGS)), ("jpeg", R.SEVERITIES + ("off",)),
                         ("color_components", ("any", "3", "4")),
                         ("reference_rig_mode", ("exact", "subset", "superset"))):
        if (str(opts[key]) if key == "color_components" else opts[key]) not in allowed:
            raise ValueError(f"{TOOL}: {key} must be one of {allowed}, not {opts[key]!r}")
    try:
        glb = path_or_glb if isinstance(path_or_glb, G.Glb) else G.Glb.load(path_or_glb)
    except Exception as error:  # noqa: BLE001 - reported as a finding
        return R.unreadable(TOOL, path_or_glb, error)
    findings, metrics = [], {"nodes": len(glb.nodes), "meshes": len(glb.json.get("meshes", [])),
                             "images": len(glb.json.get("images", [])), "skins": len(glb.json.get("skins", [])),
                             "animations": len(glb.json.get("animations", []))}
    bad = R.guarded(findings, "data", check_data, glb, opts, findings) or set()
    R.guarded(findings, "images", check_images, glb, opts, findings)
    R.guarded(findings, "primitives", check_primitives, glb, opts, findings, bad)
    R.guarded(findings, "nodes", check_nodes, glb, opts, findings)
    if not bad and not any(f["severity"] == "high" and f["check"] in ("indices", "check_error") for f in findings):
        R.guarded(findings, "pivot", check_pivot, glb, opts, findings, metrics)
        R.guarded(findings, "skins", check_skins, glb, opts, findings, metrics)
    if opts["reference_rig"] is not None:
        R.guarded(findings, "reference rig", check_reference_rig, glb, opts, findings, metrics)
    shown = {k: (v.label if isinstance(v, G.Glb) else v) for k, v in opts.items()}
    return R.make_report(TOOL, glb.label, findings, metrics, shown, G.sha256(glb.path) if glb.path else None)


def _configure(parser):
    parser.add_argument("models", nargs="+", help="GLB files")
    parser.add_argument("--pivot", choices=PIVOTS, help="floor and top judge height only; floor-center and "
                        "center also want the origin on the bounds centre")
    parser.add_argument("--pivot-tolerance", type=float, help="metres (default 0.001)")
    parser.add_argument("--textures", choices=("any", "none", "png"))
    parser.add_argument("--jpeg", choices=R.SEVERITIES + ("off",), help="severity of JPEG images (default low)")
    parser.add_argument("--color-encoding", choices=("any",) + tuple(COLOR_ENCODINGS))
    parser.add_argument("--color-components", choices=("any", "3", "4"), help="COLOR_0 width (default any)")
    parser.add_argument("--identity-nodes", action="store_true", default=None)
    parser.add_argument("--normal-tolerance", type=float)
    parser.add_argument("--max-joints", type=int)
    parser.add_argument("--max-influences", type=int)
    parser.add_argument("--bind-tolerance", type=float)
    parser.add_argument("--reference-rig", help="GLB whose skin joints this rig must match")
    parser.add_argument("--reference-rig-mode", choices=("exact", "subset", "superset"),
                        help="subset: the model's joints must be in the reference; superset/exact: every "
                             "reference joint must be in the model (default exact)")
    parser.add_argument("--rig-translation-tolerance", type=float)
    parser.add_argument("--rig-rotation-tolerance-deg", type=float)


def _run(args, config):
    opts = R.resolve_options(TOOL, DEFAULTS, config, R.cli_overrides(args, DEFAULTS))
    return [check(path, **opts) for path in args.models]


main = R.cli(__doc__, _configure, _run)

if __name__ == "__main__":
    sys.exit(main())
