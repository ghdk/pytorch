import unittest
import torch
from mesh.mesh import Mesh
from mesh.mesh import dim
from mesh.tree import Tree
from mesh.multitree import MultiTree
from iter.generated_iterator import GeneratedIterableMesh as GI
from iter.filtered_iterator import FilteredGeneratedIterableMesh as FGI
from iter.mapped_iterator import MappedGeneratedIterableMultitree as MGI
from mesh.children_traversal import ChildrenTraversal as CTRV

class Test(unittest.TestCase):

    def test_filtered_iterator(self):
        r = Tree()
        a = MultiTree()
        b = MultiTree()
        c = Tree()
        r.p(dim.y, a)
        r.p(dim.y, b)
        r.p(dim.y, c)
        e = GI(CTRV(r))
        self.assertEqual([a,b], [x for x in FGI(e, MultiTree.__is__)])

    def test_mapped_iterator(self):
        r = Tree()
        a = MultiTree()
        b = MultiTree()
        c = Tree()
        r.p(dim.y, a)
        r.p(dim.y, b)
        r.p(dim.y, c)
        e = GI(CTRV(r))
        self.assertEqual([a,b,None], [x for x in MGI(e, MultiTree.__of__)])

if '__main__' == __name__:
    unittest.main(verbosity=2)
