"""Actual head/projection equivalence and constructive/convex certificates."""
import unittest

import torch
from torch import nn

from scene_joint_net import JointSceneNet
from scene_operator_oracle import construct, raw_to_output, rear_nmse, refine


class TableHead(nn.Module):
    def __init__(self, raw, limit):
        super().__init__()
        self.table = torch.atanh(raw/limit).reshape(-1, 513, 20).transpose(1, 2)

    def forward(self, features):
        return self.table


class OracleContract(unittest.TestCase):
    def test_stock_equivalence_and_reachable_target(self):
        torch.set_num_threads(3)
        torch.manual_seed(23)
        net = JointSceneNet()
        x = torch.complex(torch.randn(1, 6, 513, 2), torch.randn(1, 6, 513, 2))
        x[..., (0, -1), :] = x[..., (0, -1), :].real.to(x.dtype)
        raw = torch.randn(1, 6, 513, 5, 2, 2)*.12
        raw[..., 1] *= net.imaginary_endpoint[None, None, :, None, None]
        target = raw_to_output(raw, x, net)
        net.operator_head = TableHead(raw, 2.)
        predicted, _ = net(x)
        torch.testing.assert_close(predicted['base'], target, atol=2e-6, rtol=1e-5)
        unbounded = construct(x, target, net, bounded=False)
        torch.testing.assert_close(raw_to_output(unbounded, x, net), target, atol=2e-6, rtol=1e-5)
        bounded = construct(x, target, net)
        self.assertLess(bounded.abs().max().item(), 2.)
        self.assertTrue(torch.isfinite(torch.atanh(bounded/2)).all())

    def test_zero_input_and_lower_bound(self):
        net = JointSceneNet()
        x = torch.zeros(1, 4, 513, 2, dtype=torch.complex64)
        target = torch.zeros(1, 4, 513, 5, dtype=torch.complex64)
        target[..., 3:] = 1
        target[..., :2] = -(2**-.5)
        raw = construct(x, target, net)
        self.assertTrue(torch.isfinite(raw).all())
        fitted, history = refine(raw, x, target, net, 1, steps=1)
        self.assertEqual(history[-1]['tangent_lower_bound'], 1.)
        self.assertTrue(torch.equal(raw, fitted))
        self.assertTrue(torch.equal(rear_nmse(raw_to_output(raw, x, net), target, 1), torch.ones(2)))

    def test_nonzero_infeasible_case(self):
        net = JointSceneNet()
        x = torch.zeros(1, 3, 513, 2, dtype=torch.complex64)
        x[..., 0] = 1
        target = torch.zeros(1, 3, 513, 5, dtype=torch.complex64)
        target[..., 3:] = 10
        target[..., 0] = 1-10*2**-.5
        target[..., 1] = -10*2**-.5
        raw = construct(x, target, net)
        initial = rear_nmse(raw_to_output(raw, x, net), target, 1).mean()
        fitted, history = refine(raw, x, target, net, 1, steps=100)
        self.assertGreater(history[-1]['tangent_lower_bound'], .1)
        self.assertLessEqual(history[-1]['tangent_lower_bound'], history[-1]['mean_rear_nmse']+1e-6)
        self.assertLessEqual(history[-1]['mean_rear_nmse'], float(initial)+1e-6)
        self.assertLess(fitted.abs().max(), 2)


if __name__ == '__main__':
    unittest.main()
