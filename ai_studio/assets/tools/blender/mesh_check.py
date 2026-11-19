"""Mesh topology check of a GLB: openings, non-manifold and miswound edges, degenerate and duplicate
faces, shells and inside-out volume, per mesh node, each finding with a fix hint.

    python mesh_check.py model.glb [--profile auto|volume|surface] [--weld 1e-6] [--tri-budget N] [--inward]

Profiles: `volume` expects closed solids (openings are high findings); `surface` allows openings
(reported as info); `auto` (default) treats a node that is closed after welding as a volume.
Positions are welded first, so hard-edge and UV splits are not counted as openings.
Parts drawn without back-face culling (--double-sided [GLOB...], or a glTF material with doubleSided)
report winding findings (miswound, inside-out, inverted shells, inward faces) as info only.
"""
import fnmatch
import sys
from collections import Counter
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[4]))

import numpy as np  # noqa: E402

from ai_studio.assets.tools.blender import mesh_metrics as M  # noqa: E402
from ai_studio.assets.tools.blender.lib import glb as G, rays, report as R  # noqa: E402

TOOL = "mesh_check"
DEFAULTS = {"profile": "auto", "weld": 1e-6, "area_epsilon": 1e-10, "tri_budget": None,
            "inward": False, "inward_max_faces": 20000, "scope": "node", "include": None, "double_sided": False,
            "runtime_texture": False}
# Findings about which way faces point; a part drawn without back-face culling shows both sides.
WINDING = ("miswound", "culled_faces", "inside_out", "inverted_shell", "inward")
# Per-vertex streams a repeated face is compared on: copies that differ in them can z-fight.
ATTRIBUTES = ("COLOR_0", "TEXCOORD_0", "NORMAL")
# Smallest difference that shows as flicker: 0.05 colour (13/255), 0.02 UV (20 texels of a 1k
# atlas), 0.26 between unit normals (15 degrees of shading).
VISIBLE_DIFFERENCE = {"COLOR_0": 0.05, "TEXCOORD_0": 0.02, "NORMAL": 0.26}
# Topology that draws no pixels (non-manifold edges, zero-area faces) is low unless the `volume`
# profile asks for a clean solid; winding and duplicates are judged by what the engine shows.
SEVERITY = {
    "volume": {"boundary": "high", "non_manifold": "high", "touching": "medium", "degenerate": "low",
               "inward": "medium"},
    "surface": {"boundary": "info", "non_manifold": "low", "touching": "low", "degenerate": "low", "inward": "low"},
}
# `auto` judges a part that happens to be closed as a solid, but shells built by touching or sharing
# an edge (box flaps, trim on a panel) are common in game props and render fine.
SEVERITY["auto_volume"] = {**SEVERITY["volume"], "non_manifold": "low", "touching": "info"}
SAMPLES = 5


def _r(point):
    return [round(float(v), 4) for v in point]


def boundary_loops(points, edges):
    """Openings as (edge count, centre), largest first: one entry per hole, not per edge."""
    if not len(edges):
        return []
    label = M.face_components(np.c_[edges, edges[:, 1]], len(points))
    out = []
    for k in range(label.max() + 1):
        sel = edges[label == k]
        out.append((len(sel), _r(points[sel.reshape(-1)].mean(axis=0))))
    return sorted(out, key=lambda item: -item[0])


def split_non_manifold(faces, count):
    """(touching, genuine) non-manifold edges as vertex pairs, on welded faces.

    Faces are grouped into sheets joined across two-face edges only. An edge is `touching` when
    every sheet meeting there brings exactly two faces (whole shells that meet along it); a fin,
    flap or third face makes it genuine."""
    a = np.concatenate([faces[:, 0], faces[:, 1], faces[:, 2]])
    b = np.concatenate([faces[:, 1], faces[:, 2], faces[:, 0]])
    face = np.tile(np.arange(len(faces)), 3)
    real = a != b
    key = (np.minimum(a, b) * count + np.maximum(a, b))[real]
    face = face[real]
    order = np.argsort(key, kind="stable")
    key, face = key[order], face[order]
    starts = np.r_[0, np.flatnonzero(np.diff(key)) + 1]
    sizes = np.diff(np.r_[starts, len(key)])
    parent = list(range(len(faces)))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for s in starts[sizes == 2].tolist():
        ra, rb = find(int(face[s])), find(int(face[s + 1]))
        if ra != rb:
            parent[ra] = rb
    touching, genuine = [], []
    for s, n in zip(starts[sizes > 2].tolist(), sizes[sizes > 2].tolist()):
        sheets = Counter(find(int(f)) for f in face[s:s + n])
        edge = [int(key[s] // count), int(key[s] % count)]
        (touching if len(sheets) > 1 and set(sheets.values()) == {2} else genuine).append(edge)
    return np.array(touching, np.int64).reshape(-1, 2), np.array(genuine, np.int64).reshape(-1, 2)


def inward_faces(points, faces, component, closed, limit):
    """Faces of closed shells whose normal points into their own shell (odd crossings along it).
    Three slightly spread rays vote, since a ray grazing a knife edge counts one crossing for two."""
    flagged, skipped = [], 0
    for k in np.flatnonzero(closed):
        idx = np.flatnonzero(component == k)
        if len(idx) > limit:
            skipped += 1
            continue
        sub = faces[idx]
        tris = rays.triangles(points, sub)
        normal = np.cross(tris[1] - tris[0], tris[2] - tris[0])
        length = np.linalg.norm(normal, axis=1)
        keep = length > 1e-12
        normal = normal[keep] / length[keep, None]
        centre = (tris[0] + tris[1] + tris[2])[keep] / 3.0
        helper = np.where(np.abs(normal[:, :1]) < 0.9, [[1.0, 0, 0]], [[0, 1.0, 0]])
        tangent = np.cross(normal, helper)
        tangent /= np.linalg.norm(tangent, axis=1, keepdims=True)
        votes = np.zeros(len(normal), int)
        extent = float(np.ptp(points[sub.reshape(-1)], axis=0).max()) or 1.0
        for spread in (0.0, 0.05, -0.05):
            d = normal + spread * tangent
            d /= np.linalg.norm(d, axis=1, keepdims=True)
            votes += rays.crossings(centre + d * 1e-5 * extent, d, tris) % 2
        flagged.extend(idx[keep][votes >= 2].tolist())
    return np.array(flagged, np.int64), skipped


def _inside_rest(t, k):
    """Whether shell k lies inside the rest of its part: an inward-facing shell there is a cavity."""
    rest = t["faces"][t["component"] != k]
    if not len(rest):
        return False
    probe = t["points"][t["faces"][t["component"] == k][0, 0]][None]
    return bool(rays.crossings(probe, np.array([0.3, 0.9, 0.1]) / np.linalg.norm([0.3, 0.9, 0.1]),
                               rays.triangles(t["points"], rest))[0] % 2)


def _double_sided(glb, part, option):
    """'option' or 'material' when the part is drawn without back-face culling, else None."""
    if option is True or option and any(fnmatch.fnmatchcase(n, g) for g in option
                                        for n in (part.name, glb.json["meshes"][part.mesh].get("name", ""))):
        return "option"
    material = glb.json["meshes"][part.mesh]["primitives"][part.primitive].get("material")
    if material is not None and glb.json.get("materials", [{}] * (material + 1))[material].get("doubleSided"):
        return "material"
    return None


class Group:
    """One judged mesh: world points and faces (mirrored parts already re-wound), plus what the
    engine draws with: per-vertex attributes and, per face, whether its winding disagrees with its
    stored normals (None without normals)."""

    def __init__(self, name, points, faces, double, attributes, against):
        self.name, self.points, self.faces, self.double = name, points, faces, double
        self.attributes, self.against = attributes, against


def _against_normals(glb, part):
    """Per face: its winding points away from its vertex normals. Judged in mesh space, where the
    engine's transform cannot change the answer; None without a NORMAL stream."""
    if "NORMAL" not in part.attributes:
        return None
    pos = glb.accessor(part.attributes["POSITION"])
    normals = glb.accessor(part.attributes["NORMAL"])
    if len(normals) != len(pos) or not len(part.faces):
        return None
    f = part.faces
    winding = np.cross(pos[f[:, 1]] - pos[f[:, 0]], pos[f[:, 2]] - pos[f[:, 0]])
    shading = normals[f].sum(axis=1)
    # Clearly opposite only (over 120 degrees): foliage cards often carry normals bent up on purpose.
    lengths = np.linalg.norm(winding, axis=1) * np.linalg.norm(shading, axis=1)
    return np.einsum("ij,ij->i", winding, shading) < -0.5 * lengths


def _textured(glb, part):
    material = glb.json["meshes"][part.mesh]["primitives"][part.primitive].get("material")
    if material is None:
        return False
    m = glb.json.get("materials", [])[material]
    return any(k.endswith("Texture") for k in (*m, *m.get("pbrMetallicRoughness", {})))


def _groups(glb, scope, include, double_sided=False, runtime_texture=False):
    parts = glb.parts(include=include)
    groups = {}
    for part in parts:
        groups.setdefault("<file>" if scope == "file" else part.name, []).append(part)
    out = []
    for name, members in groups.items():
        base, faces = 0, []
        for part in members:
            # glTF reverses the winding of a node whose transform mirrors (negative determinant).
            mirrored = np.linalg.det(part.world[:3, :3]) < 0
            faces.append((part.faces[:, ::-1] if mirrored else part.faces) + base)
            base += len(part.positions)
        # A group is double-sided only when every part of it is; one culled part shows its winding.
        sides = [_double_sided(glb, p, double_sided) for p in members]
        double = None if None in sides else "option" if "option" in sides else "material"
        attributes = {}
        for key in ATTRIBUTES:
            # UVs only show through a texture: the material's, or one the game binds (runtime_texture).
            if key == "TEXCOORD_0" and not (runtime_texture or all(_textured(glb, p) for p in members)):
                continue
            if all(key in p.attributes for p in members):
                arrays = [glb.accessor(p.attributes[key]).reshape(len(p.positions), -1) for p in members]
                if len({a.shape[1] for a in arrays}) == 1:
                    attributes[key] = np.vstack(arrays)
        against = [_against_normals(glb, p) for p in members]
        against = None if any(a is None for a in against) else np.concatenate(against)
        out.append(Group(name, np.vstack([p.positions for p in members]), np.vstack(faces), double,
                         attributes, against))
    return out


def miswound_edges(points, faces):
    """(edges, coplanar) of two-face edges that both faces run the same way. `coplanar` marks pairs
    lying in one plane with opposite normals, as the halves of a two-sided card do; whether such a
    flip shows is left to culled_share."""
    a = np.concatenate([faces[:, 0], faces[:, 1], faces[:, 2]])
    b = np.concatenate([faces[:, 1], faces[:, 2], faces[:, 0]])
    face = np.tile(np.arange(len(faces)), 3)
    real = a != b
    a, b, face = a[real], b[real], face[real]
    key = np.minimum(a, b) * len(points) + np.maximum(a, b)
    order = np.argsort(key, kind="stable")
    a, b, face, key = a[order], b[order], face[order], key[order]
    starts = np.r_[0, np.flatnonzero(np.diff(key)) + 1]
    sizes = np.diff(np.r_[starts, len(key)])
    pair = starts[sizes == 2]
    pair = pair[a[pair] == a[pair + 1]]
    edges = np.stack([a[pair], b[pair]], axis=1)
    if not len(pair):
        return edges.reshape(-1, 2), np.zeros(0, bool)
    f1, f2 = faces[face[pair]], faces[face[pair + 1]]
    p = points
    n1 = np.cross(p[f1[:, 1]] - p[f1[:, 0]], p[f1[:, 2]] - p[f1[:, 0]])
    n2 = np.cross(p[f2[:, 1]] - p[f2[:, 0]], p[f2[:, 2]] - p[f2[:, 0]])
    lengths = np.linalg.norm(n1, axis=1) * np.linalg.norm(n2, axis=1)
    return edges, np.einsum("ij,ij->i", n1, n2) < -0.999 * np.where(lengths > 0, lengths, np.inf)


def duplicate_groups(faces, attributes):
    """(copies, differing): how many faces repeat another face with the same winding, and a mask of
    those copies that carry a visibly different colour, UV or normal (the ones that can z-fight). `faces` are welded; `attributes` hold per-vertex arrays of the unwelded `raw` faces."""
    welded, raw = faces
    if not len(welded):
        return 0, np.zeros(len(welded), bool)
    order = np.argsort(welded, axis=1)
    key = np.take_along_axis(welded, order, axis=1)
    start = np.argmin(welded, axis=1)
    rolled = welded[np.arange(len(welded))[:, None], (start[:, None] + np.arange(3)) % 3]
    orient = (rolled[:, 1] < rolled[:, 2]).astype(np.int64)
    _, first, group = np.unique(np.c_[key, orient], axis=0, return_index=True, return_inverse=True)
    group = group.reshape(-1)
    copy = np.arange(len(welded)) != first[group]
    if not copy.any():
        return 0, np.zeros(len(welded), bool)
    corners = np.take_along_axis(raw, order, axis=1)
    differs = np.zeros(len(welded), bool)
    for key, values in attributes.items():
        tolerance = VISIBLE_DIFFERENCE[key]
        v = values[corners]
        differs |= np.abs(v - v[first[group]]).reshape(len(welded), -1).max(axis=1) > tolerance
    return int(copy.sum()), copy & differs


def reversed_twins(faces):
    """Faces that have a copy with the opposite winding (same welded corners)."""
    if not len(faces):
        return np.zeros(0, bool)
    start = np.argmin(faces, axis=1)
    rolled = faces[np.arange(len(faces))[:, None], (start[:, None] + np.arange(3)) % 3]
    orient = rolled[:, 1] < rolled[:, 2]
    _, group = np.unique(np.sort(faces, axis=1), axis=0, return_inverse=True)
    group = group.reshape(-1)
    sides = np.zeros((group.max() + 1, 2), bool)
    sides[group, orient.astype(int)] = True
    return sides.all(axis=1)[group]


def _trivial_volume(points, volume):
    """A shell whose |volume| is a sliver of its bounding box: back-to-back twin sheets, not a solid."""
    size = np.ptp(points, axis=0)
    box = float(np.prod(size))
    return box <= 1e-12 or abs(volume) < 0.01 * box


def winding_severity(share, whole_inside_out):
    """Severity by what back-face culling changes on screen (share of the silhouette). On reviewed
    renders a visibly broken model changed 13-20 % of its pixels, unnoticed ones under 2 %."""
    if whole_inside_out or share > 0.05:
        return "high"
    return "medium" if share > 0.02 else "low"


def flipped_patches(points, faces, component, miswound):
    """Faces wound against their neighbours: at each miswound edge, the smaller (by area) of the two
    patches meeting there, a patch being faces joined across consistently wound edges. Patches cut
    apart only by open or 3-face edges are not flips and are left alone."""
    if not len(miswound):
        return np.zeros(len(faces), bool)
    a = np.concatenate([faces[:, 0], faces[:, 1], faces[:, 2]])
    b = np.concatenate([faces[:, 1], faces[:, 2], faces[:, 0]])
    face = np.tile(np.arange(len(faces)), 3)
    real = a != b
    a, b, face = a[real], b[real], face[real]
    key = np.minimum(a, b) * len(points) + np.maximum(a, b)
    order = np.argsort(key, kind="stable")
    a, face, key = a[order], face[order], key[order]
    starts = np.r_[0, np.flatnonzero(np.diff(key)) + 1]
    sizes = np.diff(np.r_[starts, len(key)])
    two = starts[sizes == 2]
    pair, wrong = two[a[two] != a[two + 1]], two[a[two] == a[two + 1]]
    ids = np.arange(len(faces))
    patch = M.face_components(np.vstack([np.c_[face[pair], face[pair + 1], face[pair + 1]], np.c_[ids, ids, ids]]),
                              len(faces))[len(pair):]
    area = 0.5 * np.linalg.norm(np.cross(points[faces[:, 1]] - points[faces[:, 0]],
                                         points[faces[:, 2]] - points[faces[:, 0]]), axis=1)
    patch_area = np.bincount(patch, area)
    p1, p2 = patch[face[wrong]], patch[face[wrong + 1]]
    split = p1 != p2
    smaller = np.where(patch_area[p1] <= patch_area[p2], p1, p2)[split]
    return np.isin(patch, smaller)


# Game-like views: four sides from 30 degrees above.
VIEWS = [np.array([-np.cos(np.radians(30)) * np.sin(y), -np.sin(np.radians(30)), -np.cos(np.radians(30)) * np.cos(y)])
         for y in np.radians([0, 90, 180, 270])]


def culled_share(points, faces, suspect, component, grid=40):
    """Mean share of the silhouette that back-face culling changes, over VIEWS: samples whose nearest
    face is a suspect turned away from the camera, and whose next drawn face is another shell, nothing,
    or the same shell more than half its size behind (a hollow seen from inside). The far wall of a
    thin inside-out shell sits right behind its culled near wall and looks the same, so it does not
    count. Only suspect faces count, so an open sheet seen from behind is not taken for a defect.
    Calibrated on Workbench cull/no-cull renders of reviewed models (within a few % of their pixels)."""
    if not suspect.any():
        return 0.0
    a, b, c = rays.triangles(points, faces)
    normal = np.cross(b - a, c - a)
    diag = float(np.linalg.norm(np.ptp(points, axis=0))) or 1.0
    shell_size = np.zeros(component.max() + 1 if len(component) else 0)
    for k in np.unique(component[suspect]):
        shell_size[k] = np.linalg.norm(np.ptp(points[faces[component == k].reshape(-1)], axis=0))
    shares = []
    for d in VIEWS:
        u = np.cross(d, [0.0, 1.0, 0.0])
        u /= np.linalg.norm(u)
        v = np.cross(u, d)
        silhouette = 0.5 * np.clip(-(normal @ d), 0, None).sum()
        corners = points[faces[suspect].reshape(-1)]
        lo = np.r_[(corners @ u).min(), (corners @ v).min()]
        hi = np.r_[(corners @ u).max(), (corners @ v).max()]
        step = max(float((hi - lo).max()) / grid, diag / 1000)
        xs = np.arange(lo[0] + step / 2, hi[0], step)
        ys = np.arange(lo[1] + step / 2, hi[1], step)
        if silhouette <= 0 or not len(xs) or not len(ys):
            shares.append(0.0)
            continue
        uv = np.stack([np.stack([a @ u, b @ u, c @ u], axis=1), np.stack([a @ v, b @ v, c @ v], axis=1)], axis=2)
        depth = np.stack([a @ d, b @ d, c @ d], axis=1)
        t, hit = (x.reshape(-1) for x in rays.zbuffer(uv, depth, xs, ys))
        hit_face = np.maximum(hit, 0)
        culled = (hit >= 0) & suspect[hit_face] & (normal[hit_face] @ d > 0)
        if not culled.any():
            shares.append(0.0)
            continue
        front = np.flatnonzero((normal @ d) < 0)
        behind, revealed = (x.reshape(-1)[culled] for x in rays.zbuffer(uv[front], depth[front], xs, ys))
        own = component[hit_face[culled]]
        shown = component[front[np.maximum(revealed, 0)]] if len(front) else np.full(len(own), -1)
        same = (revealed >= 0) & (shown == own)
        gap = behind - t[culled]
        changed = int(np.sum(np.where(same, gap > 0.5 * shell_size[own], gap > 0.02 * diag)))
        shares.append(min(1.0, changed * step * step / silhouette))
    return float(np.mean(shares))


def check_part(group, opts):
    name, points, faces, double = group.name, group.points, group.faces, group.double
    findings = []
    t = M.triangle_topology(points, faces, weld_tolerance=opts["weld"], area_epsilon=opts["area_epsilon"])
    closed = t["boundary_edges"] == 0 and t["triangles"] > 0
    profile = opts["profile"] if opts["profile"] != "auto" else ("volume" if closed else "surface")
    sev = SEVERITY["auto_volume" if opts["profile"] == "auto" and closed else profile]
    where = {"part": name}
    loops = boundary_loops(t["points"], t["boundary"])
    wf, wp = t["faces"], t["points"]
    area = 0.5 * np.linalg.norm(np.cross(wp[wf[:, 1]] - wp[wf[:, 0]], wp[wf[:, 2]] - wp[wf[:, 0]]), axis=1)
    degenerate = area <= opts["area_epsilon"]
    # Non-manifold edges after the faces that draw nothing extra are gone: zero-area faces, exact
    # repeats and the reversed twin of a two-sided sheet.
    _, keep = np.unique(np.sort(wf[~degenerate], axis=1), axis=0, return_index=True)
    touching, genuine = split_non_manifold(wf[~degenerate][np.sort(keep)], len(wp))
    edges, folded = miswound_edges(wp, wf)
    miswound = edges[~folded]
    copies, fighting = duplicate_groups((wf[~degenerate], faces[~degenerate]), group.attributes)  # slivers draw nothing
    zfight = int(fighting.sum())
    zfight_share = float(area[~degenerate][fighting].sum()) / (float(area.sum()) or 1.0)
    row = {k: t[k] for k in ("triangles", "vertices", "welded_vertices", "boundary_edges", "degenerate_faces",
                             "double_sided_faces", "components", "open_components")}
    row.update(part=name, profile=profile, boundary_loops=len(loops), non_manifold_edges=len(genuine),
               touching_edges=len(touching), miswound_edges=len(miswound), coplanar_flip_edges=int(folded.sum()),
               duplicate_faces=copies, zfight_faces=zfight,
               signed_volume=round(t["signed_volume"], 6) if closed else None)

    if t["boundary_edges"]:
        findings.append(R.finding(
            "boundary", sev["boundary"],
            f"{name}: {t['boundary_edges']} open edges in {len(loops)} opening(s) after welding at {opts['weld']}",
            f"weld vertices (merge by distance) or fill the holes on {name}; if it is meant to be open, "
            "use --profile surface", {**where, "openings": [{"edges": e, "centre": c} for e, c in loops[:SAMPLES]]},
            t["boundary_edges"], 0))
    for kind, found, text, hint in (
            ("non_manifold", genuine, "edges shared by 3+ faces of one sheet (no visible effect)",
             f"split the extra faces off those edges, or delete them, on {name}"),
            ("touching", touching, "edges where separate shells meet",
             f"fine for a game prop; for a clean solid on {name}, offset the shells or merge them")):
        if len(found):
            findings.append(R.finding(kind, sev[kind], f"{name}: {len(found)} {text}", hint, {
                **where, "edges": [_r(wp[e].mean(axis=0)) for e in found[:SAMPLES]]}, len(found), 0))

    # Winding is judged by what back-face culling changes on screen: faces wound against their
    # normals or against the rest of their shell, and closed shells turned inside out.
    suspect = np.zeros(len(wf), bool) if group.against is None else group.against & ~degenerate
    # Only a closed shell has an outside to be wrong about; an open sheet's facing (a level wall
    # facing the arena) is the author's choice and is judged by its normals alone.
    suspect |= flipped_patches(wp, wf, t["component"], edges) & ~t["component_open"][t["component"]]
    # A face with a reversed twin is one double-sided surface: one of the two always faces the camera.
    suspect &= ~reversed_twins(wf)
    inside_out = closed and t["signed_volume"] < 0 and not _trivial_volume(wp, t["signed_volume"])
    # An explicit surface profile drops the solid reading (an inverted-hull outline is inside out on purpose).
    surface = opts["profile"] == "surface"
    inverted, cavity = [], []
    if not inside_out:
        for k in np.flatnonzero(~t["component_open"] & (t["component_volume"] < 0)):
            shell = t["component"] == k
            if _trivial_volume(wp[wf[shell].reshape(-1)], t["component_volume"][k]):
                continue
            (cavity if _inside_rest(t, k) else inverted).append(k)
        for k in inverted:
            suspect |= t["component"] == k
    share = 0.0 if double or inside_out else culled_share(wp, wf, suspect & ~degenerate, t["component"])
    level = winding_severity(share, inside_out and not surface)
    impact = f"culling changes {100 * share:.1f}% of its silhouette from game-like views"
    row["culled_silhouette_pct"] = round(100 * share, 2)
    if len(miswound):
        findings.append(R.finding(
            "miswound", level, f"{name}: {len(miswound)} edges where neighbouring faces wind the same way "
            f"(flipped faces); {impact}",
            f"recalculate normals outside on {name} (Blender: Mesh > Normals > Recalculate Outside)",
            {**where, "edges": [_r((wp[i] + wp[j]) / 2) for i, j in miswound[:SAMPLES]]}, len(miswound), 0))
    elif share > 0:
        findings.append(R.finding(
            "culled_faces", level, f"{name}: faces wound against their normals or neighbours; {impact}",
            f"recalculate normals outside on {name}, or flip those faces", where, round(100 * share, 2), 2))
    if folded.any():
        findings.append(R.finding("coplanar_flip", "info", f"{name}: {int(folded.sum())} edges join coplanar "
                                  "faces with opposite normals (the halves of a two-sided card)", "", where,
                                  int(folded.sum())))
    if inside_out:
        findings.append(R.finding(
            "inside_out", "low" if surface else "high",
            f"{name}: closed but its volume is negative (all faces point inward)",
            f"flip normals on {name}", where, round(t["signed_volume"], 6)))
    for kind, shells, severity, text, hint in (
            ("inverted_shell", inverted, level, "closed shell(s) with negative volume (faces point inward)",
             f"flip normals of those shells on {name}"),
            ("cavity", cavity, "info", "inward-facing shell(s) inside another shell (cavities)", "")):
        if shells:
            findings.append(R.finding(kind, severity, f"{name}: {len(shells)} {text}"
                                      + (f"; {impact}" if kind == "inverted_shell" else ""), hint, {
                **where, "shell_centres": [_r(wp[wf[t["component"] == k].reshape(-1)].mean(0))
                                           for k in shells[:SAMPLES]]}, len(shells), 0))
    if t["degenerate_faces"]:
        findings.append(R.finding(
            "degenerate", sev["degenerate"], f"{name}: {t['degenerate_faces']} zero-area faces (draw nothing)",
            f"dissolve degenerate faces on {name} (Mesh > Clean Up > Degenerate Dissolve)", where,
            t["degenerate_faces"], 0))
    if t["double_sided_faces"]:
        findings.append(R.finding(
            "double_sided", "info", f"{name}: {t['double_sided_faces']} faces have a reversed twin (double-sided)",
            "", where, t["double_sided_faces"]))
    if copies:
        if zfight:
            # A sliver of flicker (a bevel strip, a rivet) is a few pixels at game distance.
            findings.append(R.finding(
                "duplicate", "medium" if zfight_share >= 0.005 else "low",
                f"{name}: {zfight} faces repeat another face with a different colour, UV or normal (z-fighting, "
                f"{100 * zfight_share:.2f}% of the surface)", f"delete the copies you do not want on {name}",
                where, zfight, 0))
        if copies > zfight:
            findings.append(R.finding(
                "duplicate", "low", f"{name}: {copies - zfight} faces repeat another face with the same look (no visible "
                "difference: cannot z-fight)", f"merge by distance and delete the duplicate faces on {name}",
                where, copies - zfight, 0))
    if opts["inward"]:
        closed_shells = ~t["component_open"]
        flagged, skipped = inward_faces(wp, wf, t["component"], closed_shells, opts["inward_max_faces"])
        row["inward_faces"] = int(len(flagged))
        if len(flagged):
            centres = wp[wf[flagged[:SAMPLES]]].mean(axis=1)
            findings.append(R.finding(
                "inward", sev["inward"], f"{name}: {len(flagged)} faces point into their own closed shell (folds)",
                f"fix folded geometry on {name}: reduce the offset/solidify thickness or recalculate normals",
                {**where, "face_centres": [_r(c) for c in centres]}, int(len(flagged)), 0))
        if skipped:
            findings.append(R.finding("inward", "info", f"{name}: {skipped} shell(s) over "
                                      f"{opts['inward_max_faces']} faces skipped by the inward test", "", where))
    if t["components"] > 1:
        findings.append(R.finding("components", "info", f"{name}: {t['components']} separate shells", "", where,
                                  t["components"]))
    if double:
        row["double_sided"] = double
        source = "the material's doubleSided flag" if double == "material" else "the double_sided option"
        for f in findings:
            if f["check"] in WINDING:
                f.update(severity="info", message=f"double-sided ({source}): winding not visible; {f['message']}")
                f.pop("hint", None)
    return findings, row


def check(path_or_glb, **options):
    opts = R.resolve_options(TOOL, DEFAULTS, overrides=options)
    if opts["profile"] not in ("auto", "volume", "surface"):
        raise ValueError(f"{TOOL}: profile must be auto, volume or surface, not {opts['profile']!r}")
    try:
        glb = path_or_glb if isinstance(path_or_glb, G.Glb) else G.Glb.load(path_or_glb)
    except Exception as error:  # noqa: BLE001 - reported as a finding
        return R.unreadable(TOOL, path_or_glb, error)
    findings, rows = [], []
    groups = R.guarded(findings, "geometry", _groups, glb, opts["scope"], opts["include"], opts["double_sided"],
                       opts["runtime_texture"])
    for group in groups or []:
        result = R.guarded(findings, f"part {group.name}", check_part, group, opts)
        if result:
            findings += result[0]
            rows.append(result[1])
    triangles = sum(r["triangles"] for r in rows)
    if not rows:
        findings.append(R.finding("empty", "high", "no mesh triangles in the scene", "export the mesh objects"))
    if opts["tri_budget"] is not None and triangles > opts["tri_budget"]:
        findings.append(R.finding("tri_budget", "low", f"{triangles} triangles over the budget (a performance figure)",
                                  "decimate, or delete faces the camera never sees", value=triangles,
                                  limit=opts["tri_budget"]))
    metrics = {"profile": opts["profile"], "parts": len(rows), "triangles": triangles,
               "vertices": sum(r["vertices"] for r in rows), "per_part": rows}
    return R.make_report(TOOL, glb.label, findings, metrics, opts,
                         G.sha256(glb.path) if glb.path else None)


def _configure(parser):
    parser.add_argument("models", nargs="+", help="GLB files")
    parser.add_argument("--profile", choices=("auto", "volume", "surface"))
    parser.add_argument("--weld", type=float, help="weld distance in metres (default 1e-6; 0 disables)")
    parser.add_argument("--area-epsilon", type=float, help="faces at or below this area are degenerate")
    parser.add_argument("--tri-budget", type=int, help="medium finding above this many triangles")
    parser.add_argument("--inward", action="store_true", default=None, help="ray-test faces pointing into their shell")
    parser.add_argument("--inward-max-faces", type=int)
    parser.add_argument("--scope", choices=("node", "file"), help="measure each mesh node, or the file as one mesh")
    parser.add_argument("--include", nargs="+", help="only these node names")
    parser.add_argument("--double-sided", nargs="*", metavar="GLOB",
                        help="parts drawn without back-face culling (all, or node/mesh name globs): "
                             "winding findings become info")


def _run(args, config):
    if args.double_sided == []:
        args.double_sided = True
    opts = R.resolve_options(TOOL, DEFAULTS, config, R.cli_overrides(args, DEFAULTS))
    return [check(path, **opts) for path in args.models]


main = R.cli(__doc__, _configure, _run)

if __name__ == "__main__":
    sys.exit(main())
