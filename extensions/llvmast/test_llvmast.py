from multiprocessing import cpu_count, Pool
from pathlib import Path
import unittest
import sys
import os
import torch
import tempfile
import shutil
import lmdb
import traceback
from inspect import currentframe, getframeinfo
from bitarray import bitarray
from graph import graph
from graphdb import graphdb
from llvmast import llvmast
import cached_cflags

CFLAGS = cached_cflags.CFLAGS
RAMFS = os.environ.get("RAMFS", "")

class Test(unittest.TestCase):
    
    def setUp(self):
        self._top = os.getcwd()
        self._dir = tempfile.mkdtemp(dir=RAMFS if RAMFS else self._top)
        os.chdir(self._dir)

    def rm_test_dir(f):
        def impl(self):
            try: f(self)
            except Exception as e: traceback.print_exception(e, file=sys.stderr)
            else: shutil.rmtree(self._dir)  # leave behind if there was an assert
            finally: os.chdir(self._top)
        return impl

    @rm_test_dir
    def test_clang(self):
        llvmast.llvmast(['-c', f"{self._top}/llvmast.cc"] + ['--'] + CFLAGS)

    @rm_test_dir
    def test_clang_vendor(self):
        import subprocess
        with open('clang.log', 'a') as log:
            cmd = ['clang'] + CFLAGS + ['-Xclang', '-ast-dump', '-fsyntax-only', '-fno-diagnostics-color'] + ['-c', f"{self._top}/llvmast.cc"]
            print(cmd, file=log)
            subprocess.run(cmd, stdout=log, stderr=log, check=True)

    @rm_test_dir
    def test_db_has_record_decl(self):
        maincc = "./main.cc"

        with open(maincc, "w") as f:
            print("""
class Foo {};
struct Bar {};
union Cunion {};

int main(int argc, char** argv)
{
    return 0;
}
            """,
            file=f)
        
        self.assertTrue(os.path.isfile(maincc))
        llvmast.llvmast(['-c', maincc] + ['--'] + CFLAGS)
        
        with graphdb.TransactionNode(llvmast.DB_FILE) as txn:
            g = graph.make_graph_db(txn, llvmast.GRAPH)
            
            # Traverse all the vertices of the graph, and keep RECORD_DECLs.
    
            class ctx: pass
            def callback(vtx):

                def name_cb(buf, h4sh):
                    if not hasattr(ctx, 'name'): ctx.name = None
                    ctx.name = buf.decode('ascii')
                    return 0
                def type_cb(buf, h4sh):
                    if not hasattr(ctx, 'type'): ctx.type = None
                    ctx.type = int.from_bytes(buf, byteorder='little')
                    return 0
                
                key = graphdb.make_vertex_feature_key(llvmast.GRAPH, vtx, llvmast.feature.NAME)
                graphdb.feature.vertex.visit(txn, key, name_cb)
                key = graphdb.make_vertex_feature_key(llvmast.GRAPH, vtx, llvmast.feature.TYPE)
                graphdb.feature.vertex.visit(txn, key, type_cb)
                
                if not hasattr(ctx, 'reduce'): ctx.reduce = []
                if not hasattr(ctx, 'vertices'): ctx.vertices = []
                if llvmast.type.RECORD_DECL == ctx.type:
                    ctx.reduce.append(ctx.name)
                    ctx.vertices.append(vtx)
                return 0
                
            g.vertices(callback, 0,0,1)
            self.assertEqual(['Bar', 'Cunion', 'Foo'], sorted(ctx.reduce), ctx.reduce)
            
            # Use the hashes to get the vertices from the values.
            
            class ctxx: pass
            def callback(iter, key):
                key = graphdb.view_vertex_feature_key(key)
                if not hasattr(ctxx, 'vertex'): ctxx.vertex = None
                ctxx.vertex = key[1]
                return 0
            
            def evaluate(rec):
                graphdb.hash.vertex.visit(txn, rec.encode('ascii'), callback)
                self.assertEqual(ctxx.vertex, ctx.vertices[ctx.reduce.index(rec)])
            
            evaluate('Foo')
            evaluate('Bar')
            evaluate('Cunion')

    @rm_test_dir
    def test_db_has_function_decl(self):
        maincc = "./main.cc"

        with open(maincc, "w") as f:
            print("""
class Pure
{
public:
    virtual void virt() = 0;
    virtual ~Pure() = 0;
};

class Foo : Pure
{
public:
    int foo(void){ return 0; }
    void virt() override { return; };
public:
    Foo() = default;
};

int main(int argc, char** argv)
{
    Foo foo;
    return sizeof(foo);
}
            """,
            file=f)
        
        self.assertTrue(os.path.isfile(maincc))
        llvmast.llvmast(['-c', maincc] + ['--'] + CFLAGS)

        with graphdb.TransactionNode(llvmast.DB_FILE) as txn:
            g = graph.make_graph_db(txn, llvmast.GRAPH)
            
            class ctx: pass        
            def head_cb(iter, key):
                def name_cb(buf, h4sh):
                    if not hasattr(ctx, 'name'): ctx.name = None
                    ctx.name = buf.decode('ascii')
                    return 0
                def type_cb(buf, h4sh):
                    if not hasattr(ctx, 'type'): ctx.type = None
                    ctx.type = int.from_bytes(buf, byteorder='little')
                    return 0
    
                key = graphdb.view_vertex_feature_key(key)            
                name_k = graphdb.make_vertex_feature_key(key[0], key[1], llvmast.feature.NAME)
                graphdb.feature.vertex.visit(txn, name_k, name_cb)
                type_k = graphdb.make_vertex_feature_key(key[0], key[1], llvmast.feature.TYPE)
                graphdb.feature.vertex.visit(txn, type_k, type_cb)
                return 0
    
            def evaluate(func):
                graphdb.hash.vertex.visit(txn, func.encode('ascii'), head_cb)
                self.assertEqual(func, ctx.name)
                self.assertEqual(llvmast.type.FUNCTION_DECL, ctx.type)
    
            evaluate("Pure::virt")
            evaluate("Pure::~Pure")
            evaluate("Foo::foo")
            evaluate("Foo::virt")
            evaluate("Foo::Foo")
            evaluate("main")
