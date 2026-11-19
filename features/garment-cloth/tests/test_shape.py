import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from garment_shape import SectionGuide


def rings(heights, radii, count=12):
    angles = np.arange(count) * (2*np.pi/count)
    rows = []
    for height, radius in zip(heights, radii):
        rows.extend(np.column_stack((radius*np.cos(angles), np.full(count,height), radius*np.sin(angles))))
    return np.array(rows)


def triangles(levels, count=12):
    faces = []
    for row in range(levels-1):
        for i in range(count):
            a, b = row*count+i, row*count+(i+1)%count
            faces.extend(((a,b,a+count), (b,b+count,a+count)))
    return np.array(faces)


class SectionFitting(unittest.TestCase):
    def test_subrow_bulge_clears_face_interior_on_both_diagonals(self):
        garment = rings([1, 0], [1, 1])
        body = rings([1.1, 0.53, -0.1], [0.7, 1.12, 0.7])
        guide = SectionGuide(garment, [range(12),range(12,24)], sides=12)
        q = guide.fit(garment, body, triangles(3))
        middle = q[:12]*0.53 + q[12:]*0.47
        for i in range(12):
            j = (i+1)%12
            midpoint = (middle[i]+middle[j])*0.5
            radial = np.array([midpoint[0],0,midpoint[2]])
            normal = radial/np.linalg.norm(radial)
            self.assertGreater(np.min((midpoint-body[12:24])@normal), 0)

    def test_clear_bind_pose_and_rigid_transform(self):
        g = rings([1, 0.6, 0], [1, 1, 1])
        b = rings([1.2, 0, -0.2], [0.7]*3)
        guide = SectionGuide(g, [range(0,12),range(12,24),range(24,36)], sides=12)
        frame = np.array([[0.,-1,0,1],[1,0,0,2],[0,0,1,3],[0,0,0,1]])
        move = lambda p: p @ frame[:3,:3].T + frame[:3,3]
        np.testing.assert_allclose(guide.fit(move(g),move(b),triangles(3),frame=frame),move(g),atol=1e-7)

    def test_partial_panel_and_coincident_slit_indices(self):
        g = rings([1,0], [1,1])
        part = np.r_[2:7,14:19,14:15]
        columns = part%12
        guide = SectionGuide(g[part], [range(5),range(5,11)], sides=12, columns=columns)
        b = rings([1.2,-0.2],[0.7,0.7])
        q = guide.fit(g[part], b, triangles(2))
        np.testing.assert_allclose(q, g[part], atol=1e-7)
        with self.assertRaises(ValueError):
            SectionGuide(g[part], [range(5),range(5,10)], sides=12, columns=columns)
        with self.assertRaises(ValueError):
            guide.fit(g[part], b, [[-1,0,1]])

    def test_garment_inside_skin_is_not_accepted_as_clear(self):
        g = rings([0.8,0.2], [1,1])
        b = rings([1.1,-0.1], [1.2,1.2])
        guide = SectionGuide(g,[range(12),range(12,24)],sides=12)
        q = guide.fit(g, b, triangles(2))
        radius = np.linalg.norm(q[:,[0,2]],axis=1)
        self.assertGreater(min(radius), 1.16)


if __name__ == "__main__":
    unittest.main()
