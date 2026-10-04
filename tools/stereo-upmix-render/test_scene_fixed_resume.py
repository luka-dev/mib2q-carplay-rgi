"""Verify resumed AdamW trajectories and reject changed fixed-crop recipes."""
from dataclasses import asdict
import tempfile
from pathlib import Path
import unittest

import torch

from fit_scene_fixed_nas import restore_checkpoint, measure
from scene_joint_net import JointSceneNet, analysis
from train_scene_joint_nas import loss, sha, write


class ResumeContract(unittest.TestCase):
    def test_optimizer_replay_and_recipe_binding(self):
        torch.set_num_threads(2)
        torch.manual_seed(23)
        reference = JointSceneNet()
        target_wave = torch.randn(4096, 5)*.03
        x = analysis(target_wave@reference.downmix.T)
        y = analysis(target_wave)
        power = torch.ones(5)
        optimizer = torch.optim.AdamW(reference.parameters(), lr=.001, weight_decay=1e-4)

        def update(net, opt):
            prediction, _ = net(x)
            objective, _ = loss(prediction["base"], y, power, 2)
            opt.zero_grad(set_to_none=True)
            objective.backward()
            torch.nn.utils.clip_grad_norm_(net.parameters(), 5.)
            opt.step()

        update(reference, optimizer)
        protocol = dict(schema="joint-scene-fixed-train-v2", steps=1,
                        parent_protocol_sha256="parent", catalog_sha256="catalog", song="train-song",
                        partition="train", audio_sha256="audio", common_gain=.1, start_sample=0,
                        length_samples=4096, warm_frames=2, score_frames=11, seed=23,
                        normalization_power=power.tolist(), state_reset_each_update=True,
                        optimizer="AdamW", lr=.001, weight_decay=1e-4, gradient_clip=5.,
                        loss="parent loss", specification=reference.specification(),
                        pass_rule="frozen criterion", effect_gain=0.,
                        source_hashes={n:sha(Path(__file__).with_name(n)) for n in
                                       ("scene_joint_net.py", "train_scene_joint_nas.py")})
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write(root/"protocol.json", protocol)
            torch.save(dict(model=reference.state_dict(), optimizer=optimizer.state_dict(),
                            config=asdict(reference.config), step=1,
                            protocol_sha256=sha(root/"protocol.json")), root/"terminal.pt")
            write(root/"report.json", dict(complete=True, terminal_step=1,
                    terminal_checkpoint_sha256=sha(root/"terminal.pt"),
                    terminal=dict(metrics=measure(reference, x, y, power, 2))))
            resumed = JointSceneNet()
            resumed_optimizer = torch.optim.AdamW(resumed.parameters(), lr=.001, weight_decay=1e-4)
            info = restore_checkpoint(resumed, resumed_optimizer, root, protocol, 2)
            self.assertEqual(info["optimizer_counters"], [1])
            update(reference, optimizer)
            update(resumed, resumed_optimizer)
            for expected, actual in zip(reference.parameters(), resumed.parameters()):
                self.assertTrue(torch.equal(expected, actual))
            changed = dict(protocol, lr=.002)
            with self.assertRaisesRegex(ValueError, "frozen field: lr"):
                restore_checkpoint(resumed, resumed_optimizer, root, changed, 2)
            with (root/"terminal.pt").open("ab") as stream:
                stream.write(b"tampered")
            with self.assertRaisesRegex(ValueError, "hash mismatch"):
                restore_checkpoint(resumed, resumed_optimizer, root, protocol, 2)


if __name__=="__main__":
    unittest.main()
