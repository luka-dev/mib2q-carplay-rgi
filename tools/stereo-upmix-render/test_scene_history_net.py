"""Causal explicit-cache scope, signal/state and training contracts."""
import unittest

import torch
from torch import nn

from scene_joint_net import analysis, synthesize
from scene_joint_head_net import JointSceneHeadNet
from scene_history_net import EncodedHistoryContext, HistorySceneNet


class ZeroTemporal(nn.Module):
    def forward(self, sequence, state=None):
        return torch.zeros_like(sequence), None


class HistoryContract(unittest.TestCase):
    def setUp(self):
        torch.set_num_threads(2)
        torch.manual_seed(20261003)
        self.net = HistorySceneNet()

    def test_shared_initialization_and_cost(self):
        torch.manual_seed(20261003)
        anchor = JointSceneHeadNet()
        for name, tensor in anchor.state_dict().items():
            mapped = name.replace('band_gru.', 'band_gru.temporal.', 1) if name.startswith('band_gru.') else name
            self.assertTrue(torch.equal(tensor, self.net.state_dict()[mapped]), name)
        spec = self.net.specification()
        self.assertEqual(spec['parameters'], 68296)
        self.assertEqual(spec['dense_neural_macs_per_second'], 111953625.)
        self.assertEqual(spec['encoded_history']['oldest_offset_ms'], 16.)
        self.assertEqual(spec['encoded_history']['cache_float32_bytes'], 3840)

    def test_prefix_and_irregular_chunks_multiple_batches(self):
        x = analysis(torch.randn(8192, 2)*.05)
        x = torch.cat([x, x*.7], 0)
        changed = x.clone(); changed[:, 5:] *= -3
        self.net.eval()
        with torch.no_grad():
            whole, _ = self.net(x)
            altered, _ = self.net(changed)
            state, parts = None, []
            for first, last in [(0, 1), (1, 3), (3, 5), (5, 17), (17, x.shape[1])]:
                out, state = self.net(x[:, first:last], state)
                parts.append(out['base'])
        # Batched float32 pointwise convolution can vary by an ULP depending
        # on other matrix columns; the depthwise causal taps are tested below.
        self.assertLess((whole['base'][:, :5]-altered['base'][:, :5]).abs().max().item(), 3e-6)
        self.assertLess((torch.cat(parts, 1)-whole['base']).abs().max().item(), 3e-6)
        self.assertEqual(tuple(state['band']['encoded'].shape), (64, 3, 10))
        self.assertEqual(tuple(state['band']['temporal'].shape), (1, 64, 10))

    def test_exact_three_past_input_frames(self):
        adapter = EncodedHistoryContext(ZeroTemporal(), 10)
        with torch.no_grad():
            adapter.depthwise.weight.zero_(); adapter.depthwise.bias.zero_()
            adapter.depthwise.weight[:, 0, 0] = 1  # Oldest of four causal taps.
            adapter.project.weight.copy_(torch.eye(10)[..., None]); adapter.project.bias.zero_()
        sequence = torch.zeros(2, 7, 10); sequence[:, 0] = 1
        output, state = adapter(sequence)
        self.assertTrue(torch.equal(output[:, :3], torch.zeros_like(output[:, :3])))
        torch.testing.assert_close(output[:, 3], torch.full((2, 10), .25*torch.nn.functional.silu(torch.tensor(1.)).item()))
        self.assertTrue(torch.equal(output[:, 4:], torch.zeros_like(output[:, 4:])))
        self.assertTrue(torch.equal(state['encoded'], sequence[:, -3:]))
        self.assertEqual(state['encoded'].untyped_storage().nbytes(), 2*3*10*4)
        first, cache = adapter(sequence[:, :2]); second, _ = adapter(sequence[:, 2:], cache)
        torch.testing.assert_close(torch.cat([first, second], 1), output)

    def test_conservation_and_gradient_paths(self):
        x = analysis(torch.randn(8192, 2)*.05)
        pred, _ = self.net(x, effect_gain=.1)
        dry = synthesize(pred['output'])-synthesize(pred['effect'])
        self.assertLess((dry@self.net.downmix.T-synthesize(x)).abs().max().item(), 4e-5)
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
