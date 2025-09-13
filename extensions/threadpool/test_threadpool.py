import unittest
import sys
import random
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

    def test_threadpool_launch_random_all(self):
        class Job():
    
            __threadpool_sz__ = Test.__threadpool__.size()
            __record__ = [0] * __threadpool_sz__
    
            def __call__(self, thread):
                # the uniform distribution should result in every thread having
                # executed at least one kernel
                if random.randint(0, Job.__threadpool_sz__-1) == thread:
                    Job.__record__[thread] += 1
                return thread
    
        for i in range(Job.__threadpool_sz__ * Job.__threadpool_sz__):
            Test.__threadpool__.launch(test.Kernel(Job()))
        Test.__threadpool__.wait()
        self.assertTrue(all(Job.__record__), f"{Job.__record__}")

    def test_threadpool_launch_random_any(self):
        class Job():
    
            __threadpool_sz__ = Test.__threadpool__.size()
            __record__ = [0] * __threadpool_sz__
            __rnd__ = random.randint(0, __threadpool_sz__-1)
    
            def __call__(self, thread):
                # only a single thread should execute the kernel
                if Job.__rnd__ == thread:
                    Job.__record__[thread] += 1
                return thread
    
        Test.__threadpool__.launch(test.Kernel(Job()))
        Test.__threadpool__.wait()
        self.assertTrue(not all(Job.__record__) and any(Job.__record__), f"{Job.__record__}")

    def test_threadpool_launch_fanout(self):
        class Job():
            
            __record__ = [0] * Test.__threadpool__.size()
            
            def __call__(self, thread):
                # every thread should execute the kernel
                Job.__record__[thread] += 1
                return thread

        Test.__threadpool__.launch(test.Kernel(Job()))
        Test.__threadpool__.wait()
        self.assertEqual([1] * Test.__threadpool__.size(), Job.__record__, f"{Job.__record__}")
