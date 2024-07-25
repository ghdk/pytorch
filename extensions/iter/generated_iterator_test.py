import unittest
import torch
from iter.generated_iterator import GeneratedIterableInt as GI

class F(object):
    
    def __init__(self):
        self._a = 11
        self._t = True
    
    def __call__(self):
        ret = (self._a if self._t else None)
        self._a += 1
        self._t = (13 != self._a)
        return ret

class Test(unittest.TestCase):

    def test_generated_iterator(self):
        self.assertEqual([11,12], [x for x in GI(F())])

if '__main__' == __name__:
    unittest.main(verbosity=2)
