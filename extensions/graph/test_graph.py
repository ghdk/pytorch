import unittest
import torch
import multiprocessing as mp
from signal import SIGABRT

from bitarray import bitarray as B
from graph import graph as G
from graphdb import graphdb

def SIGABRT_test_vertex_delete_out_of_bounds():
    g = G.make_graph();
    v = g.vertex(graphdb.PAGE_SIZE * B.CELL_SIZE, False)
    
def SIGABRT_test_vertex_add_out_of_bounds():
    g = G.make_graph();
    v = g.vertex(graphdb.PAGE_SIZE * B.CELL_SIZE, True)

class Test(unittest.TestCase):
    
    def test_init(self):
        g = G.make_graph()
        self.assertEqual([], [v for v in g.vertices()])
        self.assertEqual([], [e for e in g.edges()])
        
    def test_vertex_add(self):
        g = G.make_graph()
        self.assertEqual([], [v for v in g.vertices()])
        self.assertEqual([], [e for e in g.edges()])
        v = g.vertex(0, True)
        self.assertEqual(0, v)
        self.assertEqual([0], [v for v in g.vertices()])
        v = g.vertex(0, True)
        self.assertNotEqual(0, v)

    def test_vertex_add_triggers_expansion(self):
        total = graphdb.PAGE_SIZE * B.CELL_SIZE
        g = G.make_graph()
        expected = []
        for i in range(0, total):
            expected.append(g.vertex(i, True))
        received = [v for v in g.vertices()]
        self.assertEqual(sorted(expected), sorted(received))
        # verify the values at the bounds
        self.assertEqual(0, sorted(received)[0])
        self.assertEqual(graphdb.PAGE_SIZE * B.CELL_SIZE - 1, sorted(received)[-1])
        # lets trigger expansion
        v = g.vertex(0, True)
        g.edge(0,v,True)
        g.edge(v,0,True)
        # v must be within the new page
        self.assertTrue(graphdb.PAGE_SIZE * B.CELL_SIZE <= v and v < 2 * graphdb.PAGE_SIZE * B.CELL_SIZE, v)
        # the graph must have been expanded by PAGE_SIZE
        self.assertEqual(graphdb.PAGE_SIZE * B.CELL_SIZE + 1, len(list(g.vertices())))
        self.assertEqual(2, len(list(g.edges())))        
        # the old region must have been copied
        self.assertEqual(0, sorted(received)[0])
        self.assertEqual(graphdb.PAGE_SIZE * B.CELL_SIZE - 1, sorted(received)[-1])

    def test_vertex_delete(self):
        g = G.make_graph()
        self.assertEqual([], [v for v in g.vertices()])
        self.assertEqual([], [e for e in g.edges()])
        a = g.vertex(0, True)
        self.assertEqual(0, a)
        self.assertEqual([0], [v for v in g.vertices()])
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
        g = G.make_graph()
        self.assertEqual(0, G.order(g))
        g.vertex(0,True)
        self.assertEqual(1, G.order(g))
    
    def test_graph_size(self):
        g = G.make_graph()
        self.assertEqual(0, G.size(g, G.UNDIRECTED))
        total = graphdb.PAGE_SIZE * B.CELL_SIZE
        for i in range(0, total):
            g.vertex(i, True)
        g.edge(0,0,True)
        self.assertEqual(1, G.size(g, G.UNDIRECTED))
        # add two edges
        g.edge(1,2,True)
        g.edge(2,1,True)
        self.assertEqual(2, G.size(g, G.UNDIRECTED))
        self.assertEqual(3, G.size(g, G.DIRECTED))
        
    def test_graph_iter_vertex(self):
        g = G.make_graph()
        self.assertEqual([], [v for v in g.vertices()])
        g.vertex(0, True)
        g.vertex(10, True)
        self.assertEqual([0, 10], [v for v in g.vertices()])
    
    def test_graph_iter_edge(self):
        g = G.make_graph()
        self.assertEqual([], [v for v in g.edges()])
        g.vertex(0, True)
        g.vertex(10, True)
        g.edge(0,10,True)
        g.edge(10,0,True)
        
        self.assertTrue(g.is_vertex(0))
        self.assertTrue(g.is_vertex(10))
        self.assertTrue(g.is_edge(0,10))
        self.assertTrue(g.is_edge(10,0))
        
        self.assertEqual([(0,10), (10,0)], [v for v in g.edges()])
