"""Isolated output-head ablation; the baseline encoder/projection stay intact."""
from torch import nn

from scene_joint_net import JointSceneNet


class JointSceneHeadNet(JointSceneNet):
    def __init__(self, config=None):
        # Construct the original model first: same seed gives identical body
        # initialization. The replaced head alone consumes additional RNG.
        super().__init__(config)
        hidden = nn.Conv1d(2*self.config.width, 12, 1)
        output = nn.Conv1d(12, 20, 1)
        nn.init.normal_(output.weight, std=.005)
        nn.init.zeros_(output.bias)
        self.operator_head = nn.Sequential(hidden, nn.SiLU(), output)

    def specification(self):
        result = super().specification()
        result["architecture"] = "joint-scene-band-global-mlp12-v1"
        result["operator_head"] = dict(kind="MLP", input=2*self.config.width, hidden=12,
                                       output=20, activation="SiLU", output_init_std=.005)
        counts = result["dense_neural_macs_per_frame"]
        counts["operator_head"] = self.bins*(2*self.config.width*12+12*20)
        result["dense_neural_macs_per_second"] = sum(counts.values())*self.config.sample_rate/self.config.hop
        return result
