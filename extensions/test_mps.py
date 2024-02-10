import unittest
import torch

class Test(unittest.TestCase):
    
    def test_mps(self):
        if torch.backends.mps.is_available():
            mps_device = torch.device("mps")
            x = torch.ones(1, device=mps_device)
            print (x)
        else:
            print ("MPS device not found.")
