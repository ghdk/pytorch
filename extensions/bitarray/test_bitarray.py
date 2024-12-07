import unittest
import torch
import multiprocessing as mp
import random
from signal import SIGABRT

from bitarray import bitarray

def SIGABRT_test_bitarray_set_bounds_outside():
    bitmap = torch.ByteTensor([0,0])
    bitmap = bitarray.set(bitmap, 16, True)

def SIGABRT_test_bitarray_set_bounds_negative():
    bitmap = torch.ByteTensor([0])
    bitmap = bitarray.set(bitmap, -1, True)

class Test(unittest.TestCase):

    def test_bitarray_set_bounds_negative(self):
        ctx = mp.get_context('spawn')
        p = ctx.Process(target=SIGABRT_test_bitarray_set_bounds_negative)
        p.start()
        p.join()
        self.assertEqual(SIGABRT, -1 * p.exitcode)

    def test_bitarray_set_bounds_lower(self):
        bitmap = torch.ByteTensor([0,0])
        bitmap = bitarray.set(bitmap, 0, True)
        self.assertTrue(torch.equal(bitmap, torch.ByteTensor([0x80, 0x0])))

    def test_bitarray_set_bounds_upper(self):
        bitmap = torch.ByteTensor([0,0])
        bitmap = bitarray.set(bitmap, 15, True)
        self.assertTrue(torch.equal(bitmap, torch.ByteTensor([0x0, 0x1])))

    def test_bitarray_set_bounds_outside(self):
        ctx = mp.get_context('spawn')
        p = ctx.Process(target=SIGABRT_test_bitarray_set_bounds_outside)
        p.start()
        p.join()
        self.assertEqual(SIGABRT, -1 * p.exitcode)

    def test_bitarray_set(self):
        bitmap = torch.ByteTensor([1])
        bitmap = bitarray.set(bitmap, 1, True)
        self.assertEqual(bitmap, torch.ByteTensor([0x41]))
        bitmap = bitarray.set(bitmap, 1, False)
        self.assertEqual(bitmap, torch.ByteTensor([0x1]))

    def test_bitarray_set_random(self):
        total = 4096
        bitmap = torch.ByteTensor([0] * total)
        expected = []
        for i in range(total):
            r = random.randint(0, i)
            bitmap = bitarray.set(bitmap, r, True)
            expected.append(r)
        received = []
        for i in range(total * bitarray.CELL_SIZE):
            if bitarray.get(bitmap, i):
                received.append(i)
        expected = sorted(set(expected))
        received = sorted(set(received))
        self.assertEqual(expected, received, f"{expected}, {received}")

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
