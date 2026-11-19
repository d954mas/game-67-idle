"""Fit cross-section pose targets to the animated support zone of a garment.

All coordinates are in meters, Y up. Body faces describe the skin the garment
encloses. Exterior obstacles (hands carrying a box, for instance) belong to a
separate contact pass. Indices and row order always refer to the simulation mesh.
"""
import numpy as np
from scipy.spatial import cKDTree


def _points(value, name):
    a = np.asarray(value, dtype=float)
    if a.ndim != 2 or a.shape[1] != 3 or not np.isfinite(a).all() or not len(a):
        raise ValueError(f"{name} must be a finite array of 3D points")
    return a


def _unit(v):
    return v / np.linalg.norm(v)


class _SkinContact:
    def __init__(self, triangles, clearance):
        self.body = triangles
        self.low = triangles.min(axis=1)
        self.high = triangles.max(axis=1)
        self.centers = triangles.mean(axis=1)
        self.radius = np.linalg.norm(triangles-self.centers[:,None], axis=2).max()
        n = np.cross(triangles[:,1]-triangles[:,0], triangles[:,2]-triangles[:,0])
        self.normals = n / np.maximum(np.linalg.norm(n, axis=1)[:,None], 1e-15)
        self.tree = cKDTree(self.centers)
        self.clearance = clearance

    def contacts(self, triangles):
        centers = triangles.mean(axis=1)
        radius = np.linalg.norm(triangles-centers[:,None], axis=2).max(axis=1)
        near = self.tree.query_ball_point(centers, radius+self.radius+self.clearance)
        cloth = np.repeat(np.arange(len(triangles)), [len(ids) for ids in near])
        if not len(cloth):
            return cloth, cloth
        skin = np.concatenate(near).astype(int)
        overlaps = ((triangles.max(axis=1)[cloth]+self.clearance >= self.low[skin])
                    & (triangles.min(axis=1)[cloth]-self.clearance <= self.high[skin])).all(axis=1)
        cloth, skin = cloth[overlaps], skin[overlaps]
        a, b = triangles[cloth], self.body[skin]
        ea, eb = a[:,[1,2,0]]-a, b[:,[1,2,0]]-b
        na, nb = np.cross(ea[:,0],ea[:,1]), np.cross(eb[:,0],eb[:,1])
        ok = np.ones(len(cloth), bool)
        # A face normal, an edge cross edge or an in-plane edge normal can
        # separate the triangles. Normalizing the gap gives it physical units.
        for axis in [na, nb, *(np.cross(ea[:,i],eb[:,j]) for i in range(3) for j in range(3)),
                     *(np.cross(ea[:,i],na) for i in range(3)), *(np.cross(eb[:,i],nb) for i in range(3))]:
            aa = np.einsum("nij,nj->ni", a, axis)
            bb = np.einsum("nij,nj->ni", b, axis)
            gap = np.maximum(aa.min(axis=1)-bb.max(axis=1),bb.min(axis=1)-aa.max(axis=1))
            ok &= gap <= self.clearance*np.linalg.norm(axis,axis=1)
        return cloth[ok], skin[ok]

    def buried(self, points):
        distances, _ = self.tree.query(points)
        near = self.tree.query_ball_point(points, distances+self.radius+1e-10)
        ix = np.repeat(np.arange(len(points)), [len(ids) for ids in near])
        face = np.concatenate(near).astype(int)
        tri = self.body[face]
        p = points[ix]
        normal = self.normals[face]
        signed = np.einsum("ij,ij->i", p-tri[:,0], normal)
        foot = p-signed[:,None]*normal
        edge = tri[:,[1,2,0]]-tri
        winding = np.einsum("nij,nj->ni", np.cross(edge,foot[:,None]-tri), normal)
        fraction = np.einsum("nij,nij->ni",p[:,None]-tri,edge) / np.maximum((edge*edge).sum(axis=2),1e-20)
        nearest = tri + edge*np.clip(fraction,0,1)[:,:,None]
        edge_dist2 = ((p[:,None]-nearest)**2).sum(axis=2).min(axis=1)
        dist2 = np.where((winding>=0).all(axis=1), signed*signed, edge_dist2)
        dist2[(normal*normal).sum(axis=1)<0.5] = np.inf
        order = np.lexsort((dist2, ix))
        selected = order[np.r_[True, np.diff(ix[order])!=0]]
        # Edges can bound an explicitly open support zone. Only the interior of
        # an oriented face supplies an unambiguous inside classification.
        below = (signed[selected] < -1e-5) & (winding[selected]>=-1e-11).all(axis=1)
        picked = selected[below]
        return ix[picked], face[picked], signed[picked]


class SectionGuide:
    def __init__(self, rest_points, rows, *, sides=24, clearance_m=0.008, columns=None, triangles=None):
        """Rows run top to hem. Open panels supply their original angular columns.

        The angular grid starts at +X and advances toward +Z. A slit can repeat
        a column with different particle indices. Pass its actual triangles to
        move the two lips independently. Body support faces must face outward.
        """
        self.rest = _points(rest_points, "garment")
        rows = [np.asarray(row) for row in rows]
        if len(rows) < 2 or any(a.ndim != 1 or len(a) < 3 or a.dtype.kind not in "iu" for a in rows):
            raise ValueError("provide at least two rows of three or more vertex indices")
        if not np.array_equal(np.sort(np.concatenate(rows)), np.arange(len(self.rest))):
            raise ValueError("rows must partition all garment vertices")
        if not isinstance(sides, (int, np.integer)) or sides < 4:
            raise ValueError("at least four angular sides are required")
        if not np.isfinite(clearance_m) or clearance_m < 0:
            raise ValueError("clearance must be finite and nonnegative")
        self.clearance = clearance_m
        self.rows = rows
        self.sides = sides
        heights = np.array([self.rest[row, 1].mean() for row in rows])
        if np.any(np.diff(heights) >= -1e-6):
            raise ValueError("row heights must descend from attachment to hem")
        self.heights = heights
        if columns is None:
            self.columns = np.zeros(len(self.rest), int)
            for row in rows:
                xz = self.rest[row][:, [0, 2]]
                center = (xz.min(axis=0) + xz.max(axis=0)) / 2
                theta = np.arctan2(xz[:, 1] - center[1], xz[:, 0] - center[0])
                self.columns[row] = np.rint(theta * sides / (2*np.pi)).astype(int) % sides
                if len(np.unique(self.columns[row])) != sides:
                    raise ValueError("provide angular columns for an open or irregular row")
        else:
            self.columns = np.asarray(columns)
            if (self.columns.shape != (len(self.rest),) or self.columns.dtype.kind not in "iu"
                    or self.columns.min() < 0 or self.columns.max() >= sides):
                raise ValueError("provide one valid angular column per garment vertex")
            self.columns = self.columns.astype(int, copy=False)

        angle = np.arange(2*sides) * (np.pi/sides)
        self.normals = np.stack((np.cos(angle), np.sin(angle)), axis=1)
        cells, directions = [], []
        self.copies = np.empty(len(self.rest), dtype=int)
        for band, row in enumerate(rows):
            for corner in range(sides):
                match = row[self.columns[row] == corner]
                if not len(match):
                    continue
                if np.max(np.linalg.norm(self.rest[match]-self.rest[match[0]], axis=1)) > 1e-5:
                    raise ValueError("repeated angular columns must coincide in bind space")
                self.copies[match] = match[0]
            if band+1 == len(rows):
                continue
            next_row = rows[band+1]
            for column in range(sides):
                pairs = [(r, k) for r in (row, next_row) for k in (column, (column+1)%sides)]
                if not all(np.any(self.columns[r] == k) for r,k in pairs):
                    continue
                cells.append([r[self.columns[r] == k][0] for r,k in pairs])
                directions.append(2*column+1)
        if not cells:
            raise ValueError("rows need adjacent angular columns in common")
        self.cells = np.array(cells)
        self.directions = np.array(directions)
        edges = self.cells[:,[0,1,0,2,1,3,2,3,0,3,1,2]].reshape(-1,2)
        self.faces = self.cells[:,[0,1,3,0,3,2,0,1,2,1,3,2]].reshape(-1,3)
        self.source = self.copies
        if triangles is not None:
            triangles = np.asarray(triangles)
            if (triangles.ndim != 2 or triangles.shape[1] != 3 or not len(triangles)
                    or triangles.dtype.kind not in "iu" or triangles.min() < 0
                    or triangles.max() >= len(self.rest) or len(np.unique(triangles)) != len(self.rest)):
                raise ValueError("invalid garment triangles or unused vertex")
            band = np.empty(len(self.rest), dtype=int)
            for i,row in enumerate(rows):
                band[row] = i
            gap = np.abs(self.columns[triangles]-self.columns[triangles[:,[1,2,0]]])
            area = np.linalg.norm(np.cross(self.rest[triangles[:,1]]-self.rest[triangles[:,0]],
                                           self.rest[triangles[:,2]]-self.rest[triangles[:,0]]), axis=1)
            if np.any(np.ptp(band[triangles], axis=1) != 1) or np.any(np.minimum(gap,sides-gap)>1) or np.any(area<1e-12):
                raise ValueError("each garment triangle must occupy one nondegenerate guide cell")
            edges = triangles[:,[0,1,1,2,2,0]].reshape(-1,2)
            self.faces = triangles
            self.source = np.arange(len(self.rest))
        self.edges = np.unique(np.sort(edges, axis=1), axis=0)
        self.edge_length = np.linalg.norm(self.rest[self.edges[:,1]]-self.rest[self.edges[:,0]], axis=1)
        self.degree = np.maximum(np.bincount(self.edges.ravel(), minlength=len(self.rest)), 1)
        self.mobility = np.ones(len(self.rest))
        self.mobility[self.rows[0]] = 0.0

    def fit(self, posed_targets, body_points, body_faces, *, frame=None):
        """Return fitted positions. frame is a rigid world-from-bind transform."""
        posed = _points(posed_targets, "posed targets")
        body = _points(body_points, "skin")
        faces = np.asarray(body_faces)
        if (faces.ndim != 2 or faces.shape[1] != 3 or not len(faces) or faces.dtype.kind not in "iu"
                or faces.min() < 0 or faces.max() >= len(body)):
            raise ValueError("invalid support faces")
        if posed.shape != self.rest.shape:
            raise ValueError("posed target count differs from the bind garment")
        frame = np.eye(4) if frame is None else np.asarray(frame, dtype=float)
        if (frame.shape != (4, 4) or not np.isfinite(frame).all()
                or not np.allclose(frame[-1], [0, 0, 0, 1], atol=1e-5)
                or not np.allclose(frame[:3,:3].T @ frame[:3,:3], np.eye(3), atol=2e-4)
                or np.linalg.det(frame[:3,:3]) <= 0):
            raise ValueError("frame must be a rigid world-from-bind matrix")

        skin_contact = _SkinContact(body[faces], self.clearance*0.75)
        seed = posed[self.source].copy()
        candidate = seed.copy()
        a, b = self.edges.T
        weight_a = (self.mobility[a]/np.maximum(self.mobility[a]+self.mobility[b], 1))[:,None]
        rest_edge = self.edge_length
        # Blending dissimilar thigh rotations can collapse one lip while
        # stretching the other. Preserve the textile metric before draping.
        for _ in range(28):
            edge = candidate[b]-candidate[a]
            length = np.linalg.norm(edge, axis=1)
            target = np.clip(length, 0.82*rest_edge, 1.12*rest_edge)
            correction = edge*((length-target)/np.maximum(length,1e-10))[:,None]
            delta = np.zeros_like(candidate)
            np.add.at(delta, a, weight_a*correction)
            np.add.at(delta, b, -(1-weight_a)*correction)
            candidate += self.mobility[:,None]*delta/self.degree[:,None] + 0.018*(seed-candidate)
        seed = candidate.copy()
        for iteration in range(42):
            if iteration == 20:
                skin_contact.clearance = self.clearance*0.25
            hit, hit_skin = skin_contact.contacts(candidate[self.faces])
            if not len(hit):
                trapped, skin_id, depth = skin_contact.buried(candidate)
                if not len(trapped):
                    return candidate[self.source]
                candidate[trapped] += (self.clearance-depth)[:,None]*skin_contact.normals[skin_id]
                continue
            ids = self.faces[hit]
            normal = skin_contact.normals[hit_skin]
            signed = np.einsum("nij,nj->ni", candidate[ids]-skin_contact.centers[hit_skin,None], normal)
            separation = self.clearance if iteration < 20 else self.clearance*0.45
            penetration = np.maximum(separation + 4e-5 - signed, 0)
            shift = np.zeros_like(candidate)
            count = np.zeros(len(candidate))
            for lane in range(3):
                np.add.at(shift, ids[:,lane], normal*penetration[:,lane,None])
                np.add.at(count, ids[:,lane], penetration[:,lane] > 0)
            shift /= np.maximum(count, 1)[:,None]
            shift *= np.minimum(1, 0.035/np.maximum(np.linalg.norm(shift,axis=1),1e-12))[:,None]
            # Diffuse the correction, retaining the clear bind silhouette even
            # where the opposite thigh shares the same projection on the section.
            displacement = candidate + shift - seed
            smoothing = np.zeros_like(candidate)
            np.add.at(smoothing, a, displacement[b]-displacement[a])
            np.add.at(smoothing, b, displacement[a]-displacement[b])
            smooth = 0.18 if iteration < 12 else 0.05 if iteration < 20 else 0
            candidate += shift + smooth*self.mobility[:,None]*smoothing/self.degree[:,None]

        rigid = self.rest @ frame[:3,:3].T + frame[:3,3]
        offsets = posed - rigid
        top, hem = self.rows[0], self.rows[-1]
        length = self.heights[0] - self.heights[-1]
        up = frame[:3,1]*length + offsets[top].mean(axis=0) - offsets[hem].mean(axis=0)
        if np.linalg.norm(up) < length*0.1:
            up = frame[:3,1]
        up = _unit(up)
        x = frame[:3,0] - np.dot(frame[:3,0], up)*up
        if np.linalg.norm(x) < 1e-4:
            x = np.cross(up, frame[:3,2])
        x = _unit(x)
        basis = np.column_stack((x, up, np.cross(x, up)))
        q = posed @ basis
        skin = body @ basis
        tri = skin[faces]
        cells = self.cells
        h = q[cells,1]
        lo, hi = h.min(axis=1), h.max(axis=1)
        span = hi.max() - lo.min()
        if span < 1e-7:
            raise ValueError("posed garment has zero section span")
        # More cuts follow curved profiles more closely. Slab vertices make
        # the bound conservative even where a narrow protrusion falls between cuts.
        grid = np.linspace(lo.min()-1e-6, hi.max()+1e-6, max(24, 4*len(self.rows)))
        supports = self._skin_support(skin, faces, tri, grid)
        xz = q[:,[0,2]].copy()
        proj = xz @ self.normals.T
        def at(y):
            u = (y-grid[0]) / (grid[1]-grid[0])
            index = np.clip(u.astype(int), 0, len(grid)-2)
            frac = (u-index)[:,None]
            return (1-frac)*supports[index] + frac*supports[index+1]
        lower, upper = at(lo), at(hi)
        slope = (upper-lower)/np.maximum(hi-lo, 1e-7)[:,None]
        intercept = lower - slope*lo[:,None]
        for y, support in zip(grid, supports):
            within = (lo < y) & (y < hi)
            intercept[within] = np.maximum(intercept[within], support-slope[within]*y)
        needed = intercept[:,None,:] + slope[:,None,:]*h[:,:,None] + self.clearance
        violation = needed - proj[cells]
        worst = violation.max(axis=1)
        # Every quad chooses one common separating plane. Restrict its candidates
        # to the authored side to prevent a folded panel escaping through the body.
        align = self.normals[self.directions] @ self.normals.T
        score = np.where(align >= 0.32, worst + 0.008*(1-align), np.inf)
        choice = np.argmin(score, axis=1)
        normals = self.normals[choice]
        distance = needed[np.arange(len(cells)),:,choice]
        # Each particle has at most four incident panels. Sweep the four corner
        # slots independently so each update is a projection onto a half-plane.
        rest = xz.copy()
        a,b = self.edges.T
        mobility = np.maximum(self.mobility[a] + self.mobility[b], 1)
        weight_a = (self.mobility[a]/mobility)[:,None]
        for iteration in range(48):
            if iteration < 38:
                edge = xz[b]-xz[a]
                magnitude = np.linalg.norm(edge, axis=1)
                dy = q[b,1]-q[a,1]
                ideal = np.sqrt(np.maximum((self.edge_length*1.12)**2-dy**2, 0))
                pulls = np.maximum(magnitude-ideal, 0) / np.maximum(magnitude, 1e-9)
                correction = pulls[:,None]*edge
                shift = np.zeros_like(xz)
                np.add.at(shift, a, weight_a*correction)
                np.add.at(shift, b, -(1-weight_a)*correction)
                xz += shift / self.degree[:,None]
                xz += (rest-xz)*0.012
            residual = 0
            for lane in range(4):
                vertex = cells[:,lane]
                error = distance[:,lane] - np.einsum("ij,ij->i", xz[vertex], normals)
                move = np.maximum(error, 0)
                residual = max(residual, move.max())
                xz[vertex] += move[:,None]*normals
            if iteration >= 38 and residual < 1e-6:
                break
        q[:,0], q[:,2] = xz.T
        envelope = q[self.copies] @ basis.T
        # The convex section can also contain empty space between separated
        # limbs. Only panels within the clearance band need that correction.
        base = candidate[self.source]
        blend = np.zeros(len(base))
        for _ in range(14):
            candidate = base + blend[:,None]*(envelope-base)
            hit, _ = skin_contact.contacts(candidate[self.faces])
            if not len(hit):
                trapped, _, _ = skin_contact.buried(candidate)
                if not len(trapped):
                    return candidate[self.source]
                blend[trapped] = np.minimum(1, blend[trapped]+0.16)
            needed = np.unique(self.faces[hit])
            blend[needed] = np.minimum(1,blend[needed]+0.16)
            adjacent = np.zeros_like(blend)
            np.maximum.at(adjacent, a, blend[b]*0.72)
            np.maximum.at(adjacent, b, blend[a]*0.72)
            blend = np.maximum(blend, adjacent)
        skin_contact.clearance = 0
        hit, _ = skin_contact.contacts(envelope[self.faces])
        if len(hit):
            raise ValueError(f"section fit leaves {len(np.unique(hit))} triangles in the support skin")
        return envelope

    def _skin_support(self, skin, faces, tri, grid):
        heights = tri[:,:,1]
        floor, ceiling = heights.min(axis=1), heights.max(axis=1)
        support = np.full((len(grid), len(self.normals)), -np.inf)
        for i, height in enumerate(grid):
            slab = tri[(floor <= height) & (ceiling >= height)]
            a, b = slab.reshape(-1,3), slab[:,[1,2,0]].reshape(-1,3)
            crosses = (a[:,1] <= height) != (b[:,1] <= height)
            a, b = a[crosses], b[crosses]
            if len(a):
                u = ((height-a[:,1]) / (b[:,1]-a[:,1]))[:,None]
                cuts = a[:,[0,2]] + u*(b[:,[0,2]]-a[:,[0,2]])
                support[i] = (cuts @ self.normals.T).max(axis=0)
        valid = np.isfinite(support[:,0])
        if not valid.any():
            # A garment beyond a support region can still cover its terminal
            # triangle cap if the entire region fits between two grid planes.
            return np.broadcast_to((tri[:,:,[0,2]].reshape(-1,2) @ self.normals.T).max(axis=0), support.shape)
        for col in range(support.shape[1]):
            support[:,col] = np.interp(grid, grid[valid], support[valid,col])
        points = skin[np.unique(faces)]
        bounds = support.copy()
        for i, (lo, hi) in enumerate(zip(grid[:-1], grid[1:])):
            p = points[(points[:,1] >= lo) & (points[:,1] <= hi)]
            if not len(p):
                continue
            u = (p[:,1:2]-lo)/(hi-lo)
            boundary = (1-u)*support[i] + u*support[i+1]
            excess = ((p[:,[0,2]] @ self.normals.T)-boundary).max(axis=0)
            extra = np.maximum(excess, 0)
            bounds[i:i+2] = np.maximum(bounds[i:i+2], support[i:i+2]+extra)
        return bounds
