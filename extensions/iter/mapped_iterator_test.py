import unittest
import torch
from iter.generated_iterator import GeneratedIterableInt as GI
from iter.mapped_iterator import MappedGeneratedIterableInt as MGI

class F(object):
    
    def __init__(self):
        self._a = 11
        self._t = True
    
    def __call__(self):
        ret = (self._a, self._t)
        self._a += 1
        self._t = (13 != self._a)
        return ret

def doubled(number):
    return 2 * number

class Test(unittest.TestCase):

    def test_mapped_iterator(self):
        self.assertEqual([22,24], [x for x in MGI(GI(F()), doubled)])

if '__main__' == __name__:
    unittest.main(verbosity=2)
