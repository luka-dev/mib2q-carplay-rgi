"""One frequency-context block on the frozen MLP12 joint scene encoder."""
from torch import nn

from scene_joint_head_net import JointSceneHeadNet


class InterbandContext(nn.Module):
    def __init__(self, temporal, bands, width, hidden=12):
        super().__init__()
        self.temporal = temporal
        self.bands, self.width, self.hidden = bands, width, hidden
        self.norm = nn.LayerNorm(width)
        # Bidirectional over bands WITHIN one frame; never over audio time.
        self.frequency = nn.GRU(width, hidden, bidirectional=True, batch_first=True)
        self.project = nn.Linear(2*hidden, width)
        nn.init.normal_(self.project.weight, std=.005)
        nn.init.zeros_(self.project.bias)

    def forward(self, sequence, state=None):
        band, state = self.temporal(sequence, state)
        batch_bands, frames, width = band.shape
        batch = batch_bands//self.bands
        grid = band.reshape(batch, self.bands, frames, width).permute(0, 2, 1, 3)
        frequency, _ = self.frequency(self.norm(grid).reshape(batch*frames, self.bands, width))
        mixed = grid + .25*self.project(frequency).reshape(batch, frames, self.bands, width)
        return mixed.permute(0, 2, 1, 3).reshape(batch_bands, frames, width), state


class InterbandSceneNet(JointSceneHeadNet):
    def __init__(self, config=None):
        # All original weights, including the MLP head, consume the same RNG.
        super().__init__(config)
        self.band_gru = InterbandContext(self.band_gru, self.config.bands, self.config.width)

    def specification(self):
        result = super().specification()
        c, h = self.config, self.band_gru.hidden
        result['architecture'] = 'joint-scene-mlp12-interband-gru12-v1'
        result['interband'] = dict(axis='frequency only', hidden_each_direction=h,
                                   residual_gain=.25, temporal_state='unchanged band GRU state',
                                   projection_init_std=.005)
        counts = result['dense_neural_macs_per_frame']
        counts['interband_gru'] = c.bands*2*3*(c.width*h+h*h)
        counts['interband_projection'] = c.bands*2*h*c.width
        result['dense_neural_macs_per_second'] = sum(counts.values())*c.sample_rate/c.hop
        return result
