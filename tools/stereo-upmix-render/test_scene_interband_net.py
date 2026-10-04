"""Contracts for band mixing, causal state, joint projection and gradients."""
import unittest

import torch

from scene_joint_net import analysis, synthesize
from scene_joint_head_net import JointSceneHeadNet
from scene_interband_net import InterbandSceneNet


class InterbandContract(unittest.TestCase):
    def setUp(self):
        torch.set_num_threads(2)
        torch.manual_seed(20261003)
        self.net = InterbandSceneNet()

    def test_shared_initialization_and_cost(self):
        torch.manual_seed(20261003)
        anchor = JointSceneHeadNet()
        candidate = self.net.state_dict()
        for name, tensor in anchor.state_dict().items():
            mapped = name.replace('band_gru.', 'band_gru.temporal.', 1) if name.startswith('band_gru.') else name
            self.assertTrue(torch.equal(tensor, candidate[mapped]), name)
        spec = self.net.specification()
        self.assertEqual(spec['parameters'], 70134)
        self.assertEqual(spec['dense_neural_macs_per_second'], 122057625.)

    def test_prefix_chunk_and_conservation(self):
        x = analysis(torch.randn(8192, 2)*.05)
        split = x.shape[1]//2
        future = x.clone(); future[:, split:] *= -3
        self.net.eval()
        with torch.no_grad():
            whole, _ = self.net(x, effect_gain=.1)
            altered, _ = self.net(future, effect_gain=.1)
            first, state = self.net(x[:, :split], effect_gain=.1)
            second, _ = self.net(x[:, split:], state, effect_gain=.1)
        self.assertTrue(torch.equal(whole['base'][:, :split], altered['base'][:, :split]))
        self.assertLess((torch.cat([first['base'], second['base']], 1)-whole['base']).abs().max().item(), 3e-6)
        dry = synthesize(whole['output'])-synthesize(whole['effect'])
        self.assertLess((dry@self.net.downmix.T-synthesize(x)).abs().max().item(), 4e-5)
        self.assertEqual(tuple(state['band'].shape), (1, 32, 10))

    def test_cross_band_without_cross_frame_state(self):
        # Keep temporal GRU independent per band; only new branch can transmit
        # a same-frame perturbation from band 5 to band 20.
        sequence = torch.randn(32, 1, 10)
        changed = sequence.clone(); changed[5] += torch.linspace(-2, 2, 10)
        with torch.no_grad():
            before, _ = self.net.band_gru(sequence)
            after, _ = self.net.band_gru(changed)
        self.assertGreater((before[20]-after[20]).abs().max().item(), 0.)

    def test_front_and_rear_gradient_paths(self):
        x = analysis(torch.randn(8192, 2)*.05)
        pred, _ = self.net(x)
        target = torch.randn_like(pred['base'])*.1
        for region in (slice(0, 3), slice(3, 5)):
            self.net.zero_grad(set_to_none=True)
            (pred['base'][..., region]-target[..., region]).abs().square().mean().backward(retain_graph=True)
            for name, param in self.net.band_gru.named_parameters():
                self.assertIsNotNone(param.grad, name)
                self.assertTrue(torch.isfinite(param.grad).all(), name)
                self.assertGreater(param.grad.abs().sum().item(), 0., name)


if __name__ == '__main__':
    unittest.main()
