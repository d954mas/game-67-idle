"""The render model an outfit is authored on, read through the shared GLB library: its skin, its labelled cloth
regions and the poses of its skeleton.

`_CLOTH_PIECE` (float, integer values) labels the render vertices that follow a piece's sim mesh; 0 is not cloth.
Weights are dense (vertices, skin joints) in the order of the model's skin."""
import importlib.util
from pathlib import Path

import numpy as np

from garment_proxy import outer_faces, sewn_sheet

STUDIO = Path(__file__).resolve().parents[4]
LABEL = "_CLOTH_PIECE"
# Two render vertices closer than this are one position: the outfit generators split skin and cloth at a seam
# without moving either side.
COINCIDENT_M = 1e-6


def _shared_glb():
    # Loaded under its own name: game tools put another module called `glb` on the path.
    spec = importlib.util.spec_from_file_location("garment_shared_glb", STUDIO / "ai_studio/assets/tools/blender/lib/glb.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


G = _shared_glb()


def load(path):
    """A glTF file through the shared GLB library."""
    return G.Glb.load(path)


class RenderModel:
    """A skinned render model: primitives by material name, labels, rest positions and dense weights."""

    def __init__(self, path):
        self.path = Path(path)
        self.glb = load(path)
        self.joints = self.glb.skin_joints()
        self.primitives = self.glb.json["meshes"][0]["primitives"]
        if len(self.glb.json["meshes"]) != 1:
            raise ValueError(f"{path}: expected one mesh")
        self.materials = [self.glb.json["materials"][p["material"]]["name"] for p in self.primitives]
        self.positions = [self.glb.accessor(p["attributes"]["POSITION"]) for p in self.primitives]
        self.faces = [self.glb.accessor(p["indices"], False).reshape(-1, 3) for p in self.primitives]
        self.labels = [self.glb.accessor(p["attributes"][LABEL]).ravel() if LABEL in p["attributes"]
                       else np.zeros(len(x)) for p, x in zip(self.primitives, self.positions)]
        for material, label in zip(self.materials, self.labels):
            if np.any(label < 0) or np.any(label != np.round(label)):
                raise ValueError(f"{path}/{material}: {LABEL} must hold non-negative integers")
        self.labels = [label.astype(np.int64) for label in self.labels]

    def primitive(self, material):
        found = [k for k, m in enumerate(self.materials) if m == material]
        if len(found) != 1:
            raise ValueError(f"{self.path}: expected one {material!r} primitive, found {len(found)}")
        return found[0]

    def weights(self, k):
        """Dense (vertices, joints) skin weights of primitive k."""
        ids, w = self.glb.skin_streams(self.primitives[k])
        out = np.zeros((len(self.positions[k]), len(self.joints)))
        if ids is not None:
            np.add.at(out, (np.arange(len(out))[:, None], ids), w)
        return out

    def unlabelled_points(self):
        return np.concatenate([x[label == 0] for x, label in zip(self.positions, self.labels)])

    def piece(self, label, material, outer=None):
        return RenderPiece(self, label, self.primitive(material), outer)

    def surface(self, materials):
        """(points, faces, weights) of the named primitives as one surface: the fallback body of B.3."""
        points, faces, weights, offset = [], [], [], 0
        for k, m in enumerate(self.materials):
            if m in materials:
                points.append(self.positions[k])
                faces.append(self.faces[k] + offset)
                weights.append(self.weights(k))
                offset += len(self.positions[k])
        if not points:
            raise ValueError(f"{self.path}: none of the body primitives {sorted(materials)}")
        return Surface(np.concatenate(points), np.concatenate(faces), np.concatenate(weights), self.joints)

    def normals(self, materials):
        out = []
        for k, m in enumerate(self.materials):
            if m in materials:
                a = self.primitives[k]["attributes"]
                if "NORMAL" in a:
                    n = self.glb.accessor(a["NORMAL"])
                else:
                    x, f = self.positions[k], self.faces[k]
                    n = np.zeros_like(x)
                    for col in f.T:
                        np.add.at(n, col, np.cross(x[f[:, 1]] - x[f[:, 0]], x[f[:, 2]] - x[f[:, 0]]))
                out.append(n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12))
        return np.concatenate(out)


class Surface:
    """A triangle surface with skin weights on joints, the source of a weights transfer."""

    def __init__(self, points, faces, weights, joints):
        self.points, self.faces = np.asarray(points, float), np.asarray(faces, np.int64)
        self.weights, self.joints = np.asarray(weights, float), list(joints)


class RenderPiece:
    """The render region one piece moves: the triangles of its source primitive with a labelled corner.

    Per render vertex of the primitive:
    - labelled: the vertex carries this piece's label;
    - height: 0 at the top of the region and 1 at its bottom (NaN off the region), over every corner of the region,
      the unlabelled row it is sewn to included;
    - split: a labelled vertex at the position of an unlabelled one, which must stay with it or the seam cracks.
    sheet: the outer sewn sheet of the region (its points, triangles and the render vertex of each sheet vertex),
    which `Piece.create` turns into the sim mesh."""

    def __init__(self, model, label, k, outer=None):
        self.model, self.label, self.k = model, int(label), k
        self.positions, label_k = model.positions[k], model.labels[k]
        self.labelled = label_k == self.label
        faces = model.faces[k]
        self.region = faces[self.labelled[faces].any(1)]
        if not len(self.region):
            raise ValueError(f"{model.path}: piece label {label} has no {model.materials[k]!r} triangles")
        others = [m for j, m in enumerate(model.materials) if j != k and np.any(model.labels[j] == self.label)]
        if others:
            raise ValueError(f"{model.path}: label {label} spans primitives {model.materials[k]!r} and {others}")
        used = np.unique(self.region)
        y = self.positions[used, 1]
        self.height = np.full(len(self.positions), np.nan)
        self.height[used] = (y.max() - y) / max(np.ptp(y), 1e-12)
        from scipy.spatial import cKDTree
        gap, _ = cKDTree(model.unlabelled_points()).query(self.positions, distance_upper_bound=COINCIDENT_M)
        self.split = self.labelled & np.isfinite(gap)
        self.sheet_points, self.sheet_faces, self.sheet_render, _ = sewn_sheet(
            self.positions, outer_faces(self.positions, self.region, outer))
        self.weights = model.weights(k)

    def surface(self):
        """The sheet with the render weights of its vertices: today's weights source."""
        return Surface(self.sheet_points, self.sheet_faces, self.weights[self.sheet_render], self.model.joints)


def skin_matrices(glb, joints, inverse_binds, worlds):
    """(frames, joints, 4, 4) joint world x inverse bind, from posed_worlds' {node: (frames, 4, 4)}."""
    nodes = [glb.node_index(name) for name in joints]
    return np.stack([worlds[n] for n in nodes], axis=1) @ np.asarray(inverse_binds, float)[None]


def clip_worlds(glb, clip, times):
    """{node: (frames, 4, 4)} of the model posed by a clip file's channels (matched by node name)."""
    return glb.posed_worlds(clip.channels(0), times)


def rest_worlds(glb):
    rest = glb.world_rest()
    return {i: m[None] for i, m in rest.items()}


def clips(manifest):
    """[(name, path)] of the clips an animation manifest lists; its resource paths are relative to the game root,
    the nearest folder above the manifest that holds them."""
    import json
    manifest = Path(manifest).resolve()
    doc = json.loads(manifest.read_text(encoding="utf-8"))
    first = doc["animations"][0]["resource"]
    root = next(a for a in manifest.parents if (a / first).is_file())
    return [(a["name"], root / a["resource"]) for a in doc["animations"]]


def skin_points(points, weights, matrices):
    """Linear blend skinning of rest points (V, 3) with dense weights (V, J) by matrices (frames, J, 4, 4):
    (frames, V, 3)."""
    used = np.flatnonzero(np.asarray(weights).any(0))
    w = np.asarray(weights, float)[:, used]
    m = matrices[:, used]
    hom = np.c_[points, np.ones(len(points))]
    blended = np.einsum("vj,fjab->fvab", w, m[:, :, :3, :])
    return np.einsum("fvab,vb->fva", blended, hom)


def skin_directions(directions, weights, matrices):
    """Unit directions skinned by each vertex's blended rotation, (frames, V, 3)."""
    used = np.flatnonzero(np.asarray(weights).any(0))
    w = np.asarray(weights, float)[:, used]
    blended = np.einsum("vj,fjab->fvab", w, matrices[:, used, :3, :3])
    out = np.einsum("fvab,vb->fva", blended, directions)
    return out / np.maximum(np.linalg.norm(out, axis=2, keepdims=True), 1e-12)
