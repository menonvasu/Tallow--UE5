"""CPU-only source geometry validation; no Unreal, Blender or project mutation."""
import collections
import unittest
from create_flame_assets import flame_geometry


class FlameGeometryTests(unittest.TestCase):
    def test_wick_tip_and_height_uv(self):
        verts, uv, _ = flame_geometry()
        self.assertEqual(verts[0], (0, 0, 0))
        self.assertEqual(verts[-1], (0, 0, 1))
        self.assertEqual(len(verts), len(uv))
        for vertex, texcoord in zip(verts, uv):
            self.assertEqual(vertex[2], texcoord[1])
            self.assertTrue(0 <= vertex[2] <= 1)

    def test_closed_manifold_with_no_zero_area_faces(self):
        verts, _, faces = flame_geometry()
        edges = collections.Counter(tuple(sorted((tri[i], tri[(i + 1) % 3])))
                                    for tri in faces for i in range(3))
        self.assertEqual(set(edges.values()), {2})
        self.assertEqual(len(verts) - len(edges) + len(faces), 2)
        for tri in faces:
            a, b, c = (verts[i] for i in tri)
            ab = [b[i] - a[i] for i in range(3)]
            ac = [c[i] - a[i] for i in range(3)]
            cross = [ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2], ab[0]*ac[1]-ab[1]*ac[0]]
            self.assertGreater(sum(x*x for x in cross), 1e-16)

    def test_outward_winding(self):
        verts, _, faces = flame_geometry()
        volume = 0.0
        for tri in faces:
            a, b, c = (verts[i] for i in tri)
            cross = (b[1]*c[2]-b[2]*c[1], b[2]*c[0]-b[0]*c[2], b[0]*c[1]-b[1]*c[0])
            volume += sum(a[i] * cross[i] for i in range(3)) / 6
        self.assertGreater(volume, 0)


if __name__ == '__main__':
    unittest.main()
