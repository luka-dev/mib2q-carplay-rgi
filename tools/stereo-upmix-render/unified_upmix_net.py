#!/usr/bin/env python3
"""Track B shared causal audio/scene prototype. No trained weights or hook ABI.

One trunk predicts four stereo source operators and six instrument memberships.
Time state and running input level carry across arbitrary STFT chunks. Frequency
recurrence sees only the current frame. Waveform transforms/latency are separate.
"""
import argparse
import json
import math
from pathlib import Path

import torch
from torch import nn

from sep_net import FPS, HOP, N_BINS, SR, band_edges

SOURCES = ("eligible", "vocal", "drums", "bass")
FAMILIES = ("guitar", "keys", "piano", "strings", "winds", "other")
ARCHITECTURE = "unified-upmix-prototype-v1"


class ResidualBlock(nn.Module):
    def __init__(self, width):
        super().__init__()
        self.time_norm = nn.LayerNorm(width)
        self.time_gru = nn.GRU(width, width, batch_first=True)
        self.time_proj = nn.Linear(width, width)
        self.band_norm = nn.LayerNorm(width)
        self.band_gru = nn.GRU(width, width // 2, bidirectional=True, batch_first=True)
        self.band_proj = nn.Linear(width, width)

    def forward(self, x, state=None):
        b, t, k, w = x.shape
        y = self.time_norm(x).permute(0, 2, 1, 3).reshape(b * k, t, w)
        y, state = self.time_gru(y, state)
        x = x + self.time_proj(y.reshape(b, k, t, w).permute(0, 2, 1, 3))
        y, _ = self.band_gru(self.band_norm(x).reshape(b * t, k, w))
        return x + self.band_proj(y.reshape(b, t, k, w)), state


class UnifiedUpmixNet(nn.Module):
    def __init__(self, width=24, depth=2, local=16, global_width=16, decoder=16,
                 log_mean=-3.6760525703430176, log_std=2.43422794342041):
        super().__init__()
        if width < 2 or width % 2 or min(depth, local, global_width, decoder) < 1:
            raise ValueError("Positive dimensions and an even trunk width required")
        if log_std <= 0:
            raise ValueError("Positive frozen TRAIN normalization deviation required")
        self.bands = band_edges()
        self.config = dict(width=width, depth=depth, local=local,
                           global_width=global_width, decoder=decoder,
                           log_mean=log_mean, log_std=log_std)
        self.register_buffer("log_mean", torch.tensor(log_mean))
        self.register_buffer("log_std", torch.tensor(log_std))
        self.enc = nn.ModuleList([
            nn.Sequential(nn.LayerNorm((hi - lo) * 6),
                          nn.Linear((hi - lo) * 6, width)) for lo, hi in self.bands])
        self.blocks = nn.ModuleList([ResidualBlock(width) for _ in range(depth)])
        self.interface = nn.Linear(width, local)
        self.global_gru = nn.GRU(local, global_width, batch_first=True)
        self.dec = nn.ModuleList([
            nn.Sequential(nn.Linear(local + global_width, decoder), nn.Tanh(),
                          nn.Linear(decoder, (hi - lo) * len(SOURCES) * 4))
            for lo, hi in self.bands])
        # Presence, pan and update rate per instrument family. These are native
        # 48-band outputs, NOT q6d's 32-band/264-value ABI or calibrated posteriors.
        self.scene = nn.Linear(local + global_width, len(FAMILIES) * 3)
        self.level_alpha = 1 - math.exp(-HOP / SR / 2)

    def _running_level(self, q, state):
        level = q[:, 0] if state is None else state
        result = []
        for current in q.unbind(1):
            level = level + self.level_alpha * (current - level)
            result.append(level)
        return torch.stack(result, 1).clamp_min(1e-6), level

    def forward_stft(self, mixture, state=None):
        """Complex [B,T,1025,2] -> audio/scene predictions and reusable state.

        Caller owns/reset/detaches state. Training may backpropagate across chunks;
        inference should use inference_mode. Source masks are returned explicitly
        so actual-mixture attribution uses the exact same operators.
        """
        if not mixture.is_complex() or mixture.shape[-2:] != (N_BINS, 2):
            raise ValueError("Expected complex stereo 1025-bin spectra")
        if mixture.ndim != 4 or not mixture.shape[1]:
            raise ValueError("Expected a nonempty [batch,time,frequency,stereo] input")
        state = {} if state is None else state
        b, t = mixture.shape[:2]
        scale, level = self._running_level(mixture.abs().mean((2, 3)), state.get("level"))
        normalized = mixture / scale[:, :, None, None]
        logmag = ((torch.log(mixture.abs() + 1e-5) - self.log_mean) / self.log_std).clamp(-6, 6)
        features = torch.cat([normalized.real, normalized.imag, logmag], -1)
        h = torch.stack([enc(features[:, :, lo:hi].reshape(b, t, -1))
                         for enc, (lo, hi) in zip(self.enc, self.bands)], 2)
        old_time = state.get("time", [None] * len(self.blocks))
        if len(old_time) != len(self.blocks):
            raise ValueError("Recurrent state depth differs")
        new_time = []
        for block, previous in zip(self.blocks, old_time):
            h, updated = block(h, previous)
            new_time.append(updated)
        local = self.interface(h)
        global_features, global_state = self.global_gru(local.mean(2), state.get("global_state"))
        z = torch.cat([local, global_features[:, :, None].expand(-1, -1, len(self.bands), -1)], -1)
        m = torch.cat([dec(z[:, :, i]).reshape(b, t, hi - lo, len(SOURCES), 2, 2)
                       for i, (dec, (lo, hi)) in enumerate(zip(self.dec, self.bands))], 2)
        masks = torch.complex(m[..., 0], m[..., 1]).permute(0, 1, 3, 2, 4)
        source_stft = masks * mixture[:, :, None]
        presence, pan, update = self.scene(z).chunk(3, -1)
        presence = torch.sigmoid(presence)
        pan = torch.tanh(pan)
        update = torch.sigmoid(update)
        membership_state = state.get("membership", presence[:, 0])
        pan_state = state.get("pan", pan[:, 0])
        memberships, pans = [], []
        for target, direction, rate in zip(presence.unbind(1), pan.unbind(1), update.unbind(1)):
            membership_state = membership_state + rate * (target - membership_state)
            pan_state = pan_state + rate * (direction - pan_state)
            memberships.append(membership_state)
            pans.append(pan_state)
        prediction = dict(source_stft=source_stft, source_masks=masks,
                          family_presence=presence, membership=torch.stack(memberships, 1),
                          family_pan=torch.stack(pans, 1), membership_update=update)
        next_state = dict(level=level, time=new_time, global_state=global_state,
                          membership=membership_state, pan=pan_state)
        return prediction, next_state

    def cost(self):
        w, d = self.config["width"], self.config["depth"]
        local, glob, dec = (self.config[k] for k in ("local", "global_width", "decoder"))
        k = len(self.bands)
        pieces = dict(input_projection=N_BINS * 6 * w,
                      time_grus=d * k * 3 * (w * w + w * w),
                      frequency_grus=d * k * 2 * 3 * (w * (w // 2) + (w // 2) ** 2),
                      residual_projections=d * k * 2 * w * w,
                      interface=k * w * local,
                      global_gru=3 * (local * glob + glob * glob),
                      decoder_bottleneck=k * (local + glob) * dec,
                      four_source_projection=N_BINS * dec * len(SOURCES) * 4,
                      scene_projection=k * (local + glob) * len(FAMILIES) * 3)
        return dict(parameters=sum(p.numel() for p in self.parameters()), fps=FPS,
                    dense_macs_per_frame=pieces,
                    dense_macs_per_second=sum(pieces.values()) * FPS,
                    excluded="FFT/iFFT, norms, activations, level/features, complex products, pooling, scene state, renderer, int8 kernels and memory traffic")


def smoke(out):
    torch.set_num_threads(1)
    torch.manual_seed(1)
    net = UnifiedUpmixNet().eval()
    x = torch.complex(torch.randn(2, 21, N_BINS, 2), torch.randn(2, 21, N_BINS, 2))
    with torch.inference_mode():
        full, _ = net.forward_stft(x)
        parts, state = [], None
        for lo, hi in [(0, 1), (1, 8), (8, 9), (9, 21)]:
            p, state = net.forward_stft(x[:, lo:hi], state)
            parts.append(p)
        chunk_errors = {k:float((full[k] - torch.cat([p[k] for p in parts], 1)).abs().max()) for k in full}
        perturbed = x.clone(); perturbed[:, 10:] *= -7
        other, _ = net.forward_stft(perturbed)
        future_errors = {k:float((full[k][:, :10] - other[k][:, :10]).abs().max()) for k in full}
        reset, _ = net.forward_stft(x)
        reset_error = max(float((full[k] - reset[k]).abs().max()) for k in full)
        silence, _ = net.forward_stft(torch.zeros_like(x))
        assert all(torch.isfinite(v).all() for v in silence.values())
        # Same inferred eligible operator applied to summed sources closes.
        split = torch.stack([x * .1, x * .2, x * .3, x * .4], 2)
        sent = full["source_masks"][:, :, 0:1] * split
        closure = float((sent.sum(2) - full["source_stft"][:, :, 0]).abs().square().sum()
                        / full["source_stft"][:, :, 0].abs().square().sum())
    assert max(chunk_errors.values()) < 2e-5, chunk_errors
    assert max(future_errors.values()) < 2e-5, future_errors
    assert reset_error == 0 and closure < 1e-10
    net.train()
    prediction, _ = net.forward_stft(x[:, :5])
    loss = prediction["source_stft"].abs().square().mean()
    loss += (prediction["membership"] - .2).square().mean() + prediction["family_pan"].square().mean()
    loss.backward()
    groups = dict(encoder=net.enc, trunk=net.blocks, decoder=net.dec, scene=net.scene)
    gradients = {name:sum(float(p.grad.abs().sum()) for p in module.parameters() if p.grad is not None)
                 for name, module in groups.items()}
    assert all(math.isfinite(v) and v > 0 for v in gradients.values())
    result = dict(architecture=ARCHITECTURE, trained=False, scope="CPU structural smoke only; no acoustic/device/port result",
                  torch_version=torch.__version__, config=net.config, cost=net.cost(),
                  chunk_max_abs_errors=chunk_errors, future_prefix_max_abs_errors=future_errors,
                  reset_max_abs_error=reset_error, attribution_closure_nse=closure,
                  gradient_absolute_sums=gradients, silence_finite=True)
    Path(out).write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--smoke-out", required=True)
    smoke(parser.parse_args().smoke_out)
