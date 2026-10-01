#!/usr/bin/env python3
"""One contingent upper-band decoder variant; no training or runtime promotion.

The original48x2 spectral backbone and low/mid24 decoder are unchanged. Only
bands intersecting frequencies above8kHz receive64 decoder features. A copied
parent function is preserved by zero output columns, with nonzero new input
features so the added branch can learn. This graph needs its own C/int8 port.
"""
import copy
import torch
from torch import nn
import unified_spectral_context_net as V

ARCHITECTURE = 'unified-spectral-upper-decoder-v1'
HIGH_DECODER = 64
CUTOFF_HZ = 8000
SEED = 20260930


class UpperDecoderNet(V.SpectralContextNet):
    def __init__(self, high_decoder=HIGH_DECODER, cutoff_hz=CUTOFF_HZ,
                 decoder_widths=None, **config):
        super().__init__(**config)
        if (high_decoder, cutoff_hz) != (HIGH_DECODER, CUTOFF_HZ):
            raise ValueError('This predeclared variant is64 features above8kHz, not a grid')
        expected = {'width': 48, 'depth': 2, 'local': 24, 'global_width': 24,
                    'decoder': 24, 'time_groups': 4, 'frequency_channels': 8, 'kernel': 7}
        if any(self.config[key] != value for key, value in expected.items()):
            raise ValueError('Exact original spectral48x2/24 interface required')
        self.decoder_widths = [HIGH_DECODER if (hi-1)*48000/2048 > CUTOFF_HZ else 24
                               for lo, hi in self.bands]
        if decoder_widths is not None and list(decoder_widths) != self.decoder_widths:
            raise ValueError('Saved heterogeneous decoder layout differs')
        self.high_bands = [i for i, width in enumerate(self.decoder_widths) if width == HIGH_DECODER]
        if len(self.high_bands) != 13 or sum(self.bands[i][1]-self.bands[i][0] for i in self.high_bands) != 685:
            raise ValueError('Original13 upper bands/685 bins required')
        for i in self.high_bands:
            lo, hi = self.bands[i]
            self.dec[i] = nn.Sequential(nn.Linear(48, HIGH_DECODER), nn.Tanh(),
                                        nn.Linear(HIGH_DECODER, (hi-lo)*len(V.U.SOURCES)*4))
        self.config.update(high_decoder=HIGH_DECODER, cutoff_hz=CUTOFF_HZ,
                           decoder_widths=list(self.decoder_widths))

    def cost(self):
        cost = super().cost()
        common_width = self.config['local']+self.config['global_width']
        bottleneck = sum(common_width*width for width in self.decoder_widths)
        projection = sum((hi-lo)*width*4 for (lo, hi), width in zip(self.bands, self.decoder_widths))
        for key, multiplier in [('all_source_dense_macs_per_frame', len(V.U.SOURCES)),
                                ('eligible_scene_dense_macs_per_frame', 1)]:
            cost[key]['decoder_bottleneck'] = bottleneck
            cost[key]['source_projection'] = projection*multiplier
        cost['all_source_dense_macs_s'] = sum(cost['all_source_dense_macs_per_frame'].values())*V.U.FPS
        cost['eligible_scene_dense_macs_s'] = sum(cost['eligible_scene_dense_macs_per_frame'].values())*V.U.FPS
        return cost


def widen(parent):
    if type(parent) is not V.SpectralContextNet or any(p.device.type != 'cpu' or p.dtype != torch.float32 for p in parent.parameters()):
        raise ValueError('Original float32 CPU spectral model required before training initialization')
    # Local RNG scope; does not change the caller's sampler or initialize CUDA.
    with torch.device('cpu'), torch.random.fork_rng(devices=[]):
        torch.random.default_generator.manual_seed(SEED)
        net = UpperDecoderNet(**parent.config)
    old = parent.state_dict()
    common = {name: value for name, value in old.items() if not name.startswith('dec.')}
    result = net.load_state_dict(common, strict=False)
    if result.unexpected_keys or set(result.missing_keys) != {name for name in net.state_dict() if name.startswith('dec.')}:
        raise ValueError('Shared backbone/scene state layout changed')
    with torch.no_grad():
        for i, (source, target) in enumerate(zip(parent.dec, net.dec)):
            if i not in net.high_bands:
                target.load_state_dict(source.state_dict(), strict=True)
                continue
            target[0].weight[:24].copy_(source[0].weight)
            target[0].bias[:24].copy_(source[0].bias)
            # Keep seeded new input rows/bias nonzero. Zeroing both sides would
            # make all added units permanently dead under ordinary gradients.
            target[2].weight.zero_()
            target[2].weight[:, :24].copy_(source[2].weight)
            target[2].bias.copy_(source[2].bias)
    net.train(parent.training)
    for parameter in net.scene.parameters():
        parameter.requires_grad_(False)
    return net


class EligibleProjection(nn.Module):
    """Executed selected-output reference; zero scatter is not a dense MAC.

    Keeps the original four-source tensor ABI for structural comparison while
    computing only eligible rows. Protected auxiliary masks become zero; this
    graph must never be used to measure protected auxiliary reconstruction.
    """
    def __init__(self, parent):
        super().__init__()
        if parent.out_features % 16:
            raise ValueError('Four-source complex stereo projection required')
        self.output_features = parent.out_features
        rows = (torch.arange(parent.out_features//16)[:, None]*16+torch.arange(4)).flatten()
        self.register_buffer('rows', rows)
        self.linear = nn.Linear(parent.in_features, len(rows))
        with torch.no_grad():
            self.linear.weight.copy_(parent.weight[rows])
            self.linear.bias.copy_(parent.bias[rows])

    def forward(self, value):
        eligible = self.linear(value)
        output = eligible.new_zeros(*eligible.shape[:-1], self.output_features)
        return output.index_copy(-1, self.rows, eligible)


def selected_graph(parent):
    if type(parent) is not UpperDecoderNet or any(p.device.type != 'cpu' for p in parent.parameters()):
        raise ValueError('CPU heterogeneous reference required')
    net = copy.deepcopy(parent)
    for decoder in net.dec:
        decoder[2] = EligibleProjection(decoder[2])
    return net
