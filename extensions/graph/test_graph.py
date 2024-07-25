import unittest
import torch
import multiprocessing as mp
from signal import SIGABRT

from bitarray import bitarray as B
from graph import graph as G

def SIGABRT_test_vertex_delete_out_of_bounds():
    g = G.Graph();
    v = g.vertex(G.PAGE_SIZE * B.CELL_SIZE, False)
    
def SIGABRT_test_vertex_add_out_of_bounds():
    g = G.Graph();
    v = g.vertex(G.PAGE_SIZE * B.CELL_SIZE, True)

class Test(unittest.TestCase):
    
    def test_init(self):
        g = G.Graph()
        self.assertEqual([0] * G.PAGE_SIZE, g.vertices().tolist())
        self.assertEqual([[0] * G.PAGE_SIZE] * G.PAGE_SIZE * B.CELL_SIZE, g.edges().tolist())
        
    def test_vertex_add(self):
        g = G.Graph()
        self.assertEqual([0] * G.PAGE_SIZE, g.vertices().tolist())
        self.assertEqual([[0] * G.PAGE_SIZE] * G.PAGE_SIZE * B.CELL_SIZE, g.edges().tolist())
        v = g.vertex(0, True)
        self.assertEqual(0, v)
        self.assertEqual(0x80, g.vertices()[0])
        v = g.vertex(0, True)
        self.assertNotEqual(0, v)

    def test_vertex_add_triggers_expansion(self):
        total = G.PAGE_SIZE * B.CELL_SIZE
        g = G.Graph()
        ret = []
        for i in range(0, total):
            ret.append(g.vertex(i, True))
        self.assertEqual([i for i in range(0, total)], ret)
        # verify the values at the bounds
        self.assertEqual(0xFF, g.vertices()[0])
        self.assertEqual(0xFF, g.vertices()[G.PAGE_SIZE-1])
        # lets trigger expansion
        v = g.vertex(0, True)
        # v must be within the new page
        self.assertTrue(G.PAGE_SIZE * B.CELL_SIZE <= v and v < 2 * G.PAGE_SIZE * B.CELL_SIZE, v)
        # the graph must have been expanded by PAGE_SIZE
        self.assertEqual([2 * G.PAGE_SIZE], list(g.vertices().shape))
        self.assertEqual([2 * G.PAGE_SIZE * B.CELL_SIZE, 2 * G.PAGE_SIZE], list(g.edges().shape))
        # the old region must have been copied
        self.assertEqual(0xFF, g.vertices()[0])
        self.assertEqual(0xFF, g.vertices()[G.PAGE_SIZE-1])

    def test_vertex_delete(self):
        g = G.Graph()
        self.assertEqual([0] * G.PAGE_SIZE, g.vertices().tolist())
        self.assertEqual([[0] * G.PAGE_SIZE] * G.PAGE_SIZE * B.CELL_SIZE, g.edges().tolist())
        a = g.vertex(0, True)
        self.assertEqual(0, a)
        self.assertEqual(0x80, g.vertices()[0])
        b = g.vertex(0, True)
        self.assertNotEqual(0, b)
        self.assertNotEqual(a, b)
        # add edges
        g.edge(a,b,True)
        g.edge(b,a,True)
        g.edge(a,a,True)
        g.edge(b,b,True)
        # delete a
        g.vertex(a, False)
        # verify edges
        self.assertFalse(g.is_edge(a,b))
        self.assertFalse(g.is_edge(b,a))
        self.assertFalse(g.is_edge(a,a))
        self.assertTrue(g.is_edge(b,b))
        # verify vertices
        self.assertFalse(g.is_vertex(a))
        self.assertTrue(g.is_vertex(b))

    def test_vertex_delete_out_of_bounds(self):
        ctx = mp.get_context('spawn')
        p = ctx.Process(target=SIGABRT_test_vertex_delete_out_of_bounds)
        p.start()
        p.join()
        self.assertEqual(SIGABRT, -1 * p.exitcode)

    def test_vertex_add_out_of_bounds(self):
        ctx = mp.get_context('spawn')
        p = ctx.Process(target=SIGABRT_test_vertex_add_out_of_bounds)
        p.start()
        p.join()
        self.assertEqual(SIGABRT, -1 * p.exitcode)

    def test_graph_order(self):
        g = G.Graph()
        self.assertEqual(0, G.order(g))
        g.vertex(0,True)
        self.assertEqual(1, G.order(g))
    
    def test_graph_size(self):
        g = G.Graph()
        self.assertEqual(0, G.size(g))
        total = G.PAGE_SIZE * B.CELL_SIZE
        for i in range(0, total):
            g.vertex(i, True)
        g.edge(0,0,True)
        self.assertEqual(1, G.size(g))
