import unittest
import torch
from iter import iter

class Test(unittest.TestCase):

    def test_generated_iterator(self):
        iter.test.test_generated_iterator()

    def test_enumerated_iterator(self):
        iter.test.test_enumerated_iterator()

    def test_mapped_iterator(self):
        iter.test.test_mapped_iterator()

    def test_filtered_iterator(self):
        iter.test.test_filtered_iterator()
