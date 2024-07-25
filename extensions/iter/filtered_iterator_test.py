import unittest
import torch
from iter.generated_iterator import GeneratedIterableInt as GI
from iter.filtered_iterator import FilteredGeneratedIterableInt as FGI

class F(object):
    
    def __init__(self):
        self._a = 11
        self._t = True
    
    def __call__(self):
        ret = (self._a, self._t)
        self._a += 1
        self._t = (13 != self._a)
        return ret

def even(number):
    return 0 == (number % 2)

class Test(unittest.TestCase):

    def test_filtered_iterator(self):
        self.assertEqual([12], [x for x in FGI(GI(F()), even)])

if '__main__' == __name__:
    unittest.main(verbosity=2)
