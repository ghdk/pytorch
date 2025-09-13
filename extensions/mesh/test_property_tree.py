import unittest
import torch
from mesh.mesh import Mesh
from mesh.mesh import dim
from mesh.tree import Tree
from mesh.multitree import MultiTree
from mesh.property_tree import PropertyTree
from mesh.property_tree import Test
X = Test.X
Y = Test.Y
Z = Test.Z

class Test(unittest.TestCase):

    def test_util_mesh_patterns_property_tree_property(self):
        r = PropertyTree()
        x = X()
        y = Y()
        z1 = Z()
        z2 = Z()
        r.p(dim.y, x)
        r.p(dim.y, y)
        x.p(dim.z, z1)
        x.p(dim.z, z2)
        # z1
        n = r.p(dim.y).p(dim.z)
        self.assertEqual(r.p(dim.y), n.n(dim.z))
        self.assertEqual(None, n.p(dim.x))
        self.assertEqual(z2, n.p(dim.z))
        self.assertEqual(None, n.p(dim.y))
        # z2
        n = r.p(dim.y).p(dim.z).p(dim.z)
        self.assertEqual(r.p(dim.y), n.n(dim.z))
        self.assertEqual(None, n.p(dim.x))
        self.assertEqual(None, n.p(dim.z))
        self.assertEqual(None, n.p(dim.y))
        # None
        r.p(dim.y).p(dim.z, None)
        self.assertEqual(None, r.p(dim.y).p(dim.z))

if '__main__' == __name__:
    unittest.main(verbosity=2)
