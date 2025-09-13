import unittest

import torch
from theory.theory import matrix



class Test(unittest.TestCase):
    
    def test_augment(self):

        A = matrix.random(2,3)
        B = matrix.random(2,4)

        R = matrix.augment(A,B)
        
        Am = A.size()[0]
        An = A.size()[1]
        Bm = B.size()[0]
        Bn = B.size()[1]
        Rm = R.size()[0]
        Rn = R.size()[1]
        
        self.assertTrue(Rm == Am and Am == Bm, f"{A.size()}, {B.size()}, {R.size()}")
        self.assertTrue(Rn == An + Bn, f"{A.size()}, {B.size()}, {R.size()}")
        
    def test_identity(self):
        
        I = torch.Tensor([[1,0,0],
                          [0,1,0],
                          [0,0,1]])
        
        R = matrix.identity(3)
        
        self.assertTrue(torch.equal(I,R), f"{I} == {R}")
        
    def test_sum(self):

        A = torch.Tensor([[1,2],
                          [3,4]])
        B = torch.Tensor([[10,20],
                          [30,40]])
        M = torch.Tensor([[1+10, 2+20],
                          [3+30, 4+40]])
        
        R = matrix.sum(A,B)

        self.assertTrue(torch.equal(M,R), f"{M} == {R}")

    def test_product(self):
        A = torch.Tensor([[1,2],
                          [3,4]])
        B = torch.Tensor([[10,20],
                          [30,40]])
        
        M = torch.Tensor([[1*10+2*30,  1*20+2*40],
                          [3*10+4*30,  3*20+4*40]])
        
        R = matrix.product(A,B)
        self.assertTrue(torch.equal(M,R), f"{M} == {R}")

    def test_transpose(self):

        A = torch.Tensor([[1,2,3],
                          [4,5,6]])

        M = torch.Tensor([[1,4],
                          [2,5],
                          [3,6]])

        R = matrix.transpose(A)
        self.assertTrue(torch.equal(M,R), f"{M} == {R}")
        
    def test_reverse(self):
        # mml-book, example 2.4
        A = torch.Tensor([[1,2,1],
                          [4,4,5],
                          [6,7,7]])
        
        M = torch.Tensor([[-7,-7, 6],
                          [ 2, 1,-1],
                          [ 4, 5,-4]])
        
        R = matrix.reverse(A)
        self.assertTrue(torch.equal(M,R), f"{M} == {R}")
        
        I = matrix.identity(3)
        R = matrix.product(A,R)
        self.assertTrue(torch.equal(I,R), f"{A},{R}")

        # mml-book, example 2.9
        A = torch.Tensor([[1,0,2,0],
                          [1,1,0,0],
                          [1,2,0,1],
                          [1,1,1,1]])
        M = torch.Tensor([[-1, 2,-2, 2],
                          [ 1,-1, 2,-2],
                          [ 1,-1, 1,-1],
                          [-1, 0,-1, 2]])
        R = matrix.reverse(A)
        self.assertTrue(torch.equal(M,R), f"{M} == {R}")

    def test_basis(self):
        # mml-book, example 2.17
        A = torch.Tensor([[ 1, 2, 3,-1],
                          [ 2,-1,-4, 8],
                          [-1, 1, 3,-5],
                          [-1, 2, 5,-6],
                          [-1,-2,-3, 1]])
        O = torch.tensor([1,0,3,4,2],dtype=torch.long)
        A = matrix.reorder(A,O)
        
        REF = matrix.ref(A)
        R = matrix.basis(A, REF)
        
        M = torch.Tensor([[ 1, 2,0,-1],
                          [ 2,-1,0, 8],
                          [-1, 1,0,-5],
                          [-1, 2,0,-6],
                          [-1,-2,0, 1]])
        M = matrix.reorder(M,O)
        self.assertTrue(torch.equal(M,R), f"{M} == {R}")

    def test_rank(self):
        # mml-book, example 2.17
        A = torch.Tensor([[ 1, 2, 3,-1],
                          [ 2,-1,-4, 8],
                          [-1, 1, 3,-5],
                          [-1, 2, 5,-6],
                          [-1,-2,-3, 1]])
        O = torch.tensor([1,0,3,4,2],dtype=torch.long)
        A = matrix.reorder(A,O)
        
        REF = matrix.ref(A)
        rank = matrix.rank(REF)
        self.assertEqual(3, rank, f"{rank}, {REF}")
