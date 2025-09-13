from multiprocessing import cpu_count, Pool
from pathlib import Path
import unittest
import sys
import os
import torch
import tempfile
import shutil
import lmdb
from graphdb import graphdb

class Test(unittest.TestCase):
    
    def setUp(self):
        self._dir = tempfile.mkdtemp(dir="./")
        os.chdir(self._dir)
        
    def rm_test_dir(f):
        def impl(self):
            f(self)
            os.chdir('..')
            shutil.rmtree(self._dir)  # leave behind if there was an assert
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
    def test_graphdb_find_slot(self):
        graphdb.test.test_graphdb_find_slot()
