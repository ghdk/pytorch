import unittest
import sys
import torch
from threadpool.threadpool import test

class Test(unittest.TestCase):
    
    @classmethod
    def setUpClass(cls)->None:
        super(Test, cls).setUpClass()
        Test.__threadpool__ = test.ThreadPool()
    
    @classmethod
    def tearDownClass(cls)->None:
        super(Test, cls).tearDownClass()
        Test.__threadpool__.join()

    def test_threadpool_async_random(self):
        class Job():
            
            __record__ = [0] * Test.__threadpool__.size()
            
            def __call__(self, thread):
                Job.__record__[thread] += 1
                return thread

        for i in range(Test.__threadpool__.size() * Test.__threadpool__.size()):
            Test.__threadpool__.rndasync(test.Functor(Job()))
        Test.__threadpool__.wait()
        self.assertTrue(all(Job.__record__), f"{Job.__record__}")

    def test_threadpool_async_broadcast(self):
        class Job():
            
            __record__ = [0] * Test.__threadpool__.size()
            
            def __call__(self, thread):
                Job.__record__[thread] += 1
                return thread

        Test.__threadpool__.bcasync(test.Functor(Job()))
        Test.__threadpool__.wait()
        self.assertEqual([1] * Test.__threadpool__.size(), Job.__record__, f"{Job.__record__}")
