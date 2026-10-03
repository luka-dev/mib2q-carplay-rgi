"""Meaningful signal/state checks for the isolated Track B scene pilot."""
import unittest

import torch

from scene_joint_net import JointSceneNet, analysis, synthesize


class SceneContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        torch.set_num_threads(2)
        torch.manual_seed(17)
        cls.wave = torch.randn(8192, 2) * .05

    def test_audio_roundtrip_and_effect_accounting(self):
        x = analysis(self.wave)
        net = JointSceneNet().eval()
        with torch.no_grad():
            p, _ = net(x, effect_gain=.1)
        d = net.downmix.to(x.dtype)
        reconstructed = torch.einsum("oc,btfc->btfo", d, p["base"])
        self.assertLess((reconstructed-x).abs().max().item(), 2e-6)
        dry = synthesize(p["base"])
        effect = synthesize(p["effect"])
        wet = synthesize(p["output"])
        reference = synthesize(x)
        self.assertLess((reference-self.wave[:len(reference)]).abs().max().item(), 3e-5)
        self.assertLess(((wet-effect)@net.downmix.T-reference).abs().max().item(), 4e-5)
        self.assertGreater(effect.square().sum().item(), 0)
        self.assertTrue(torch.equal(p["base"][..., 0, :].imag,
                                    torch.zeros_like(p["base"][..., 0, :].imag)))

    def test_future_invariance_and_chunk_state(self):
        x = analysis(self.wave)
        net = JointSceneNet().eval()
        split = x.shape[1]//2
        changed = x.clone()
        changed[:, split:] *= -3
        with torch.no_grad():
            full, _ = net(x)
            perturbed, _ = net(changed)
            first, state = net(x[:, :split])
            second, _ = net(x[:, split:], state)
        self.assertTrue(torch.equal(full["base"][:, :split], perturbed["base"][:, :split]))
        joined = torch.cat([first["base"], second["base"]], 1)
        self.assertLess((joined-full["base"]).abs().max().item(), 3e-6)

    def test_front_and_rear_losses_reach_all_output_heads(self):
        x = analysis(self.wave)
        net = JointSceneNet()
        pred, _ = net(x)
        desired = torch.randn_like(pred["base"]) * .1
        # Separate front/rear reconstruction gradients must survive projection.
        losses = [(pred["base"][..., :2]-desired[..., :2]).abs().square().mean(),
                  (pred["base"][..., 3:]-desired[..., 3:]).abs().square().mean()]
        for loss in losses:
            net.zero_grad(set_to_none=True)
            loss.backward(retain_graph=True)
            head = net.operator_head.weight.grad.reshape(5, 4, -1)
            self.assertTrue(torch.isfinite(head).all())
            self.assertTrue((head.abs().sum((1, 2)) > 0).all())
        self.assertLess(net.specification()["dense_neural_macs_per_second"], 110e6)


if __name__ == "__main__":
    unittest.main()
