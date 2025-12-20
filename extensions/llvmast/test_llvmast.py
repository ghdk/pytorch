from multiprocessing import cpu_count, Pool
from pathlib import Path
import resource
import unittest
import sys
import os
import torch
import tempfile
import shutil
import lmdb
import traceback
from contextlib import redirect_stderr
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
            print("rss", resource.getrusage(resource.RUSAGE_SELF).ru_maxrss + resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss, file=sys.stderr, end=' ')
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
            # NB. the exception on rc != 0 allows us to keep the log file with the AST

    @rm_test_dir
    def test_visit_record_decl(self):
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
            graph_i = graphdb.make_graph_vtx_set_key(llvmast.GRAPH)
            graphdb.graph.init(txn, graph_i)
            
            # Traverse all the vertices of the graph, and keep RECORD_DECLs.
    
            class ctx: pass
            def callback(vtx):

                def name_cb(buf, h4sh):
                    if not hasattr(ctx, 'name'): ctx.name = None
                    ctx.name = buf.decode('ascii')
                    return 0
                def rule_cb(buf, h4sh):
                    if not hasattr(ctx, 'rule'): ctx.rule = None
                    ctx.rule = int.from_bytes(buf, byteorder='little')
                    return 0
                
                key = graphdb.make_vertex_feature_key(llvmast.GRAPH, vtx, llvmast.feature.NAME)
                graphdb.feature.vertex.visit(txn, key, name_cb)
                key = graphdb.make_vertex_feature_key(llvmast.GRAPH, vtx, llvmast.feature.RULE)
                graphdb.feature.vertex.visit(txn, key, rule_cb)
                
                if not hasattr(ctx, 'reduce'): ctx.reduce = []
                if not hasattr(ctx, 'vertices'): ctx.vertices = []
                if llvmast.rule.RECORD_DECL == ctx.rule:
                    ctx.reduce.append(ctx.name)
                    ctx.vertices.append(vtx)
                return 0

            graphdb.graph.vertices(txn, graph_i, callback)
            self.assertEqual(['Bar', 'Cunion', 'Foo'], sorted(ctx.reduce), ctx.reduce)
            
            # Use the hashes to get the vertices from the values.
            
            class ctxx: pass
            def callback(iter, key):
                key = graphdb.view_vertex_feature_key(key)
                if not hasattr(ctxx, 'vertex'): ctxx.vertex = None
                ctxx.vertex = key[1]
                return 0
            
            def evaluate(token):
                graphdb.hash.vertex.visit(txn, token.encode('ascii'), callback)
                self.assertEqual(ctxx.vertex, ctx.vertices[ctx.reduce.index(token)])
            
            evaluate('Foo')
            evaluate('Bar')
            evaluate('Cunion')

    @rm_test_dir
    def test_visit_function_decl(self):
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
            graph_i = graphdb.make_graph_vtx_set_key(llvmast.GRAPH)
            graphdb.graph.init(txn, graph_i)
            
            class ctx: pass        
            def head_cb(iter, key):
                def name_cb(buf, h4sh):
                    if not hasattr(ctx, 'name'): ctx.name = None
                    ctx.name = buf.decode('ascii')
                    return 0
                def rule_cb(buf, h4sh):
                    if not hasattr(ctx, 'rule'): ctx.rule = None
                    ctx.rule = int.from_bytes(buf, byteorder='little')
                    return 0
    
                key = graphdb.view_vertex_feature_key(key)            
                name_k = graphdb.make_vertex_feature_key(key[0], key[1], llvmast.feature.NAME)
                graphdb.feature.vertex.visit(txn, name_k, name_cb)
                rule_k = graphdb.make_vertex_feature_key(key[0], key[1], llvmast.feature.RULE)
                graphdb.feature.vertex.visit(txn, rule_k, rule_cb)
                return 0
    
            def evaluate(token):
                graphdb.hash.vertex.visit(txn, token.encode('ascii'), head_cb)
                self.assertEqual(token, ctx.name)
                self.assertEqual(llvmast.rule.FUNCTION_DECL, ctx.rule)
    
            evaluate("Pure::virt")
            evaluate("Pure::~Pure")
            evaluate("Foo::foo")
            evaluate("Foo::virt")
            evaluate("Foo::Foo")
            evaluate("main")

    @rm_test_dir
    def test_visit_parm_var_decl(self):
        maincc = "./main.cc"

        with open(maincc, "w") as f:
            print("""
int main(int argc, char** argv)
{
    return sizeof(argc);
}
            """,
            file=f)
        
        self.assertTrue(os.path.isfile(maincc))
        llvmast.llvmast(['-c', maincc] + ['--'] + CFLAGS)

        with graphdb.TransactionNode(llvmast.DB_FILE) as txn:
            graph_i = graphdb.make_graph_vtx_set_key(llvmast.GRAPH)
            # graphdb.graph.init(txn, graph_i)
            
            class ctx: pass        
            def head_cb(iter, key):
                def name_cb(buf, h4sh):
                    if not hasattr(ctx, 'name'): ctx.name = None
                    ctx.name = buf.decode('ascii')
                    return 0
                def rule_cb(buf, h4sh):
                    if not hasattr(ctx, 'rule'): ctx.rule = None
                    ctx.rule = int.from_bytes(buf, byteorder='little')
                    return 0
    
                key = graphdb.view_vertex_feature_key(key)            
                name_k = graphdb.make_vertex_feature_key(key[0], key[1], llvmast.feature.NAME)
                graphdb.feature.vertex.visit(txn, name_k, name_cb)
                rule_k = graphdb.make_vertex_feature_key(key[0], key[1], llvmast.feature.RULE)
                graphdb.feature.vertex.visit(txn, rule_k, rule_cb)
                return 0
    
            def evaluate(token):
                graphdb.hash.vertex.visit(txn, token.encode('ascii'), head_cb)
                self.assertEqual(token, ctx.name)
                self.assertEqual(llvmast.rule.PARM_VAR_DECL, ctx.rule)
    
            evaluate("argc")
            evaluate("argv")

        with graphdb.TransactionNode(llvmast.DB_FILE) as txn:
            graph_i = graphdb.make_graph_vtx_set_key(llvmast.GRAPH)

            class ctx: pass
            def edges_cb(vtx_i, vtx_j):
                def name_cb(buf, h4sh):
                    if not hasattr(ctx, 'names'): ctx.names = []
                    ctx.names.append(buf.decode('ascii'))
                    return 0
                def rule_cb(buf, h4sh):
                    if not hasattr(ctx, 'rules'): ctx.rules = []
                    ctx.rules.append(int.from_bytes(buf, byteorder='little'))
                    return 0
                
                key = graphdb.make_vertex_feature_key(llvmast.GRAPH, vtx_i, llvmast.feature.NAME)
                graphdb.feature.vertex.visit(txn, key, name_cb)
                key = graphdb.make_vertex_feature_key(llvmast.GRAPH, vtx_i, llvmast.feature.RULE)
                graphdb.feature.vertex.visit(txn, key, rule_cb)

                key = graphdb.make_vertex_feature_key(llvmast.GRAPH, vtx_j, llvmast.feature.NAME)
                graphdb.feature.vertex.visit(txn, key, name_cb)
                key = graphdb.make_vertex_feature_key(llvmast.GRAPH, vtx_j, llvmast.feature.RULE)
                graphdb.feature.vertex.visit(txn, key, rule_cb)
                return 0

            graphdb.graph.edges(txn, graph_i, edges_cb)

            # the ctx now should have a list of names and a list of rules;
            # every two elements of those lists correspond to an edge;
            # vertices are random, so symbols might appear in different order,
            # and so edges might be in different order too

            for i in range(0, len(ctx.names), 2):
                name_slice = ctx.names[i:i+2]
                rule_slice = ctx.rules[i:i+2]
                bitmap = [False] * 4
                bitmap[0] = "main" in name_slice
                bitmap[1] = "argv" in name_slice or "argc" in name_slice
                bitmap[2] = llvmast.rule.FUNCTION_DECL in rule_slice
                bitmap[3] = llvmast.rule.PARM_VAR_DECL in rule_slice
                self.assertTrue(all(bitmap), f"{ctx.names}, {ctx.rules}")

    @rm_test_dir
    def test_context_location(self):
        maincc = "main.cc"
        mainh  = "main.h"

        with open(mainh, "w") as f:
            print("""
static int foo() { return 5; }
            """,
            file=f)

        with open(maincc, "w") as f:
            print("""
#include "main.h"
int main(int argc, char** argv)
{
    return foo();
}
            """,
            file=f)
        
        self.assertTrue(os.path.isfile(maincc))
        llvmast.llvmast(['-c', maincc] + ['--'] + CFLAGS)

        with graphdb.TransactionNode(llvmast.DB_FILE) as txn:
            graph_i = graphdb.make_graph_vtx_set_key(llvmast.GRAPH)
            graphdb.graph.init(txn, graph_i)
            
            class ctx: pass        
            def head_cb(iter, key):
                def name_cb(buf, h4sh):
                    if not hasattr(ctx, 'name'): ctx.name = None
                    ctx.name = buf.decode('ascii')
                    return 0
                def file_cb(buf, h4sh):
                    if not hasattr(ctx, 'file'): ctx.file = None
                    ctx.file = buf.decode('ascii')
                    return 0
    
                key = graphdb.view_vertex_feature_key(key)            
                name_k = graphdb.make_vertex_feature_key(key[0], key[1], llvmast.feature.NAME)
                graphdb.feature.vertex.visit(txn, name_k, name_cb)
                file_k = graphdb.make_vertex_feature_key(key[0], key[1], llvmast.feature.FILE)
                graphdb.feature.vertex.visit(txn, file_k, file_cb)
                return 0
    
            def evaluate(token, file):
                graphdb.hash.vertex.visit(txn, token.encode('ascii'), head_cb)
                self.assertEqual(token, ctx.name)
                self.assertTrue(file in ctx.file, f"{file}, {ctx.file}")
    
            evaluate("foo", mainh)
            evaluate("main", maincc)
