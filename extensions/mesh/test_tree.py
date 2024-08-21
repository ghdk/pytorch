import unittest
import torch
from mesh.mesh import Mesh
from mesh.mesh import dim
from mesh.tree import Tree
from mesh.tree import Test
X = Test.X
Y = Test.Y
Z = Test.Z

class Test(unittest.TestCase):

    def test_util_mesh_patterns_tree_child(self):
        r = Tree()
        x = X()
        y = Y()
        r.p(dim.y, x)
        r.p(dim.y, y)
        # r
        n = r
        self.assertEqual(None, n.n(dim.y))
        self.assertEqual(None, n.p(dim.x))
        self.assertEqual(x, n.p(dim.y))
        # x
        n = r.p(dim.y)
        self.assertEqual(r, n.n(dim.y))
        self.assertEqual(y, n.p(dim.x))
        self.assertEqual(None, n.p(dim.y))
        # y
        n = r.p(dim.y).p(dim.x)
        self.assertEqual(r, n.n(dim.y))
        self.assertEqual(None, n.p(dim.x))
        self.assertEqual(None, n.p(dim.y))
        # None
        r.p(dim.y, None)
        self.assertEqual(None, r.p(dim.y))

if '__main__' == __name__:
    unittest.main(verbosity=2)
