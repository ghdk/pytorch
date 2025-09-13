import unittest
import torch
from mesh.mesh import Mesh
from mesh.mesh import dim
from mesh.tree import Tree
from mesh.star import Star
from mesh.star import Test
X = Test.X
Y = Test.Y

class Test(unittest.TestCase):

    def test_util_mesh_patterns_triangle_formed_from_single_node(self):
        r = Star()
        x = X()
        y = Y()
        r.p(dim.x, x)
        r.p(dim.y, y)
        x.p(dim.x, y)
        # r
        n = r
        self.assertEqual(x, n.p(dim.x))
        self.assertEqual(y, n.p(dim.y))
        # x
        n = r.p(dim.x)
        self.assertEqual(r, n.n(dim.x))
        self.assertEqual(y, n.p(dim.x))
        # y
        n = r.p(dim.x).p(dim.x)
        self.assertEqual(x, n.n(dim.x))
        self.assertEqual(r, n.n(dim.y))

    def test_util_mesh_patterns_triangle_formed_from_multiple_nodes(self):
        r = Star()
        x = X()
        y = Y()
        r.p(dim.x, x)
        x.p(dim.x, y)
        y.p(dim.x, r)
        # r
        n = r
        self.assertEqual(x, n.p(dim.x))
        self.assertEqual(y, n.n(dim.x))
        # x
        n = r.p(dim.x)
        self.assertEqual(r, n.n(dim.x))
        self.assertEqual(y, n.p(dim.x))
        # y
        n = r.p(dim.x).p(dim.x)
        self.assertEqual(x, n.n(dim.x))
        self.assertEqual(r, n.p(dim.x))
        # break the cycle
        r.p(dim.x, None)

    def test_util_mesh_patterns_tree_star_mix(self):
        s = Star()
        t = Tree()
        s.p(dim.y, t)
        s.n(dim.y, t)
        self.assertEqual(t, s.p(dim.y))
        self.assertEqual(t, s.n(dim.y))

if '__main__' == __name__:
    unittest.main(verbosity=2)
