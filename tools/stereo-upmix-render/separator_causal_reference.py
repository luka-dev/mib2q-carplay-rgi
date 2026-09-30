#!/usr/bin/env python3
"""Normalized residual causal encoder with the original fixed output interface.

PC mechanism diagnostic, not a faithful BSRNN replica or a device model.
Normalization never aggregates over time; only frequency recurrence is bidirectional.
"""
import sep_net as SN


class CausalReference(SN.BandSepNet):
    def __init__(self, torch, width=96, blocks=4):
        # Preserve the original global-GRU initialization and decoder topology.
        super().__init__(torch, norm='stream', mask='complex2')
        nn = torch.nn

        class Block(nn.Module):
            def __init__(self):
                super().__init__()
                self.time_norm = nn.LayerNorm(width)
                self.time_gru = nn.GRU(width, width, batch_first=True)
                self.time_proj = nn.Linear(width, width)
                self.band_norm = nn.LayerNorm(width)
                self.band_gru = nn.GRU(width, width//2, batch_first=True, bidirectional=True)
                self.band_proj = nn.Linear(width, width)

            def forward(self, x):
                b, t, k, c = x.shape
                y = self.time_norm(x).permute(0, 2, 1, 3).reshape(b*k, t, c)
                y, _ = self.time_gru(y)
                y = y.reshape(b, k, t, c).permute(0, 2, 1, 3)
                x = x + self.time_proj(y)
                y, _ = self.band_gru(self.band_norm(x).reshape(b*t, k, c))
                return x + self.band_proj(y.reshape(b, t, k, c))

        self.enc = nn.ModuleList([nn.Sequential(nn.LayerNorm((hi-lo)*6),
                                  nn.Linear((hi-lo)*6, width)) for lo, hi in self.bands])
        self.blocks = nn.ModuleList([Block() for _ in range(blocks)])
        self.interface = nn.Linear(width, self.h2)
        for name in ['gru1', 'gru2', 'mix1', 'mix2']:
            delattr(self, name)
        self.modules = nn.ModuleList([self.enc, self.blocks, self.interface, self.ggru, self.dec])
        self.width, self.depth = width, blocks
        k = len(self.bands)
        time = 3*(width*width + width*width)*k
        frequency = 2*3*(width*(width//2)+(width//2)**2)*k
        projections = 2*width*width*k
        self.macs = (SN.N_BINS*6*width + blocks*(time+frequency+projections)
                     + width*self.h2*k + 3*(self.h2*self.hg+self.hg*self.hg)
                     + SN.N_BINS*4*(self.h2+self.hg))

    def separate_stft(self, x):
        torch = self.torch
        b, t = x.shape[:2]
        scale = self.running_level(x.abs().mean(dim=(2, 3)))[:, :, None, None].clamp(min=1e-6)
        normalized = x/scale
        logmag = torch.log(x.abs()+1e-5)
        if SN.STATS['mean'] is not None:
            logmag = (logmag-SN.STATS['mean'])/SN.STATS['std']
        features = torch.cat([normalized.real, normalized.imag, logmag.clamp(-6, 6)], -1)
        h = torch.stack([enc(features[:, :, lo:hi].reshape(b, t, -1))
                         for enc, (lo, hi) in zip(self.enc, self.bands)], 2)
        for block in self.blocks:
            h = block(h)
        local = self.interface(h)
        global_state, _ = self.ggru(local.mean(2))
        z = torch.cat([local, global_state[:, :, None].expand(-1, -1, len(self.bands), -1)], -1)
        m = torch.cat([decoder(z[:, :, i]).reshape(b, t, hi-lo, 4)
                       for i, (decoder, (lo, hi)) in enumerate(zip(self.dec, self.bands))], 2)
        self.last_mask = torch.complex(m[..., 0::2], m[..., 1::2])
        return x*self.last_mask


def make(torch, mode):
    if mode == 'original':
        return SN.BandSepNet(torch, norm='stream', mask='complex2')
    if mode == 'residual96x4':
        return CausalReference(torch)
    raise ValueError(mode)


def load(torch, path, device='cpu'):
    ck = torch.load(path, map_location=device, weights_only=True)
    if ck.get('architecture') != 'causal-reference-v1':
        raise ValueError('Unsupported checkpoint semantics')
    net = make(torch, ck['mode']).to(device)
    net.modules.load_state_dict(ck['state'], strict=True)
    net.modules.eval()
    SN.STATS.update(ck['stats'])
    return net, ck
