"""State, signal and training-path contracts for the isolated MLP head."""
import unittest

import torch

from scene_joint_net import JointSceneNet, analysis, synthesize
from scene_joint_head_net import JointSceneHeadNet


class HeadContract(unittest.TestCase):
    def test_same_body_signal_state_and_head_gradients(self):
        torch.set_num_threads(2)
        torch.manual_seed(20261003)
        baseline = JointSceneNet()
        torch.manual_seed(20261003)
        candidate = JointSceneHeadNet()
        for name, tensor in baseline.state_dict().items():
            if not name.startswith("operator_head."):
                self.assertTrue(torch.equal(tensor, candidate.state_dict()[name]), name)
        self.assertEqual(candidate.specification()["parameters"], 68136)
        self.assertEqual(candidate.specification()["dense_neural_macs_per_second"], 111113625.)
        x = analysis(torch.randn(8192, 2)*.05)
        candidate.eval()
        split = x.shape[1]//2
        changed = x.clone(); changed[:, split:] *= -3
        with torch.no_grad():
            full, _ = candidate(x, effect_gain=.1)
            future, _ = candidate(changed, effect_gain=.1)
            first, state = candidate(x[:, :split], effect_gain=.1)
            second, _ = candidate(x[:, split:], state, effect_gain=.1)
        self.assertTrue(torch.equal(full["base"][:, :split], future["base"][:, :split]))
        self.assertLess((torch.cat([first["base"], second["base"]], 1)-full["base"]).abs().max().item(), 3e-6)
        wet, effect, reference = synthesize(full["output"]), synthesize(full["effect"]), synthesize(x)
        self.assertLess(((wet-effect)@candidate.downmix.T-reference).abs().max().item(), 4e-5)
        pred, _ = candidate(x)
        target = torch.randn_like(pred["base"])*.1
        for region in (slice(0, 2), slice(3, 5)):
            candidate.zero_grad(set_to_none=True)
            objective = (pred["base"][..., region]-target[..., region]).abs().square().mean()
            objective.backward(retain_graph=True)
            self.assertTrue(torch.isfinite(candidate.operator_head[0].weight.grad).all())
            self.assertGreater(candidate.operator_head[0].weight.grad.abs().sum().item(), 0.)
            head = candidate.operator_head[2].weight.grad.reshape(5, 4, -1)
            self.assertTrue(torch.isfinite(head).all())
            self.assertTrue((head.abs().sum((1, 2))>0).all())


if __name__=="__main__":
    unittest.main()
