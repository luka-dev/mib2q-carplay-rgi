#!/usr/bin/env python3
"""The separator student (astra review 8): estimate the REAR-ELIGIBLE AUDIO, not a
cell's ownership.  Two-way (protected = vocals + drums + bass, eligible = the rest),
causal, 2048/512 at 48 kHz (93.75 frames/s), sized for the car's budget after the
two GRU controllers are retired (~120-150 M MAC/s):

  log|L|, log|R| per bin (1025) -> strided frequency convs 1025 -> 513 -> 257 -> 129
  -> a causal GRU per position over time (shared weights) + a global causal GRU
  -> a head at 257 positions producing 4 bins x 2 channels of magnitude ratio mask
  -> linear upsample to 1025 -> eligible = mask x mixture STFT (mixture phase) -> iSTFT

Trained in the AUDIO domain: L1 on the magnitude STFT and on the complex STFT of the
estimate vs the eligible target (MoisesDB truth + the four-stem teacher cache), so the
objective is "usable rear content", not "explain ambiguous cells".  Evaluated through
the real DSP with `upmix_stem_tests.py --inject student:CKPT`.

  train     --cache MOISES --teacher-cache TC4 --out DIR
  separate  --model CKPT --wav IN --out ELIG.wav
"""
import argparse
import json
import os
import sys
import time

import numpy as np

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mask_net as MN  # noqa: E402  (caches, read_window, expand_slots, split)

SR = 48000
N_FFT, HOP = 2048, 512
N_BINS = N_FFT // 2 + 1          # 1025
FPS = SR / HOP                   # 93.75 frames/s
# seven cache slots -> protected / eligible
ELIGIBLE_SLOTS = (6,)            # other
STATS = {"mean": None, "std": None}
TRUTH_FRAC = 0.5                 # share of windows drawn from the truth pool when a pseudo-label pool is present


def _torch():
    import torch
    return torch


def stft(torch, x):
    """x [B, 2, n] -> [B, T, 1025, 2] complex, Hann 2048 / hop 512, centre-padded."""
    win = torch.hann_window(N_FFT, device=x.device)
    B, C, n = x.shape
    X = torch.stft(x.reshape(B * C, n), N_FFT, HOP, window=win, return_complex=True, center=True)   # [BC, 1025, T]
    return X.reshape(B, C, N_BINS, -1).permute(0, 3, 2, 1)


def istft(torch, X, n):
    """[B, T, 1025, 2] complex -> [B, 2, n]."""
    win = torch.hann_window(N_FFT, device=X.device)
    B, T, _, C = X.shape
    y = torch.istft(X.permute(0, 3, 2, 1).reshape(B * C, N_BINS, T), N_FFT, HOP, window=win, length=n, center=True)
    return y.reshape(B, C, n)


class SepNet:
    def __init__(self, torch, c1=16, c2=32, c3=48, ht=16, hg=64, dec=24):
        nn = torch.nn
        self.torch = torch
        self.enc1 = nn.Sequential(nn.Conv1d(2, c1, 7, stride=2, padding=3), nn.Tanh())                                  # 1025 -> 513
        self.enc2 = nn.Sequential(nn.Conv1d(c1, c1, 5, stride=2, padding=2, groups=c1), nn.Conv1d(c1, c2, 1), nn.Tanh())  # -> 257
        self.enc3 = nn.Sequential(nn.Conv1d(c2, c2, 5, stride=2, padding=2, groups=c2), nn.Conv1d(c2, c3, 1), nn.Tanh())  # -> 129
        self.tgru = nn.GRU(c3, ht, batch_first=True)             # causal, per position over time, shared
        self.ggru = nn.GRU(c3, hg, batch_first=True)             # causal, global (mean over positions)
        self.gproj = nn.Linear(hg, 16)
        # head at 257 positions: [enc2 32, upsampled (c3 + ht) 64, global 16] -> 4 bins x 2 channels
        self.head = nn.Sequential(nn.Linear(c2 + c3 + ht + 16, dec), nn.Tanh(), nn.Linear(dec, 8))
        self.modules = nn.ModuleList([self.enc1, self.enc2, self.enc3, self.tgru, self.ggru, self.gproj, self.head])
        self.c1, self.c2, self.c3, self.ht, self.hg, self.dec = c1, c2, c3, ht, hg, dec
        self.macs = (7 * 2 * c1 * 513 + (5 * c1 + c1 * c2) * 257 + (5 * c2 + c2 * c3) * 129
                     + 3 * (c3 * ht + ht * ht) * 129 + 3 * (c3 * hg + hg * hg) + hg * 16
                     + ((c2 + c3 + ht + 16) * dec + dec * 8) * 257)

    def to(self, dev):
        self.modules.to(dev); return self

    def parameters(self):
        return self.modules.parameters()

    def mask(self, logmag):
        """logmag [B,T,1025,2] (standardised) -> mask [B,T,1025,2] in (0,1)."""
        torch = self.torch
        B, T = logmag.shape[:2]
        x = logmag.reshape(B * T, N_BINS, 2).transpose(1, 2)                  # [BT,2,1025]
        e1 = self.enc1(x)                                                       # [BT,c1,513]
        e2 = self.enc2(e1)                                                      # [BT,c2,257]
        e3 = self.enc3(e2)                                                      # [BT,c3,129]
        tm, _ = self.tgru(e3.reshape(B, T, self.c3, 129).permute(0, 3, 1, 2).reshape(B * 129, T, self.c3))
        tm = tm.reshape(B, 129, T, self.ht).permute(0, 2, 3, 1).reshape(B * T, self.ht, 129)
        gm, _ = self.ggru(e3.reshape(B, T, self.c3, 129).mean(-1))              # [B,T,hg]
        g = self.gproj(gm).reshape(B * T, 16, 1).expand(-1, -1, 257)
        up = torch.nn.functional.interpolate(torch.cat([e3, tm], 1), size=257, mode="linear", align_corners=True)
        z = torch.cat([e2, up, g], 1).transpose(1, 2)                           # [BT,257,c2+c3+ht+16]
        m = torch.sigmoid(self.head(z)).reshape(B * T, 257, 4, 2)               # 4 bins per position
        m = m.permute(0, 3, 1, 2).reshape(B * T, 2, 1028)[:, :, :N_BINS]        # [BT,2,1025]
        return m.transpose(1, 2).reshape(B, T, N_BINS, 2)

    def separate_stft(self, X):
        """X [B,T,1025,2] complex mixture -> eligible estimate, same shape."""
        torch = self.torch
        lm = torch.log(X.abs() + 1e-5)
        lm = (lm - STATS["mean"]) / STATS["std"] if STATS["mean"] is not None else lm
        return X * self.mask(lm.clamp(-6, 6))


def band_edges(n_bins=N_BINS, n_bands=48):
    """48 unequal bands over the 1025 bins of a 2048-point FFT (23.4 Hz/bin): 4-bin bands to ~1 kHz,
    8-bin to ~3 kHz, 16-bin to ~8 kHz, 32-bin to ~14 kHz, the rest in five wide bands."""
    widths = [4] * 11 + [8] * 11 + [16] * 13 + [32] * 8
    rest = n_bins - sum(widths)
    widths += [rest // 5] * 4 + [rest - 4 * (rest // 5)]
    assert len(widths) == n_bands and sum(widths) == n_bins, (len(widths), sum(widths))
    edges = np.cumsum([0] + widths)
    return [(int(edges[i]), int(edges[i + 1])) for i in range(n_bands)]


class BandSepNet:
    """astra review 9, point 3: band encoders (every bin kept), shared band GRUs 48 -> 32 -> 32 over
    time, two depthwise/pointwise mixers across the 48 bands, a global pooled GRU, and DIRECT per-bin
    decoders producing stereo COMPLEX ratio masks (unbounded, initialised near attenuation).
    ~125 M neural MAC/s at 93.75 frames/s.  Causal: recurrence over time only.
    `brnn` > 0 (astra review 10, leading diagnosis "inadequate cross-band modelling"): replace the two
    depthwise mixers with a BSRNN-style bidirectional GRU ACROSS THE BANDS at every frame (frequency
    is not causal), hidden `brnn` per direction, projected back to h2 with a tanh residual.
    `lookahead` = L > 0 (astra review 12, the budget-compatible future-context test): the mask applied to
    frame t is produced at step t + L (output delay), and the decoder also receives frame t's own band
    embedding, so the L future frames are real evidence rather than a memory test.  The stream's last L
    frames hold the final mask.  `bidir` (review 12's diagnostic twin): the time GRUs run in both
    directions with half the hidden size per direction (seconds of future context; NOT deployable)."""

    def __init__(self, torch, feat=48, h1=32, h2=32, hg=32, brnn=0, lookahead=0, bidir=False, dec_skip=False, norm="window", mask="complex2"):
        nn = torch.nn
        self.torch = torch
        self.bands = band_edges()
        self.enc = nn.ModuleList([nn.Linear((b - a) * 6, feat) for a, b in self.bands])
        d = 2 if bidir else 1
        self.gru1 = nn.GRU(feat, h1 // d, batch_first=True, bidirectional=bidir)
        self.gru2 = nn.GRU(h1, h2 // d, batch_first=True, bidirectional=bidir)
        if brnn:
            self.brnn = nn.GRU(h2, brnn, batch_first=True, bidirectional=True)
            self.bproj = nn.Linear(2 * brnn, h2)
            cross = [self.brnn, self.bproj]
        else:
            self.mix1 = nn.Sequential(nn.Conv1d(h2, h2, 3, padding=1, groups=h2), nn.Conv1d(h2, h2, 1), nn.Tanh())
            self.mix2 = nn.Sequential(nn.Conv1d(h2, h2, 3, padding=1, groups=h2), nn.Conv1d(h2, h2, 1), nn.Tanh())
            cross = [self.mix1, self.mix2]
        self.ggru = nn.GRU(h2, hg // d, batch_first=True, bidirectional=bidir)
        dec_skip = bool(dec_skip or lookahead)                     # the frame's own band embedding into the decoder
        dec_in = h2 + hg + (feat if dec_skip else 0)
        self.mask_mode = mask
        mo = 1 if mask == "shared_sigmoid" else (8 if mask in ("complex_role", "deep2", "matrix2") else 4)
        self.dec = nn.ModuleList([nn.Linear(dec_in, (b - a) * mo) for a, b in self.bands])
        for d in self.dec:                                     # near attenuation: mask ~ 0.3 + 0i (sigmoid(-0.85) = 0.3)
            nn.init.normal_(d.weight, std=0.01)
            with torch.no_grad():
                d.bias.zero_()
                if mask == "complex2":
                    d.bias.view(-1, 4)[:, 0] = 0.3; d.bias.view(-1, 4)[:, 2] = 0.3
                elif mask in ("complex_role", "deep2", "matrix2"):
                    d.bias.view(-1, 8)[:, 0] = 0.3; d.bias.view(-1, 8)[:, 2] = 0.3
                    if mask == "complex_role":
                        d.bias.view(-1, 8)[:, 7] = 0.25
                elif mask == "role_softmax":
                    d.bias.view(-1, 4)[:, 3] = 0.25       # eligible prior ~0.30; V/D/B share the rest
                else:
                    d.bias.fill_(-0.85)
        self.modules = nn.ModuleList([self.enc, self.gru1, self.gru2] + cross + [self.ggru, self.dec])
        self.feat, self.h1, self.h2, self.hg, self.nbrnn, self.lookahead, self.bidir, self.dec_skip, self.norm = feat, h1, h2, hg, brnn, lookahead, bidir, dec_skip, norm
        self.mo = mo
        nb = len(self.bands)
        cross_macs = (2 * 3 * (h2 * brnn + brnn * brnn) + 2 * brnn * h2) * nb if brnn else 2 * (3 * h2 + h2 * h2) * nb
        rec = lambda i, h: 2 * 3 * (i * (h // 2) + (h // 2) ** 2) if bidir else 3 * (i * h + h * h)   # per direction, half hidden
        self.macs = (N_BINS * 6 * feat + rec(feat, h1) * nb + rec(h1, h2) * nb
                     + cross_macs + rec(h2, hg) + N_BINS * mo * dec_in)

    def to(self, dev):
        self.modules.to(dev); return self

    def parameters(self):
        return self.modules.parameters()

    STREAM_TAU = 2.0                                               # seconds (astra 13: a starting constant)

    def running_level(self, q):
        """q [B,T] -> s [B,T], the causal EMA s[t] = a s[t-1] + (1-a) q[t] with s[0] = q[0], computed in closed form per
        chunk of 1024 frames (a^-k stays finite) and carried across chunks: no Python loop over frames."""
        torch = self.torch
        a = float(np.exp(-HOP / (SR * self.STREAM_TAU)))
        B, T = q.shape; out = []; carry = None
        for c0 in range(0, T, 1024):
            qc = q[:, c0:c0 + 1024]; n = qc.shape[1]
            k = torch.arange(n, device=q.device, dtype=q.dtype)
            if carry is None:
                # s[t] = a^t ((1-a) sum_{k<=t} a^-k q[k] + a q[0])
                s = a ** k * ((1 - a) * torch.cumsum(qc * a ** (-k), 1) + a * qc[:, :1])
            else:
                # s[t] = a^(t+1) s_prev + (1-a) sum_{k<=t} a^(t-k) q[k]
                s = a ** (k + 1) * carry[:, None] + a ** k * (1 - a) * torch.cumsum(qc * a ** (-k), 1)
            carry = s[:, -1]; out.append(s)
        return torch.cat(out, 1)

    def apply_last_filter(self, Z):
        """Apply the frozen mixture-inferred operator to an aligned source STFT.

        Source-attribution diagnostics must not infer a fresh operator from the
        isolated source.  For deep2, the second tap reads only the past frame.
        """
        if self.mask_mode == "deep2":
            current, previous = self.last_taps
            past = self.torch.cat([self.torch.zeros_like(Z[:, :1]), Z[:, :-1]], 1)
            return current * Z + previous * past
        if self.mask_mode == "matrix2":
            diagonal, cross = self.last_matrix
            left = diagonal[..., 0] * Z[..., 0] + cross[..., 1] * Z[..., 1]
            right = cross[..., 0] * Z[..., 0] + diagonal[..., 1] * Z[..., 1]
            return self.torch.stack((left, right), -1)
        return self.last_mask * Z

    def separate_stft(self, X):
        """X [B,T,1025,2] complex mixture -> eligible estimate (complex ratio mask per channel)."""
        torch = self.torch
        B, T = X.shape[:2]
        if self.norm == "stream":
            # astra 13 §4: a CAUSAL running level - stereo-linked q[t] = mean over bins and channels of |X[t]|,
            # s[t] = a s[t-1] + (1 - a) q[t], a = exp(-hop / (sr * 2 s)), s[0] = q[0]; identical in training and inference
            scale = self.running_level(X.abs().mean(dim=(2, 3)))[:, :, None, None].clamp(min=1e-6)      # [B,T,1,1]
        else:
            scale = X.abs().mean(dim=(1, 2, 3), keepdim=True) + 1e-6             # per-window level normalisation (non-causal)
        Xn = X / scale
        lm = torch.log(X.abs() + 1e-5)
        lm = (lm - STATS["mean"]) / STATS["std"] if STATS["mean"] is not None else lm
        f = torch.cat([Xn.real, Xn.imag, lm.clamp(-6, 6)], -1)                    # [B,T,1025,6]
        e = torch.stack([enc(f[:, :, a:b, :].reshape(B, T, -1)) for enc, (a, b) in zip(self.enc, self.bands)], 2)  # [B,T,48,feat]
        e = torch.tanh(e)
        nb = e.shape[2]
        h, _ = self.gru1(e.permute(0, 2, 1, 3).reshape(B * nb, T, self.feat))
        h, _ = self.gru2(h)
        if self.nbrnn:
            hb = h.reshape(B, nb, T, self.h2).permute(0, 2, 1, 3).reshape(B * T, nb, self.h2)   # [BT,48,h2]
            y, _ = self.brnn(hb)                                                  # bidirectional over the 48 bands
            h = (hb + torch.tanh(self.bproj(y))).permute(0, 2, 1)                 # [BT,h2,48]
        else:
            h = h.reshape(B, nb, T, self.h2).permute(0, 2, 3, 1).reshape(B * T, self.h2, nb)   # [BT,h2,48]
            h = h + self.mix1(h)
            h = h + self.mix2(h)
        h = h.reshape(B, T, self.h2, nb)
        g, _ = self.ggru(h.mean(-1))                                             # [B,T,hg]
        L = self.lookahead
        # output delay: the decoder at step t + L masks frame t, seeing the band and global STATES at t + L (the
        # last L frames of a stream reuse the final states while e[t] still varies — an end-of-stream convention
        # the model is trained with; never apply it at ordinary streaming chunk boundaries) plus, with dec_skip,
        # frame t's own band embedding
        idx = torch.clamp(torch.arange(T, device=X.device) + L, max=T - 1) if L else slice(None)
        parts = [h.permute(0, 1, 3, 2)[:, idx], g[:, idx, None, :].expand(-1, -1, nb, -1)]
        if self.dec_skip:
            parts.append(e)
        z = torch.cat(parts, -1)                                                  # [B,T,48,h2+hg(+feat)]
        m = torch.cat([dec(z[:, :, i, :]).reshape(B, T, b - a, self.mo) for i, (dec, (a, b)) in enumerate(zip(self.dec, self.bands))], 2)
        if self.mask_mode == "role_softmax":
            self.role_logits = m                              # V, D, B, eligible
            mask = torch.softmax(m, -1)[..., 3, None].expand(-1, -1, -1, 2).to(X.dtype)
        elif self.mask_mode == "complex_role":
            self.role_logits = m[..., 4:8]                    # fast V, D, B, eligible auxiliary head
            mask = torch.complex(m[..., 0:4:2], m[..., 1:4:2]) # stereo complex eligible-audio head
        elif self.mask_mode == "deep2":
            current = torch.complex(m[..., 0:4:2], m[..., 1:4:2])
            previous = torch.complex(m[..., 4:8:2], m[..., 5:8:2])
            self.last_taps = (current, previous)
            self.last_mask = current
            return self.apply_last_filter(X)
        elif self.mask_mode == "matrix2":
            diagonal = torch.complex(m[..., 0:4:2], m[..., 1:4:2])
            cross = torch.complex(m[..., 4:8:2], m[..., 5:8:2])  # L->R, R->L
            self.last_matrix = (diagonal, cross)
            self.last_mask = diagonal
            return self.apply_last_filter(X)
        elif self.mo == 1:
            mask = torch.sigmoid(m).expand(-1, -1, -1, 2).to(X.dtype)            # [B,T,1025,2] one real mask in [0,1] for L and R
        else:
            mask = torch.complex(m[..., 0::2], m[..., 1::2])                      # [B,T,1025,2] complex (L, R)
        self.last_mask = mask
        return X * mask


def multires_loss(torch, est_wav, tgt_wav, mix_wav=None, floor_rel=None, scale=1.0):
    """L1 on |STFT| at three resolutions of the waveforms: log1p-compressed magnitudes (default), or, with
    `mix_wav` + `floor_rel` (diag_gap 27.09: the treble is erased at -19 dB because quiet cells cost nothing
    under log1p, which is linear for small values), LOG magnitudes with a relative floor so a quiet treble cell
    costs as much as a loud bass cell.  astra 11: the floor is floor_rel x the MIXTURE's mean |STFT| at each
    resolution (same units, detached), and the whole bundle carries one fixed `scale` calibrated to the old
    bundle's output-gradient norm (diag_lossfloor.py)."""
    total = 0.0
    for n_fft, hop in ((512, 128), (1024, 256), (2048, 512)):
        win = torch.hann_window(n_fft, device=est_wav.device)
        E = torch.stft(est_wav.reshape(-1, est_wav.shape[-1]), n_fft, hop, window=win, return_complex=True)
        Tg = torch.stft(tgt_wav.reshape(-1, tgt_wav.shape[-1]), n_fft, hop, window=win, return_complex=True)
        if floor_rel is None:
            total = total + (torch.log1p(E.abs()) - torch.log1p(Tg.abs())).abs().mean()
        else:
            M = torch.stft(mix_wav.reshape(-1, mix_wav.shape[-1]), n_fft, hop, window=win, return_complex=True).abs()
            fl = (floor_rel * M.mean(dim=(1, 2), keepdim=True)).detach().clamp_min(1e-7)   # per (window, channel); astra 13: silence -> log(0)-log(0) = NaN
            total = total + (torch.log(E.abs() + fl) - torch.log(Tg.abs() + fl)).abs().mean()
    return scale * total / 3


REMIX_GROUPS = ((0, 1), (2, 3, 4), (5,), (6,))   # vocal / drums / bass / other: each group from its own song with --remix


def windows(torch, tracks, seconds, batch, rng, merge_none=None, with_vocal=False, with_sources=False, remix=0.0):
    """[B, 2, n] mixture and [B, 2, n] eligible target from seven-slot (or expanded) caches (+ the vocal stem with
    `with_vocal`, or the separate vocal / drum / bass stems with `with_sources`,
    for attribution losses).  `remix` = probability that a truth-pool window takes each REMIX_GROUPS group from an
    independent random song and offset (the standard cross-song stem remix; teacher windows are never remixed).  `tracks` may be a pair (truth_list, pseudo_list):
    then half of every batch comes from each."""
    n = int(seconds * SR)
    mixes, eligs, vocs, drums, basses = [], [], [], [], []
    pools = tracks if isinstance(tracks, tuple) else (tracks,)
    while len(mixes) < batch:
        pool = pools[0] if (len(pools) == 1 or rng.random() < TRUTH_FRAC) else pools[1]
        path = pool[rng.integers(len(pool))]
        N = MN.npy_header(path)[0][2]
        if N <= n:
            continue
        s = rng.integers(0, N - n)
        w = MN.expand_slots(MN.read_window(path, s, n)).astype(np.float32) / 32768   # [7,2,n]
        if remix > 0 and pool is pools[0] and rng.random() < remix:
            for grp in REMIX_GROUPS[1:]:                           # the vocal group stays from the first song
                p2 = pool[rng.integers(len(pool))]; N2 = MN.npy_header(p2)[0][2]
                if N2 <= n:
                    continue
                w2 = MN.expand_slots(MN.read_window(p2, rng.integers(0, N2 - n), n)).astype(np.float32) / 32768
                w[list(grp)] = w2[list(grp)]
        gains = (10 ** (rng.uniform(-6, 6, size=(7, 1, 1)) / 20)).astype(np.float32)
        w = w * gains
        mixes.append(w.sum(0)); eligs.append(w[list(ELIGIBLE_SLOTS)].sum(0))
        if with_vocal or with_sources:
            vocs.append(w[0] + w[1])
        if with_sources:
            drums.append(w[2] + w[3] + w[4]); basses.append(w[5])
    out = (torch.as_tensor(np.stack(mixes)), torch.as_tensor(np.stack(eligs)))
    if with_sources:
        return out + tuple(torch.as_tensor(np.stack(part)) for part in (vocs, drums, basses))
    return out + (torch.as_tensor(np.stack(vocs)),) if with_vocal else out


def loss_bandbal(torch, est, tgt, mix, floor_rel=3e-3, cap=100.0):
    """review 15 exp. 3: per-band magnitude L1 divided by the floored target-band level (cap x the window level at
    most), so a weak band's error counts like a strong band's - does weak-band gradient explain the treble erasure?"""
    lvl = (floor_rel * mix.abs().mean(dim=(1, 2), keepdim=True)).detach().clamp_min(1e-7)        # [B,1,1,2]
    total = 0.0
    for a, b in band_edges():
        tb = tgt[:, :, a:b].abs(); den = tb.mean(dim=(1, 2), keepdim=True).detach()
        den = torch.maximum(den, lvl).clamp_min(mix.abs().mean(dim=(1, 2), keepdim=True).detach() / cap)
        total = total + ((est[:, :, a:b].abs() - tb).abs() / den).mean()
    return total / len(band_edges())


def loss_attr(torch, mask, V, E, floor_rel=3e-3):
    """review 15 exp. 2: source attribution in the loss - the predicted mask on the aligned truth stems:
    transmitted vocal energy / vocal energy  +  eligible reconstruction error / eligible energy (floored)."""
    def energy(Z): return (Z.abs() ** 2).sum(dim=(1, 2, 3))
    # 28.09: a window whose vocal AND eligible stems are silent gave 0/0 = NaN at step < 1000 -> absolute floor too
    fl = (floor_rel ** 2 * energy(V + E).detach()).clamp_min(1e-6)
    vocal = energy(mask * V) / (energy(V).detach() + fl)
    elig = energy(mask * E - E) / (energy(E).detach() + fl)
    return (vocal + elig).mean()


def loss_source_attr(torch, mask, E, V, D, B, floor_rel=3e-3):
    """Balanced source-attributed retention and protected transmission.

    The same predicted mask is applied to the aligned truth stems.  Normalising
    each active source separately prevents a loud drum or bass window from
    hiding quiet vocal leakage, and the mixture-relative floor keeps silent
    sources finite.  This is a training loss, not a renderer metric.
    """
    energy = lambda Z: (Z.abs() ** 2).sum(dim=(1, 2, 3))
    floor = (floor_rel ** 2 * (energy(E) + energy(V) + energy(D) + energy(B)).detach()).clamp_min(1e-6)
    eligible = energy(mask * E - E) / (energy(E).detach() + floor)
    protected = sum(energy(mask * source) / (energy(source).detach() + floor)
                    for source in (V, D, B)) / 3.0
    return (eligible + protected).mean()


def loss_source_attr_operator(torch, net, E, V, D, B, floor_rel=3e-3):
    """Source-attributed retention/protection through the full inferred operator.

    The operator must first be inferred from the mixture, then applied without
    re-inference to each aligned true source. For matrix2 this includes both
    cross-channel terms. This has the same source normalization as the earlier
    shared-mask loss, allowing a controlled architecture-specific comparison.
    """
    energy = lambda Z: (Z.abs() ** 2).sum(dim=(1, 2, 3))
    floor = (floor_rel ** 2 * (energy(E) + energy(V) + energy(D) + energy(B)).detach()).clamp_min(1e-6)
    eligible = energy(net.apply_last_filter(E) - E) / (energy(E).detach() + floor)
    protected = sum(energy(net.apply_last_filter(source)) / (energy(source).detach() + floor)
                    for source in (V, D, B)) / 3.0
    return (eligible + protected).mean()


def loss_vocal_invariance(torch, with_vocal, without_vocal, eligible, mixture, floor_rel=3e-3):
    """Keep eligible audio fixed when only the training mixture's vocal level changes.

    Both predictions come from ordinary stereo mixtures. True stems construct
    the paired training inputs and normalize the objective; no stem is supplied
    at inference. A mixture-relative floor keeps silent eligible crops finite.
    """
    energy = lambda Z: (Z.abs() ** 2).sum(dim=(1, 2, 3))
    floor = (floor_rel ** 2 * energy(mixture).detach()).clamp_min(1e-6)
    return (energy(with_vocal - without_vocal) / (energy(eligible).detach() + floor)).mean()


def vocal_entry_counterfactual(torch, mixture, vocal):
    """Training-only pair: remove vocal until crop midpoint, then fade it in over 250 ms.

    Eligible/drum/bass signals stay sample-identical to the ordinary mixture.
    The live model still receives just its ordinary stereo input.
    """
    n = mixture.shape[-1]
    t = torch.arange(n, device=mixture.device, dtype=mixture.dtype)
    phase = ((t - n // 2) / (SR // 4)).clamp(0, 1)
    vocal_gain = 0.5 - 0.5 * torch.cos(np.pi * phase)
    return mixture - vocal * (1 - vocal_gain)[None, None, :]


def loss_vocal_entry_target(torch, entry_est, entry_mix, eligible,
                            log_loss=False, loss_floor=3e-3, loss_scale=1.0):
    """Anchor a 0.5-s-pre/3-s-post vocal-entry interval to true eligible audio."""
    n = entry_mix.shape[-1]
    estimate = istft(torch, entry_est, n)
    lo = n // 2 - SR // 2
    hi = min(n, n // 2 + 3 * SR)
    observed, target, mixture = (x[..., lo:hi] for x in (estimate, eligible, entry_mix))
    return (loss_audio(torch, stft(torch, observed), stft(torch, target),
                       stft(torch, mixture) if log_loss else None,
                       loss_floor if log_loss else None, loss_scale)
            + multires_loss(torch, observed, target,
                            mixture if log_loss else None,
                            loss_floor if log_loss else None, loss_scale))


def loss_role_ce(torch, logits, E, V, D, B, floor_rel=3e-3):
    """Four-role power target from aligned stems, with quiet cells downweighted.

    This trains the shared eligible mask as one member of a competing source
    distribution. The quarter-power cell weight limits loud bass dominance
    while retaining evidence from quieter instruments. Silence is excluded by
    the relative floor; the audio reconstruction loss still trains the output.
    """
    powers = torch.stack([(s.abs() ** 2).sum(-1) for s in (V, D, B, E)], -1).detach()
    total = powers.sum(-1)
    floor = (floor_rel ** 2 * total.mean(dim=(1, 2), keepdim=True)).clamp_min(1e-8)
    target = powers / total.clamp_min(1e-8)[..., None]
    weight = total.clamp_min(floor).pow(0.25)
    weight = torch.where(total > floor, weight, 0)
    ce = -(target * torch.log_softmax(logits, -1)).sum(-1)
    return ((ce * weight).sum(dim=(1, 2)) / weight.sum(dim=(1, 2)).clamp_min(1e-8)).mean()


def loss_role_focus(torch, logits, E, V, D, B, floor_rel=3e-3, purity=0.8):
    """Equal-role CE on source-dominant audible cells, including rare eligible cells.

    The ordinary soft role CE and audio losses still train mixed cells. This
    extra term only tests whether underweighted confident examples explain the
    role head's flat eligible output; it does not change the causal graph.
    """
    power = torch.stack([(s.abs() ** 2).sum(-1) for s in (V, D, B, E)], -1).detach()
    total = power.sum(-1)
    fraction, owner = (power / total.clamp_min(1e-8)[..., None]).max(-1)
    floor = (floor_rel ** 2 * total.mean(dim=(1, 2), keepdim=True)).clamp_min(1e-8)
    audible = total > floor
    cell_loss = -torch.log_softmax(logits, -1).gather(-1, owner[..., None]).squeeze(-1)
    weight = total.clamp_min(floor).pow(0.25)
    role_losses = []
    for role in range(4):
        chosen = (owner == role) & (fraction >= purity) & audible
        weighted = weight * chosen
        if bool(chosen.any()):
            role_losses.append((cell_loss * weighted).sum() / weighted.sum().clamp_min(1e-8))
    if not role_losses:
        return logits.sum() * 0
    return sum(role_losses) / len(role_losses)


def loss_audio(torch, est, tgt, mix=None, floor_rel=None, scale=1.0):
    """L1 on the magnitude STFT (linear; or, with `mix` + `floor_rel`, log with a floor relative to the mixture's
    mean |X| per window and channel, times `scale`) + 0.5 L1 on the complex STFT (unchanged in both modes)."""
    if floor_rel is None:
        mag = (est.abs() - tgt.abs()).abs().mean()
    else:
        fl = (floor_rel * mix.abs().mean(dim=(1, 2), keepdim=True)).detach().clamp_min(1e-7)   # [B,1,1,2]
        mag = (torch.log(est.abs() + fl) - torch.log(tgt.abs() + fl)).abs().mean()
    return scale * mag + 0.5 * (est - tgt).abs().mean()


def si_sdr(torch, est, ref):
    ref = ref - ref.mean(-1, keepdim=True); est = est - est.mean(-1, keepdim=True)
    a = (est * ref).sum(-1, keepdim=True) / ((ref ** 2).sum(-1, keepdim=True) + 1e-9)
    proj = a * ref; noise = est - proj
    return 10 * torch.log10(((proj ** 2).sum(-1) + 1e-9) / ((noise ** 2).sum(-1) + 1e-9))


def evaluate(torch, net, tracks, dev, seconds=20.0, songs=12):
    """SI-SDR of the eligible estimate vs the truth on DEV (MoisesDB) excerpts, and the leak: vocal energy in the estimate."""
    net.modules.eval()
    sdrs, leaks, base = [], [], []
    rng = np.random.default_rng(0)
    with torch.no_grad():
        for p in tracks[:songs]:
            N = MN.npy_header(p)[0][2]
            n = min(int(seconds * SR), N)
            s = MN.dev_excerpt(p, n) if N > n else 0
            w = torch.as_tensor(MN.expand_slots(MN.read_window(p, s, n)).astype(np.float32) / 32768, device=dev)
            mix = w.sum(0)[None]; elig = w[list(ELIGIBLE_SLOTS)].sum(0)[None]; voc = (w[0] + w[1])[None]
            est = istft(torch, net.separate_stft(stft(torch, mix)), n)
            sdrs.append(float(si_sdr(torch, est, elig).mean()))
            base.append(float(si_sdr(torch, mix, elig).mean()))                     # the mixture itself as the estimate
            # leak: how much of the vocal ends up in the estimate, relative to the vocal's own energy
            Xv = stft(torch, voc); Xe = stft(torch, est); Xt = stft(torch, elig)
            vcell = (Xv.abs() ** 2) > 10 * ((stft(torch, mix) - Xv).abs() ** 2 + 1e-9)     # vocal-owned cells (>= 10 dB)
            leaks.append(float(((Xe.abs() ** 2) * vcell).sum() / ((Xv.abs() ** 2) * vcell).sum().clamp(min=1e-9)))
    net.modules.train()
    return {"si_sdr": float(np.median(sdrs)) if sdrs else 0.0, "vocal_leak": float(np.median(leaks)) if leaks else 1.0,
            "mix_si_sdr": float(np.median(base)) if base else 0.0}


def train(a):
    torch = _torch()
    dev = "cuda" if torch.cuda.is_available() else "cpu"
    torch.manual_seed(a.seed); rng = np.random.default_rng(a.seed)
    split = MN.load_split(a.cache)
    tracks = [os.path.join(a.cache, t + ".npy") for t in split["train"]]
    if a.teacher_cache:
        tracks += [os.path.join(a.teacher_cache, f) for f in sorted(os.listdir(a.teacher_cache))
                   if f.endswith(".npy") and os.path.exists(os.path.join(a.teacher_cache, f[:-4] + ".json"))]
    dev_tracks = [os.path.join(a.cache, t + ".npy") for t in split["dev"]]
    net = (BandSepNet(torch, feat=a.feat, h1=a.h1, h2=a.h2, hg=a.bhg, brnn=a.band_rnn, lookahead=a.lookahead, bidir=a.bidir, dec_skip=a.dec_skip, norm=a.norm, mask=a.mask) if a.arch == "band" else SepNet(torch, ht=a.ht, hg=a.hg, dec=a.dec)).to(dev)
    if a.teacher_cache and a.half_half:
        tracks = (tracks[:len(split["train"])], tracks[len(split["train"]):])      # (truth, pseudo-labels)
        print(f"half/half batches: {len(tracks[0])} truth songs, {len(tracks[1])} pseudo-labelled", flush=True)
    n_songs = sum(len(t) for t in tracks) if isinstance(tracks, tuple) else len(tracks)
    print(f"device {dev}; {n_songs} training songs; params {sum(p.numel() for p in net.parameters())}, "
          f"~{net.macs} MAC per frame = {net.macs * FPS / 1e6:.0f} M MAC/s (+ one 2048-point FFT/iFFT pair per hop)", flush=True)
    os.makedirs(a.out, exist_ok=True)
    start = 0
    opt = torch.optim.AdamW(net.parameters(), lr=a.lr, weight_decay=1e-4)
    warm = torch.optim.lr_scheduler.LinearLR(opt, start_factor=0.05, total_iters=a.warmup)
    cos = torch.optim.lr_scheduler.CosineAnnealingLR(opt, max(1, a.steps - a.warmup))
    sched = torch.optim.lr_scheduler.SequentialLR(opt, [warm, cos], milestones=[a.warmup])
    best = -1e9
    if a.init:
        ck = torch.load(a.init, map_location=dev)
        net.modules.load_state_dict(ck["state"]); STATS.update(ck["stats"])
        if ck.get("opt"): opt.load_state_dict(ck["opt"])
        start = ck.get("step", 0)
        if ck.get("sched"):                                    # astra 13: replaying the warm-up gave 10x the LR at 1k
            sched.load_state_dict(ck["sched"])
        else:
            for _ in range(start): sched.step()
        best = ck.get("best", best)                            # astra 13: -1e9 let the next eval overwrite a better checkpoint
        print(f"resumed at step {start}, best so far {best:.2f}, lr {opt.param_groups[0]['lr']:.2e}", flush=True)
    import itertools
    counter = itertools.count(start)                            # astra 13: one RNG shared by the prefetch threads was not reproducible
    batches = MN.prefetch(lambda: windows(torch, tracks, a.seconds, a.batch, np.random.default_rng([a.seed, next(counter)]),
                                        remix=a.remix, with_vocal=a.attr > 0, with_sources=a.source_attr > 0 or a.role_ce > 0 or a.role_focus > 0 or a.vocal_invariance > 0 or a.vocal_entry_target > 0))
    if STATS["mean"] is None:
        with torch.no_grad():
            lm = torch.cat([torch.log(stft(torch, next(batches)[0].to(dev)).abs() + 1e-5).flatten() for _ in range(4)])
            STATS["mean"], STATS["std"] = float(lm.mean()), float(lm.std() + 1e-3)
    print(f"log-mag stats mean {STATS['mean']:.2f} std {STATS['std']:.2f}", flush=True)
    t0 = time.time()
    for step in range(start + 1, a.steps + 1):
        opt.zero_grad()
        for _ in range(a.accum):                                   # gradient accumulation: batch = a.batch x a.accum
            bt = [t.to(dev) for t in next(batches)]; mix, elig = bt[0], bt[1]
            X = stft(torch, mix); Tt = stft(torch, elig)
            Est = net.separate_stft(X)
            log_loss = a.loss == "log"
            loss = loss_audio(torch, Est, Tt, X if log_loss else None, a.loss_floor if log_loss else None, a.loss_scale)
            if a.bandbal > 0:
                loss = loss + a.bandbal * loss_bandbal(torch, Est, Tt, X)
            if a.attr > 0:
                loss = loss + a.attr * loss_attr(torch, net.last_mask, stft(torch, bt[2]), Tt)
            if a.source_attr > 0:
                V, D, B = (stft(torch, bt[2]), stft(torch, bt[3]), stft(torch, bt[4]))
                source_loss = (loss_source_attr_operator(torch, net, Tt, V, D, B)
                               if getattr(net, "mask_mode", None) == "matrix2" else
                               loss_source_attr(torch, net.last_mask, Tt, V, D, B))
                loss = loss + a.source_attr * source_loss
            if a.vocal_invariance > 0:
                quiet_mix = mix - bt[2]
                quiet_est = net.separate_stft(stft(torch, quiet_mix))
                loss = loss + a.vocal_invariance * loss_vocal_invariance(
                    torch, Est, quiet_est, Tt, X)
            if a.vocal_entry_target > 0:
                entry_mix = vocal_entry_counterfactual(torch, mix, bt[2])
                entry_X = stft(torch, entry_mix)
                entry_est = net.separate_stft(entry_X)
                loss = loss + a.vocal_entry_target * loss_vocal_entry_target(
                    torch, entry_est, entry_mix, elig, log_loss, a.loss_floor, a.loss_scale)
            if a.role_ce > 0 or a.role_focus > 0:
                V, D, B = (stft(torch, bt[2]), stft(torch, bt[3]), stft(torch, bt[4]))
            if a.role_ce > 0:
                loss = loss + a.role_ce * loss_role_ce(
                    torch, net.role_logits, Tt, V, D, B)
            if a.role_focus > 0:
                loss = loss + a.role_focus * loss_role_focus(torch, net.role_logits, Tt, V, D, B)
            if a.arch == "band":
                est_wav = istft(torch, Est, mix.shape[-1])
                loss = loss + multires_loss(torch, est_wav, elig, mix if log_loss else None, a.loss_floor if log_loss else None, a.loss_scale)
                if a.sisdr_after and step > a.sisdr_after:
                    loss = loss - 0.02 * si_sdr(torch, est_wav, elig).mean()
            (loss / a.accum).backward()
        torch.nn.utils.clip_grad_norm_(net.parameters(), 5.0)
        opt.step(); sched.step()
        if step % a.eval_every == 0 or step == a.steps:
            met = evaluate(torch, net, dev_tracks, dev)
            line = (f"step {step} loss {loss.item():.4f} dev si_sdr {met['si_sdr']:.2f} dB (mixture {met['mix_si_sdr']:.2f}) "
                    f"vocal_leak {met['vocal_leak']:.3f}  {time.time() - t0:.0f}s")
            print(line, flush=True)
            ck = {"state": net.modules.state_dict(), "stats": dict(STATS), "step": step, "dev": met,
                  "ht": a.ht, "hg": a.hg, "dec": a.dec, "arch": a.arch, "feat": a.feat, "h1": a.h1, "h2": a.h2, "bhg": a.bhg, "brnn": a.band_rnn, "lookahead": a.lookahead, "bidir": a.bidir, "dec_skip": a.dec_skip, "norm": a.norm, "mask": a.mask}
            torch.save({**ck, "opt": opt.state_dict(), "sched": sched.state_dict(), "best": max(best, met["si_sdr"])}, os.path.join(a.out, "last.pt"))
            if step in a.save_at:                    # rapid-loop validation: fixed-step snapshots (review 15)
                torch.save(ck, os.path.join(a.out, f"step{step}.pt"))
            score = met["si_sdr"]                    # sep1's leak-penalised score picked a near-silent checkpoint
            if score > best:
                best = score; torch.save(ck, os.path.join(a.out, "sep_net.pt"))
                open(os.path.join(a.out, "best.txt"), "w").write(line + "\n")
    open(os.path.join(a.out, "train_done.txt"), "w").write(f"{a.steps}\n")


def load(model_path):
    torch = _torch(); dev = "cuda" if torch.cuda.is_available() else "cpu"
    ck = torch.load(model_path, map_location=dev)
    net = (BandSepNet(torch, feat=ck.get("feat", 48), h1=ck.get("h1", 32), h2=ck.get("h2", 32), hg=ck.get("bhg", 32), brnn=ck.get("brnn", 0), lookahead=ck.get("lookahead", 0), bidir=ck.get("bidir", False), dec_skip=ck.get("dec_skip", False), norm=ck.get("norm", "window"), mask=ck.get("mask", "complex2")) if ck.get("arch") == "band"
           else SepNet(torch, ht=ck.get("ht", 16), hg=ck.get("hg", 64), dec=ck.get("dec", 24))).to(dev)
    net.modules.load_state_dict(ck["state"]); net.modules.eval(); STATS.update(ck["stats"])
    return torch, net, dev


def separate_array(loaded, x48):
    """x48 [2, n] float32 -> whole-file eligible estimate [2, n].

    Causality also requires streaming normalization, bounded lookahead and a
    stateful chunk implementation; this convenience path alone proves none.
    """
    torch, net, dev = loaded
    with torch.no_grad():
        x = torch.as_tensor(x48, device=dev, dtype=torch.float32)
        return istft(torch, net.separate_stft(stft(torch, x[None])), x.shape[1])[0].cpu().numpy()


def separate(a):
    import wave
    with wave.open(a.wav, "rb") as w:
        x = np.frombuffer(w.readframes(w.getnframes()), "<i2").reshape(-1, w.getnchannels()).T.astype(np.float32) / 32768
    y = separate_array(load(a.model), x)
    with wave.open(a.out, "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR); w.writeframes((np.clip(y, -1, 1).T * 32767).astype("<i2").tobytes())


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    t = sub.add_parser("train"); t.add_argument("--cache", required=True); t.add_argument("--teacher-cache"); t.add_argument("--out", required=True)
    t.add_argument("--steps", type=int, default=6000); t.add_argument("--batch", type=int, default=6); t.add_argument("--seconds", type=float, default=8.0)
    t.add_argument("--lr", type=float, default=1e-3); t.add_argument("--seed", type=int, default=1); t.add_argument("--eval-every", type=int, default=250)
    t.add_argument("--ht", type=int, default=16); t.add_argument("--hg", type=int, default=64); t.add_argument("--dec", type=int, default=24)
    t.add_argument("--init")
    t.add_argument("--arch", default="sep", choices=("sep", "band")); t.add_argument("--half-half", action="store_true")
    t.add_argument("--truth-frac", type=float, default=0.5, help="with --half-half: probability of drawing a truth window")
    t.add_argument("--warmup", type=int, default=2000); t.add_argument("--sisdr-after", type=int, default=0)
    t.add_argument("--feat", type=int, default=48); t.add_argument("--h1", type=int, default=32); t.add_argument("--h2", type=int, default=32); t.add_argument("--bhg", type=int, default=32)
    t.add_argument("--band-rnn", type=int, default=0, help="band model: hidden size of a bidirectional GRU across the bands (0 = the depthwise mixers)")
    t.add_argument("--lookahead", type=int, default=0, help="band model: frames of future context (output delay; 8 = 85 ms)")
    t.add_argument("--bidir", action="store_true", help="band model: bidirectional time GRUs (the non-causal diagnostic twin)")
    t.add_argument("--dec-skip", action="store_true", help="band model: the frame's own band embedding into the decoder (implied by --lookahead; alone = the L=0 control)")
    t.add_argument("--norm", default="window", choices=("window", "stream"), help="band model input level: one scalar per window (non-causal, default) or a causal 2-s running level")
    t.add_argument("--save-at", default="", help="comma-separated steps at which to save stepN.pt snapshots (proxy validation)")
    t.add_argument("--mask", default="complex2", choices=("complex2", "shared_sigmoid", "role_softmax", "complex_role", "deep2", "matrix2"), help="band model head: diagonal stereo complex mask (default), shared real mask, role mask, complex audio plus role, causal two-tap FIR, or full 2x2 stereo complex matrix")
    t.add_argument("--bandbal", type=float, default=0.0, help="weight of the band-balanced magnitude L1 (review 15 exp. 3; 0 = off)")
    t.add_argument("--attr", type=float, default=0.0, help="weight of the attribution loss on the truth stems (review 15 exp. 2; 0 = off)")
    t.add_argument("--source-attr", type=float, default=0.0,
                   help="weight of eligible retention plus separate vocal/drum/bass transmission loss (0 = off)")
    t.add_argument("--vocal-invariance", type=float, default=0.0,
                   help="weight of paired full-vocal versus no-vocal eligible-output consistency (matrix2 with --source-attr; 0 = off)")
    t.add_argument("--vocal-entry-target", type=float, default=0.0,
                   help="weight of true eligible reconstruction after a training-only midpoint vocal entry (matrix2 with --source-attr; 0 = off)")
    t.add_argument("--role-ce", type=float, default=0.0,
                   help="weight of four-role stem-power cross entropy (requires --mask role_softmax or complex_role; 0 = off)")
    t.add_argument("--role-focus", type=float, default=0.0,
                   help="extra equal-role CE on >=80%% pure audible cells; requires --mask role_softmax")
    t.add_argument("--loss", default="lin", choices=("lin", "log"), help="magnitude terms: linear L1 (default) or log-magnitude L1 with a relative floor")
    t.add_argument("--loss-floor", type=float, default=3e-3, help="--loss log: floor relative to the mixture's mean |X| per window and channel (3e-3 = -50 dB)")
    t.add_argument("--loss-scale", type=float, default=1.0, help="--loss log: fixed factor on the magnitude bundle (calibrate with diag_lossfloor.py)")
    t.add_argument("--remix", type=float, default=0.0, help="probability of a cross-song stem remix per truth window (0 = off)")
    t.add_argument("--accum", type=int, default=1, help="gradient accumulation steps (effective batch = batch x accum)")
    s = sub.add_parser("separate"); s.add_argument("--model", required=True); s.add_argument("--wav", required=True); s.add_argument("--out", required=True)
    a = ap.parse_args()
    if a.cmd == "train":
        if min(a.attr, a.source_attr, a.role_ce, a.role_focus, a.vocal_invariance, a.vocal_entry_target) < 0 or sum(v > 0 for v in (a.attr, a.source_attr, a.role_ce)) > 1:
            ap.error("--attr, --source-attr and --role-ce must be nonnegative and at most one can be active")
        if a.vocal_invariance > 0 and (a.mask != "matrix2" or a.source_attr <= 0):
            ap.error("--vocal-invariance requires --mask matrix2 and --source-attr")
        if a.vocal_entry_target > 0 and (a.mask != "matrix2" or a.source_attr <= 0 or a.vocal_invariance > 0):
            ap.error("--vocal-entry-target requires --mask matrix2 and --source-attr, without --vocal-invariance")
        if (a.mask in ("role_softmax", "complex_role")) != (a.role_ce > 0 or a.role_focus > 0):
            ap.error("--mask role_softmax/complex_role requires --role-ce or --role-focus, and these require a role head")
        if a.mask == "deep2" and (a.attr > 0 or a.source_attr > 0):
            ap.error("--mask deep2 requires source losses to apply both FIR taps; use audio losses for this probe")
        if a.mask == "matrix2" and a.attr > 0:
            ap.error("--mask matrix2 does not support --attr; use --source-attr on the full operator")
        global TRUTH_FRAC
        TRUTH_FRAC = a.truth_frac
        a.save_at = {int(x) for x in a.save_at.split(",") if x.strip()}
    {"train": train, "separate": separate}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
