import unittest
import torch
from bitarray import bitarray

class Test(unittest.TestCase):

    def test_bitarray_set_bounds_negative(self):
        bitmap = torch.ByteTensor([0])
        try:
            bitmap = bitarray.set(bitmap, -1, True)
            self.assertTrue(False, "we should not reach this")
        except TypeError: pass

    def test_bitarray_set_bounds_lower(self):
        bitmap = torch.ByteTensor([0,0])
        bitmap = bitarray.set(bitmap, 0, True)
        self.assertTrue(torch.equal(bitmap, torch.ByteTensor([0x80, 0x0])))

    def test_bitarray_set_bounds_upper(self):
        bitmap = torch.ByteTensor([0,0])
        bitmap = bitarray.set(bitmap, 15, True)
        self.assertTrue(torch.equal(bitmap, torch.ByteTensor([0x0, 0x1])))

    def test_bitarray_set_bounds_outside(self):
        bitmap = torch.ByteTensor([0,0])
        try:
            bitmap = bitarray.set(bitmap, 16, True)
            self.assertTrue(False, "we should not reach this")
        except IndexError: pass

    def test_bitarray_set(self):
        bitmap = torch.ByteTensor([1])
        bitmap = bitarray.set(bitmap, 1, True)
        self.assertEqual(bitmap, torch.ByteTensor([0x41]))
        bitmap = bitarray.set(bitmap, 1, False)
        self.assertEqual(bitmap, torch.ByteTensor([0x1]))

    def test_bitarray_get(self):
        bitmap = torch.ByteTensor([5])
        self.assertEqual(torch.BoolTensor([True]), bitarray.get(bitmap, 5))
        self.assertEqual(torch.BoolTensor([False]), bitarray.get(bitmap, 6))
        self.assertEqual(torch.BoolTensor([True]), bitarray.get(bitmap, 7))

    def test_bitarray_negate(self):
        bitmap = torch.ByteTensor([0,1,2])
        bitarray.negate(bitmap)
        self.assertEqual(bitmap.tolist(), torch.ByteTensor([0xff, 0xfe, 0xfd]).tolist())
        
if '__main__' == __name__:
    unittest.main(verbosity=2)
