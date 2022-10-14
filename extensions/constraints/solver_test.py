import unittest
import torch
import time
from csp.solver import Permutation
from csp.solver import cxxfoo
from csp.solver import cxxfoo2
from csp.conflict_directed_backjumping import ConflictDirectedBackjumping as CBJ
from csp.forward_checking import ForwardChecking as FC
from csp.constraint_mutex import ConstraintMutex as Mutex
from iter.generated_iterator import GeneratedIterableTensor as GT

ONE_SEC = 1000000000  #ns

class Test(unittest.TestCase):

    def setUp(self):
        self._small = {'s': [0,0,0],
                       'd': [[1.,1.,0.],[2.,3.,4.],[1.,5.,0.]],
                       'cs': None,  # conflict set
                       'ps': None}  # pruning set
        conflict_set = [0 for i in range(len(self._small['s']) * len(self._small['s']))]
        pruning_set = [[dimension[i] if not i else len(self._small['d']) for i in range(len(dimension))]
                       for _,dimension in enumerate(self._small['d'])]
        self._small['cs'] = conflict_set
        self._small['ps'] = pruning_set
        self._large = {'s': None,
                       'd': None,
                       'cs': None,  # conflict set
                       'ps': None}  # pruning set
        state = [0 for i in range(65)]
        dom = [0.25, 0.33, 0.5, 0.67, 0.75, 1, 1.25, 1.33, 1.5, 1.67, 1.75, 2, 2.25, 2.33, 2.5, 2.67, 2.75, 3, 3.25, 3.33, 3.5, 3.67, 3.75, 4]
        domains = [[len(dom)] for i in range(len(state))]
        domains = [l+dom for l in domains]
        self._large['s'] = state
        self._large['d'] = domains
        conflict_set = [0 for i in range(len(self._large['s']) * len(self._large['s']))]
        pruning_set = [[dimension[i] if not i else len(self._large['d']) for i in range(len(dimension))]
                       for _,dimension in enumerate(self._large['d'])]
        self._large['cs'] = conflict_set
        self._large['ps'] = pruning_set
        self._count_threshold = 1417399

    def test_constraints_solver_permutation(self):
        state = torch.Tensor(self._small['s'])
        domains = torch.Tensor(self._small['d'])
        p = Permutation(state, domains, [])
        self.assertEqual([[1., 3., 5.], [1., 4., 5.]],
                         [t.tolist() for t in [i.detach().clone() for i in GT(p)]])
        self.assertEqual([1., 4., 5.], state.tolist())  # the tensor's storage is shared

    def test_conflict_directed_backjumping_without_conflict_set(self):
        state = torch.Tensor(self._small['s'])
        domains = torch.Tensor(self._small['d'])
        # without CBJ
        p = Permutation(state, domains, [])
        solA = [t.tolist() for t in [i.detach().clone() for i in GT(p)]]
        # with CBJ but no conflict sets
        cs = torch.tensor(0, dtype=torch.int64)
        cbj = CBJ(cs)
        p = Permutation(state, domains, [cbj])
        solB = [t.tolist() for t in [i.detach().clone() for i in GT(p)]]
        self.assertEqual(solA, solB)

    def test_conflict_directed_backjumping_with_conflict_set(self):
        state = torch.Tensor(self._small['s'])
        domains = torch.Tensor(self._small['d'])
        # Variable 2 conflicts with variable 1. We will backjump to variable 1,
        # and we will instantiate it with its 2nd value. We will call CBJ on the
        # new assignment which will reset the conflict set of variable 2.
        cs = torch.tensor(self._small['cs'])
        cs[len(state) * 2 + 1] = 1
        cbj = CBJ(cs)
        p = Permutation(state, domains, [cbj])
        self.assertEqual([[1., 3., 5.], [1., 4., 5.]],
                         [t.tolist() for t in [i.detach().clone() for i in GT(p)]])
        self.assertTrue(not any(cs.tolist()))
        # This time variable 2 conflicts with variable 0. Variable 0 has only
        # one value in its domain, hence when we backjump to it we have no other
        # value to instantiate it with. CBJ is not called again, hence the
        # conflict set of variable 2 is not cleared.
        cs = torch.tensor(self._small['cs'])
        cs[len(state) * 2 + 0] = 1
        cbj = CBJ(cs)
        p = Permutation(state, domains, [cbj])
        self.assertEqual([[1., 3., 5.]],
                         [t.tolist() for t in [i.detach().clone() for i in GT(p)]])
        self.assertTrue(any(cs.tolist()))
        
    def test_forward_checking(self):
        state = torch.Tensor(self._large['s'])
        sz = state.size()[0]
        domains = torch.Tensor(self._large['d'])
        zero = torch.tensor(0, dtype=torch.int64)
        prunning_set = torch.Tensor(self._large['ps'])
        prunning_set = prunning_set.long()
        fc = FC(prunning_set)
        mutex = Mutex(state, domains, zero, prunning_set)
        p = Permutation(state, domains, [fc,mutex])
        solution = next(iter(GT(p))).detach().clone().tolist()
        for i in range(len(state)):
            prunning_set_area = prunning_set[i][1:3].tolist()
            if not i:
                self.assertEqual([sz,sz], prunning_set_area)
            elif not i % 2:
                self.assertEqual([sz,i-1], prunning_set_area)
            else:
                self.assertEqual([i-1,sz], prunning_set_area)

    def test_time_without_constraints(self):
        state = torch.Tensor(self._large['s'])
        domains = torch.Tensor(self._large['d'])
        p = Permutation(state, domains, [])
        count = 0
        start = time.time_ns()
        for i in GT(p):
            count += 1
            if time.time_ns() - start >= ONE_SEC: break
        print("count:", count, round(count / self._count_threshold, 2))
        self.assertLess(self._count_threshold * 0.95, count)

    def test_time_with_constraint_mutex(self):
        state = torch.Tensor(self._large['s'])
        domains = torch.Tensor(self._large['d'])
        zero = torch.tensor(0, dtype=torch.int64)
        mutex = Mutex(state, domains, zero, zero)
        p = Permutation(state, domains, [mutex])
        count = 0
        start = time.time_ns()
        for i in GT(p):
            count += 1
            if time.time_ns() - start >= ONE_SEC: break
        print("count:", count, round(count / self._count_threshold, 2))
        self.assertLess(self._count_threshold * 0.8, count)

    def test_time_with_conflict_directed_backjumping(self):
        state = torch.Tensor(self._large['s'])
        domains = torch.Tensor(self._large['d'])
        cs = torch.tensor(self._large['cs'])
        cbj = CBJ(cs)
        zero = torch.tensor(0, dtype=torch.int64)
        mutex = Mutex(state, domains, cs, zero)
        p = Permutation(torch.Tensor(state), torch.Tensor(domains), [cbj,mutex])
        count = 0
        start = time.time_ns()
        for i in GT(p):
            count += 1
            if time.time_ns() - start >= ONE_SEC: break
        print("count:", count, round(count / self._count_threshold, 2))
        self.assertLess(self._count_threshold * 0.6, count)

    def test_time_with_forward_checking(self):
        state = torch.Tensor(self._large['s'])
        domains = torch.Tensor(self._large['d'])
        pruning_set = torch.Tensor(self._large['ps'])
        pruning_set = pruning_set.long()
        fc = FC(pruning_set)
        zero = torch.tensor(0, dtype=torch.int64)
        mutex = Mutex(state, domains, zero, pruning_set)
        p = Permutation(torch.Tensor(state), torch.Tensor(domains), [fc,mutex])
        count = 0
        start = time.time_ns()
        for i in GT(p):
            count += 1
            if time.time_ns() - start >= ONE_SEC: break
        print("count:", count, round(count / self._count_threshold, 2))
        self.assertLess(self._count_threshold * 0.6, count)

    def test_time_with_conflict_directed_backjumping_and_forward_checking(self):
        state = torch.Tensor(self._large['s'])
        domains = torch.Tensor(self._large['d'])
        conflict_set = torch.Tensor(self._large['cs'])
        conflict_set = conflict_set.long()
        cbj = CBJ(conflict_set)
        pruning_set = torch.Tensor(self._large['ps'])
        pruning_set = pruning_set.long()
        fc = FC(pruning_set)
        mutex = Mutex(state, domains, conflict_set, pruning_set)
        p = Permutation(torch.Tensor(state), torch.Tensor(domains), [cbj,fc,mutex])
        count = 0
        start = time.time_ns()
        for i in GT(p):
            count += 1
            if time.time_ns() - start >= ONE_SEC: break
        print("count:", count, round(count / self._count_threshold, 2))
        self.assertLess(self._count_threshold * 0.5, count)

if '__main__' == __name__:
    unittest.main(verbosity=2)
