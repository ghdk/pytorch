import multiprocessing as mp
from pathlib import Path
import unittest
import sys
import os
import struct
import torch
import tempfile
import shutil
import lmdb
import random
import traceback
from graphdb import graphdb
from bitarray import bitarray
from graph import graph

class Test(unittest.TestCase):
    
    def setUp(self):
        self._dir = tempfile.mkdtemp(dir="./")
        os.chdir(self._dir)
        
    def rm_test_dir(f):
        def impl(self):
            try: f(self)
            except Exception as e: traceback.print_exception(e, file=sys.stderr)
            else: shutil.rmtree(self._dir)  # leave behind if there was an assert
            finally: os.chdir('..')
        return impl

    @rm_test_dir
    def test_graphdb_cardinalities(self):
        graphdb.test.test_graphdb_cardinalities()

    @rm_test_dir
    def test_graphdb_int_api(self):
        graphdb.test.test_graphdb_int_api()

    @rm_test_dir
    def test_graphdb_tensor_api(self):
        graphdb.test.test_graphdb_tensor_api()

    @rm_test_dir
    def test_graphdb_string_api(self):
        graphdb.test.test_graphdb_string_api()

    @rm_test_dir
    def test_graphdb_key_type(self):
        graphdb.test.test_graphdb_key_type()

    @rm_test_dir
    def test_graphdb_list_write_and_read(self):
        graphdb.test.test_graphdb_list_write_and_read()

    @rm_test_dir
    def test_graphdb_list_write_and_clear(self):
        graphdb.test.test_graphdb_list_write_and_clear()
        
    @rm_test_dir
    def test_graphdb_hash_write(self):
        graphdb.test.test_graphdb_hash_write()

    @rm_test_dir
    def test_graphdb_hash_remove(self):
        graphdb.test.test_graphdb_hash_remove()
        
    @rm_test_dir
    def test_graphdb_hash_visit(self):
        graphdb.test.test_graphdb_hash_visit()

    @rm_test_dir
    def test_graphdb_find_head(self):
        graphdb.test.test_graphdb_find_head()

    @rm_test_dir
    def test_graphdb_cursor_next(self):
        graphdb.test.test_graphdb_cursor_next()

    @rm_test_dir
    def test_graphdb_transaction_node(self):
        graphdb.test.test_graphdb_transaction_node()

    @rm_test_dir
    def test_graphdb_transaction_api(self):
        graphdb.test.test_graphdb_transaction_api()        

    @classmethod
    def impl_graph_make_graph_db(cls, filename, graph_i):
        env = lmdb.open(filename, subdir=False, readonly=False, create=False, max_dbs=(len(graphdb.SCHEMA)))
        VS  = env.open_db(graphdb.SCHEMA[graphdb.VERTEX_SET].encode())
        VSL = env.open_db(graphdb.SCHEMA[graphdb.VERTEX_SET_L].encode())
        AM  = env.open_db(graphdb.SCHEMA[graphdb.ADJACENCY_MATRIX].encode())
        AML = env.open_db(graphdb.SCHEMA[graphdb.ADJACENCY_MATRIX_L].encode())
        with env.begin(db=VS, write=True) as txn:
            value = txn.get(struct.pack('=Q', graph_i))
            assert 16 == len(value), f"len={len(value)}"
            head,tail = struct.unpack('=QQ', value)
            assert 0 != head and 0 == tail, f"head={head}, tail={tail}"
            with env.begin(db=VSL, parent=txn, write=True) as txnc:
                value = txnc.get(struct.pack('=QQ', head, tail))
                assert graphdb.PAGE_SIZE == len(value), f"len={len(value)}"
        with env.begin(db=AM, write=True) as txn:
            for i in range(graphdb.PAGE_SIZE):
                value = txn.get(struct.pack('=QQ', graph_i, i))
                assert 16 == len(value), f"len={len(value)}"
                head,tail = struct.unpack('=QQ', value)
                assert 0 != head and 0 == tail, f"head={head}, tail={tail}"
                with env.begin(db=AML, parent=txn, write=True) as txnc:
                    value = txnc.get(struct.pack('=QQ', head, tail))
                    assert graphdb.PAGE_SIZE == len(value), f"len={len(value)}"

    @rm_test_dir
    def test_graph_make_graph_db(self):
        filename = f"./test.db"
        graph_i = 0xACE
        def sub():
            txn = graphdb.make_transaction_node(filename)
            g = graph.make_graph_db(txn, graph_i)
        sub()
        ctx = mp.get_context('spawn')
        p = ctx.Process(target=self.__class__.impl_graph_make_graph_db, args=(filename, graph_i))
        p.start()
        p.join()
        self.assertEqual(0, -1 * p.exitcode)

    @classmethod
    def impl_graph_vertex_add(cls, filename, graph_i):
        env = lmdb.open(filename, subdir=False, readonly=False, create=False, max_dbs=(len(graphdb.SCHEMA)))
        VS  = env.open_db(graphdb.SCHEMA[graphdb.VERTEX_SET].encode())
        VSL = env.open_db(graphdb.SCHEMA[graphdb.VERTEX_SET_L].encode())
        with env.begin(db=VS, write=True) as txn:
            value = txn.get(struct.pack('=Q', graph_i))
            assert 16 == len(value), f"len={len(value)}"
            head,tail = struct.unpack('=QQ', value)
            assert 0 != head and 0 == tail, f"head={head}, tail={tail}"
            with env.begin(db=VSL, parent=txn, write=True) as txnc:
                value = txnc.get(struct.pack('=QQ', head, tail))
                assert graphdb.PAGE_SIZE == len(value), f"len={len(value)}"
                assert 0x80 == value[0], f"[0]={value[0]}"
                assert 0x1  == value[-1], f"[-1]={value[-1]}"

    @rm_test_dir
    def test_graph_vertex_add(self):
        filename = f"./test.db"
        graph_i = 0xACE
        def sub():
            txn = graphdb.make_transaction_node(filename)
            g = graph.make_graph_db(txn, graph_i)
            v = g.vertex(0,True)
            v = g.vertex(graphdb.PAGE_SIZE * bitarray.CELL_SIZE - 1,True)  # the last vertex of the page
        sub()
        ctx = mp.get_context('spawn')
        p = ctx.Process(target=self.__class__.impl_graph_vertex_add, args=(filename, graph_i))
        p.start()
        p.join()
        self.assertEqual(0, -1 * p.exitcode)

    @classmethod
    def impl_graph_vertex_expand(cls, filename, graph_i):
        env = lmdb.open(filename, subdir=False, readonly=False, create=False, max_dbs=(len(graphdb.SCHEMA)))
        VS  = env.open_db(graphdb.SCHEMA[graphdb.VERTEX_SET].encode())
        VSL = env.open_db(graphdb.SCHEMA[graphdb.VERTEX_SET_L].encode())
        AM  = env.open_db(graphdb.SCHEMA[graphdb.ADJACENCY_MATRIX].encode())
        AML = env.open_db(graphdb.SCHEMA[graphdb.ADJACENCY_MATRIX_L].encode())
        
        with env.begin(db=VS, write=True) as txn:  # Vertex Set
            with txn.cursor() as crs:
                vtx_set = [i for i in iter(crs)]
                assert 1 == len(vtx_set)
                vtx_set_key = vtx_set[0][0]
                vtx_set_key, *_ = struct.unpack('=Q', vtx_set_key)
                assert graph_i == vtx_set_key, f"{graph_i} != {vtx_set_key}"
                vtx_set_value = vtx_set[0][1]
                head, tail = struct.unpack('=QQ', vtx_set_value)
                assert 0 != head and 0 == tail, f"head={head}, tail={tail}"
        with env.begin(db=VSL, write=True) as txn:  # Vertex Set Linked List
            with txn.cursor() as crs:
                pages = [i for i in iter(crs)]
                assert 3 == len(pages)
                keyA, valueA = pages[0]
                headA, tailA = struct.unpack('=QQ', keyA)
                keyB, valueB = pages[1]
                headB, tailB = struct.unpack('=QQ', keyB)
                keyC, valueC = pages[2]
                headC, tailC = struct.unpack('=QQ', keyC)
                assert 0 == headA, f"{headA}"
                assert headB == headC, f"{headB} == {headC}"
                assert [0,0,1] == [tailA, tailB, tailC], f"{[tailA, tailB, tailC]}"
        adj_mtx_heads = []
        with env.begin(db=AM, write=True) as txn:  # Adjacency Matrix
            with txn.cursor() as crs:
                total = 2 * graphdb.PAGE_SIZE * bitarray.CELL_SIZE
                adj_mtx = [i for i in iter(crs)]
                assert total == len(adj_mtx), f"{len(adj_mtx)}"
                adj_mtx_keys = [struct.unpack('=QQ', key) for key,_ in adj_mtx]
                adj_mtx_heads = [struct.unpack('=QQ', head) for _,head in adj_mtx]                
                expected = [(graph_i, vtx) for vtx in range(total)]
                assert expected == sorted(adj_mtx_keys), f"{adj_mtx_keys}"
        with env.begin(db=AML, write=True) as txn:  # Adjacency Matrix Linked List
            with txn.cursor() as crs:
                expected  = [(head,tail) for head,tail in adj_mtx_heads]
                expected += [(head,   1) for head,_    in adj_mtx_heads]
                expected += [(0,0)]  # the AML's metadata list
                expected = sorted(expected)
                pages = [i for i in iter(crs)]
                pages_keys = [struct.unpack('=QQ', key) for key,_ in pages]
                pages_keys = sorted(pages_keys)
                assert expected == pages_keys, f"{expected} == {pages_keys}"

    @rm_test_dir
    def test_graph_vertex_expand(self):
        filename = f"./test.db"
        graph_i = 0xACE
        def sub():
            txn = graphdb.make_transaction_node(filename)
            g = graph.make_graph_db(txn, graph_i)
            for i in range(graphdb.PAGE_SIZE * bitarray.CELL_SIZE):
                v = g.vertex(i,True)
            v = g.vertex(0,True)  # force expansion
        sub()
        ctx = mp.get_context('spawn')
        p = ctx.Process(target=self.__class__.impl_graph_vertex_expand, args=(filename, graph_i))
        p.start()
        p.join()
        self.assertEqual(0, -1 * p.exitcode)

    @classmethod
    def impl_graph_vertex_delete(cls, filename, graph_i, indexes):
        env = lmdb.open(filename, subdir=False, readonly=False, create=False, max_dbs=(len(graphdb.SCHEMA)))
        VS  = env.open_db(graphdb.SCHEMA[graphdb.VERTEX_SET].encode())
        VSL = env.open_db(graphdb.SCHEMA[graphdb.VERTEX_SET_L].encode())
        AM  = env.open_db(graphdb.SCHEMA[graphdb.ADJACENCY_MATRIX].encode())
        AML = env.open_db(graphdb.SCHEMA[graphdb.ADJACENCY_MATRIX_L].encode())

        head,tail = (None, None)

        with env.begin(db=VS, write=True) as txn:  # Vertex Set
            with txn.cursor() as crs:
                vtx_set = [i for i in iter(crs)]
                assert 1 == len(vtx_set)
                vtx_set_key = vtx_set[0][0]
                vtx_set_key, *_ = struct.unpack('=Q', vtx_set_key)
                assert graph_i == vtx_set_key, f"{graph_i} != {vtx_set_key}"
                vtx_set_value = vtx_set[0][1]
                head, tail = struct.unpack('=QQ', vtx_set_value)
                assert 0 != head and 0 == tail, f"head={head}, tail={tail}"
        with env.begin(db=VSL, write=True) as txn:  # Vertex Set Linked List
            with txn.cursor() as crs:
                pages = [i for i in iter(crs)]
                assert 2 == len(pages)
                keyA, valueA = pages[0]
                headA, tailA = struct.unpack('=QQ', keyA)
                keyB, valueB = pages[1]
                headB, tailB = struct.unpack('=QQ', keyB)
                assert 0 == headA, f"{headA}"
                assert headB == head, f"{headB} == {head}"
                assert [0,0] == [tailA, tailB], f"{[tailA, tailB]}"
                tensor = torch.frombuffer(valueB, dtype=torch.uint8, count=len(valueB), requires_grad=False)
                
                # on error print the whole tensor
                torch.set_printoptions(profile="full")

                # at indexes we should get zero
                assert not bitarray.get(tensor, indexes[0]), f"{tensor}"
                assert not bitarray.get(tensor, indexes[1]), f"{tensor}"
                assert not bitarray.get(tensor, indexes[2]), f"{tensor}"
                
                # lets evaluate the surrounding bits
                assert bitarray.get(tensor, indexes[0]+1), f"{tensor}"
                assert bitarray.get(tensor, indexes[1]-1), f"{tensor}"
                assert bitarray.get(tensor, indexes[1]+1), f"{tensor}"
                assert bitarray.get(tensor, indexes[2]-1), f"{tensor}"

    @rm_test_dir
    def test_graph_vertex_delete(self):
        filename = f"./test.db"
        graph_i = 0xACE
        indexes = []
        def sub():
            nonlocal indexes
            txn = graphdb.make_transaction_node(filename)
            g = graph.make_graph_db(txn, graph_i)
            for i in range(graphdb.PAGE_SIZE * bitarray.CELL_SIZE):
                v = g.vertex(i,True)
            indexes = [0,
                       graphdb.PAGE_SIZE * bitarray.CELL_SIZE // 2,
                       graphdb.PAGE_SIZE * bitarray.CELL_SIZE - 1]
            v = g.vertex(indexes[0],False)
            v = g.vertex(indexes[1], False)
            v = g.vertex(indexes[2], False)
        sub()
        ctx = mp.get_context('spawn')
        p = ctx.Process(target=self.__class__.impl_graph_vertex_delete, args=(filename, graph_i, indexes))
        p.start()
        p.join()
        self.assertEqual(0, -1 * p.exitcode)

    @rm_test_dir
    def test_graph_is_vertex(self):
        filename = f"./test.db"
        graph_i = 0xACE
        txn = graphdb.make_transaction_node(filename)
        g = graph.make_graph_db(txn, graph_i)
        expected = []
        for i in range(1, graphdb.PAGE_SIZE * bitarray.CELL_SIZE, 2):
            expected.append(g.vertex(i,True))
        received = []
        for i in range(graphdb.PAGE_SIZE * bitarray.CELL_SIZE):
            if g.is_vertex(i): received.append(i)
        self.assertEqual(expected, received)

    @rm_test_dir
    def test_graph_is_edge(self):
        filename = f"./test.db"
        graph_i = 0xACE
        txn = graphdb.make_transaction_node(filename)
        g = graph.make_graph_db(txn, graph_i)
        expected = []
        for i in range(1, graphdb.PAGE_SIZE * bitarray.CELL_SIZE // 1000, 2):
            r = random.randint(0, i)
            expected.append((i,r))
            g.vertex(i,True)
            g.vertex(r,True)
            g.edge(i,r,True)
        received = []
        for i in range(graphdb.PAGE_SIZE * bitarray.CELL_SIZE // 1000):
            for j in range(graphdb.PAGE_SIZE * bitarray.CELL_SIZE // 1000):
                if g.is_edge(i,j): received.append((i,j))
        self.assertEqual(expected, received)

    @rm_test_dir
    def test_graph_iter_vertex(self):
        filename = f"./test.db"
        graph_i = 0xACE
        txn = graphdb.make_transaction_node(filename)
        g = graph.make_graph_db(txn, graph_i)
        received = []
        g.vertices(lambda n : received.append(n), 0,0,1)
        self.assertEqual([], received)
        expected = [g.vertex(0,True) for _ in range(10)]
        g.vertices(lambda n : received.append(n), 0,0,1)
        self.assertEqual(sorted(expected), sorted(received))

    @rm_test_dir
    def test_graph_iter_edge(self):
        filename = f"./test.db"
        graph_i = 0xACE
        txn = graphdb.make_transaction_node(filename)
        g = graph.make_graph_db(txn, graph_i)
        received = []
        g.edges(lambda x,y: received.append((x,y)), 0,0,1)
        self.assertEqual([], received)
        expected = []
        for i in range(1, graphdb.PAGE_SIZE * bitarray.CELL_SIZE, 2):
            r = random.randint(0, i)
            x = g.vertex(i,True)
            y = g.vertex(r,True)
            expected.append((x,y))
            g.edge(x,y,True)
        received = []
        g.edges(lambda x,y: received.append((x,y)), 0,0,1)
        self.assertEqual(sorted(expected), sorted(received))

    @rm_test_dir
    def test_graphdb_feature_write_and_read(self):
        graphdb.test.test_graphdb_feature_write_and_read()
        
    def test_db_keys_factories(self):
        k = graphdb.make_list_key(1,2)
        self.assertEqual((1,2), graphdb.view_list_key(k))

