import unittest

import torch
from theory import matrix

class Test(unittest.TestCase):
    

    def test_transition_matrix(self):
        # elementary linear algebra, Ex. 3 (a), p. 260
        FROM = torch.Tensor([[1, 0],
                             [0, 1]])
        TO = torch.Tensor([[1, 2],
                           [1, 1]])
        R = matrix.transition(FROM, TO)

        M = torch.Tensor([[-1,  2],
                          [ 1, -1]])
        self.assertTrue(torch.equal(M,R), f"{M} == {R}")

        # elementary linear algebra, Ex. 3 (b), p. 260
        TO = torch.Tensor([[1, 0],
                           [0, 1]])
        FROM = torch.Tensor([[1, 2],
                             [1, 1]])
        R = matrix.transition(FROM, TO)

        M = torch.Tensor([[1, 2],
                          [1, 1]])
        self.assertTrue(torch.equal(M,R), f"{M} == {R}")
