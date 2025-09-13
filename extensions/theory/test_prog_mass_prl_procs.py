import unittest
import threading

import torch
from theory import parallel

class Test(unittest.TestCase):

    def test_fanout(self):

        barrier = [0] * torch.get_num_threads()
        ids = set()

        def kernel(begin, end):
            ids.add(threading.get_native_id())
            barrier[begin] += 1

        parallel.fanout(kernel)
        self.assertTrue(all(map(lambda x: 1 == x, barrier)), barrier)

        # If the computation is short the same thread might execute more
        # than one kernels.
        self.assertTrue(len(barrier) >= len(ids), ids)

    def test_kogge_stone(self):
        ib = torch.randint(16, (128,))
        ob = torch.zeros((128,), dtype=torch.int64)
        parallel.koggestone(ib,ob)
        self.assertEqual(ib.sum(), ob[-1], f"{ib}, {ob}")
