import unittest

import torch
from theory import matrix



class Test(unittest.TestCase):
    
    def test_trace(self):
        # elementary linear algebra, Ex. 12, p. 37
        B = torch.Tensor([
            [-1,  2,  7,  0],
            [ 3,  5, -8,  4],
            [ 1,  2,  7, -3],
            [ 4, -2,  1,  0]
        ])
        
        M = torch.Tensor([11]);
        R = matrix.trace(B)
        
        self.assertTrue(torch.equal(M,R), f"{M} == {R}")

    def test_symmetric(self):
        
        A = torch.Tensor([
            [1,  4, 5],
            [4, -3, 0],
            [5,  0, 7]
        ])
        
        self.assertTrue(matrix.is_symmetric(A))
        
        A = torch.Tensor([
            [1,  5, 4],  # < shuffled
            [4, -3, 0],
            [5,  0, 7]
        ])
        
        self.assertFalse(matrix.is_symmetric(A))

        # AA^T and A^TA are symmetric

        A = torch.Tensor([
            [1, -2,  4],
            [3,  0, -5]
        ])
        
        T = matrix.transpose(A)
        
        P = matrix.product(A, T)
        self.assertTrue(matrix.is_symmetric(P), f"{P}")
        P = matrix.product(T, A)
        self.assertTrue(matrix.is_symmetric(P), f"{P}")

    def test_determinant(self):
        # elementary linear algebra, Ex. 3, p. 129
        A = torch.Tensor([[ 0, 1, 5],
                          [ 3,-6, 9],
                          [ 2, 6, 1]])
        O = torch.tensor([1,0,2],dtype=torch.long)  # hence remember to multiply the det by -1
        A = matrix.reorder(A,O)
        
        REF = matrix.ref(A)
        rank = matrix.determinant(REF)
        self.assertEqual(-1 * 165, rank, f"{rank}, {REF}")
