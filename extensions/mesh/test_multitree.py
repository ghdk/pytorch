import unittest
import torch
from mesh.mesh import Mesh
from mesh.mesh import dim
from mesh.tree import Tree
from mesh.multitree import MultiTree
from mesh.multitree import Test
X = Test.X
Y = Test.Y
Z = Test.Z

class Test(unittest.TestCase):

    def test_util_mesh_patterns_multitree_child(self):
        r = MultiTree()
        a = MultiTree()
        b = MultiTree()
        r.p(dim.y, a)
        r.p(dim.y, b)
        x = X()
        y = Y()
        z = Z()
        x.p(dim.y, y)
        x.p(dim.y, z)
        a.p(dim.y, x)
        b.p(dim.y, x)
        self.assertEqual(x, b.p(dim.y).deref())
        self.assertEqual(a, x.n(dim.y))
        # None
        b.p(dim.y, None)
        self.assertEqual(None, b.p(dim.y))
        self.assertEqual(a, x.n(dim.y))

    def test_util_mesh_patterns_multitree_image_sequence(self):
        r = MultiTree()
        a = MultiTree()
        b = MultiTree()
        r.p(dim.y, a)
        r.p(dim.y, b)
        x = X()
        y = Y()
        z = Z()
        a.p(dim.y, x)
        a.p(dim.y, y)
        a.p(dim.y, z)
        b.p(dim.y, x)
        b.p(dim.y, z)
        b.p(dim.y, y)
        self.assertEqual(x, b.p(dim.y).deref())
        self.assertEqual(a, b.p(dim.y).deref().n(dim.y))
        self.assertEqual(z, b.p(dim.y).p(dim.x).deref())
        self.assertEqual(a, b.p(dim.y).p(dim.x).deref().n(dim.y))
        self.assertEqual(y, b.p(dim.y).p(dim.x).p(dim.x).deref())
        self.assertEqual(a, b.p(dim.y).p(dim.x).p(dim.x).deref().n(dim.y))

    def test_util_mesh_patterns_multitree_image(self):
        r = MultiTree()
        a = MultiTree()
        b = MultiTree()
        c = MultiTree()
        r.p(dim.y, a)
        r.p(dim.y, b)
        r.p(dim.y, c)
        x = X()
        a.p(dim.y, x)
        b.p(dim.y, x)
        c.p(dim.y, x)
        self.assertEqual(b, x.n(dim.x).n(dim.y))
        self.assertEqual(x, x.n(dim.x).p(dim.y))
        self.assertEqual(c, x.n(dim.x).n(dim.x).n(dim.y))
        self.assertEqual(x, x.n(dim.x).p(dim.y))

if '__main__' == __name__:
    unittest.main(verbosity=2)
