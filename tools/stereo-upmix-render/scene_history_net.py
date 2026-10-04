"""Explicit 16ms encoded history alongside the unchanged temporal band GRU."""
import torch
from torch import nn
from torch.nn import functional as F

from scene_joint_head_net import JointSceneHeadNet


class EncodedHistoryContext(nn.Module):
    def __init__(self, temporal, width):
        super().__init__()
        self.temporal = temporal
        self.depthwise = nn.Conv1d(width, width, 4, groups=width)
        self.project = nn.Conv1d(width, width, 1)
        nn.init.normal_(self.project.weight, std=.005)
        nn.init.zeros_(self.project.bias)

    def forward(self, sequence, state=None):
        state = {} if state is None else state
        band, temporal = self.temporal(sequence, state.get('temporal'))
        cache = state.get('encoded', sequence.new_zeros(sequence.shape[0], 3, sequence.shape[2]))
        if cache.shape != (sequence.shape[0], 3, sequence.shape[2]):
            raise ValueError('Three cached encoded frames of the same batch/width required')
        history = torch.cat([cache, sequence], 1)
        branch = self.project(F.silu(self.depthwise(history.transpose(1, 2)))).transpose(1, 2)
        # Own only the cache, rather than retaining the entire chunk storage.
        # Gradient detachment for TBPTT remains the caller's responsibility.
        return band+.25*branch, {'temporal': temporal, 'encoded': history[:, -3:].clone()}


class HistorySceneNet(JointSceneHeadNet):
    def __init__(self, config=None):
        super().__init__(config)
        self.band_gru = EncodedHistoryContext(self.band_gru, self.config.width)

    def specification(self):
        result = super().specification()
        c = self.config
        result['architecture'] = 'joint-scene-mlp12-encoded-history16-v1'
        result['encoded_history'] = dict(past_frames=3, oldest_offset_ms=3000*c.hop/c.sample_rate,
                                         input='pooled normalized encoder BEFORE temporal GRU',
                                         kernel=4, activation='SiLU', residual_gain=.25,
                                         cache_float32_bytes=3*c.bands*c.width*4,
                                         temporal_state='unchanged GRU state plus explicit encoded cache',
                                         projection_init_std=.005)
        counts = result['dense_neural_macs_per_frame']
        counts['history_depthwise'] = c.bands*4*c.width
        counts['history_projection'] = c.bands*c.width*c.width
        result['dense_neural_macs_per_second'] = sum(counts.values())*c.sample_rate/c.hop
        return result
