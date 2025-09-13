import unittest
import torch

import bitarray.bitarray
from graph.graph import Graph
from graph.converter import add_vertex
from graph.converter import rm_vertex
from graph.accessor  import rm_edge
from graph.accessor  import add_edge
from graph.accessor  import edge
from graph.accessor  import vertex

class Test(unittest.TestCase):

    def test_graph_add_vertex(self):
        g = Graph()
        for i in range(1,10):
            g = add_vertex(g, torch.tensor([i] * i, dtype=torch.int64).detach())
        self.assertEqual(g.vertices().tolist(), torch.Tensor([[1,-1,-1,-1,-1,-1,-1,-1,-1],
                                                              [2, 2,-1,-1,-1,-1,-1,-1,-1],
                                                              [3, 3, 3,-1,-1,-1,-1,-1,-1],
                                                              [4, 4, 4, 4,-1,-1,-1,-1,-1],
                                                              [5, 5, 5, 5, 5,-1,-1,-1,-1],
                                                              [6, 6, 6, 6, 6, 6,-1,-1,-1],
                                                              [7, 7, 7, 7, 7, 7, 7,-1,-1],
                                                              [8, 8, 8, 8, 8, 8, 8, 8,-1],
                                                              [9, 9, 9, 9, 9, 9, 9, 9, 9]]).tolist())
        self.assertEqual(g.edges().tolist(), torch.Tensor([[0,0],
                                                           [0,0],
                                                           [0,0],
                                                           [0,0],
                                                           [0,0],
                                                           [0,0],
                                                           [0,0],
                                                           [0,0],
                                                           [0,0]]).tolist())

    def test_graph_add_edge(self):
        g = Graph()
        for i in range(1,3):
            g = add_vertex(g, torch.tensor([i] * i, dtype=torch.int64).detach())
            g = add_edge(g, [i-1,i-1])
        self.assertEqual(g.vertices().tolist(), torch.Tensor([[1,-1],
                                                              [2, 2]]).tolist())
        self.assertEqual(g.edges().tolist(), torch.Tensor([[0x80],
                                                           [0x40]]).tolist())
        self.assertTrue(edge(g, [0,0]))
        self.assertTrue(edge(g, [1,1]))
        self.assertFalse(edge(g, [0,1]))
        self.assertFalse(edge(g, [1,0]))

    def test_graph_remove_edge(self):
        g = Graph()
        for i in range(1,3):
            g = add_vertex(g, torch.tensor([i] * i, dtype=torch.int64).detach())
            g = add_edge(g, [i-1,i-1])
        self.assertEqual(g.vertices().tolist(), torch.Tensor([[1,-1],
                                                              [2, 2]]).tolist())
        self.assertEqual(g.edges().tolist(), torch.Tensor([[0x80],
                                                           [0x40]]).tolist())
        g = rm_edge(g, [0,0])
        self.assertEqual(g.edges().tolist(), torch.Tensor([[0x0],
                                                           [0x40]]).tolist())

    def test_remove_vertex_from_empty(self):
        g = Graph()
        g = rm_vertex(g,0)
        g = rm_vertex(g,1)

    def test_remove_vertex_from_beginning(self):
        g = Graph()
        for i in range(1,10):
            g = add_vertex(g, torch.tensor([i] * i, dtype=torch.int64).detach())
            g = add_edge(g, [i-1,i-1])
        self.assertEqual(g.vertices().tolist(), torch.Tensor([[1,-1,-1,-1,-1,-1,-1,-1,-1],
                                                              [2, 2,-1,-1,-1,-1,-1,-1,-1],
                                                              [3, 3, 3,-1,-1,-1,-1,-1,-1],
                                                              [4, 4, 4, 4,-1,-1,-1,-1,-1],
                                                              [5, 5, 5, 5, 5,-1,-1,-1,-1],
                                                              [6, 6, 6, 6, 6, 6,-1,-1,-1],
                                                              [7, 7, 7, 7, 7, 7, 7,-1,-1],
                                                              [8, 8, 8, 8, 8, 8, 8, 8,-1],
                                                              [9, 9, 9, 9, 9, 9, 9, 9, 9]]).tolist())
        self.assertEqual(g.edges().tolist(), torch.Tensor([[0x80,0],
                                                           [0x40,0],
                                                           [0x20,0],
                                                           [0x10,0],
                                                           [0x8 ,0],
                                                           [0x4 ,0],
                                                           [0x2 ,0],
                                                           [0x1 ,0],
                                                           [0,0x80]]).tolist())
        g = rm_vertex(g, 0)
        self.assertEqual(g.vertices().tolist(), torch.Tensor([[2, 2,-1,-1,-1,-1,-1,-1,-1],
                                                              [3, 3, 3,-1,-1,-1,-1,-1,-1],
                                                              [4, 4, 4, 4,-1,-1,-1,-1,-1],
                                                              [5, 5, 5, 5, 5,-1,-1,-1,-1],
                                                              [6, 6, 6, 6, 6, 6,-1,-1,-1],
                                                              [7, 7, 7, 7, 7, 7, 7,-1,-1],
                                                              [8, 8, 8, 8, 8, 8, 8, 8,-1],
                                                              [9, 9, 9, 9, 9, 9, 9, 9, 9]]).tolist())
        self.assertEqual(g.edges().tolist(), torch.Tensor([[0x80,0],
                                                           [0x40,0],
                                                           [0x20,0],
                                                           [0x10,0],
                                                           [0x8 ,0],
                                                           [0x4 ,0],
                                                           [0x2 ,0],
                                                           [0x1 ,0]]).tolist())

    def test_remove_vertex_from_middle(self):
        g = Graph()
        for i in range(1,10):
            g = add_vertex(g, torch.tensor([i] * i, dtype=torch.int64).detach())
            g = add_edge(g, [i-1,i-1])
        self.assertEqual(g.vertices().tolist(), torch.Tensor([[1,-1,-1,-1,-1,-1,-1,-1,-1],
                                                              [2, 2,-1,-1,-1,-1,-1,-1,-1],
                                                              [3, 3, 3,-1,-1,-1,-1,-1,-1],
                                                              [4, 4, 4, 4,-1,-1,-1,-1,-1],
                                                              [5, 5, 5, 5, 5,-1,-1,-1,-1],
                                                              [6, 6, 6, 6, 6, 6,-1,-1,-1],
                                                              [7, 7, 7, 7, 7, 7, 7,-1,-1],
                                                              [8, 8, 8, 8, 8, 8, 8, 8,-1],
                                                              [9, 9, 9, 9, 9, 9, 9, 9, 9]]).tolist())
        self.assertEqual(g.edges().tolist(), torch.Tensor([[0x80,0],
                                                           [0x40,0],
                                                           [0x20,0],
                                                           [0x10,0],
                                                           [0x8 ,0],
                                                           [0x4 ,0],
                                                           [0x2 ,0],
                                                           [0x1 ,0],
                                                           [0,0x80]]).tolist())
        g = rm_vertex(g, 2)
        self.assertEqual(g.vertices().tolist(), torch.Tensor([[1,-1,-1,-1,-1,-1,-1,-1,-1],
                                                              [2, 2,-1,-1,-1,-1,-1,-1,-1],
                                                              [4, 4, 4, 4,-1,-1,-1,-1,-1],
                                                              [5, 5, 5, 5, 5,-1,-1,-1,-1],
                                                              [6, 6, 6, 6, 6, 6,-1,-1,-1],
                                                              [7, 7, 7, 7, 7, 7, 7,-1,-1],
                                                              [8, 8, 8, 8, 8, 8, 8, 8,-1],
                                                              [9, 9, 9, 9, 9, 9, 9, 9, 9]]).tolist())
        self.assertEqual(g.edges().tolist(), torch.Tensor([[0x80,0],
                                                           [0x40,0],
                                                           [0x20,0],
                                                           [0x10,0],
                                                           [0x8 ,0],
                                                           [0x4 ,0],
                                                           [0x2 ,0],
                                                           [0x1 ,0]]).tolist())

    def test_remove_vertex_from_end(self):
        g = Graph()
        for i in range(1,10):
            g = add_vertex(g, torch.tensor([i] * i, dtype=torch.int64).detach())
            g = add_edge(g, [i-1,i-1])
        self.assertEqual(g.vertices().tolist(), torch.Tensor([[1,-1,-1,-1,-1,-1,-1,-1,-1],
                                                              [2, 2,-1,-1,-1,-1,-1,-1,-1],
                                                              [3, 3, 3,-1,-1,-1,-1,-1,-1],
                                                              [4, 4, 4, 4,-1,-1,-1,-1,-1],
                                                              [5, 5, 5, 5, 5,-1,-1,-1,-1],
                                                              [6, 6, 6, 6, 6, 6,-1,-1,-1],
                                                              [7, 7, 7, 7, 7, 7, 7,-1,-1],
                                                              [8, 8, 8, 8, 8, 8, 8, 8,-1],
                                                              [9, 9, 9, 9, 9, 9, 9, 9, 9]]).tolist())
        self.assertEqual(g.edges().tolist(), torch.Tensor([[0x80,0],
                                                           [0x40,0],
                                                           [0x20,0],
                                                           [0x10,0],
                                                           [0x8 ,0],
                                                           [0x4 ,0],
                                                           [0x2 ,0],
                                                           [0x1 ,0],
                                                           [0,0x80]]).tolist())
        g = rm_vertex(g, 8)
        self.assertEqual(g.vertices().tolist(), torch.Tensor([[1,-1,-1,-1,-1,-1,-1,-1,-1],
                                                              [2, 2,-1,-1,-1,-1,-1,-1,-1],
                                                              [3, 3, 3,-1,-1,-1,-1,-1,-1],
                                                              [4, 4, 4, 4,-1,-1,-1,-1,-1],
                                                              [5, 5, 5, 5, 5,-1,-1,-1,-1],
                                                              [6, 6, 6, 6, 6, 6,-1,-1,-1],
                                                              [7, 7, 7, 7, 7, 7, 7,-1,-1],
                                                              [8, 8, 8, 8, 8, 8, 8, 8,-1]]).tolist())
        self.assertEqual(g.edges().tolist(), torch.Tensor([[0x80,0],
                                                           [0x40,0],
                                                           [0x20,0],
                                                           [0x10,0],
                                                           [0x8 ,0],
                                                           [0x4 ,0],
                                                           [0x2 ,0],
                                                           [0x1 ,0]]).tolist())
