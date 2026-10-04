"""Small continuous-state connectome reservoir, not a fly brain emulation."""
import hashlib
import json
import math
from pathlib import Path

import torch
from torch import nn
from torch.nn import functional as F

from scene_joint_head_net import JointSceneHeadNet


def matrix(graph, variant):
    n = graph['nodes']
    out = torch.zeros(n, n)
    edges = graph['fly_edges' if variant == 'fly' else 'shuffled_edges']
    for a, b, count in edges:
        out[b, a] = graph['signs'][a]*count**.5
    norms = out.abs().sum(1, keepdim=True)
    return .9*out/norms.clamp_min(1e-12)


class ConnectomeReservoir(nn.Module):
    """GRU-compatible [B,T,H] interface with reusable [1,B,H] state.

    Fixed signed graph, trained input projection and node leakage; tanh and
    incoming norm<=.9 bound the homogeneous recurrence. No spikes, synapse
    learning or biological time constants. Dense multiplication is explicit.
    """
    def __init__(self, graph, variant, sample_rate=48000, hop=256):
        super().__init__()
        n = graph['nodes']
        self.input = nn.Linear(n, n)
        self.register_buffer('recurrent', matrix(graph, variant))
        # Same node-wise initialization in fly and shuffled variants.
        tau = torch.logspace(1., math.log10(160.), n)
        alpha = 1-torch.exp(-(1000*hop/sample_rate)/tau)
        self.leak_logits = nn.Parameter(torch.logit((alpha-.02)/.78))

    def forward(self, x, state=None):
        n = self.recurrent.shape[0]
        if x.ndim != 3 or x.shape[-1] != n or x.shape[1] < 1:
            raise ValueError("Nonempty [B,T,nodes] sequence required")
        h = x.new_zeros(x.shape[0], n) if state is None else state[0]
        alpha = .02+.78*self.leak_logits.sigmoid()
        drive = self.input(x)
        rows = []
        for t in range(x.shape[1]):
            proposal = torch.tanh(drive[:, t]+F.linear(h, self.recurrent))
            h = h+alpha*(proposal-h)
            rows.append(h)
        return torch.stack(rows, 1), h[None]


class FlySceneNet(JointSceneHeadNet):
    def __init__(self, config=None, graph_path=None, variant='fly'):
        if variant not in ('fly', 'shuffled') or graph_path is None:
            raise ValueError("Explicit graph and fly/shuffled variant required")
        super().__init__(config)
        path = Path(graph_path)
        graph = json.loads(path.read_text())
        if graph['schema'] != 'scene-flywire-graph-v1' or graph['nodes'] != self.config.global_width:
            raise ValueError("Graph does not match global state width")
        self.variant, self.graph = variant, graph
        self.graph_sha256 = hashlib.sha256(path.read_bytes()).hexdigest()
        self.global_gru = ConnectomeReservoir(graph, variant, self.config.sample_rate, self.config.hop)

    def specification(self):
        result = super().specification()
        n = self.config.global_width
        counts = result['dense_neural_macs_per_frame']
        counts['global_gru'] = 2*n*n  # Actual dense input and fixed recurrence kernels.
        result['dense_neural_macs_per_second'] = sum(counts.values())*self.config.sample_rate/self.config.hop
        result.update(architecture='joint-scene-mlp12-reservoir-'+self.variant+'-v1',
                      connectome=dict(variant=self.variant, graph_sha256=self.graph_sha256,
                                     nodes=n, active_recurrent_edges=len(self.graph['fly_edges']),
                                     frozen_recurrent=True, trainable_leak=True,
                                     alpha_range=[.02, .8], recurrence_row_l1_bound=.9,
                                     recurrent_kernel='dense; no sparse speed claim',
                                     physical_emulation=False))
        return result
