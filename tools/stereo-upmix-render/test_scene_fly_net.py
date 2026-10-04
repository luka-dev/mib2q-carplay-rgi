"""Topology null, causal state, conservation and trainable-path contracts."""
from collections import Counter
import json
from pathlib import Path
import unittest

import torch

from scene_joint_head_net import JointSceneHeadNet
from scene_fly_net import FlySceneNet


GRAPH = Path(__file__).with_name('scene_flywire_783_64.json')


class FlyContract(unittest.TestCase):
    def test_topology_null(self):
        g = json.loads(GRAPH.read_text())
        a, b = g['fly_edges'], g['shuffled_edges']
        self.assertEqual(g['nodes'], 64)
        for edges in (a, b):
            self.assertEqual(len({(u, v) for u, v, _ in edges}), len(edges))
            self.assertTrue(all(u != v and w >= 5 for u, v, w in edges))
        self.assertEqual(Counter(u for u, v, w in a), Counter(u for u, v, w in b))
        self.assertEqual(Counter(v for u, v, w in a), Counter(v for u, v, w in b))
        self.assertEqual(Counter((v, w, g['signs'][u]) for u, v, w in a),
                         Counter((v, w, g['signs'][u]) for u, v, w in b))
        self.assertLess(g['null_shared_edge_count'], .8*len(a))

    def test_network_contracts(self):
        torch.set_num_threads(3)
        models = []
        for variant in ('gru', 'fly', 'shuffled'):
            torch.manual_seed(20261003)
            models.append(JointSceneHeadNet() if variant == 'gru' else FlySceneNet(graph_path=GRAPH, variant=variant))
        anchor, fly, shuffled = models
        for name, value in anchor.state_dict().items():
            if name.startswith('global_gru.'):
                continue
            for net in (fly, shuffled):
                self.assertTrue(torch.equal(value, net.state_dict()[name]), name)
        for name, value in fly.global_gru.state_dict().items():
            if name != 'recurrent':
                self.assertTrue(torch.equal(value, shuffled.global_gru.state_dict()[name]), name)
        torch.manual_seed(9)
        x = torch.complex(torch.randn(1, 9, 513, 2), torch.randn(1, 9, 513, 2))
        for net in (fly, shuffled):
            spec = net.specification()
            self.assertEqual(spec['parameters'], 47400)
            self.assertEqual(spec['dense_neural_macs_per_second'], 108041625)
            recurrent = net.global_gru.recurrent
            self.assertTrue((recurrent.abs().sum(1) <= .900001).all())
            full, _ = net(x, effect_gain=.1)
            prefix, state = net(x[:, :4], effect_gain=.1)
            suffix, _ = net(x[:, 4:], state, effect_gain=.1)
            torch.testing.assert_close(prefix['output'], full['output'][:, :4], rtol=1e-5, atol=2e-6)
            torch.testing.assert_close(suffix['output'], full['output'][:, 4:], rtol=1e-5, atol=2e-6)
            changed = x.clone(); changed[:, 4:] *= 7
            other, _ = net(changed, effect_gain=.1)
            torch.testing.assert_close(other['output'][:, :4], prefix['output'], rtol=1e-5, atol=2e-6)
            recovered = torch.einsum('oc,btfc->btfo', net.downmix.to(x.dtype), full['output']-full['effect'])
            torch.testing.assert_close(recovered, x, rtol=1e-5, atol=2e-6)
            net.zero_grad(set_to_none=True)
            full['base'].abs().square().mean().backward()
            for p in (net.global_gru.input.weight, net.global_gru.leak_logits):
                self.assertIsNotNone(p.grad)
                self.assertTrue(torch.isfinite(p.grad).all())
                self.assertGreater(p.grad.abs().sum().item(), 0)
            self.assertIsNone(recurrent.grad)


if __name__ == '__main__':
    unittest.main()
